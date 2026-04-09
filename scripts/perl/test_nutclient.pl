#!/usr/bin/perl
# -*- coding: utf-8 -*-

# This source code is provided for testing/debugging purpose ;)
# This script is a Perl equivalent of scripts/python/module/test_nutclient.py.in

use strict;
use warnings;
use UPS::Nut;
use Term::ANSIColor;

# Main logic
if (1) {
    my $NUT_HOST = $ENV{'NUT_HOST'} || '127.0.0.1';
    my $NUT_PORT = $ENV{'NUT_PORT'} || '3493';
    my $NUT_USER = $ENV{'NUT_USER'} || undef;
    my $NUT_PASS = $ENV{'NUT_PASS'} || undef;
    my $NUT_SSL  = (($ENV{'NUT_SSL'} || "false") eq "true") ? 1 : 0;
    my $NUT_FORCESSL = (($ENV{'NUT_FORCESSL'} || "false") eq "true" || ($ENV{'NUT_FORCESSL'} || "false") eq "1") ? 1 : 0;
    my $NUT_CERTVERIFY = (($ENV{'NUT_CERTVERIFY'} || "false") eq "true" || ($ENV{'NUT_CERTVERIFY'} || "false") eq "1") ? 1 : 0;
    my $NUT_CAFILE = $ENV{'NUT_CAFILE'} || undef;
    my $NUT_CAPATH = $ENV{'NUT_CAPATH'} || undef;
    # Note: Python's cert_file, key_file, key_pass are not directly supported by current Nut.pm STARTTLS as independent args,
    # but passed via %arg. Nut.pm uses STARTTLS method which takes %arg.

    my $NUT_DEBUG = (($ENV{'DEBUG'} || "false") eq "true" || defined($ENV{'NUT_DEBUG_LEVEL'})) ? 1 : 0;

    # Account "unexpected" failures (more due to coding than circumstances)
    # e.g. lack of protected access when no credentials were passed is okay
    my @failed = ();

    print "UPS::Nut test...\n";

    my $nut;
    eval {
        $nut = UPS::Nut->new(
            # A name is used right where we initialize the object,
            # otherwise it falls back to "default". It should be
            # registered in ups.conf; one UPS at a time per connection!
            # We fiddle those for NIT script (dummy, UPS1, UPS2) below.
            NAME => "dummy",
            HOST => $NUT_HOST,
            PORT => $NUT_PORT,
            USERNAME => $NUT_USER,
            PASSWORD => $NUT_PASS,
            DEBUG => $NUT_DEBUG,
            # TRACKING => 'ON', # undef by default, enabled in certain tests below
            # STARTTLS related (passed via %arg to StartTLS in Nut.pm)
            CERTVERIFY => $NUT_CERTVERIFY,
            CAPATH => $NUT_CAPATH, # Nut.pm uses CAPATH for SSL_ca_file
            FORCESSL => $NUT_FORCESSL,
            # In case PyNUT's cert_file, key_file are needed:
            SSL_cert_file => $ENV{'NUT_CERTFILE'},
            SSL_key_file => $ENV{'NUT_KEYFILE'},
            SSL_key_pass => $ENV{'NUT_KEYPASS'},
            SSL_ca_file => $NUT_CAFILE,
        );
    };
    if ($@ || !defined($nut)) {
        my $ex = $@ || $nut->Error();
        print "EXCEPTION during initialization: $ex\n";
        if ($NUT_SSL && ($ex =~ /FEATURE-NOT-CONFIGURED/ || $ex =~ /FEATURE-NOT-SUPPORTED/)) {
            print "(anticipated error: server does not support STARTTLS)\n";
            exit(0);
        }
        die $ex;
    }

    print "-" x 80 . "\nTesting 'ListUPS' :\n";
    my $result = $nut->ListUPS();
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? join(', ', keys %$result) : "NULL" );

    # [dummy]
    # driver = dummy-ups
    # desc = "Test device"
    # port = /src/nut/data/evolution500.seq
    print "-" x 80 . "\nTesting 'ListVar' for 'dummy' (should be registered in ups.conf) :\n";
    # TOTHINK: Extend into a test for ListVar("bogus") - that it should fail?
    $result = $nut->ListVar("dummy");
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? join(', ', map { "$_ => $result->{$_}" } keys %$result) : "NULL" );

    print "-" x 80 . "\nTesting 'CheckUPSAvailable' (via ListUPS) :\n";
    my $ups_list = $nut->ListUPS();
    $result = exists($ups_list->{"dummy"}) ? "Available" : "Not Available";
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", $result );

    print "-" x 80 . "\nTesting 'ListCmd' :\n";
    $result = $nut->ListCmd();
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? join(', ', @$result) : "NULL" );

    print "-" x 80 . "\nTesting 'ListRW' :\n";
    $result = $nut->ListRW();
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? join(', ', keys %$result) : "NULL" );

    print "-" x 80 . "\nTesting 'InstCmd' (Test front panel) :\n";
    eval {
        $nut->{name} = "UPS1";
        $result = $nut->InstCmd("test.panel.start");
        if (!defined($NUT_USER)) {
            die "Secure operation should have failed due to lack of credentials, but did not";
        }
    };
    if ($@) {
        my $ex = $@;
        $result = "EXCEPTION: $ex";
        if (!defined($NUT_USER) && $ex =~ /USERNAME-REQUIRED/) {
            $result .= "\n(anticipated error: no credentials were provided)";
        } else {
            if ($ex !~ /CMD-NOT-SUPPORTED/ && (defined($NUT_USER) && $ex !~ /ACCESS-DENIED/)) {
                $result .= "\nTEST-CASE FAILED";
                push @failed, 'InstCmd';
            }
        }
    }
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? $result : "NULL" );

    print "-" x 80 . "\nTesting 'Set' (set ups.id to test):\n";
    eval {
        $nut->{name} = "UPS1";
        $result = $nut->Set("ups.id", "test");
        if (!defined($NUT_USER)) {
            die "Secure operation should have failed due to lack of credentials, but did not";
        }
    };
    if ($@) {
        my $ex = $@;
        $result = "EXCEPTION: $ex";
        if (!defined($NUT_USER) && $ex =~ /USERNAME-REQUIRED/) {
            $result .= "\n(anticipated error: no credentials were provided)";
        } else {
            if ($ex !~ /VAR-NOT-SUPPORTED/ && (defined($NUT_USER) && $ex !~ /ACCESS-DENIED/)) {
                $result .= "\nTEST-CASE FAILED";
                push @failed, 'SetVar';
            }
        }
    }
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? $result : "NULL" );

    print "-" x 80 . "\nTesting 'Set' with TRACKING for 'driver.debug' (1s interval, 10s timeout):\n";
    eval {
        my $target_ups = "UPS1";
        $nut->{name} = $target_ups;
        # Check if UPS exists
        my $vars = $nut->ListVar();
        if (!defined($vars)) {
            $target_ups = "dummy";
            $nut->{name} = $target_ups;
        }

        # Set with tracking
        my ($tid_res, $tid) = $nut->Set("driver.debug", "1", 1, 10);
        if (!defined($NUT_USER)) {
            die "Secure operation should have failed due to lack of credentials, but did not";
        }

        if (ref($tid) eq 'UPS::Nut::TrackingID') {
            printf("Got TRACKING ID: %s, created: %s, age: %0.2fs\n", $tid->id, $tid->created, $tid->age);
            $result = $tid->id;
        } else {
            $result = $tid_res;
        }
    };
    if ($@) {
        my $ex = $@;
        $result = "EXCEPTION: $ex";
        if (!defined($NUT_USER) && $ex =~ /USERNAME-REQUIRED/) {
            $result .= "\n(anticipated error: no credentials were provided)";
        } else {
            if ($ex !~ /VAR-NOT-SUPPORTED/ && (defined($NUT_USER) && $ex !~ /ACCESS-DENIED/)) {
                $result .= "\nTEST-CASE FAILED";
                push @failed, 'SetVar-Tracking';
            }
        }
    }
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? $result : "NULL" );

    # testing who has an upsmon-like log-in session to a device
    print "-" x 80 . "\nTesting 'ListClient' for 'dummy' (should be registered in ups.conf) before test client is connected :\n";
    eval {
        $result = $nut->ListClient("dummy");
    };
    if ($@) {
        my $ex = $@;
        $result = "EXCEPTION: $ex\nTEST-CASE FAILED";
        push @failed, 'ListClient-dummy-before';
    }
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? join(', ', keys %$result) : "NULL" );

    print "-" x 80 . "\nTesting 'ListClient' for missing device (should raise an exception) :\n";
    eval {
        $result = $nut->ListClient("MissingBogusDummy");
    };
    if ($@) {
        my $ex = $@;
        $result = "EXCEPTION: $ex";
        if ($ex =~ /UNKNOWN-UPS/) {
            $result .= "\n(anticipated error: bogus device name was tested)";
        } else {
            $result .= "\nTEST-CASE FAILED";
            push @failed, 'ListClient-MissingBogusDummy';
        }
    }
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? $result : "NULL" );

    my $loggedIntoDummy = 0;
    print "-" x 80 . "\nTesting 'Login' (DeviceLogin) for 'dummy' (should be registered in ups.conf; current credentials should have an upsmon role in upsd.users) :\n";
    eval {
        $nut->{name} = "dummy";
        $result = $nut->Login($NUT_USER, $NUT_PASS);
        if (!defined($NUT_USER)) {
            die "Secure operation should have failed due to lack of credentials, but did not";
        }
        $loggedIntoDummy = 1;
    };
    if ($@) {
        my $ex = $@;
        $result = "EXCEPTION: $ex";
        if (!defined($NUT_USER) && $ex =~ /USERNAME-REQUIRED/) {
            $result .= "\n(anticipated error: no credentials were provided)";
        } else {
            if (defined($NUT_USER) && $ex !~ /ACCESS-DENIED/) {
                $result .= "\nTEST-CASE FAILED";
                push @failed, 'Login-dummy';
            }
        }
    }
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? $result : "NULL" );

    print "-" x 80 . "\nTesting 'ListClient' for all (passing no args to ListClient uses \$nut->{name}) :\n";
    eval {
        # Python's nut.ListClients() with no args lists all devices and their clients.
        # In Nut.pm, ListClient(undef) uses $self->{name}.
        # PyNUT's ListClients(None) iterates over all UPSes.
        # Let's implement similar logic here.
        my $ups_list_all = $nut->ListUPS();
        my %all_clients = ();
        foreach my $ups_name (keys %$ups_list_all) {
            my $clients = $nut->ListClient($ups_name);
            if (defined($clients)) {
                $all_clients{$ups_name} = $clients;
            }
        }
        $result = \%all_clients;

        if (ref($result) ne 'HASH') {
            die "ListClient() did not return a hash ref";
        } else {
            if ($loggedIntoDummy) {
                if (!exists($result->{'dummy'})) {
                    die "ListClient() result missing 'dummy' key";
                }
                if (scalar keys %{$result->{'dummy'}} < 1) {
                    die "ListClient() returned an empty hash for 'dummy' where at least one client was expected";
                }
            }
        }
    };
    if ($@) {
        my $ex = $@;
        $result = "EXCEPTION: $ex\nTEST-CASE FAILED";
        push @failed, 'ListClient-all-after';
    }
    printf( color('bold yellow') . "%s" . color('reset') . "\n\n", defined($result) ? join(', ', map { "$_ => [" . join(', ', keys %{$result->{$_}}) . "]" } keys %$result) : "NULL" );

    print "-" x 80 . "\nTesting 'UPS::Nut' instance teardown (end of test script)\n";

    if (scalar @failed > 0) {
        print "SOME TEST CASES FAILED in an unexpected manner: " . join(', ', @failed) . "\n";
        exit(1);
    }
}
