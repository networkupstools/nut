#!/usr/bin/python2.7

# This Python script takes structure contents from existing legacy
# NUT *-mib.c sources. This is the first stage for DMF generation,
# which just dumps those structures as JSON markup that can be consumed
# by anyone interested.
#
#    Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
#    Copyright (C) 2016 - 2017 Jim Klimov <EvgenyKlimov@eaton.com>
#

from __future__ import print_function
import argparse
import copy
import functools
import json
import sys
import subprocess
import os

from pycparser import c_parser, c_ast, parse_file

__doc__ = """
Parses the mib description and prints all the snmp_info_t and info_lkp_t
structures found in the file. The output is dumped as JSON for easier
manipulation.
"""
__author__ = """
Michal Vyskocil <michal.vyskocil@gmail.com>
"""

# TODO:
#   - read the file to find and prints the comments

def warn (msg):
    print ("W: %s" % msg, file=sys.stderr)

def info (msg):
    print ("I: %s" % msg, file=sys.stderr)

def debug (msg):
    if os.environ.get("DEBUG") == "yes":
        print ("D: %s" % msg, file=sys.stderr)

def f2f(node):
    """convert c_ast node flags to list of numbers
    (1, 2, 4, 8, 4194304) == SU_FLAG_OK | SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_STALE | SU_FLAG_SEMI_STATIC
    """
    if  isinstance (node, c_ast.BinaryOp) and \
        node.op == "<<":
        return (int (node.left.value) << int (node.right.value), )
    elif isinstance (node, c_ast.Constant):
        return (int (node.value), )
    else:
        r = list()
        r.extend (f2f (node.left))
        r.extend (f2f (node.right))
    return r

def widen_tuples(iter, width, default=None):
    for item in iter:
        if len(item) < width:
            item = list(item)
            while len(item) < width:
                item.append(default)
            item = tuple(item)
        yield item

class Visitor(c_ast.NodeVisitor):

    def __init__(self, *args, **kwargs):
        super(Visitor, self).__init__(*args, **kwargs)
        self._mappings = {
            "INFO" : dict (),
            "MIB2NUT" : dict (),
            "SNMP-INFO" : dict (),
            "ALARMS-INFO" : dict ()}

    def _visit_snmp_info_t (self, node):
        ret = list ()
        for _, ilist in node.init.children ():
            ditem = dict ()

            kids = ilist.children ()

            # 0: const char *info_type
            _, info_type = kids [0]
            try:
                ditem ["info_type"] = info_type.value.strip ('"')
                # some pycparser versions need this check for 0 vs. NULL instead
                if ( ditem ["info_type"] == 0 ):
                    continue
                if ( ditem ["info_type"] == "0" ):
                    continue
                if ( ditem ["info_type"] == "NULL" ):
                    continue
            except AttributeError:
                # There is { NULL, 0, 0 ...} on the end of each structure
                # we should skip this one
                continue

            # This is a trick - we store _reference_ to ditem in list
            # so all modification becomes visible in the list
            ret.append (ditem)

            # 1: int info_flags
            _, info_flags = kids [1]
            if isinstance (info_flags, c_ast.Constant):
                ditem ["info_flags"] = (int (info_flags.value, 16), )
            elif isinstance (info_flags, c_ast.BinaryOp):
                assert info_flags.op == '|'
                ditem ["info_flags"] = (int (info_flags.left.value, 16), int (info_flags.right.value, 16))

            # 2: double info_len
            _, info_len = kids [2]
            ditem ["info_len"] = float (info_len.value.strip ('"'))

            # 3: const char *OID
            _, OID = kids [3]
            try:
                ditem ["OID"] = OID.value.strip ('"')
                # some pycparser versions need this check for 0 vs. NULL instead
                if ( ditem ["OID"] == "0" ):
                    ditem ["OID"] = None
            except:
                ditem ["OID"] = None

            # 4: const char *dfl
            # NOTE: Some MIB.C's incorrectly declare the value as numeric zero
            # rather than symbolic NULL. No more examples should remain in the
            # upstream NUT (recently fixed), but may happen in downstream forks
            _, default = kids [4]
            ditem ["dfl"] = None
            if isinstance (default, c_ast.Constant):
                if default.type == "string":
                    ditem ["dfl"] = default.value.strip ('"')
                elif default.type == "int":
                    # Note: after pycparser NULL is resolved into 0 too
                    # So we only warn if some other number is encountered
                    if ( int(default.value) != 0 ):
                        warn ("numeric value '%s' passed as 'char *dfl' in 'snmp_info_t'; ASSUMING this is explicit NULL for your platform" % default.value)
            elif isinstance (default, c_ast.Cast):
                ditem ["dfl"] = None

            # 5: unsigned long flags
            _, flags = kids [5]
            ditem ["flags"] = tuple (f2f (flags))

            # 6: info_lkp_t *oid2info
            _, oid2info = kids [6]
            ditem ["oid2info"] = None
            if isinstance (oid2info, c_ast.Cast):
                ditem ["oid2info"] = None
            elif isinstance (oid2info, c_ast.ID):
                ditem ["oid2info"] = oid2info.name
            elif isinstance (oid2info, c_ast.UnaryOp) and oid2info.op == '&':
                ditem ["oid2info"] = oid2info.expr.name.name

            # 7: int *setvar
            try:
                _, setvar = kids [7]
            except IndexError:
                warn ("%s: %d: missing setvar of %s" % (oid2info.coord.file, oid2info.coord.line, ditem ["info_type"]))
                ditem ["setvar"] = None
                continue

            if isinstance (setvar, c_ast.Cast):
                ditem ["setvar"] = None
            elif isinstance (setvar, c_ast.UnaryOp):
                ditem ["setvar"] = setvar.expr.name

        return tuple (ret)

    def _visit_info_lkp_t (self, node):
        # Depending on version of NUT and presence of WITH_SNMP_LKP_FUN macro,
        # the source code structure can have 2 fields (oid, value) or 6 fields
        # adding (fun_l2s, nuf_s2l, fun_s2l, nuf_l2s) pointers to lookup processing functions.
        ret = []
        for _, ilist in node.init.children ():
            key_node = ilist.exprs [0]
            if isinstance (key_node, c_ast.UnaryOp):
                key = -1 * int (key_node.expr.value)
            else:
                key = int (key_node.value)

            # array ends with {0, NULL} or {0, NULL, NULL, NULL}
            if isinstance (ilist.exprs [1], c_ast.Cast):
                continue

            # in some pycparser versions this check for {0, NULL} works instead
            if ( key == 0 ):
                if ( ilist.exprs [1].value.strip ('"') == "0" ):
                    # No quoted string for value
                    continue
                elif ( ilist.exprs [1] == "0" ):
                    # Numeric null pointer for value
                    continue

            # See https://stackoverflow.com/questions/21728808/extracting-input-parameters-and-its-identifier-type-while-parsing-a-c-file-using
            # for a bigger example of function introspection
            try:
                fun_l2s = ilist.exprs [2]
                if fun_l2s is not None:
                    debug("fun_l2s : %s" % (fun_l2s))
                    if fun_l2s.name is None:
                        debug("fun_l2s.name : None")
                        fun_l2s = '"' + fun_l2s + '"'
                    else:
                        debug("fun_l2s.name : %s" % (fun_l2s.name))
                        fun_l2s = str(fun_l2s.name)
            except (IndexError, NameError, AttributeError):
                fun_l2s = None

            try:
                nuf_s2l = ilist.exprs [3]
                if nuf_s2l is not None:
                    debug("nuf_s2l : %s" % (nuf_s2l))
                    if nuf_s2l.name is None:
                        debug("nuf_s2l.name : None")
                        nuf_s2l = '"' + nuf_s2l + '"'
                    else:
                        debug("nuf_s2l.name : %s" % (nuf_s2l.name))
                        nuf_s2l = str(nuf_s2l.name)
            except (IndexError, NameError, AttributeError):
                nuf_s2l = None

            try:
                fun_s2l = ilist.exprs [4]
                if fun_s2l is not None:
                    debug("fun_s2l : %s" % (fun_s2l))
                    if fun_s2l.name is None:
                        debug("fun_s2l.name : None")
                        fun_s2l = '"' + fun_s2l + '"'
                    else:
                        debug("fun_s2l.name : %s" % (fun_s2l.name))
                        fun_s2l = str(fun_s2l.name)
            except (IndexError, NameError, AttributeError):
                fun_s2l = None

            try:
                nuf_l2s = ilist.exprs [5]
                if nuf_l2s is not None:
                    debug("nuf_l2s : %s" % (nuf_l2s))
                    if nuf_l2s.name is None:
                        debug("nuf_l2s.name : None")
                        nuf_l2s = '"' + nuf_l2s + '"'
                    else:
                        debug("nuf_l2s.name : %s" % (nuf_l2s.name))
                        nuf_l2s = str(nuf_l2s.name)
            except (IndexError, NameError, AttributeError):
                nuf_l2s = None

            ret.append ((key, ilist.exprs [1].value.strip ('"'), fun_l2s, nuf_s2l, fun_s2l, nuf_l2s))
        return ret

    def _visit_mib2nut_info_t (self, node):
        ret = dict ()
        kids = [c for _, c in node.init.children ()]

        # 0 - 3, 5) mib_name - oid_auto_check, 5 sysOID
        for i, key in enumerate (("mib_name", "mib_version", "oid_pwr_status", "oid_auto_check", None, "sysOID")):
            if key is None:
                continue
            try:
                kids [i]
            except IndexError:
                ret [key] = None
                continue

            if isinstance (kids [i], c_ast.Cast):
                ret [key] = None
            else:
                ret [key] = kids [i].value.strip ('"')

            if ( ret [key] == "0" ):
                ret [key] = None

        # 4 snmp_info
        ret ["snmp_info"] = kids [4].name

        # 6 alarms_info
        if len (kids) == 6:
            warn ("alarms_info_t is missing for %s" % node.name)
        elif len (kids) > 6:
            ret ["alarms_info"] = kids [6].name
        return ret

    def _visit_alarms_info_t (self, node):
        lst = list ()
        for _, ilist in node.init.children ():
            kids = [k for _, k in ilist.children ()]
            ret = dict ()
            for i, key in enumerate (("OID", "status_value", "alarm_value")):
                try:
                    kids [i]
                except IndexError:
                    ret [key] = None
                    continue

                if isinstance (kids [i], c_ast.Cast):
                    ret [key] = None
                else:
                    ret [key] = kids [i].value.strip ('"')

                if ( ret [key] == "0" ):
                    ret [key] = None

            # skip alarm_info with all the values None
            if any(x is not None for x in ret.values ()):
                lst.append (ret)

        return lst

    def visit_Decl (self, node):
        if  isinstance (node.type, c_ast.ArrayDecl) and \
            isinstance (node.type.type, c_ast.TypeDecl) and \
            isinstance (node.type.type.type, c_ast.IdentifierType):

            if node.type.type.type.names == ['snmp_info_t']:
                self._mappings ["SNMP-INFO"][node.name] = self._visit_snmp_info_t (node)
            elif node.type.type.type.names == ['info_lkp_t']:
                self._mappings ["INFO"][node.type.type.declname] = \
                self._visit_info_lkp_t (node)
            elif node.type.type.type.names == ['alarms_info_t']:
                self._mappings ["ALARMS-INFO"][node.type.type.declname] = \
                self._visit_alarms_info_t (node)

        if  isinstance (node.type, c_ast.TypeDecl) and \
            isinstance (node.type.type, c_ast.IdentifierType) and \
            node.type.type.names == ['mib2nut_info_t'] and \
            node.storage == [] :
                self._mappings ["MIB2NUT"][node.name] = \
                self._visit_mib2nut_info_t (node)

# Find the shellscript to parse away undesired constructs from GNU CPP output
def s_cpp_path ():
    return \
        os.path.join (
            os.path.dirname (
                os.path.abspath (__file__)),
            "nut_cpp")

def s_info2c (fout, jsinfo):
    for key in jsinfo.keys ():
        print ("\nstatic info_lkp_t %s_TEST[] = {" % key, file=fout)
        gotfun = 0
        for key, value, fun_l2s, nuf_s2l, fun_s2l, nuf_l2s in widen_tuples(jsinfo [key],6):
            if nuf_l2s is None:
                if fun_s2l is None:
                    if nuf_s2l is None:
                        if fun_l2s is None:
                            # No NULLs in the end, because depending on macro value
                            # WITH_SNMP_LKP_FUN info_lkp_t can have more or less fields
                            print ("    { %d, \"%s\" }," % (key, value), file=fout)
                        else:
                            # No quotation for fun/nuf names!
                            print ("    { %d, \"%s\", %s }," % (key, value, fun_l2s), file=fout)
                            gotfun = 1
                    else:
                            print ("    { %d, \"%s\", %s, %s }," % (key, value, fun_l2s, nuf_s2l), file=fout)
                            gotfun = 1
                else:
                            print ("    { %d, \"%s\", %s, %s, %s }," % (key, value, fun_l2s, nuf_s2l, fun_s2l), file=fout)
                            gotfun = 1
            else:
                print ("    { %d, \"%s\", %s, %s, %s, %s }," % (key, value, fun_l2s, nuf_s2l, fun_s2l, nuf_l2s), file=fout)
                gotfun = 1
        if gotfun == 0:
            print ("    { 0, NULL }\n};\n", file=fout)
        else:
            print ("    { 0, NULL, NULL, NULL, NULL, NULL }\n};\n", file=fout)

def s_snmp2c (fout, js, name):
    print ("\nstatic snmp_info_t %s_TEST[] = {" % name, file=fout)
    for info in js[name]:
        pinfo = copy.copy (info)
        pinfo ["info_flags"] = functools.reduce (lambda x, y : x|y, pinfo ["info_flags"])
        pinfo ["flags"] = functools.reduce (lambda x, y : x|y, pinfo ["flags"])
        for k in ("OID", "oid2info", "dfl", "info_type", "setvar"):
            if not k in pinfo or pinfo [k] is None:
                pinfo [k] = "NULL"

        for k in ("dfl", "OID"):
            if isinstance (pinfo [k], int):
                continue
            if pinfo [k] != "NULL":
                pinfo [k] = '"' + pinfo [k] + '"'

        if pinfo ["setvar"] != "NULL":
            pinfo ["setvar"] = '&' + pinfo ["setvar"]
        print ('    { "%(info_type)s", %(info_flags)d, %(info_len)f, %(OID)s, %(dfl)s, %(flags)d, %(oid2info)s, %(setvar)s},' % pinfo, file=fout)
    print ("    { NULL, 0, 0, NULL, NULL, 0, NULL }", file=fout)
    print ("};", file=fout)

def s_mib2nut (fout, js, name):
    pinfo = copy.copy (js [name])
    pinfo ["name"] = name

    for key in ("mib_name", "mib_version", "oid_pwr_status", "oid_auto_check", "sysOID"):
        if pinfo.get (key) is None:
            pinfo [key] = "NULL"
        elif ( pinfo [key] == "0" ):
            pinfo [key] = "NULL"
        else:
            pinfo [key] = '"%s"' % pinfo [key]

    for key in ("snmp_info", "alarms_info"):
        if pinfo.get (key) is None:
            pinfo [key] = "NULL"
        elif ( pinfo [key] == "0" ):
            pinfo [key] = "NULL"

    print ("""
static mib2nut_info_t %(name)s_TEST = { %(mib_name)s, %(mib_version)s, %(oid_pwr_status)s, %(oid_auto_check)s, %(snmp_info)s, %(sysOID)s, %(alarms_info)s };
""" % pinfo, file=fout)

def s_json2c (fout, MIB_name, js):
    print ("""
#include <stdbool.h>
#include "main.h"
#include "snmp-ups.h"

// for setvar field
int input_phases, output_phases, bypass_phases;

// avoid macro-conflict with snmp-ups
#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_BUGREPORT
#endif
#include "%s.c"

// Replicate what drivers/main.c exports
int do_synchronous = 0;

static inline bool streq (const char* x, const char* y)
{
    if (!x && !y)
        return true;
    if (!x || !y) {
        fprintf(stderr, "\\nDEBUG: strEQ(): One compared string (but not both) is NULL:\\n\\t%%s\\n\\t%%s\\n\\n", x ? x : "<NULL>" , y ? y : "<NULL>");
        return false;
        }
    int cmp = strcmp (x, y);
    if (cmp != 0) {
        fprintf(stderr, "\\nDEBUG: strEQ(): Strings not equal (%%i):\\n\\t%%s\\n\\t%%s\\n\\n", cmp, x, y);
    }
    return cmp == 0;
}

static inline bool strneq (const char* x, const char* y)
{
    if (!x && !y) {
        fprintf(stderr, "\\nDEBUG: strNEQ(): Both compared strings are NULL\\n");
        return false;
        }
    if (!x || !y) {
        return true;
        }
    int cmp = strcmp (x, y);
    if (cmp == 0) {
        fprintf(stderr, "\\nDEBUG: strNEQ(): Strings are equal (%%i):\\n\\t%%s\\n\\t%%s\\n\\n", cmp, x, y);
    }
    return cmp != 0;
}

""" % MIB_name, file=fout)

    s_info2c (fout, js["INFO"])
    for key in js ["SNMP-INFO"].keys ():
        s_snmp2c (fout, js ["SNMP-INFO"], key)
    for key in js ["MIB2NUT"].keys ():
        s_mib2nut (fout, js ["MIB2NUT"], key)

    # generate test function
    print ("""
int main () {
    size_t i = 0;""", file=fout)

    for key in js["INFO"]:
        print ("""
    fprintf (stderr, "Test %(k)s: ");
    for (i = 0; %(k)s_TEST [i].oid_value != 0 && %(k)s_TEST [i].info_value != NULL; i++) {
        fprintf (stderr, "[%%zi] ", i);
        assert (%(k)s [i].oid_value == %(k)s_TEST [i].oid_value);
        assert (%(k)s [i].info_value && %(k)s_TEST [i].info_value);
        assert (streq (%(k)s [i].info_value, %(k)s_TEST [i].info_value));
    }
    fprintf (stderr, "OK\\n");""" % {'k' : key}, file=fout)

    for key in js ["SNMP-INFO"].keys ():
        print ("""
    fprintf (stderr, "Test %(k)s: ");
    for (i = 0; %(k)s_TEST [i].info_type != NULL; i++) {
        fprintf (stderr, "[%%zi] ", i);
        assert (streq (%(k)s [i].info_type, %(k)s_TEST [i].info_type));
        assert (%(k)s [i].info_flags == %(k)s_TEST [i].info_flags);
        assert (%(k)s [i].info_len == %(k)s_TEST [i].info_len);
        assert (streq (%(k)s [i].OID, %(k)s_TEST [i].OID));
        assert (streq (%(k)s [i].dfl, %(k)s_TEST [i].dfl));
        assert (%(k)s [i].flags == %(k)s_TEST [i].flags);
        if (%(k)s [i].oid2info != %(k)s_TEST [i].oid2info) {
            fprintf (stderr, "%(k)s[%%zi].oid2info     =<%%p>\\n", i, %(k)s[i].oid2info);
            fprintf (stderr, "%(k)s_TEST[%%zi].oid2info=<%%p>\\n", i, %(k)s_TEST[i].oid2info);
            return 1;
        }
        assert (%(k)s [i].setvar == %(k)s_TEST [i].setvar);
    }
    fprintf (stderr, "OK\\n");""" % {'k' : key}, file=fout)

    for key in js ["MIB2NUT"].keys ():
        print ("""
    fprintf (stderr, "Test %(k)s\\n");
    assert (streq (%(k)s_TEST.mib_name, %(k)s.mib_name));
    assert (streq (%(k)s_TEST.mib_version, %(k)s.mib_version));
    assert (streq (%(k)s_TEST.oid_pwr_status, %(k)s.oid_pwr_status));
    assert (streq (%(k)s_TEST.oid_auto_check, %(k)s.oid_auto_check));
    assert (%(k)s_TEST.snmp_info == %(k)s.snmp_info);
    assert (streq (%(k)s_TEST.sysOID, %(k)s.sysOID));
    assert (%(k)s_TEST.alarms_info == %(k)s.alarms_info);
""" % {'k' : key}, file=fout)

    print ("""
    return 0;
}
""", file=fout)

def s_mkparser ():
    p = argparse.ArgumentParser ()
    p.add_argument ("--test", default=False, action='store_true',
            help="compile json dump back to C and compare against original static structure")
    p.add_argument ("source", help="source code of MIB.c")
    return p

## MAIN
p = s_mkparser ()
args = p.parse_args (sys.argv[1:])
drivers_dir = os.path.dirname (os.path.abspath (args.source))
include_dir = os.path.abspath (os.path.join (drivers_dir, "../include"))
info ("CALL parse_file():")

try:
    gcc_cppflags = os.environ["CPPFLAGS"].split()
except KeyError:
    gcc_cppflags = []

try:
    ### NOTE: If 'nut-cpp' fails here and returns exit code != 0 alone,
    ### there is no exception; so to abort pycparser we also print some
    ### invalid C pragma so the parser does die early.
    ast = parse_file (
        args.source,
        use_cpp=True,
        cpp_path=s_cpp_path (),
        cpp_args=["-I"+drivers_dir, "-I"+include_dir] + gcc_cppflags
        )
    if not isinstance(ast, c_ast.FileAST):
        raise RuntimeError("Got a not c_ast.FileAST instance after parsing")
    c = 0
    for idx, node in ast.children ():
        c = c+1
    if c == 0 :
        raise RuntimeError ("Got no data in resulting tree")
except Exception as e:
    warn ("FAILED to parse %s: %s" % (args.source, e))
    sys.exit(1)

info ("Parsed %s OK" % args.source)

info ("CALL Visitor():")
v = Visitor ()
for idx, node in ast.children ():
    v.visit (node)

if args.test:
    info ("CALL test()")
    test_file = os.path.basename (args.source)
    MIB_name = os.path.splitext (test_file) [0]
    test_file = MIB_name + "_TEST.c"
    prog_file = MIB_name + "_TEST.exe"
    with open (test_file, "wt") as fout:
        s_json2c (fout, MIB_name, v._mappings)

    try:
        gcc = os.environ["CC"]
    except KeyError:
        gcc = "cc"

    try:
        gcc_cflags = os.environ["CFLAGS"].split()
    except KeyError:
        gcc_cflags = []

    try:
        gcc_ldflags = os.environ["LDFLAGS"].split()
    except KeyError:
        gcc_ldflags = []

    cmd = [gcc, "-std=c99", "-ggdb", "-I"+drivers_dir, "-I"+include_dir] + gcc_cflags + ["-o", prog_file, test_file] + gcc_ldflags
    info ("COMPILE: " + " ".join (cmd))
    try:
        subprocess.check_call (cmd)
    except subprocess.CalledProcessError as retcode:
        warn ("COMPILE FAILED with code %s" % retcode.returncode)
        sys.exit (retcode.returncode)
    info ("SELFTEST ./" + prog_file)
    try:
        subprocess.check_call ("./%s" % prog_file)
    except subprocess.CalledProcessError as retcode:
        warn ("SELFTEST FAILED with code %s" % retcode.returncode)
        sys.exit (retcode.returncode)
    info ("SELFTEST %s PASSED" % prog_file)

info ("JSONDUMP")
json.dump (v._mappings, sys.stdout, indent=4)
sys.exit (0)
