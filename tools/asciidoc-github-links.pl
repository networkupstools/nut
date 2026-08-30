#!/usr/bin/env perl
#
# Convert certain text patterns into ASCIIDOC markup for GitHub links to
# issues, PRs, security advisories, etc.
#
# Copyright (C) 2023-2026 by Jim Klimov <jimklimov+nut@gmail.com>
# Based on earlier work with a stack of SED expressions in NUT::docs/Makefile.am

use strict;

my $ghsa_id_re = "GHSA-[A-Za-z0-9][A-Za-z0-9][A-Za-z0-9][A-Za-z0-9]-[A-Za-z0-9][A-Za-z0-9][A-Za-z0-9][A-Za-z0-9]-[A-Za-z0-9][A-Za-z0-9][A-Za-z0-9][A-Za-z0-9]";

local $/ = undef;
while (<>) {
    # 1. link to a sibling project's issue/pull/advisory e.g. [networkupstools/nut#2048]
    s/(link:https*:\/\/github\.com\/networkupstools\/[a-zA-Z0-9.\/-]+\/[1-9][0-9]*\/*\[[^]]*)\#([1-9][0-9]*)/$1##$2/g;

    # 2. markdown links [text](url) -> link:url[text]
    s/\[([^]]*)\]\((https*:\/\/[^ ]*)\)/{ "link:" . $2 . "[" . $1 . "]" }/ge;

    # 3. link ellipsis ...
    s/(link:https*:\/\/github\.com\/[^ ]*)\.\.\.([^ ]*)/$1..$2/g;

    # 4. [#GHSA-...]
    s%(\[#*)($ghsa_id_re)(\])%{ "[link:https://github.com/networkupstools/nut/security/advisories/" . $2 . "[" . $2 . "]]" }%ge;

    # 5. advisory #GHSA-... (multiple words / space separated, can be broken across lines)
    s%\b(advisory)\s*\#*($ghsa_id_re)([^A-Za-z0-9]|$)%{ "link:https://github.com/networkupstools/nut/security/advisories/" . $2 . "[" . $1 . " " . $2 . "]" . $3 }%ge;

    # 6. issue #123
    s%\b(issue)\s*\#([1-9][0-9]*)([^0-9]|$)%{ "link:https://github.com/networkupstools/nut/issues/" . $2 . "[" . $1 . " ##" . $2 . "]" . $3 }%ge;

    # 7. PR #123 or pull request #123 (multi-word pull request can be broken across lines)
    s%\b(PR|pull\s+request)\s*\#([1-9][0-9]*)([^0-9]|$)%{ "link:https://github.com/networkupstools/nut/pull/" . $2 . "[" . $1 . " ##" . $2 . "]" . $3 }%ge;

    # 8. [[ ,]#123
    s%([[,\s])\#([1-9][0-9]*)([^0-9]|$)%{ $1 . "link:https://github.com/networkupstools/nut/issues/" . $2 . "[##" . $2 . "]" . $3 }%ge;

    # 9. issue networkupstools/foo#123
    s%\b(issue)\s+networkupstools\/([^ \s]+)\#([1-9][0-9]*)([^0-9]|$)%{ "link:https://github.com/networkupstools/" . $2 . "/issues/" . $3 . "[" . $1 . " " . $2 . "##" . $3 . "]" . $4 }%ge;

    # 10. PR networkupstools/foo#123 or pull request networkupstools/foo#123 (multi-word)
    s%\b(PR|pull\s+request)\s+networkupstools\/([^ \s]+)\#([1-9][0-9]*)([^0-9]|$)%{ "link:https://github.com/networkupstools/" . $2 . "/pull/" . $3 . "[" . $1 . " " . $2 . "##" . $3 . "]" . $4 }%ge;

    # 11. [ ,]networkupstools/foo#123
    s%([,\s])networkupstools\/([^ \s]+)\#([1-9][0-9]*)([^0-9]|$)%{ $1 . "link:https://github.com/networkupstools/" . $2 . "/issues/" . $3 . "[" . $2 . "##" . $3 . "]" . $4 }%ge;

    # 12. ##123 -> #123
    s/\#(\#[1-9][0-9]*)/$1/g;

    # 13. URL plus sign encoding
    s/(https*:\/\/[^ \+]*)([\]]*\+)/$1%2B/g;

    print $_;
}
