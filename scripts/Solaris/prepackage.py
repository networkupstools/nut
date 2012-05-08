#!/usr/bin/env python

import sys
import commands

# pkginfo.in file creation

platform = commands.getoutput('uname -s')
architecture = commands.getoutput('uname -p')

fp=open("pkginfo.in","w")
fp.write("PKG=\"NUT\"\n")
fp.write("NAME=\"Network UPS Tools\"\n")

if platform == "SunOS" and architecture == "i386":
	fp.write("ARCH=\"Solaris Intel\"\n")
elif platform == "SunOS" and architecture == "sparc":
	fp.write("ARCH=\"Solaris Sparc\"\n")
fp.write("VERSION=\"@PACKAGE_VERSION@\"\n")
fp.write("CATEGORY=\"application\"\n")
fp.write("VENDOR=\"http://www.networkupstools.org\"\n")
fp.write("EMAIL=\"\"\n")
fp.write("PSTAMP=\"\"\n")
fp.write("DESCRIPTION=\"Network UPS Tools (NUT) is a client/server monitoring system that allows computers to share uninterruptible power supply (UPS) and power distribution unit (PDU) hardware. Clients access the hardware through the server, and are notified whenever the power status changes.\"\n")
fp.write("BASEDIR=\"@prefix@\"\n")
fp.write("CLASSES=\"none\"\n")
fp.close()

# checkinstall script creation

fp=open("checkinstall","w")
fp.write("#!/bin/sh\n")
fp.write("\nexpected_platform=SunOS\n")
if platform == "SunOS" and architecture == "i386":
	fp.write("expected_architecture=i386\n")
elif platform == "SunOS" and architecture == "sparc":
	fp.write("expected_architecture=sparc\n")

fp.write("platform=`uname -s`\n")
fp.write("architecture=`uname -p`\n\n")

fp.write("if [ ${platform} -eq  ${expected_platform} ]; then\n")
fp.write("\tif [ ${architecture} -eq ${expected_architecture} ]; then\n")
fp.write("\t\techo \"Checkinstall complete\"\n")
fp.write("\telse\n")
fp.write("\t\techo \"This is not Solaris $architecture machine\"\n")
fp.write("\t\texit 1\n")
fp.write("\tfi\n")
fp.write("else\n")
fp.write("\techo \"This is not Solaris machine\"\n")
fp.write("\texit 1\n")
fp.write("fi\n")
fp.write("exit 0\n")

fp.close()


