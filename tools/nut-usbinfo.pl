#!/usr/bin/perl
#   Current Version : 1.0
#   Copyright (C) 2008
#            Arnaud Quette <arnaud.quette@gmail.com>
#            dloic (loic.dardant AT gmail DOT com)
#
#	Based on the usbdevice.pl script, made for the Ubuntu Media Center
#   for the final use of the LIRC project.
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
   
use File::Find;
use strict;

# path to scan for USB_DEVICE pattern
my $scanPath="../drivers";

# HAL output file
my $outputHAL="../scripts/hal/ups-nut-device.fdi.in";

# Hotplug output file
my $outputHotplug="../scripts/hotplug/libhid.usermap";

# output file udev
my $outputUdev="../scripts/udev/nut-usbups.rules.in";

# array of products indexed by vendorID 
my %vendor;

# contain for each vendor, its name (and...)
my %vendorName;

################# MAIN #################

find(\&find_usbdevs,$scanPath);
&gen_usb_files;

################# SUB METHOD #################
sub gen_usb_files
{
	# HAL file header
	open my $outHAL, ">$outputHAL" || die "error $outputHAL : $!";
	print $outHAL '<?xml version="1.0" encoding="ISO-8859-1"?> <!-- -*- SGML -*- -->'."\n";
	print $outHAL '<deviceinfo version="0.2">'."\n";
	print $outHAL '  <device>'."\n";
	print $outHAL '    <match key="@HAL_DEVICE_MATCH_KEY@" string="usb_device">'."\n";

	# Hotplug file header
	open my $outHotplug, ">$outputHotplug" || die "error $outputHotplug : $!";
	print $outHotplug '# This file is generated and installed by the Network UPS Tools package.'."\n";
	print $outHotplug "#\n";
	print $outHotplug '# Sample entry (replace 0xVVVV and 0xPPPP with vendor ID and product ID respectively) :'."\n";
	print $outHotplug '# libhidups      0x0003      0xVVVV   0xPPPP    0x0000       0x0000       0x00         0x00';
	print $outHotplug '            0x00            0x00            0x00               0x00               0x00000000'."\n";
	print $outHotplug "#\n";
	print $outHotplug '# usb module   match_flags idVendor idProduct bcdDevice_lo bcdDevice_hi';
	print $outHotplug ' bDeviceClass bDeviceSubClass bDeviceProtocol bInterfaceClass bInterfaceSubClass';
	print $outHotplug ' bInterfaceProtocol driver_info'."\n";

	# Udev file header
	open my $outUdev, ">$outputUdev" || die "error $outputUdev : $!";
	print $outUdev '# This file is generated and installed by the Network UPS Tools package.'."\n\n";
	print $outUdev 'ACTION!="add", GOTO="nut-usbups_rules_end"'."\n";
	print $outUdev 'SUBSYSTEM=="usb_device", GOTO="nut-usbups_rules_real"'."\n";
	print $outUdev 'SUBSYSTEM=="usb", GOTO="nut-usbups_rules_real"'."\n";
	print $outUdev 'BUS!="usb", GOTO="nut-usbups_rules_end"'."\n\n";
	print $outUdev 'LABEL="nut-usbups_rules_real"'."\n";

	# generate the file in alphabetical order (first for VendorID, then for ProductID)
	foreach my $vendorId (sort { lc $a cmp lc $b } keys  %vendorName)
	{
		# HAL vendor header
		if ($vendorName{$vendorId}) {
			print $outHAL "\n      <!-- ".$vendorName{$vendorId}." -->\n";
		}
		print $outHAL "      <match key=\"usb_device.vendor_id\" int=\"".$vendorId."\">\n";

		# Hotplug vendor header
		if ($vendorName{$vendorId}) {
			print $outHotplug "\n# ".$vendorName{$vendorId}."\n";
		}

		# udev vendor header
		if ($vendorName{$vendorId}) {
			print $outUdev "\n# ".$vendorName{$vendorId}."\n";
		}

		foreach my $productId (sort { lc $a cmp lc $b } keys %{$vendor{$vendorId}})
		{
			# HAL device entry
			print $outHAL "        <!-- ".$vendor{$vendorId}{$productId}{"comment"}." -->\n";
			print $outHAL "        <match key=\"usb_device.product_id\" int=\"".$productId."\">\n";
   			print $outHAL '          <append key="info.category" type="string">battery</append>'."\n";
   			print $outHAL '          <merge key="info.capabilities" type="strlist">battery</merge>'."\n";
   			print $outHAL "          <merge key=\"info.addons\" type=\"strlist\">hald-addon-".$vendor{$vendorId}{$productId}{"driver"}."</merge>\n";
   			print $outHAL '          <merge key="battery.type" type="string">ups</merge>'."\n";
  			print $outHAL '        </match>'."\n";

			# Hotplug device entry
			print $outHotplug "# ".$vendor{$vendorId}{$productId}{"comment"}."\n";
			print $outHotplug "libhidups      0x0003      ".$vendorId."   ".$productId."    0x0000       0x0000       0x00";
			print $outHotplug "         0x00            0x00            0x00            0x00               0x00               0x00000000\n";

			# udev device entry
			print $outUdev "# ".$vendor{$vendorId}{$productId}{"comment"}.' - '.$vendor{$vendorId}{$productId}{"driver"}."\n";
			print $outUdev "SYSFS{idVendor}==\"".removeHexPrefix($vendorId);
			print $outUdev "\", SYSFS{idProduct}==\"".removeHexPrefix($productId)."\",";
			print $outUdev ' MODE="664", GROUP="@RUN_AS_GROUP@"'."\n";
		}
		# HAL vendor footer
		print $outHAL "      </match>\n";
	}
	# HAL footer
	print $outHAL "    </match>\n";
	print $outHAL "  </device>\n";
	print $outHAL "</deviceinfo>\n";
	
	# Udev footer
	print $outUdev "\n".'LABEL="nut-usbups_rules_end"'."\n";
}

sub find_usbdevs
{
	return $File::Find::prune = 1 if $_ eq '.svn';

	my $nameFile=$_;
	my $lastComment="";
	
	open my $file,$nameFile or die "error open file $nameFile";
	while(my $line=<$file>)
	{
		# catch comment (should permit comment on the precedent or on the current line of USB_DEVICE declaration)
		if($line =~/\s*\/\*(.+)\*\/\s*$/)
		{
			$lastComment=$1;
		}

		if($line =~/^\s*\{\s*USB_DEVICE\((.+)\,(.+)\)\s*/) # for example : { USB_DEVICE(MGE_VENDORID, 0x0001)... }
		{
			my $VendorID=trim($1);
			my $ProductID=trim($2);
			my $VendorName="";

			# special thing for backward declaration using #DEFINE
			# Format: #define VENDORID 0x???? /* vendor name */
			if(!($VendorID=~/\dx(\d|\w)+/))
			{
				open my $fh,$nameFile or die "error open file $nameFile";
				while(my $data=<$fh>)
				{
					# catch Vendor Name
					if($data =~/\s*\/\*(.+)\*\/\s*$/)
					{			
						$VendorName=$1;
					}
					# catch VendorID
					if ($data =~ /(#define|#DEFINE)\s+$VendorID\s+(\dx(\d|\w)+)/)
					{
						$VendorID=$2;
						last;
					}
				}
			}
			# same thing for the productID
			if(!($ProductID=~/\dx(\d|\w)+/))
			{
				my $data = do { open my $fh, $nameFile or die "error open file $nameFile"; join '', <$fh> };
				if ($data =~ /(#define|#DEFINE)\s+$ProductID\s+(\dx(\d|\w)+)/)
				{
					$ProductID=$2;
				}
				else
				{
					die "In file $nameFile, for product $ProductID, can't find the declaration of the constant";
				}
			}
			
			# store date (to be optimized)
			$vendorName{$VendorID}=$VendorName;
			$vendor{$VendorID}{$ProductID}{"comment"}=$lastComment;
			# process the driver name
			my $driver=$nameFile;
			if($nameFile=~/(.+)-hid\.c/) {
				$driver="usbhid-ups";
			}
			elsif ($nameFile eq "nut_usb.c") {
				$driver="bcmxcp_usb";
			}
			# FIXME: make a common matching rule *.c => *
			elsif ($nameFile eq "tripplite_usb.c") {
				$driver="tripplite_usb";
			}
			elsif ($nameFile eq "megatec_usb.c") {
				$driver="megatec_usb";
			}
			elsif ($nameFile eq "blazer_usb.c") {
				$driver="blazer_usb";
			}
			elsif ($nameFile eq "richcomm_usb.c") {
				$driver="richcomm_usb";
			}
			else {
				die "Unknown driver type: $nameFile";
			}
			$vendor{$VendorID}{$ProductID}{"driver"}=$driver;
		}
	}
}

sub removeHexPrefix {
	# make a local copy, not to alter the original entry
	my $string = $_[0];
	$string =~ s/0x//;
	return $string;
}

sub trim {
    my($str) = shift =~ m!^\s*(.+?)\s*$!i;
    defined $str ? return $str : return '';
}
