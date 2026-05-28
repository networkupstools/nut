#!/usr/bin/env perl

# Find unicode sequences in sources that should be ASCII
# (C programs/headers, asciidoc text)
#
# Copyright (C) 2026 Jim Klimov <jimklimov+nut@gmail.com>

my %hits;

while (<>) {
    # xE2x80x90 / xE2x80x92 / xE2x80x93 - various-length dashes (change to - or --)
    # xE2x86x92 - right arrow (change to =>)
    if (/\xE2[\x80\x82\x86]/) {
        print "${ARGV}:$.:\t$_" ;

        if (!(defined ($hits{ARGV}))) {
            $hits{$ARGV} = 0;
        }

        $hits{$ARGV} = $hits{$ARGV} + 1;
    }

    if (eof) { close ARGV; }
}

my $hitCount = scalar keys %hits;
if ($hitCount > 0) {
    die("FAILED: found Unicode characters in " .
        $hitCount . " ASCII source" .
        ($hitCount > 1 ? "s" : "" ) .
        ": " . (join ", ", keys %hits) ."\n");
}
