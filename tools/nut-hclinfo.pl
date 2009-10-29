#!/usr/bin/perl
#
#   Copyright (C) 2009 - Arnaud Quette <arnaud.quette@gmail.com>
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

# HCL file location and name
my $rawHCL="../data/driver.list";

# Website output
my $webJsonHCL="../docs/website/scripts/ups_data.js";
my $webStaticHCL="../docs/website/???";

################# MAIN #################

open my $fh,$rawHCL or die "error open file $rawHCL";

# JSON file header
open my $webJsonHCL, ">$webJsonHCL" || die "error $webJsonHCL : $!";
print $webJsonHCL "var UPSData =\n[\n";

# Parse the first line to get the NUT version:
# Network UPS Tools - @PACKAGE_VERSION@ - Hardware Compatibility List
my $line=<$fh>;
my $nut_version;
if ($line =~ /^\s*# Network UPS Tools - /) {
	$nut_version = substr $line, 22, 5;
	print "NUT version: ".$nut_version."\n";
}

# Parse the second line for the HCL format version
# version=2
my $line=<$fh>;
my $hcl_version;
if ($line =~ /^\s*# version=/) {
	$hcl_version = substr $line, 10, 1;
	print "HCL file format version: ".$hcl_version."\n";
}

my $first=1;

# Now loop on HCL entries
while($line=<$fh>)
{
	next if ($line =~ /^$/);	# ignore null lines
	next if ($line =~ /^\s*#/); # ignore comment lines

	# strip off all trailing tabs, spaces, new lines and returns
	$line =~ s/\s+$//;

	# replace all tabs by comma
	$line =~ s/\t/,/g;
	
	if ($first == 0) {
		print $webJsonHCL ",\n";
	}

	# and output the line 
	print $webJsonHCL "  [".$line."]";

	$first=0;
}

# JSON file footer
print $webJsonHCL "\n]";

