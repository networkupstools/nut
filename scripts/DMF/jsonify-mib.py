from __future__ import print_function
import json
import sys
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
        self._mappings = {"INFO" : dict ()}

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
                ditem ["dfl"] = default.value.strip ('"')
            elif isinstance (default, c_ast.Cast):
                ditem ["dfl"] = None

            # 5: unsigned long flags
            _, flags = kids [5]
            ditem ["flags"] = tuple (f2f (flags))

            # 6: info_lkp_t *oid2info
            _, oid2info = kids [6]
            if isinstance (oid2info, c_ast.Cast):
                ditem ["oid2info"] = None
            if isinstance (oid2info, c_ast.ID):
                ditem ["oid2info"] = oid2info.name

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
        ret = {}
        for _, ilist in node.init.children ():
            key_node = ilist.exprs [0]
            if isinstance (key_node, c_ast.UnaryOp):
                key = -1 * int (key_node.expr.value)
            else:
                key = int (key_node.value)

            # array ends with {0, NULL}
            if isinstance (ilist.exprs [1], c_ast.Cast):
                continue

            ret [key] = ilist.exprs [1].value.strip ('"')
        return ret

    def visit_Decl (self, node):
        if  isinstance (node.type, c_ast.ArrayDecl) and \
            isinstance (node.type.type, c_ast.TypeDecl) and \
            isinstance (node.type.type.type, c_ast.IdentifierType):

            if node.type.type.type.names == ['snmp_info_t']:
                self._mappings [node.name] = self._visit_snmp_info_t (node)
            elif node.type.type.type.names == ['info_lkp_t']:
                self._mappings ["INFO"][node.type.type.declname] = \
                self._visit_info_lkp_t (node)

def s_cpp_path ():
    return \
        os.path.join (
            os.path.dirname (
                os.path.abspath (__file__)),
            "nut_cpp")

## MAIN

ast = parse_file (
        sys.argv[1],
        use_cpp=True,
        cpp_path=s_cpp_path (),
        )
v = Visitor ()
for idx, node in ast.children ():
    v.visit (node)

json.dump (v._mappings, sys.stdout, indent=4)
