#!/usr/bin/python

# This Python script takes structure contents from existing legacy
# NUT *-mib.c sources. This is the first stage for DMF generation,
# which just dumps those structures as JSON markup that can be consumed
# by anyone interested.
#
#    Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
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

def f2f(node):
    """convert c_ast node flags to list of numbers
    (1, 2, 4) == SU_FLAG_OK | SU_FLAG_STATIC | SU_FLAG_ABSENT
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
            except:
                ditem ["OID"] = None

            # 4: const char *dfl
            _, default = kids [4]
            if isinstance (default, c_ast.Constant):
                if default.type == "string":
                    ditem ["dfl"] = default.value.strip ('"')
                elif default.type == "int":
                    ditem ["dfl"] = int (default.value)
            elif isinstance (default, c_ast.Cast):
                ditem ["dfl"] = None

            # 5: unsigned long flags
            _, flags = kids [5]
            ditem ["flags"] = tuple (f2f (flags))

            # 6: info_lkp_t *oid2info
            _, oid2info = kids [6]
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
        ret = []
        for _, ilist in node.init.children ():
            key_node = ilist.exprs [0]
            if isinstance (key_node, c_ast.UnaryOp):
                key = -1 * int (key_node.expr.value)
            else:
                key = int (key_node.value)

            # array ends with {0, NULL}
            if isinstance (ilist.exprs [1], c_ast.Cast):
                continue

            ret.append ((key, ilist.exprs [1].value.strip ('"')))
        return ret

    def _visit_mib2nit_info_t (self, node):
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
                self._visit_mib2nit_info_t (node)

def s_cpp_path ():
    return \
        os.path.join (
            os.path.dirname (
                os.path.abspath (__file__)),
            "nut_cpp")

def s_info2c (fout, jsinfo):
    for key in jsinfo.keys ():
        print ("\nstatic info_lkp_t %s_TEST[] = {" % key, file=fout)
        for key, value in jsinfo [key]:
            print ("    { %d, \"%s\" }," % (key, value), file=fout)
        print ("    { 0, NULL }\n};\n", file=fout)

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
        else:
            pinfo [key] = '"%s"' % pinfo [key]

    for key in ("snmp_info", "alarms_info"):
        if pinfo.get (key) is None:
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

#include "%s.c"

static inline bool streq (const char* x, const char* y)
{
    if (!x && !y)
        return true;
    if (!x || !y)
        return false;
    return strcmp (x, y) == 0;
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
        assert (%(k)s [i].oid_value == %(k)s_TEST [i].oid_value);
        assert (%(k)s [i].info_value && %(k)s_TEST [i].info_value);
        assert (!strcmp (%(k)s [i].info_value, %(k)s_TEST [i].info_value));
    }
    fprintf (stderr, "OK\\n");""" % {'k' : key}, file=fout)

    for key in js ["SNMP-INFO"].keys ():
        print ("""
    fprintf (stderr, "Test %(k)s: ");
    for (i = 0; %(k)s_TEST [i].info_type != NULL; i++) {
        if (!streq (%(k)s [i].info_type, %(k)s_TEST [i].info_type)) {
            fprintf (stderr, "%(k)s[%%d].info_type=%%s\\n", i, %(k)s[i].info_type);
            fprintf (stderr, "%(k)s_TEST[%%d].info_type=%%s\\n", i, %(k)s_TEST[i].info_type);
            return 1;
        }
        assert (%(k)s [i].info_flags == %(k)s_TEST [i].info_flags);
        assert (%(k)s [i].info_len == %(k)s_TEST [i].info_len);
        assert (streq (%(k)s [i].OID, %(k)s_TEST [i].OID));
        if (!streq (%(k)s [i].dfl, %(k)s_TEST [i].dfl)) {
            fprintf (stderr, "%(k)s[%%d].dfl=%%s\\n", i, %(k)s[i].dfl);
            fprintf (stderr, "%(k)s_TEST[%%d].dfl=%%s\\n", i, %(k)s_TEST[i].dfl);
            return 1;
        }
        assert (%(k)s [i].flags == %(k)s_TEST [i].flags);
        if (%(k)s [i].oid2info != %(k)s_TEST [i].oid2info) {
            fprintf (stderr, "%(k)s[%%d].oid2info=<%%p>\\n", i, %(k)s[i].oid2info);
            fprintf (stderr, "%(k)s_TEST[%%d].oid2info=<%%p>\\n", i, %(k)s_TEST[i].oid2info);
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
ast = parse_file (
        args.source,
        use_cpp=True,
        cpp_path=s_cpp_path (),
        )
v = Visitor ()
for idx, node in ast.children ():
    v.visit (node)

if args.test:
    test_file = os.path.basename (args.source)
    MIB_name = os.path.splitext (test_file) [0]
    test_file = MIB_name + "_TEST.c"
    with open (test_file, "wt") as fout:
        s_json2c (fout, MIB_name, v._mappings)

    drivers_dir = os.path.dirname (os.path.abspath (args.source))
    include_dir = os.path.abspath (os.path.join (drivers_dir, "../include"))
    cmd = ["cc", "-std=c11", "-ggdb", "-I", drivers_dir, "-I", include_dir, "-o", MIB_name, test_file]
    print (" ".join (cmd), file=sys.stderr)
    subprocess.check_call (cmd)
    subprocess.check_call ("./%s" % MIB_name)

json.dump (v._mappings, sys.stdout, indent=4)
sys.exit (0)
