#!/usr/bin/env perl

# Report ShellCheck findings that a change adds to NUT shell scripts.
#
# Many existing scripts have known ShellCheck findings, so checking the
# whole tree would always fail. Instead, compare the shell scripts that
# changed since a base Git revision: run shellcheck on their base and
# current versions, and report only the findings that are new.
#
# Findings are matched by file, ShellCheck code and the text of the
# reported source line (not its number), so code moving around within
# a file does not count as new findings.
#
# Usage (from anywhere in the Git workspace):
#
#     perl tools/shellcheck-ci.pl [BASE_REF]
#
# BASE_REF defaults to "origin/master". The current versions are taken
# from the working tree, so uncommitted changes are checked too. Both
# versions are checked with the current .shellcheckrc file.
#
# Exit code is 1 if new "error" or "warning" level findings were found,
# 2 for usage or tool problems, else 0. New "note" level findings (info
# and style in ShellCheck terms) are reported, but do not fail the check.
#
# In GitHub Actions (GITHUB_ACTIONS=true) findings are printed as workflow
# commands, so they show as annotations on the changed lines, and listed
# in the job summary (GITHUB_STEP_SUMMARY), if available.
#
# Only core Perl 5 modules are used, and the "gcc" output format of
# shellcheck is parsed, to not require a JSON module on older systems.
#
# Copyright (C) 2026 by nmbro <nmbro@users.noreply.github.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.

use strict;
use warnings;
use Cwd qw(getcwd);
use File::Basename qw(dirname);
use File::Path qw(mkpath);
use File::Temp qw(tempdir);

my $base = 'origin/master';
if (@ARGV > 1 || (@ARGV == 1 && $ARGV[0] =~ /^-/)) {
    print STDERR "Usage: $0 [BASE_REF]\n";
    exit 2;
}
$base = $ARGV[0] if (@ARGV == 1);

my $gha = (defined($ENV{GITHUB_ACTIONS}) && $ENV{GITHUB_ACTIONS} eq 'true');

# Run a command (list form, no shell), return its stdout and exit code
sub run_cmd {
    my @cmd = @_;
    my $out = '';
    my $pid = open(my $fh, '-|');
    if (!defined($pid)) {
        die "ERROR: can not fork for: @cmd\n";
    }
    if ($pid == 0) {
        exec { $cmd[0] } @cmd or exit 127;
    }
    binmode($fh);
    { local $/; $out = <$fh>; }
    close($fh);
    $out = '' unless defined($out);
    return ($out, $? >> 8);
}

sub have_tool {
    my $tool = shift;
    foreach my $dir (split(/:/, $ENV{PATH} || '')) {
        return 1 if ($dir ne '' && -x "$dir/$tool");
    }
    return 0;
}

sub is_shell_script {
    # Same detection as `make check-scripts-syntax` (see Makefile.am)
    my ($out, $rc) = run_cmd('file', '-b', shift);
    return ($rc == 0 && $out =~ /shell script/);
}

# Return { current path => base path or undef } for added/changed files
sub changed_files {
    my ($out, $rc) = run_cmd('git', 'diff', '--name-status', '-z', '-M',
        '--diff-filter=AMR', $base, '--');
    die "ERROR: git diff failed\n" if ($rc != 0);
    my @fields = split(/\0/, $out);
    my %files;
    while (@fields) {
        my $status = shift(@fields);
        last if ($status eq '');
        if ($status =~ /^R/) {
            my $old = shift(@fields);
            my $new = shift(@fields);
            $files{$new} = $old;
        } else {
            my $path = shift(@fields);
            $files{$path} = ($status eq 'A') ? undef : $path;
        }
    }
    return %files;
}

# Run shellcheck in a directory, return a list of finding hashes
sub run_shellcheck {
    my ($dir, @paths) = @_;
    return () unless (@paths);
    my ($out, $rc);
    {
        my $cwd = getcwd();
        chdir($dir) or die "ERROR: can not chdir to $dir\n";
        ($out, $rc) = run_cmd('shellcheck', '-f', 'gcc', '--', @paths);
        chdir($cwd) or die "ERROR: can not chdir back to $cwd\n";
    }
    # 0 = no findings, 1 = findings, anything else = could not check
    if ($rc != 0 && $rc != 1) {
        print STDERR "ERROR: shellcheck failed with exit code $rc in $dir\n";
        exit 2;
    }
    my @findings;
    foreach my $line (split(/\n/, $out)) {
        if ($line =~ /^(.+?):(\d+):(\d+): (\w+): (.*) \[SC(\d+)\]$/) {
            push(@findings, {
                file => $1, line => $2, column => $3,
                level => $4, message => $5, code => $6,
            });
        }
    }
    return @findings;
}

my %line_cache;
sub finding_key {
    my ($dir, $f) = @_;
    my $path = "$dir/$f->{file}";
    if (!exists($line_cache{$path})) {
        my @lines;
        if (open(my $fh, '<', $path)) {
            binmode($fh);
            @lines = <$fh>;
            close($fh);
        }
        $line_cache{$path} = \@lines;
    }
    my $text = $line_cache{$path}->[$f->{line} - 1];
    $text = '' unless defined($text);
    $text =~ s/^\s+//;
    $text =~ s/\s+$//;
    return join("\0", $f->{file}, $f->{code}, $text);
}

sub escape_data {
    my $s = shift;
    $s =~ s/%/%25/g;
    $s =~ s/\r/%0D/g;
    $s =~ s/\n/%0A/g;
    return $s;
}

sub escape_property {
    my $s = escape_data(shift);
    $s =~ s/:/%3A/g;
    $s =~ s/,/%2C/g;
    return $s;
}

sub escape_markdown {
    my $s = shift;
    $s =~ s/([\\`*_\[\]<>|])/\\$1/g;
    return $s;
}

sub report {
    my $f = shift;
    my $code = "SC$f->{code}";
    if ($gha) {
        my $cmd = ($f->{level} eq 'error' || $f->{level} eq 'warning')
            ? $f->{level} : 'notice';
        printf("::%s file=%s,line=%d,col=%d,title=%s::%s\n",
            $cmd, escape_property($f->{file}), $f->{line}, $f->{column},
            escape_property("ShellCheck $code ($f->{level})"),
            escape_data("$f->{message} See https://www.shellcheck.net/wiki/$code"));
    } else {
        printf("%s:%d:%d: %s: %s [%s]\n", $f->{file}, $f->{line},
            $f->{column}, $f->{level}, $f->{message}, $code);
    }
}

sub write_summary {
    my $text = shift;
    my $path = $ENV{GITHUB_STEP_SUMMARY};
    return unless ($gha && defined($path) && $path ne '');
    if (open(my $fh, '>>', $path)) {
        print $fh $text;
        close($fh);
    }
}

sub summary_table {
    my @findings = @_;
    # Link the locations to the checked out (PR merge) commit, if known
    my $blob;
    if ($ENV{GITHUB_SERVER_URL} && $ENV{GITHUB_REPOSITORY} && $ENV{GITHUB_SHA}) {
        $blob = "$ENV{GITHUB_SERVER_URL}/$ENV{GITHUB_REPOSITORY}/blob/$ENV{GITHUB_SHA}/";
    }
    my $text = sprintf("### ShellCheck: %d new finding(s) in changed shell scripts\n\n",
        scalar(@findings));
    $text .= "| Location | Level | Code | Message |\n|---|---|---|---|\n";
    foreach my $f (@findings) {
        my $loc = escape_markdown("$f->{file}:$f->{line}:$f->{column}");
        $loc = "[$loc]($blob$f->{file}#L$f->{line})" if (defined($blob));
        my $code = "SC$f->{code}";
        $text .= "| $loc | $f->{level} | [$code](https://www.shellcheck.net/wiki/$code) | "
            . escape_markdown($f->{message}) . " |\n";
    }
    $text .= "\nOnly new \"warning\" and \"error\" level findings fail the check."
        . " Run `perl tools/shellcheck-ci.pl` locally to check before pushing"
        . " changes.\n\n";
    return $text;
}

my ($topdir, $rc) = run_cmd('git', 'rev-parse', '--show-toplevel');
if ($rc != 0) {
    print STDERR "ERROR: not in a Git workspace\n";
    exit 2;
}
chomp($topdir);
chdir($topdir) or die "ERROR: can not chdir to $topdir\n";

foreach my $tool ('git', 'shellcheck', 'file') {
    if (!have_tool($tool)) {
        print STDERR "ERROR: $tool is not installed\n";
        exit 2;
    }
}

(undef, $rc) = run_cmd('git', 'rev-parse', '--verify', '--quiet', "$base^{commit}");
if ($rc != 0) {
    print STDERR "ERROR: unknown base revision: $base\n";
    exit 2;
}

my %all = changed_files();
my %files;
foreach my $new (keys %all) {
    $files{$new} = $all{$new} if (-f $new && is_shell_script($new));
}
my @paths = sort(keys %files);
if (!@paths) {
    print "No changed shell scripts since $base\n";
    write_summary("### ShellCheck: no changed shell scripts\n\n");
    exit 0;
}
printf("Checking %d changed shell script(s) since %s: %s\n",
    scalar(@paths), $base, join(' ', @paths));

my @head = run_shellcheck($topdir, @paths);

# Base versions go under their current names into a scratch tree,
# with the current settings file, so the findings are comparable
my %base_counts;
{
    my $basedir = tempdir('shellcheck-ci-XXXXXX', TMPDIR => 1, CLEANUP => 1);
    if (-f '.shellcheckrc') {
        open(my $in, '<', '.shellcheckrc') or die "ERROR: .shellcheckrc: $!\n";
        open(my $out, '>', "$basedir/.shellcheckrc") or die "ERROR: $!\n";
        print $out $_ while (<$in>);
        close($in);
        close($out);
    }
    my @base_paths;
    foreach my $new (@paths) {
        my $old = $files{$new};
        next unless (defined($old));
        my ($content, $grc) = run_cmd('git', 'show', "$base:$old");
        die "ERROR: git show $base:$old failed\n" if ($grc != 0);
        mkpath(dirname("$basedir/$new"));
        open(my $fh, '>', "$basedir/$new") or die "ERROR: $!\n";
        binmode($fh);
        print $fh $content;
        close($fh);
        push(@base_paths, $new);
    }
    foreach my $f (run_shellcheck($basedir, @base_paths)) {
        $base_counts{finding_key($basedir, $f)}++;
    }
}

my @new_findings;
foreach my $f (@head) {
    my $key = finding_key($topdir, $f);
    if ($base_counts{$key}) {
        $base_counts{$key}--;
    } else {
        push(@new_findings, $f);
    }
}

report($_) foreach (@new_findings);

my %levels;
$levels{$_->{level}}++ foreach (@new_findings);
my $summary = join(', ', map { "$levels{$_} $_" }
    grep { $levels{$_} } ('error', 'warning', 'note'));
my $failed = ($levels{error} || $levels{warning}) ? 1 : 0;
printf("%s new ShellCheck finding(s) since %s: %s\n",
    ($failed ? 'FAILED:' : 'OK:'), $base, ($summary ne '' ? $summary : 'none'));

if (@new_findings) {
    write_summary(summary_table(@new_findings));
} else {
    write_summary(sprintf("### ShellCheck: no new findings in %d changed shell script(s)\n\n",
        scalar(@paths)));
}
exit($failed);
