#!/usr/bin/python

from __future__ import print_function

import argparse
import sys
import json
import xml.dom.minidom as MD

def mkElement (_element, **attrs):
    el = MD.Element (_element)
    for name, value in attrs.items ():
        el.setAttribute (name, str(value))
    return el

def mk_lookup (inp, root):
    if not "INFO" in inp:
        return

    for name, lookup in inp ["INFO"].items ():
        lookup_el = mkElement ("lookup", name=name)
        for oid, value in lookup:
            info_el = mkElement ("info", oid=oid, value=value)
            lookup_el.appendChild (info_el)
        root.appendChild (lookup_el)

def mk_alarms (inp, root):
    if not "ALARMS-INFO" in inp:
        return
    for name, lookup in inp ["ALARMS-INFO"].items ():
        lookup_el = mkElement ("alarm", name=name)
        for info in lookup:
            info_el = mkElement ("info_alarm", oid=info ["OID"], status=info ["status_value"], alarm=info ["alarm_value"])
            lookup_el.appendChild (info_el)
        root.appendChild (lookup_el)

def s_mkparser ():
    p = argparse.ArgumentParser ()
    p.add_argument ("json", help="json input file (default stdin)", default='-', nargs='?')
    return p

## MAIN
p = s_mkparser ()
args = p.parse_args (sys.argv[1:])

impl = MD.getDOMImplementation ()
doc = impl.createDocument (None, "nut", None)
root = doc.documentElement

inp = None
if args.json == '-':
    inp = json.load (sys.stdin)
else:
    with open (args.json, "rt") as fp:
        inp = json.load (fp)

mk_lookup (inp, root)
mk_alarms (inp, root)
print (doc.toprettyxml ())
