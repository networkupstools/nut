#!/usr/bin/python2.7

# This Python script takes structure contents from JSON markup generated
# by `jsonify-mib.py` and generates XML DMF structure that can be parsed
# by `dmf.c` routines.
#
#    Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
#    Copyright (C) 2016 Carlos Dominguez <CarlosDominguez@eaton.com>
#    Copyright (C) 2016 - 2019 Jim Klimov <EvgenyKlimov@eaton.com>
#

from __future__ import print_function

import argparse
import sys
import os
import json
import xml.dom.minidom as MD

# ext commons.h
#/* state tree flags */
ST_FLAG_RW = 0x0001
ST_FLAG_STRING = 0x0002
ST_FLAG_IMMUTABLE = 0x0004

# snmp-ups.h
SU_FLAG_OK = (1 << 0)		#/* show element to upsd - internal to snmp driver */
SU_FLAG_STATIC = (1 << 1)	#/* retrieve info only once. */
SU_FLAG_ABSENT = (1 << 2)	#/* data is absent in the device,
				# * use default value. */
SU_FLAG_STALE = (1 << 3)	#/* data stale, don't try too often - internal to snmp driver */
SU_FLAG_NEGINVALID = (1 << 4)	#/* Invalid if negative value */
SU_FLAG_UNIQUE = (1 << 5)	#/* There can be only be one
				# * provider of this info,
				# * disable the other providers */
# Free slot : setvar is now deprecated; support it to spew errors
SU_FLAG_SETINT = (1 << 6)	#/* save value */
SU_OUTLET = (1 << 7)	        #/* outlet template definition */
SU_CMD_OFFSET = (1 << 8)	#/* Add +1 to the OID index */

SU_STATUS_PWR = (0 << 8)	#/* indicates power status element */
SU_STATUS_BATT = (1 << 8)	#/* indicates battery status element */
SU_STATUS_CAL = (2 << 8)	#/* indicates calibration status element */
SU_STATUS_RB = (3 << 8)		#/* indicates replace battery status element */
SU_STATUS_NUM_ELEM = 4
SU_OUTLET_GROUP = (1 << 10)	#/* outlet group template definition */

#/* Phase specific data */
SU_PHASES = (0x3F << 12)
SU_INPHASES = (0x3 << 12)
SU_INPUT_1 = (1 << 12)		#/* only if 1 input phase */
SU_INPUT_3 = (1 << 13)		#/* only if 3 input phases */
SU_OUTPHASES = (0x3 << 14)
SU_OUTPUT_1 = (1 << 14)		#/* only if 1 output phase */
SU_OUTPUT_3 = (1 << 15)		#/* only if 3 output phases */
SU_BYPPHASES = (0x3 << 16)
SU_BYPASS_1 = (1 << 16)		#/* only if 1 bypass phase */
SU_BYPASS_3 = (1 << 17)		#/* only if 3 bypass phases */

#/* hints for su_ups_set, applicable only to rw vars */
SU_TYPE_INT = (0 << 18)		#/* cast to int when setting value */
SU_TYPE_STRING = (1 << 18)	#/* cast to string. FIXME: redundant with ST_FLAG_STRING */
SU_TYPE_TIME = (2 << 18)	#/* cast to int */
SU_TYPE_CMD = (3 << 18)		#/* instant command */
SU_TYPE_DAISY_1 = (1 << 19)
SU_TYPE_DAISY_2 = (2 << 19)
SU_FLAG_ZEROINVALID = (1 << 20)	#/* Invalid if "0" value */
SU_FLAG_NAINVALID = (1 << 21)	#/* Invalid if "N/A" value */

SU_FLAG_SEMI_STATIC = (1 << 22)	#/* retrieve info every few update walks. */

def die (msg):
    print ("E: " + msg, file=sys.stderr)
    sys.exit (1)

def warn (msg):
    print ("W: " + msg, file=sys.stderr)

def debug (msg):
    if os.environ.get("DEBUG") == "yes":
        print ("D: " + msg, file=sys.stderr)

def mkElement (_element, **attrs):
    el = MD.Element (_element)
    for name, value in attrs.items ():
        if value is None:
            continue
        el.setAttribute (name, str(value))
    return el

def widen_tuples(iter, width, default=None):
    for item in iter:
        if len(item) < width:
            item = list(item)
            while len(item) < width:
                item.append(default)
            item = tuple(item)
        yield item

def mk_lookup (inp, root):
    if not "INFO" in inp:
        return

    debug("INPUT : '%s'" % inp ["INFO"].items() )
    for name, lookup in inp ["INFO"].items ():
        lookup_el = mkElement ("lookup", name=name)
        debug ("name= '%s' lookup = '%s' (%d elem)" %(name, lookup, len(lookup) ))

        # We can have variable-length C structures, with trailing entries
        # assumed to be NULLified by the compiler if unspecified explicitly.
        for (oid, value, fun_l2s, nuf_s2l, fun_s2l, nuf_l2s) in widen_tuples(lookup, 6):
            debug ("Lookup '%s'[%d] = '%s' '%s' '%s' '%s' '%s'" %(name, oid, value, fun_l2s, nuf_s2l, fun_s2l, nuf_l2s))
            if fun_l2s is not None or nuf_s2l is not None or fun_s2l is not None or nuf_l2s is not None:
                # TODO: Provide equivalent LUA under special comments
                # markup in the C file, so we can copy-paste it in DMF?
                # The functionset can then be used to define the block
                # of LUA-C gateway functions used in these lookups.
                warn("BIG WARNING: DMF does not currently support lookup functions in original C code")
                info_el = mkElement ("lookup_info", oid=oid, value="dummy", fun_l2s=fun_l2s, nuf_s2l=nuf_s2l, fun_s2l=fun_s2l, nuf_l2s=nuf_l2s, functionset="lkp_func__"+name)
            else:
                info_el = mkElement ("lookup_info", oid=oid, value=value)
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

def mk_snmp (inp, root):
    if not "SNMP-INFO" in inp:
        return
    for name, lookup in inp ["SNMP-INFO"].items ():
        lookup_el = mkElement ("snmp", name=name)
        for info in lookup:

            kwargs = dict (
                    name=info ["info_type"],
                    default=info.get ("dfl"),
                    lookup=info.get ("oid2info"),
                    oid=info.get ("OID")
                    )

            ### process info_flags
            kwargs ["multiplier"] = info ["info_len"]

            # I detected some differences against the original structures
            # if the info_flags is ignored!
            for name, info_flag, value in (
                    ("writable", ST_FLAG_RW, "yes"),
                    ("string", ST_FLAG_STRING, "yes"),
                    ("immutable", ST_FLAG_IMMUTABLE, "yes"),
                    ):
                if not info_flag in info ["info_flags"]:
                    continue
                kwargs [name] = value
                info ["info_flags"].remove (info_flag)

            # ignore the "0" flag which means no bits set
            len1 = len (info ["info_flags"])
            if 0 in info ["info_flags"]:
                info ["info_flags"].remove (0)
                len2 = len (info ["info_flags"])
                if ((len1-1) != len2):
                    die ("Killed too much in info_flags array!")

            # This is a assert - if there are info_flags we do not cover,
            # fail here!!! (Mostly useful for NUT forks that might have a
            # different schema that this stock script does not cover OOB).
            if len (info ["info_flags"]) > 0:
                die ("There are unprocessed items in info_flags (len == %d) in '%s'" % ( (len (info ["info_flags"])), info, ))

            ### process flags
            for name, flag, value in (
                    ("flag_ok", SU_FLAG_OK, "yes"),
                    ("static", SU_FLAG_STATIC, "yes"),
                    ("semistatic", SU_FLAG_SEMI_STATIC, "yes"),
                    ("absent", SU_FLAG_ABSENT, "yes"),
                    ("stale", SU_FLAG_STALE, "yes"),
                    ("positive", SU_FLAG_NEGINVALID, "yes"),
                    ("unique", SU_FLAG_UNIQUE, "yes"),
                    ("power_status", SU_STATUS_PWR, "yes"),
                    ("battery_status", SU_STATUS_BATT, "yes"),
                    ("calibration", SU_STATUS_CAL, "yes"),
                    ("replace_battery", SU_STATUS_RB, "yes"),
                    ("command", SU_TYPE_CMD, "yes"),
                    ("outlet_group", SU_OUTLET_GROUP, "yes"),
                    ("outlet", SU_OUTLET, "yes"),
                    ("output_1_phase", SU_OUTPUT_1, "yes"),
                    ("output_3_phase", SU_OUTPUT_3, "yes"),
                    ("input_1_phase", SU_INPUT_1, "yes"),
                    ("input_3_phase", SU_INPUT_3, "yes"),
                    ("bypass_1_phase", SU_BYPASS_1, "yes"),
                    ("bypass_3_phase", SU_BYPASS_3, "yes"),
                    ("type_daisy", SU_TYPE_DAISY_1, "1"),
                    ("type_daisy", SU_TYPE_DAISY_2, "2"),
                    ("zero_invalid", SU_FLAG_ZEROINVALID, "yes"),
                    ("na_invalid", SU_FLAG_NAINVALID, "yes"),
                    ):
                if not flag in info ["flags"]:
                    continue
                kwargs [name] = value
                info ["flags"].remove (flag)

            # ignore flags not relevant to XML generations
            for flag in (SU_FLAG_OK, SU_TYPE_STRING, SU_TYPE_INT):
                if flag in info ["flags"]:
                    info ["flags"].remove (flag)

            if SU_FLAG_SETINT in info ["flags"]:
                die ("Obsoleted and removed SU_FLAG_SETINT in flags for '%s'", (info, ))
#                if not "setvar" in info:
#                    die ("SU_FLAG_SETINT in flags, but not setvar for '%s'", (info, ))
#                kwargs ["setvar"] = info ["setvar"]
#                info ["flags"].remove (SU_FLAG_SETINT)

            # ignore the "0" flag which means no bits set
            len1 = len (info ["flags"])
            if 0 in info ["flags"]:
                info ["flags"].remove (0)
                len2 = len (info ["flags"])
                if ((len1-1) != len2):
                    die ("Killed too much in flags array!")

            # This is a assert - if there are info_flags we do not cover, fail here!!!
            if len (info ["flags"]) > 0:
                die ("There are unprocessed items in flags (len == %d) in '%s'" % ( (len (info ["flags"])), info, ))

            info_el = mkElement ("snmp_info", **kwargs)
            lookup_el.appendChild (info_el)
        root.appendChild (lookup_el)

def mk_mib2nut (inp, root):
    if not "MIB2NUT" in inp:
        return

    for name, lookup in inp ["MIB2NUT"].items ():

        kwargs = dict (name=name)
        for attr, key in (
                ("oid", "sysOID"),
                ("version", "mib_version"),
                ("power_status", "oid_pwr_status"),
                ("auto_check", "oid_auto_check"),
                ("mib_name", "mib_name"),
                ("snmp_info", "snmp_info"),
                ("alarms_info", "alarms_info")):
            if not key in lookup or lookup [key] is None:
                continue
            kwargs [attr] = lookup [key]

        lookup_el = mkElement ("mib2nut", **kwargs)
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
mk_snmp (inp, root)
mk_mib2nut (inp, root)
print (doc.toprettyxml ())
