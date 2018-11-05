#!/usr/bin/env python
#   Copyright (C) 2011 - Frederic Bohe <FredericBohe@Eaton.com>
#   Copyright (C) 2016 - Arnaud Quette <ArnaudQuette@Eaton.com>
#   Copyright (C) 2016 - Jim Klimov <EvgenyKlimov@Eaton.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

# This program extracts all SNMP information related to NUT snmp-ups drivers.

import glob
import re
import sys
import os

TOP_SRCDIR = os.getenv('TOP_SRCDIR');
if TOP_SRCDIR is None:
    TOP_SRCDIR="..";

TOP_BUILDDIR = os.getenv('TOP_BUILDDIR');
if TOP_BUILDDIR is None:
    TOP_BUILDDIR="..";

output_file_name = TOP_BUILDDIR + "/tools/nut-scanner/nutscan-snmp.c"
output_file = open(output_file_name,'w')

#expand #define constant
def expand_define(filename,constant):
	ret_line = ""
	f = open(filename, 'r')
	for line in f:
		if constant in line and "#define" in line:
			line_without_carriage_return  = re.sub("[\n\r]", "", line)
			line_with_single_blank = re.sub("[ \t]+", " ", line_without_carriage_return)
			define_line = line_with_single_blank.split(" ");
			#define_line[0] = "#define"
			#define_line[1] = const name
			#define_line[2...] = const value (may be other const name)
			if constant in define_line[1]:
				define_line.pop(0) #remove #define
				define_line.pop(0) #remove the constant name
				for elem in define_line:
					if elem[0] == "\"":
						clean_elem = re.sub("\"", "", elem)
						ret_line = ret_line + clean_elem
					else:
						ret_line = ret_line + expand_define(filename,elem);
	return ret_line


output_file.write( "/* nutscan-snmp.c - fully generated during build of NUT\n" )
output_file.write( " *  Copyright (C) 2011 - Frederic Bohe <FredericBohe@Eaton.com>\n" )
output_file.write( " *  Copyright (C) 2016 - Arnaud Quette <ArnaudQuette@Eaton.com>\n" )
output_file.write( " *  Copyright (C) 2016 - Jim Klimov <EvgenyKlimov@Eaton.com>\n" )
output_file.write( " *\n" )
output_file.write( " *  This program is free software; you can redistribute it and/or modify\n" )
output_file.write( " *  it under the terms of the GNU General Public License as published by\n" )
output_file.write( " *  the Free Software Foundation; either version 2 of the License, or\n" )
output_file.write( " *  (at your option) any later version.\n" )
output_file.write( " *\n" )
output_file.write( " *  This program is distributed in the hope that it will be useful,\n" )
output_file.write( " *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n" )
output_file.write( " *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" )
output_file.write( " *  GNU General Public License for more details.\n" )
output_file.write( " *\n" )
output_file.write( " *  You should have received a copy of the GNU General Public License\n" )
output_file.write( " *  along with this program; if not, write to the Free Software\n" )
output_file.write( " *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA\n" )
output_file.write( " */\n" )
output_file.write( "\n" )
output_file.write( "#include \"nutscan-snmp.h\"\n" )
output_file.write( "\n" )
output_file.write( "#ifndef NULL\n" )
output_file.write( "#define NULL (void*)0ULL\n" )
output_file.write( "#endif\n" )
output_file.write( "\n" )
output_file.write( "// marker to tell humans and GCC that the unused parameter is there for some\n" )
output_file.write( "// reason (i.e. API compatibility) and compiler should not warn if not used\n" )
output_file.write( "#ifndef UNUSED_PARAM\n" )
output_file.write( "# ifdef __GNUC__\n" )
output_file.write( "#  define UNUSED_PARAM __attribute__ ((__unused__))\n" )
output_file.write( "# else\n" )
output_file.write( "#  define UNUSED_PARAM\n" )
output_file.write( "# endif\n" )
output_file.write( "#endif\n" )
output_file.write( "\n" )
output_file.write( "/* SNMP IDs device table, not used in this file itself - silence the warning if we can */\n" )
output_file.write( "snmp_device_id_t snmp_device_table_builtin[] UNUSED_PARAM = {\n" )

for filename in sorted(glob.glob(TOP_SRCDIR + '/drivers/*-mib.c')):
	list_of_line = open(filename,'r').read().split(';')
	for line in list_of_line:
		if "mib2nut_info_t" in line:
			# Discard commented lines
			# Note that we only search for the beginning of the comment, the
			# end can be in the following line, due to the .split(';')
			m = re.search(r'/\*.*', line)
			if m:
				#sys.stderr.write('discarding line'+line+'\n')
				continue
			#clean up line
			line2 = re.sub("[\n\t\r}]", "", line)
			# split line
			line = line2.split("{",1)
			#line[1] is the part between {}
			line2 = line[1].split(",")
			mib = line2[0]
			#line2[3] is the OID of the device model name which
			#could be made of #define const and string.
			source_oid = line2[3]
			#line2[5] is the SysOID of the device which
			#could be made of #define const and string.
			if len(line2) >= 6:
				source_sysoid = line2[5]
			else:
				source_sysoid = "NULL"

			#decode source_oid
			line = source_oid.lstrip(" ")
			line2 = line.split(" ")

			oid = ""
			for elem in line2:
				if elem[0] == "\"":
					clean_elem = re.sub("\"", "", elem)
					oid = oid+clean_elem
				else:
					oid = oid + expand_define(filename,elem);

			#decode source_sysoid
			line = source_sysoid.lstrip(" ")
			line = line.rstrip(" ")
			line2 = line.split(" ")

			sysoid = ""
			for elem in line2:
				if elem[0] == "\"":
					clean_elem = re.sub("\"", "", elem)
					sysoid = sysoid+clean_elem
				else:
					sysoid = sysoid + expand_define(filename,elem);

			if sysoid == "":
				sysoid = "NULL"
			else:
				sysoid = "\"" + sysoid + "\""

			output_file.write( "\t{ \"" + oid + "\", " + mib + ", " + sysoid + "},\n" )

output_file.write( "        /* Terminating entry */\n" )
output_file.write( "        { NULL, NULL, NULL}\n" )
output_file.write( "};\n" )
output_file.write( "\n" )
