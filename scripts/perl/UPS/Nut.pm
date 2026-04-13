# UPS::Nut - a class to talk to a UPS via the Network Utility Tools upsd.
# Original author Kit Peters <perl@clownswilleatyou.com>
# Rewritten by Gabor Kiss <kissg@ssg.ki.iif.hu>
# Idea to implement TLS:http://www.logix.cz/michal/devel/smtp-cli/smtp-client.pl

# ### changelog: made debug messages slightly more descriptive, improved
# ### changelog: comments in code
# ### changelog: Removed timeleft() function.
# ### changelog: 1.60: JK 2026-04-08: Added basic STARTTLS, as well as TRACKING support, LIST CLIENT, LIST RANGE, GET UPSDESC and PRIMARY/MASTER aliasing.
# ### changelog: 1.61: JK 2026-04-08: Make TrackingID a class, similar to C++.
# ### changelog: 1.62: JK 2026-04-10: Added a testing script nearby; revised API and NUT protocol support, notably TRACKING and STARTTLS.

package UPS::Nut;
use strict;
use warnings FATAL => 'all';
use Carp;
use FileHandle;
use IO::Socket;
use IO::Select;
use Dumpvalue; my $dumper = Dumpvalue->new;

# The following globals dictate whether the accessors and instant-command
# functions are created.
# ### changelog: tie hash interface and AUTOLOAD contributed by
# ### changelog: Wayne Wylupski

my $_eol = "\n";

BEGIN {
  use Exporter ();
  use vars qw ($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  $VERSION     = 1.62;
  @ISA         = qw(Exporter IO::Socket::INET);
  @EXPORT      = qw();
  @EXPORT_OK   = qw();
  %EXPORT_TAGS = ();
}

sub new {
# Author: Kit Peters
  my $proto = shift;
  my $class = ref($proto) || $proto;
  my %arg = @_; # hash of arguments
  my $self = {};	# _initialize will fill it later
  bless $self, $class;
  unless ($self->_initialize(%arg)) { # can't initialize
    carp "Can't initialize: $self->{err}";
    return undef;
  }
  return $self;
}

# accessor functions.  Return a value if successful, return undef 
# otherwise.

sub BattPercent { # get battery percentage
  return shift->GetVar('battery.charge');
}

sub LoadPercent { # get load percentage
  my $self = shift;
  my $context = shift;
  $context = "L$context" if $context =~ /^[123]$/;
  $context = ".$context" if $context;
  return $self->GetVar("output$context.power.percent");
}

sub LineVoltage { # get line voltage
  my $self = shift;
  my $context = shift;
  $context = "L$context-N" if $context =~ /^[123]$/;
  $context = ".$context" if $context;
  return $self->GetVar("input$context.voltage");
}

sub Status { # get status of UPS
  return shift->GetVar('ups.status');
}

sub Temperature { # get the internal temperature of UPS
  return shift->GetVar('battery.temperature');
}

# control functions: they control our relationship to upsd, and send 
# commands to upsd.

sub SetTrackingMode {
  # Enable/disable TRACKING ability for SETVAR/INSTCMD ('ON'/'OFF')
  # Remember in $self->{tracking} if we could set it with this upsd
  # version, and then to which value?
  my $self = shift;
  my $value = shift;
  my $ans; # scalar to hold responses from upsd

  # 'ON'/'OFF'/undef
  if (!(defined $value)) {
    $self->_debug($self->{err} = "Invalid setting for TRACKING mode was requested: undef");
    return undef;
  }

  if ($value ne 'ON' && $value ne 'OFF') {
    $self->_debug($self->{err} = "Invalid setting for TRACKING mode was requested: '$value'");
    return undef;
  }

  $ans = $self->_send("SET TRACKING $value");
  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^OK/) {
    $self->{tracking} = $value;
    return $value;
  }

  $self->{tracking} = undef;
  $self->_debug($self->{err} = "Error: $ans");
  return undef;
}

sub EnableTrackingModeOnce {
  my $self = shift;

  if (defined $self->{tracking} && $self->{tracking} eq 'ON') {
    return 1;
  }

  my $actualMode = $self->SetTrackingMode('ON');
  if (defined $actualMode && $actualMode eq 'ON') {
    return 1;
  }

  # Unsupported by server? Other errors?
  return undef;
}

sub isValidProtocolVersion {
  # Sends a PROTVER/NETVER query using the active connection and
  # returns True if the returned version string matches a valid
  # NUT protocol version regex (defaults to an "X(.Y)" number aka
  # "^\\d+(?:\\.\\d+)?$" if version_re is None).
  my ($self, $version_re) = @_;

  my $ans = $self->_send("PROTVER");
  if (!defined $ans) {
    # Deprecated and hidden, but may be what ancient NUT servers say
    # May throw if the error is due to (non-)connection?
    $ans = $self->_send("NETVER");
  }

  if (!defined $ans) {
    return undef;
  }
  chomp $ans;

  if (!defined $version_re) {
    # Valid versions as of NUT 2.8.2: 1.0, 1.1, 1.2, 1.3
    # Is it an X(.Y) number?
    $version_re = qr/^\d+(\.\d+)?$/;
  }

  return ($ans =~ $version_re);
}

sub StartTLS {
  my $self = shift;
  my %arg = @_;
  my $ans;

  if (defined $self->{debugssl} && $self->{debugssl} > 0) {
    # Pass to SSL lib as numeric level "debug<num>":
    $self->_debug("debugssl = $self->{debugssl}");
    eval "use IO::Socket::SSL qw(debug$self->{debugssl}); 1;";
  } else {
    eval "require IO::Socket::SSL; 1;";
  }
  if ($@) {
    $self->_debug($self->{err} = "IO::Socket::SSL not available: FEATURE-NOT-SUPPORTED on client side");
    return undef;
  }

  $ans = $self->_send("STARTTLS");
  if (defined $ans && $ans =~ /^OK STARTTLS/) {
    $self->_debug("STARTTLS accepted, upgrading socket.");
    my %argdef = ();
    my $strarg = "[" . scalar(%arg) . "]";
    for (keys %arg) {
        $strarg .= " $_=>" . ($arg{$_}//"undef");
        if (defined $arg{$_}) {
          $argdef{$_} = $arg{$_};
        }
    }
    $self->_debug("STARTTLS args: SSL_verify_mode=>"
        . ($arg{CERTVERIFY} ? "SSL_VERIFY_PEER" : "SSL_VERIFY_NONE")
        . " Other args: " . $strarg
        );

    # Translation for some common NUT SSL argument names to IO::Socket::SSL:
    if (!defined($argdef{SSL_ca_file}) && defined($argdef{CAFILE})) {
      $argdef{SSL_ca_file} = $argdef{CAFILE};
    }
    if (!defined($argdef{SSL_ca_path}) && defined($argdef{CAPATH})) {
      $argdef{SSL_ca_path} = $argdef{CAPATH};
    }
    if (!defined($argdef{SSL_cert_file}) && defined($argdef{CERTFILE})) {
      $argdef{SSL_cert_file} = $argdef{CERTFILE};
    }
    if (!defined($argdef{SSL_key_file}) && defined($argdef{KEYFILE})) {
      $argdef{SSL_key_file} = $argdef{KEYFILE};
    }
    if (!defined($argdef{SSL_passwd_cb}) && defined($argdef{CERTPASS})) {
      # Use a sub to return the password (simplest for IO::Socket::SSL)
      $argdef{SSL_passwd_cb} = sub { return $argdef{CERTPASS}; };
    }

    if (!defined($argdef{SSL_hostname}) && (
        defined($argdef{HOST}) || defined($self->{expected_server_certname})
    )) {
      # If specified, allows cert validation for host names *and* IP addresses
      $argdef{SSL_hostname} = $self->{expected_server_certname} || $argdef{HOST};
    }

    if (defined $self->{expected_server_certname}) {
      # IO::Socket::SSL uses SSL_verifycn_name for CN verification
      $argdef{SSL_verifycn_name} = $self->{expected_server_certname};
    }

    if ($self->{debug}) {
      $dumper->dumpValue(\%argdef);
    }

    if ($arg{CERTVERIFY} && ($argdef{SSL_ca_file} || $argdef{SSL_ca_path}) && ($^O eq "darwin" )) {
        # https://github.com/networkupstools/nut/issues/3404
        print STDERR "WARNING: Custom CA certificate verification may fail on $^O platform, in that case unset CERTVERIFY in your client configuration";
    }

    # NOTE: Currently nothing fancy like client's own certificate databases...
    IO::Socket::SSL->start_SSL(
      $self->{srvsock},
      SSL_verify_mode => $arg{CERTVERIFY} ? IO::Socket::SSL::SSL_VERIFY_PEER() : IO::Socket::SSL::SSL_VERIFY_NONE(),
      %argdef
    ) or do {
      $self->_debug($self->{err} = "SSL upgrade failed: " . IO::Socket::SSL->errstr());
      return undef;
    };
    return 1;
  }

  $self->_debug($self->{err} = "STARTTLS failed: $ans");
  return undef;
}

sub Login { # login to upsd, so that it won't shutdown unless we say we're 
            # ok.  This should only be used if you're actually connected 
            # to the ups that upsd is monitoring.
            # This assumes the upsmon SECONDARY role so we can be alerted
            # and initiate a shutdown if someone else sends the FSD command
            # to this UPS; we can further becomePrimary() if we are the
            # system which manages it.

# Author: Kit Peters
# ### changelog: modified login logic a bit.  Now it doesn't check to see 
# ### changelog: if we got OK, ERR, or something else from upsd.  It 
# ### changelog: simply checks for a response beginning with OK from upsd.
# ### changelog: Anything else is an error.
#
# ### changelog: uses the new _send command
#
  my $self = shift; # myself
  my $user = shift; # username
  my $pass = shift; # password
  # NOTE: This clumsily goes after other args above, to avoid
  #  breaking API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  my $errmsg; # error message, sent to _debug and $self->{err}
  my $ans; # scalar to hold responses from upsd

  if (!($self->Authenticate($user, $pass))) {
    if ($self->{err} !~ /ERR ALREADY-SET-USERNAME/) {
      $self->_debug("Authenticate before LOGIN failed.");
      return undef;
    }
  }

  $ans = $self->_send( "LOGIN $ups" );
  if (defined $ans && $ans =~ /^OK/) { # Login successful.
    $self->_debug("LOGIN successful.");
    return 1;
  }
  if (defined $ans) {
      $errmsg = "LOGIN failed. Last message from upsd: $ans";
  }
  else {
      $errmsg = "Network error: $!";
  }
  $self->_debug($self->{err} = $errmsg);
  return undef;
}

sub Authenticate { # Announce to the UPS who we are to set up the proper 
                   # management level.  See upsd.conf man page for details.

# Contributor: Wayne Wylupski
  my $self = shift; # myself
  my $user = shift; # username
  my $pass = shift; # password

  if ($self->{authenticated}) {
    $self->_debug("Already authenticated, skip");
    return 1;
  }

  my $errmsg; # error message, sent to _debug and $self->{err}
  my $ans; # scalar to hold responses from upsd

  # only attempt authentication if username and password given
  if (defined $user and defined $pass) {
    $ans = $self->_send("USERNAME $user");
    if (defined $ans && $ans =~ /^OK/) { # username OK, send password

      $ans = $self->_send("PASSWORD $pass");
      if (defined $ans && $ans =~ /^OK/) {
        $self->{authenticated} = 1;
        return 1
      }
    }
  } else {
    $self->_debug($self->{err} = "Authentication failed: username and/or password not provided, internal equivalent of USERNAME-REQUIRED");
    return undef;
  }
  if (defined $ans) {
      $errmsg = "Authentication failed. Last message from upsd: $ans";
  }
  else {
      $errmsg = "Network error: $!";
  }
  $self->_debug($self->{err} = $errmsg);
  return undef;
}

sub Logout { # logout of upsd and close connection
# Author: Kit Peters
# ### changelog: uses the new _send command
#
  my $self = shift;
  if ($self->{srvsock}) { # are we still connected to upsd?
    my $ans = $self->_send( "LOGOUT" );
    close ($self->{srvsock});
    delete ($self->{srvsock});
    $self->{authenticated} = 0;
  }
}

# internal functions.  These are only used by UPS::Nut internally, so 
# please don't use them otherwise.  If you really think an internal 
# function should be externalized, let me know.

sub _initialize {
# Author: Kit Peters
  my $self = shift;
  my %arg = @_;
  my $host = $arg{HOST}     || 'localhost'; # Host running upsd and probably drivers
  my $port = $arg{PORT}     || '3493'; # 3493 is IANA assigned port for NUT
  my $proto = $arg{PROTO}   || 'tcp'; # use tcp unless user tells us to
  my $user = $arg{USERNAME} || undef; # username passed to upsd
  my $pass = $arg{PASSWORD} || undef; # password passed to upsd
  my $login = $arg{LOGIN}   || 0; # login to upsd on init?

  # Explicitly enable/disable TRACKING mode for SETVAR/INSTCMD on init?
  # Remember in $self->{tracking} if we could toggle it with
  # this upsd version, and to which value? ('ON'/'OFF'/undef)
  my $tracking = $arg{TRACKING} || undef;

  $self->{name} = $arg{NAME} || 'default'; # UPS name in etc/ups.conf on $host
  $self->{timeout} = $arg{TIMEOUT} || 30; # timeout
  $self->{debug} = $arg{DEBUG} || 0; # debugging?
  $self->{debugssl} = defined $arg{DEBUGSSL} ? $arg{DEBUGSSL} : $self->{debug}; # debugging IO::Socket::SSL upon use?
  $self->{debugout} = $arg{DEBUGOUT} || undef; # where to send debug messages

  my $srvsock = $self->{srvsock} = # establish connection to upsd
    IO::Socket::INET->new(
      PeerAddr => $host,
      PeerPort => $port,
      Proto    => $proto
    );

  unless ( defined $srvsock) { # can't connect
    $self->_debug($self->{err} = "Unable to connect via $proto to $host:$port: $!");
    return undef;
  }

  $self->{select} = IO::Select->new( $srvsock );

  my $can_ssl = 1;
  eval "require IO::Socket::SSL; 1";
  if ($@) {
    $can_ssl = 0;
  }

  # CERTHOST <hostname> <certificate name> <certverify> <forcessl>
  # If specified as a hash: { host => [certname, certverify, forcessl], ... }
  # If specified as a list of lists: [ [host, certname, certverify, forcessl], ... ]
  # If specified as a list of hashes: [ { host => [certname, certverify, forcessl] }, ... ]
  $self->{certhost} = {};
  if (defined $arg{CERTHOST}) {
    if (ref($arg{CERTHOST}) eq 'HASH') {
      $self->{certhost} = $arg{CERTHOST};
    } elsif (ref($arg{CERTHOST}) eq 'ARRAY') {
      foreach my $entry (@{$arg{CERTHOST}}) {
        if (ref($entry) eq 'HASH') {
          foreach my $h (keys %$entry) {
            $self->{certhost}->{$h} = $entry->{$h};
          }
        } elsif (ref($entry) eq 'ARRAY' && scalar(@$entry) >= 4) {
          $self->{certhost}->{$entry->[0]} = [$entry->[1], $entry->[2], $entry->[3]];
        }
      }
    }
  }

  my $use_ssl = $arg{USESSL};
  my $force_ssl = $arg{FORCESSL} || 0;
  my $cert_verify = $arg{CERTVERIFY};

  # If current host is in CERTHOST, override defaults
  if (defined $self->{certhost}->{$host}) {
    my ($ch_certname, $ch_verify, $ch_force) = @{$self->{certhost}->{$host}};
    $self->_debug("CERTHOST match for $host: certname=$ch_certname verify=$ch_verify force=$ch_force");
    $self->{expected_server_certname} = $ch_certname;
    if (!defined $cert_verify) {
      $cert_verify = ($ch_verify == 1);
    }
    $use_ssl = 1 if ($ch_force == 1);
    $force_ssl = 1 if ($ch_force == 1);
  }

  if (!defined $use_ssl) {
    $self->_debug("USESSL option was undef, flipping to IO::Socket::SSL module availability: $can_ssl");
    $use_ssl = $can_ssl;
  }

  if (($can_ssl && $use_ssl) || $force_ssl) {
    # Always try to elevate, do not bother if this fails unless required by args
    my %tls_args = %arg;
    $tls_args{CERTVERIFY} = $cert_verify if defined $cert_verify;
    $tls_args{FORCESSL} = $force_ssl;
    $tls_args{HOST} = $host;

    my $startedTLS = $self->StartTLS(%tls_args);
    if (defined $startedTLS && $startedTLS) {
      # Make sure handshake succeeded or abort early
      # (there is currently no way for the server to
      # report its fault to the client when connection
      # is half-way secure):
      if (!$self->isValidProtocolVersion()) {
        if ($force_ssl) {
          $self->_debug($self->{err} = "STARTTLS setup claimed to succeed, but protocol version check in the secured session failed, and SSL is required: $self->{err}");
          return undef;
        }
        $self->_debug($self->{err} = "STARTTLS setup claimed to succeed, but protocol version check in the secured session failed, but SSL is not required: $self->{err}");
        # TODO: Drop SSL context or restart the connection as plaintext if SSL is not required?
      }
    } else {
      if ($force_ssl) {
        $self->_debug($self->{err} = "SSL setup failed but it is required: $self->{err}");
        return undef;
      }
    }
  } else {
    $self->_debug("SSL setup neither requested nor required, skipped StartTLS altogether");
  }

  $self->{authenticated} = 0;
  if ($user and $pass) { # attempt login to upsd if that option is specified
    if ($login) { # attempt login to upsd if that option is specified
      $self->Login($user, $pass) or carp $self->{err};
    }
    else {
      $self->Authenticate($user, $pass) or carp $self->{err};
    }
  }

  # get a hash of vars for both the TIE functions as well as for 
  # expanding vars.
  $self->{vars} = $self->ListVar;

  unless ( defined $self->{vars} ) {
    # FIXME: Can well be `ERR UNKNOWN-UPS` due to NAME=default
    #  Better report that as such...
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  }

  # Can error out on invalid "TRACKING" value setting, returns undef then:
  $self->{tracking} = $self->SetTrackingMode($tracking);

  return $self;
}

#
# _send
#
# Sends a command to the server and retrieves the results.
# If there was a network error, return undef; $! will contain the 
# error.
sub _send
{
# Contributor: Wayne Wylupski
  my $self = shift;
  my $cmd = shift;
  my @handles;
  my $result;		# undef by default

  my $socket = $self->{srvsock};
  my $select = $self->{select};

  $self->{err} = "";

  @handles = IO::Select->select( undef, $select, $select, $self->{timeout} );
  return undef if ( !scalar $handles[1] );

  $socket->print( $cmd . $_eol );

  @handles = IO::Select->select( $select, undef, $select, $self->{timeout} );
  return undef if ( !scalar  $handles[0]);

  $result = $socket->getline;
  return undef if ( !defined ( $result ) );
  chomp $result;

  return $result;
}

sub _getline
{
# Contributor: Wayne Wylupski
  my $self = shift;
  my $result;		# undef by default

  my $socket = $self->{srvsock};
  my $select = $self->{select};

  # Different versions of IO::Socket has different error detection routines.
  return undef if ( $IO::Socket::{has_error} && $select->has_error(0) );
  return undef if ( $IO::Socket::{has_exception} && $select->has_exception(0) );

  chomp ( $result = $socket->getline );
  return $result;
}

# Compatibility layer
sub Request { goto &GetVar; }

sub GetVar { # request a variable from the UPS
# Author: Kit Peters
  my $self = shift;
# ### changelog: 8/3/2002 - KP - Request() now returns undef if not
# ### changelog: connected to upsd via $srvsock 
# ### changelog: uses the new _send command
#
# Modified by Gabor Kiss according to protocol version 1.5+
  my $var = shift;
  # NOTE: This clumsily goes after other args above, to avoid
  #  breaking API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  my $req = "GET VAR $ups $var"; # build request
  my $ans = $self->_send( $req );

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) {
    $self->_debug($self->{err} = "Error: $ans.  Requested $var.");
    return undef;
  }
  elsif ($ans =~ /^VAR/) {
    my $checkvar; # to make sure the var we asked for is the var we got.
    my $retval; # returned value for requested VAR
    (undef, undef, $checkvar, $retval) = split(' ', $ans, 4);
        # get checkvar and retval from the answer
    if ($checkvar ne $var) { # did not get expected var
      $self->_debug($self->{err} = "Requested $var, received $checkvar");
      return undef;
    }
    $retval =~ s/^"(.*)"$/$1/;
    return $retval; # return the requested value
  }
  else { # unrecognized response
    $self->_debug($self->{err} = "Unrecognized response from upsd: $ans");
    return undef;
  }
}

sub Set {
# Contributor: Wayne Wylupski
# ### changelog: uses the new _send command
#
  my $self = shift;
  my $var = shift;
  (my $value = shift) =~ s/^"?(.*)"?$/"$1"/;	# add quotes if missing

  # Optional TRACKING wait support:
  my $wait_interval_sec = shift || undef;
  my $wait_max_count = shift || undef;

  # NOTE: This clumsily goes after other args above, to avoid
  #  breaking API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  my $do_wait = 0;
  if (defined $wait_interval_sec && defined $wait_max_count && $wait_max_count > 0 && $wait_interval_sec > 0) {
    $self->EnableTrackingModeOnce;
    $do_wait = 1;
  }

  my $req = "SET VAR $ups $var $value"; # build request
  my $ans = $self->_send( $req );

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) {
    $self->_debug($self->{err} = "Error: $ans");
    return undef;
  }
  elsif ($ans =~ /^OK TRACKING /) { # command successful
    my $id;
    (undef, undef, $id) = split(' ', $ans, 3);
    if (defined $id && $id =~ /^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$/
    ) {
      # UUID
      $self->_debug("Variable setting $var $value sent successfully, got tracking ID '$id'.");
      my $tid = UPS::Nut::TrackingID->new($id);
      if ($do_wait && $self->WaitTrackingResult($tid, $wait_interval_sec, $wait_max_count)) {
        return $value;
      }
      return ($value, $tid);
    }
    $self->_debug("Variable setting $var $value sent successfully, but got bogus tracking ID: $ans");
    return $value;
  }
  elsif ($ans =~ /^OK/) {
    $self->_debug("Variable setting $var $value sent successfully.");
    return $value;
  }
  else { # unrecognized response
    $self->_debug($self->{err} = "Unrecognized response from upsd: $ans");
    return undef;
  }
}

sub FSD { # set forced shutdown flag
# Author: Kit Peters
# ### changelog: uses the new _send command
#
  my $self = shift;
  my $ups = shift || $self->{name};

  my $req = "FSD $ups"; # build request
  my $ans = $self->_send( $req );

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) { # can't set forced shutdown flag
    $self->_debug($self->{err} = "Can't set FSD flag.  Upsd reports: $ans");
    return undef;
  }
  elsif ($ans =~ /^OK FSD-SET/) { # forced shutdown flag set
    $self->_debug("FSD flag set successfully.");
    return 1;
  }
  else {
    $self->_debug($self->{err} = "Unrecognized response from upsd: $ans");
    return undef;
  }
}

sub InstCmd { # send instant command to ups
# Contributor: Wayne Wylupski
  my $self = shift;

  chomp (my $cmd = shift);

  # Optional TRACKING wait support:
  my $wait_interval_sec = shift || undef;
  my $wait_max_count = shift || undef;

  # NOTE: This clumsily goes after other args above, to avoid
  #  breaking API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  my $do_wait = 0;
  if (defined $wait_interval_sec && defined $wait_max_count && $wait_max_count > 0 && $wait_interval_sec > 0) {
    $self->EnableTrackingModeOnce;
    $do_wait = 1;
  }

  my $req = "INSTCMD $ups $cmd";
  my $ans = $self->_send( $req );

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) { # error reported from upsd
    $self->_debug($self->{err} = "Can't send instant command $cmd. Reason: $ans");
    return undef;
  }
  elsif ($ans =~ /^OK TRACKING /) { # command successful
    my $id;
    (undef, undef, $id) = split(' ', $ans, 3);
    if (defined $id && $id =~ /^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$/
    ) {
      # UUID
      $self->_debug("Instant command $cmd sent successfully, got tracking ID '$id'.");
      my $tid = UPS::Nut::TrackingID->new($id);
      if ($do_wait && $self->WaitTrackingResult($tid, $wait_interval_sec, $wait_max_count)) {
        return 1;
      }
      return (1, $tid);
    }
    $self->_debug("Instant command $cmd sent successfully, but got bogus tracking ID: $ans");
    return 1;
  }
  elsif ($ans =~ /^OK/) { # command successful
    $self->_debug("Instant command $cmd sent successfully.");
    return 1;
  }
  else { # unrecognized response
    $self->_debug($self->{err} = "Can't send instant command $cmd. Unrecognized response from upsd: $ans");
    return undef;
  }
}

sub ListUPS {
  my $self = shift;
  return $self->_get_list("LIST UPS", 2, 1);
}

sub GetTrackingResult {
  my $self = shift;
  my $tid = shift;

  my $id = ref($tid) eq 'UPS::Nut::TrackingID' ? $tid->id : $tid;
  my $ans = $self->_send("GET TRACKING $id");
  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  chomp $ans;
  if ($ans eq 'SUCCESS') {
    $self->_debug("Request with TRACKING ID $id has successfully completed");
  } elsif ($ans =~ 'ERR') {
    $self->_debug($self->{err} = "Request with TRACKING ID $id has completed with a failure: $ans");
  } elsif ($ans eq 'PENDING') {
    $self->_debug("Still waiting for TRACKING ID $id");
  } else {
    $self->_debug($self->{err} = "Got bogus reply while waiting for TRACKING ID $id: $ans");
    return undef;
  }

  return $ans;
}

sub WaitTrackingResult {
  my $self = shift;
  my $tid = shift;
  my $id = ref($tid) eq 'UPS::Nut::TrackingID' ? $tid->id : $tid;

  my $wait_interval_sec = shift || 1;
  my $wait_max_count = shift || 10;

  if (!(defined $id && defined $wait_interval_sec && defined $wait_max_count)) {
    return undef;
  }

  do {
    my $value = $self->GetTrackingResult($tid);
    if (defined $value) {
      # Note: debug messages are printed by GetTrackingResult() already
      chomp $value;
      if ($value eq 'SUCCESS') {
        return 1;
      } elsif ($value =~ 'ERR') {
        return -1;
      }
    } else {
      # TOTHINK: Keep retrying? Here in case of network or explicit error...
      $self->_debug("Got bogus reply while waiting for TRACKING ID $id: undef");
    }

    sleep($wait_interval_sec);
    $wait_max_count = $wait_max_count - 1;
  } while ($wait_max_count > 0);

  # Timed out?..
  $self->_debug("Timed out while waiting for TRACKING ID $id");
  return 0;
}

sub ListClient {
  my $self = shift;
  my $ups = shift || $self->{name};

  return $self->_get_list("LIST CLIENT $ups", 2, 2);
}

sub GetUPSDesc {
  my $self = shift;
  my $ups = shift || $self->{name};

  my $ans = $self->_send("GET UPSDESC $ups");
  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };
  if ($ans =~ /^UPSDESC/) {
    my @fields = split(' ', $ans, 3);
    my $desc = $fields[2];
    $desc =~ s/^"(.*)"$/$1/;
    return $desc;
  }

  $self->_debug($self->{err} = "Error: $ans");
  return undef;
}

sub ListVar {
  my $self = shift;
  my $ups = shift || $self->{name};

  my $vars = $self->_get_list("LIST VAR $ups", 3, 2);
  return $vars unless @_;			# return all variables
  return {map { $_ => $vars->{$_} } @_};	# return selected ones
}

sub ListRW {
  my $self = shift;
  my $ups = shift || $self->{name};

  return $self->_get_list("LIST RW $ups", 3, 2);
}

sub ListCmd {
  my $self = shift;
  my $ups = shift || $self->{name};

  return $self->_get_list("LIST CMD $ups", 2);
}

sub ListEnum {
  my $self = shift;
  my $var = shift;
  # NOTE: This clumsily goes after $var, to avoid breaking
  #  API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  return $self->_get_list("LIST ENUM $ups $var", 3);
}

sub ListRange {
  my $self = shift;
  my $var = shift;
  # NOTE: This clumsily goes after other args above, to avoid
  #  breaking API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  my $req = "LIST RANGE $ups $var";
  my $ans = $self->_send($req);

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) {
    $self->_debug($self->{err} = "Error: $ans");
    return undef;
  }
  elsif ($ans =~ /^BEGIN LIST RANGE/) {
    my $retval = [];
    my $line;
    while ($line = $self->_getline) {
      last if $line =~ /^END LIST RANGE/;
      # RANGE <ups> <var> "<min>" "<max>"
      if ($line =~ /^RANGE \S+ \S+ "([^"]+)" "([^"]+)"/) {
        push(@$retval, { min => $1, max => $2 });
      }
    }
    return $retval;
  }

  $self->_debug($self->{err} = "Unrecognized response: $ans");
  return undef;
}

sub _get_list {
  my $self = shift;
  my ($req, $valueidx, $keyidx) = @_;
  my $ans = $self->_send($req);

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) {
    $self->_debug($self->{err} = "Error: $ans");
    return undef;
  }
  elsif ($ans =~ /^BEGIN LIST/) { # command successful
    my $retval = $keyidx ? {} : [];
    my $line;
    while ($line = $self->_getline) {
      last if $line =~ /^END LIST/;
      my @fields = split(' ', $line, $valueidx+1);
      (my $value = $fields[$valueidx]) =~ s/^"(.*)"$/$1/;
      if ($keyidx) {
        $retval->{$fields[$keyidx]} = $value;
      }
      else {
        push(@$retval, $value);
      }
    }
    unless ($line) {
      $self->_debug($self->{err} = "Network error: $!");
      return undef;
    };
    $self->_debug("$req command sent successfully.");
    return $retval;
  }
  else { # unrecognized response
    $self->_debug($self->{err} = "Can't send $req. Unrecognized response from upsd: $ans");
    return undef;
  }
}

# Compatibility layer
sub VarDesc { goto &GetDesc; }

sub GetDesc {
# Contributor: Wayne Wylupski
# Modified by Gabor Kiss according to protocol version 1.5+
  my $self = shift;
  my $var = shift;
  # NOTE: This clumsily goes after other args above, to avoid
  #  breaking API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  my $req = "GET DESC $ups $var";
  my $ans = $self->_send( $req );

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) {
    $self->_debug($self->{err} = "Error: $ans");
    return undef;
  }
  elsif ($ans =~ /^DESC/) { # command successful
    $self->_debug("$req command sent successfully.");
    (undef, undef, undef, $ans) = split(' ', $ans, 4);
    $ans =~ s/^"(.*)"$/$1/;
    return $ans;
  }
  else { # unrecognized response
    $self->_debug($self->{err} = "Can't send $req. Unrecognized response from upsd: $ans");
    return undef;
  }
}

# Compatibility layer
sub VarType { goto &GetType; }

sub GetType {
# Contributor: Wayne Wylupski
# Modified by Gabor Kiss according to protocol version 1.5+
  my $self = shift;
  my $var = shift;
  # NOTE: This clumsily goes after other args above, to avoid
  #  breaking API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  my $req = "GET TYPE $ups $var";
  my $ans = $self->_send( $req );

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) {
    $self->_debug($self->{err} = "Error: $ans");
    return undef;
  }
  elsif ($ans =~ /^TYPE/) { # command successful
    $self->_debug("$req command sent successfully.");
    (undef, undef, undef, $ans) = split(' ', $ans, 4);
    return $ans;
  }
  else { # unrecognized response
    $self->_debug($self->{err} = "Can't send $req. Unrecognized response from upsd: $ans");
    return undef;
  }
}

# Compatibility layer
sub InstCmdDesc { goto &GetCmdDesc; }

sub GetCmdDesc {
# Contributor: Wayne Wylupski
# Modified by Gabor Kiss according to protocol version 1.5+
  my $self = shift;
  my $cmd = shift;
  # NOTE: This clumsily goes after other args above, to avoid
  #  breaking API compatibility for older release consumers:
  my $ups = shift || $self->{name};

  my $req = "GET CMDDESC $ups $cmd";
  my $ans = $self->_send( $req );

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^ERR/) {
    $self->_debug($self->{err} = "Error: $ans");
    return undef;
  }
  elsif ($ans =~ /^DESC/) { # command successful
    $self->_debug("$req command sent successfully.");
    (undef, undef, undef, $ans) = split(' ', $ans, 4);
    $ans =~ s/^"(.*)"$/$1/;
    return $ans;
  }
  else { # unrecognized response
    $self->_debug($self->{err} = "Can't send $req. Unrecognized response from upsd: $ans");
    return undef;
  }
}

sub DESTROY { # destructor, all it does is call Logout
# Author: Kit Peters
  my $self = shift;
  $self->_debug("Object destroyed.");
  $self->Logout();
}

sub _debug { # print debug messages to stdout or file
# Author: Kit Peters
  my $self = shift;
  if ($self->{debug}) {
    chomp (my $msg = shift);
    my $out; # filehandle for output
    if ($self->{debugout}) { # if filename is given, use that
      $out = new FileHandle ($self->{debugout}, ">>") or warn "Error: $!";
    }
    if ($out) { # if out was set to a filehandle, create nifty timestamp
      my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
      $year = sprintf("%02d", $year % 100); # Y2.1K compliant, even!
      my $timestamp = join '/', ($mon + 1), $mday, $year; # today
      $timestamp .= " ";
      $timestamp .= join ':', $hour, $min, $sec;
      print $out "$timestamp $msg\n";
    }
    else { print "DEBUG: $msg\n"; } # otherwise, print to stdout
  }
}

sub Error { # what was the last thing that went bang?
# Author: Kit Peters
  my $self = shift;
  if ($self->{err}) { return $self->{err}; }
  else { return "No error explanation available."; }
}

sub becomePrimary { goto &Master; }

sub Master { # check for MASTER level access
# Author: Kit Peters
# ### changelog: uses the new _send command
# ### changelog: 8/3/2002 - KP - Master() returns undef rather than 0 on
# ### failure.  this makes it consistent with other methods
#
# NOTE: API changed since NUT 2.8.0 to replace MASTER with PRIMARY
# (and backwards-compatible alias handling)
  my $self = shift;
  my $ups = shift || $self->{name};

  my $req = "PRIMARY $ups"; # build request
  my $ans = $self->_send( $req );

  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^OK/) { # access granted
    $self->_debug("PRIMARY level access granted.  Upsd reports: $ans");
    return 1;
  }

  # Retry with MASTER if PRIMARY failed
  $req = "MASTER $ups";
  $ans = $self->_send( $req );
  unless (defined $ans) {
    $self->_debug($self->{err} = "Network error: $!");
    return undef;
  };

  if ($ans =~ /^OK/) { # access granted
    $self->_debug("MASTER level access granted.  Upsd reports: $ans");
    return 1;
  }
  else { # access denied, or unrecognized response
    $self->_debug($self->{err} = "PRIMARY/MASTER level access denied.  Upsd responded: $ans");
    return undef;
  }
}

sub AUTOLOAD {
# Contributor: Wayne Wylupski
  my $self = shift;
  my $name = $UPS::Nut::AUTOLOAD;
  $name =~ s/^.*:://;

  # for a change we will only load cmds if needed.
  if (!defined $self->{cmds} ) {
      %{$self->{cmds}} = map{ $_ =>1 } @{$self->ListCmd};
  }

  croak "No such InstCmd: $name" if (! $self->{cmds}{$name} );

  return $self->InstCmd( $name );
}

#-------------------------------------------------------------------------
# tie hash interface
#
# The variables of the array, including the hidden 'numlogins' can
# be accessed as a hash array through this method.
#
# Example:
#  tie %ups, 'UPS::Nut',
#      NAME => "my-ups",
#      HOST => "some-machine.somewhere.com",
#      ... # same options as new();
#  ;
#
#  $ups{UPSIDENT} = "MyUPS";
#  print $ups{MFR}, " " $ups{MODEL}, "\n";
#
#-------------------------------------------------------------------------
sub TIEHASH {
  my $class = shift || 'UPS::Nut';
  return $class->new( @_ );
}

sub FETCH {
  my $self = shift;
  my $key = shift;

  return $self->Request( $key );
}

sub STORE {
  my $self = shift;
  my $key = shift;
  my $value = shift;

  return $self->Set( $key, $value );
}

sub DELETE {
  croak "DELETE operation not supported";
}

sub CLEAR {
  croak "CLEAR operation not supported";
}

sub EXISTS {
  exists shift->{vars}{shift};
}

sub FIRSTKEY {
  my $self = shift;
  my $a = keys %{$self->{vars}};
  return scalar each %{$self->{vars}};
}

sub NEXTKEY {
  my $self = shift;
  return scalar each %{$self->{vars}};
}

sub UNTIE {
  $_[0]->Logout;
}

=head1 NAME

Nut - a module to talk to a UPS via NUT (Network UPS Tools) upsd

=head1 SYNOPSIS

 use UPS::Nut;

 $ups = new UPS::Nut( NAME => "my-ups",
                      HOST => "some-machine.somewhere.com",
                      PORT => "3493",
                      USERNAME => "upsuser",
                      PASSWORD => "upspasswd",
                      TIMEOUT => 30,
                      DEBUG => 1,
                      DEBUGOUT => "/some/file/somewhere",
                    );
 if ($ups->Status() =~ /OB/) {
    print "Oh, no!  Power failure!\n";
 }

 tie %other_ups, 'UPS::Nut',
     NAME => "my-ups",
     HOST => "some-machine.somewhere.com",
     ... # same options as new();
 ;

 print $other_ups{MFR}, " ", $other_ups{MODEL}, "\n";

=head1 DESCRIPTION

This is an object-oriented (whoo!) interface between Perl and upsd from 
the Network UPS Tools package version 1.5 and above
(https://www.networkupstools.org/).
Note that it only talks to upsd for you in a Perl-ish way.
It does not monitor the UPS continuously.

=head1 CONSTRUCTOR

Shown with defaults: new UPS::Nut( NAME => "default",
                                   HOST => "localhost",
                                   PORT => "3493",
                                   USERNAME => "",
                                   PASSWORD => "",
                                   DEBUG => 0,
                                   DEBUGOUT => "",
                                 );
* NAME is the name of the UPS to monitor, as specified in ups.conf
* HOST is the host running upsd
* PORT is the port that upsd is running on
* USERNAME and PASSWORD are those that you use to login to upsd.  This 
  gives you the right to do certain things, as specified in upsd.conf.
* DEBUG turns on debugging output, set to 1 or 0
* DEBUGOUT is de thing you do when de s*** hits the fan.  Actually, it's 
  the filename where you want debugging output to go.  If it's not 
  specified, debugging output comes to standard output.

=head1 Important notice

This version of UPS::Nut is not compatible with version 0.04. It is totally
rewritten in order to talk the new protocol of NUT 1.5+. You should not use
this module as a drop-in replacement of previous version from 2002.
Almost all method has changed slightly.

=head1 Methods

Unlike in version 0.04 no methods return list values but a
single reference or undef.

=head2 Methods for querying UPS status

=over 4

=item Getvar($varname)

returns value of the specified variable. Returns undef if variable 
unsupported.
Old method named Request() is also supported for compatibility.

=item Set($varname, $value)

sets the value of the specified variable. Returns undef if variable 
unsupported, or if variable cannot be set for some other reason. See
Authenticate() if you wish to use this function.

=item BattPercent()

returns percentage of battery left. Returns undef if we can't get 
battery percentage for some reason. Same as GetVar('battery.charge').

=item LoadPercent($context)

returns percentage of the load on the UPS. Returns undef if load 
percentage is unavailable. $context is a selector of 3 phase systems.
Possible values are 1, 2, 3, 'L1', 'L2', 'L3'. It should be omitted
in case of single phase UPS.

=item LineVoltage($context)

returns input line (e.g. the outlet) voltage. Returns undef if line 
voltage is unavailable. $context is a selector of 3 phase systems.
Possible values are 1, 2, 3, 'L1', 'L2', 'L3'. It should be omitted
in case of single phase UPS.

=item Status()

returns UPS status, one of OL or OB. OL or OB may be followed by LB,
which signifies low battery state. OL or OB may also be followed by 
FSD, which denotes that the forced shutdown state 
( see UPS::Nut->FSD() ) has been set on upsd. Returns undef if status 
unavailable. Same as GetVar('ups.status').

=item Temperature()

returns UPS internal temperature. Returns undef if internal
temperature unavailable. Same as GetVar('battery.temperature').

=back

=head2 Other methods

These all operate on the UPS specified in the NAME argument to the 
constructor.

=over 4

=item Authenticate($username, $password)

With NUT certain operations are only available if the user has the 
privilege. The program has to authenticate with one of the accounts
defined in upsd.conf.

=item Login($username, $password)

Notify upsd that client is drawing power from the given UPS.
It is automatically done if new() is called with USERNAME, PASSWORD
and LOGIN parameters.

=item Logout()

Notify upsd that client is released UPS. (E.g. it is shutting down.)
It is automatically done if connection closed.

=item Master()

Use this to find out whether or not we have MASTER privileges for
this UPS. Returns 1 if we have MASTER privileges, returns 0 otherwise.

TODO: API change pending to replace MASTER with PRIMARY
(and backwards-compatible alias handling)

=item ListVar($variable, ...)

This is an implementation of "LIST VAR" command.
Returns a hash reference to selected variable names and values supported
by the UPS. If no variables given it returns all.
Returns undef if "LIST VAR" failed.
(Note: This method significantly differs from the old ListVars()
and ListRequest().)

=item ListRW()

Similar to ListVar() but cares only with read/writeable variables.

=item ListEnum($variable)

Returns a reference to the list of all possible values of $variable.
List is empty if $variable is not an ENUM type. (See GetType().)
Returns undef if error occurred.

=item ListCmd()

Returns a reference to the list of all instant commands supported
by the UPS. Returns undef if these are unavailable.
This method replaces the old ListInstCmds().

=item InstCmd($command)

Send an instant command to the UPS. Returns 1 on success. Returns 
undef if the command can't be completed.

=item FSD()

Set the FSD (forced shutdown) flag for the UPS. This means that we're 
planning on shutting down the UPS very soon, so the attached load should 
be shut down as well. Returns 1 on success, returns undef on failure.
This cannot be unset, so don't set it unless you mean it.

=item Error()

why did the previous operation fail? The answer is here. It will 
return a concise, well-written, and brilliantly insightful few words as 
to why whatever you just did went bang.

=item GetDesc($variable)

Returns textual description of $variable or undef in case of error.
Old method named VarDesc() is also supported for compatibility.

=item GetCmdDesc($command)

This is like GetDesc() above but applies to the instant commands.
Old method named InstCmdDesc() is also supported for compatibility.

=item GetType($variable)

Returns a string UNKNOWN or constructed one or more words of RW,
ENUM and STRING:n (where n is a number). (Seems to be not working
perfectly at upsd 2.2.)
Old method named VarType() is also supported for compatibility.

=item ListUPS()

Returns a reference to hash of all available UPS names and descriptions.

=back

=head1 AUTOLOAD

The "instant commands" are available as methods of the UPS object. They
are AUTOLOADed when called. For example, if the instant command is FPTEST,
then it can be called by $ups->FPTEST.

=head1 TIE Interface

If you wish to simply query or set values, you can tie a hash value to
UPS::Nut and pass as extra options what you need to connect to the host.
If you need to exercise an occasional command, you may find the return
value of 'tie' useful, as in:

  my %ups;
  my $ups_obj = tie %ups, 'UPS::Nut', HOSTNAME=>"firewall";

  print $ups{UPSIDENT}, "\n";

  $ups_obj->Authenticate( "user", "pass" );

  $ups{UPSIDENT} = "MyUPS";

=head1 AUTHOR

  Original version made by Kit Peters 
  perl@clownswilleatyou.com
  http://www.awod.com/staff/kpeters/perl/

  Rewritten by Gabor Kiss <kissg@ssg.ki.iif.hu>.

=head1 CREDITS

Developed with the kind support of A World Of Difference, Inc.
<http://www.awod.com/>

Many thanks to Ryan Jessen <rjessen@cyberpowersystems.com> at CyberPower 
Systems for much-needed assistance.

Thanks to Wayne Wylupski <wayne@connact.com> for the code to make 
accessor methods for all supported vars.

=head1 LICENSE

This module is distributed under the same license as Perl itself.

=cut

package UPS::Nut::TrackingID;
use strict;

sub new {
  my $class = shift;
  my $id = shift;
  my $self = {
    id => $id,
    created => time()
  };
  bless $self, $class;
  return $self;
}

sub id {
  my $self = shift;
  return $self->{id};
}

sub created {
  my $self = shift;
  return $self->{created};
}

sub age {
  my $self = shift;
  return time() - $self->{created};
}

sub toString {
  my $self = shift;
  return $self->{id};
}

sub isValid {
  my $self = shift;
  return defined $self->{id} && $self->{id} ne "";
}

1;
__END__

