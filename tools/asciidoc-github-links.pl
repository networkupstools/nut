#!/usr/bin/env perl
#
# Convert certain text patterns into ASCIIDOC markup for GitHub links to
# issues, PRs, security advisories, etc.
#
# Copyright (C) 2023-2026 by Jim Klimov <jimklimov+nut@gmail.com>
# Based on earlier work with a stack of SED expressions in NUT::docs/Makefile.am

use strict;

# These may be eventually overridden by caller to re-use with other projects
# (need to add CLI or envvar inputs):
my $gh_orgname = "networkupstools";
my $gh_prjname = "nut";

# NOTE: Schema in regexes would vary, but in injected URLs must be specific
my $gh_schema_re = qr/[Hh][Tt][Tt][Pp][Ss]?:\/\//;
my $gh_schema = "https://";
my $gh_hostname_re = qr/github\.com/;
my $gh_hostname = "github.com";

# URI parts (under a project base URI) for singular item; note not all are
# consistently named singular/plural - for item vs. list). Also note that
# historically "issues" and "pulls" were handled by the same github-side
# handler so their URI parts could be interchangeable:
my $gh_uripart_pull = "pull";
my $gh_uripart_issue = "issues";
my $gh_uripart_secadv = "security/advisories";

my $issue_id_re = qr/[1-9][0-9]*/;
my $ghsa_id_re = qr/GHSA-[A-Za-z0-9][A-Za-z0-9][A-Za-z0-9][A-Za-z0-9]-[A-Za-z0-9][A-Za-z0-9][A-Za-z0-9][A-Za-z0-9]-[A-Za-z0-9][A-Za-z0-9][A-Za-z0-9][A-Za-z0-9]/;

my $end_issue_id_re = qr/[^0-9]|$/;
my $end_ghsa_id_re = qr/[^A-Za-z0-9]|$/;

# Values for easier substitution below:
my $gh_url_site = "${gh_schema}${gh_hostname}";
my $gh_url_org = "${gh_url_site}/${gh_orgname}";
my $gh_url_prj = "${gh_url_org}/${gh_prjname}";

local $/ = undef;
while (<>) {
    # 1. link to a sibling project's issue/pull/advisory e.g. [networkupstools/nut#2048]
    s/(link:${gh_schema_re}${gh_hostname_re}\/${gh_orgname}\/[a-zA-Z0-9.\/-]+\/[1-9][0-9]*\/*\[[^]]*)\#([1-9][0-9]*)/$1##$2/g;

    # 2. markdown links [text](url) -> link:url[text]
    s/\[([^]]*)\]\((${gh_schema_re}[^ ]*)\)/{ "link:" . $2 . "[" . $1 . "]" }/ge;

    # 3. link ellipsis ...
    s/(link:${gh_schema_re}${gh_hostname_re}\/[^ ]*)\.\.\.([^ ]*)/$1..$2/g;

    # 4. single [#GHSA-...]
    s%(\[#*)(${ghsa_id_re})(\])%{ "[link:${gh_url_prj}/${gh_uripart_secadv}/" . $2 . "[" . $2 . "]]" }%ge;

    # 5. "advisory GHSA-..." or "advisory #GHSA-..." (multiple words / space separated, can be broken across lines)
    s%\b(advisory)\s*\#*(${ghsa_id_re})(${end_ghsa_id_re})%{ "link:${gh_url_prj}/${gh_uripart_secadv}/" . $2 . "[" . $1 . " " . $2 . "]" . $3 }%ge;

    # 6. issue #123
    s%\b(issue)\s*\#(${issue_id_re})(${end_issue_id_re})%{ "link:${gh_url_prj}/${gh_uripart_issue}/" . $2 . "[" . $1 . " ##" . $2 . "]" . $3 }%ge;

    # 7. PR #123 or pull request #123 (multi-word "pull request" can be broken across lines)
    s%\b(PR|pull\s+request)\s*\#(${issue_id_re})(${end_issue_id_re})%{ "link:${gh_url_prj}/${gh_uripart_pull}/" . $2 . "[" . $1 . " ##" . $2 . "]" . $3 }%ge;

    # 8. [[ ,]#123
    s%([[,\s])\#(${issue_id_re})(${end_issue_id_re})%{ $1 . "link:${gh_url_prj}/${gh_uripart_issue}/" . $2 . "[##" . $2 . "]" . $3 }%ge;

    # 9. issue networkupstools/foo#123
    s%\b(issue)\s+${gh_orgname}\/([^ \s]+)\#(${issue_id_re})(${end_issue_id_re})%{ "link:${gh_url_org}/" . $2 . "/" . $gh_uripart_issue . "/" . $3 . "[" . $1 . " " . $2 . "##" . $3 . "]" . $4 }%ge;

    # 10. PR networkupstools/foo#123 or pull request networkupstools/foo#123 (multi-word)
    s%\b(PR|pull\s+request)\s+${gh_orgname}\/([^ \s]+)\#(${issue_id_re})(${end_issue_id_re})%{ "link:${gh_url_org}/" . $2 . "/" . $gh_uripart_pull . "/" . $3 . "[" . $1 . " " . $2 . "##" . $3 . "]" . $4 }%ge;

    # 11. [ ,]networkupstools/foo#123
    s%([,\s])${gh_orgname}\/([^ \s]+)\#(${issue_id_re})(${end_issue_id_re})%{ $1 . "link:${gh_url_org}/" . $2 . "/" . $gh_uripart_issue . "/" . $3 . "[" . $2 . "##" . $3 . "]" . $4 }%ge;

    # 12. ##123 -> #123
    s/\#(\#${issue_id_re})/$1/g;

    # 13. URL plus sign encoding
    s/(${gh_schema_re}[^ \+]*)([\]]*\+)/$1%2B/g;

    print $_;
}
