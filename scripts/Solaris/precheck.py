#!/usr/bin/env python

import sys

if sys.version_info >= ( 2, 6 ):
    import subprocess
    p = subprocess.Popen(["uname", "-s"], stdout=subprocess.PIPE)
    platform = p.communicate()[0]
    if p.returncode != 0:
        raise Exception("FAILED to get platform from 'uname -s'!")

    p = subprocess.Popen(["uname", "-p"], stdout=subprocess.PIPE)
    architecture = p.communicate()[0]
    if p.returncode != 0:
        raise Exception("FAILED to get architecture from 'uname -p'!")
else:
    import commands
    platform = commands.getoutput('uname -s')
    architecture = commands.getoutput('uname -p')

# checkinstall script creation
fp=open("checkinstall","w")
# Note: same arch is relevant for different bitnesses that
# can be discerned via `isainfo` further (if ever needed)
fp.write("#!/bin/sh\n")
fp.write("\nexpected_platform=SunOS\n")
if platform == "SunOS" and architecture == "i386":
	fp.write("expected_architecture=i386\n")
elif platform == "SunOS" and architecture == "sparc":
	fp.write("expected_architecture=sparc\n")

fp.write("platform=\"`uname -s`\"\n")
fp.write("architecture=\"`uname -p`\"\n\n")

fp.write("if [ \"${platform}\" -eq \"${expected_platform}\" ]; then\n")
fp.write("\tif [ \"${architecture}\" -eq \"${expected_architecture}\" ]; then\n")
fp.write("\t\techo \"Checkinstall complete\"\n")
fp.write("\telse\n")
fp.write("\t\techo \"This is not Solaris $architecture machine: platform='${platform}' expected_platform='${expected_platform}'\"\n")
fp.write("\t\texit 1\n")
fp.write("\tfi\n")
fp.write("else\n")
fp.write("\techo \"This is not Solaris machine: architecture='${architecture}' expected_architecture='${expected_architecture}'\"\n")
fp.write("\texit 1\n")
fp.write("fi\n")
fp.write("exit 0\n")

fp.close()
