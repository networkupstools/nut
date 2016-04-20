#!/usr/bin/python

from __future__ import print_function

import xml.dom.minidom as MD

"""
impl = MD.getDOMImplementation()
doc = impl.createDocument(None, "nut", None)
root = doc.documentElement

lkp1 = MD.Element ("lookup")
lkp1.setAttribute ("name", "lkp1")

print (nut.toprettyxml ())
"""

JSON = \
{
    "INFO": {
        "pw_alarm_lb": [
            [
                1,
                "LB"
            ],
            [
                2,
                ""
            ]
        ],
        }
}

impl = MD.getDOMImplementation ()
doc = impl.createDocument (None, "nut", None)
root = doc.documentElement

def mkElement (_element, **attrs):
    el = MD.Element (_element)
    for name, value in attrs.items ():
        el.setAttribute (name, str(value))
    return el

for name, lookup in JSON ["INFO"].items ():
    lookup_el = mkElement ("lookup", name=name)
    for oid, value in lookup:
        info_el = mkElement ("info", oid=oid, value=value)
        lookup_el.appendChild (info_el)
    root.appendChild (lookup_el)

print (doc.toprettyxml ())
