#!/usr/bin/env perl

$temp = "prototype1";
$prototype="prototype";
$pkginfo = "pkginfo";
$checkinstall = "checkinstall";
$preinstall = "preinstall";
$postinstall = "postinstall";
$preremove = "preremove";
$postremove = "postremove";

open (PREPROTO,"< $temp") || die "Unable to read prototype information ($!)\n";
open (PROTO,"> $prototype") || die "Unable to write file prototype ($!)\n";
print PROTO "i pkginfo=./$pkginfo\n";
print PROTO "i checkinstall=./$checkinstall\n";
print PROTO "i preinstall=./$preinstall\n";
print PROTO "i postinstall=./$postinstall\n";
print PROTO "i preremove=./$preremove\n";
print PROTO "i postremove=./$postremove\n";
while (<PREPROTO>) {
	# Read the prototype information from /tmp/prototype$$
	chomp;
	$thisline = $_;
	if ($thisline =~ " prototype1 ") {
	  # We don't need that line
	} elsif ($thisline =~ "^[fd] ") {
	  # Change the ownership for files and directories
	  ($dir, $none, $file, $mode, $user, $group) = split / /,$thisline;
	  print PROTO "$dir $none $file=$file $mode nut nut\n";
	} else {
	  # Symlinks and other stuff should be printed as well ofcourse
	  print PROTO "$thisline\n";
	}
}
print PROTO "f $none nut $mode root nut\n";
close PROTO;
close PREPROTO;
