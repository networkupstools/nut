#!/usr/bin/perl
# Standalone protocol/configuration regression; core modules only, Perl 5.005+.
use 5.005;
use strict;
use UPS::Nut;
use FileHandle;
use File::Path;
use Cwd;

my ($checks, $failures) = (0, 0);
sub check {
    my ($ok, $name) = @_;
    $checks++;
    print(($ok ? 'ok' : 'not ok'), " $checks - $name\n");
    $failures++ unless $ok;
}
sub equal {
    my ($got, $want, $name) = @_;
    check(defined($got) && $got eq $want, $name);
}

# Exercise public readers independently of transport scheduling.
package ParserFixture;
use vars qw(@ISA);
@ISA = qw(UPS::Nut);
sub _send { return shift->{answer}; }
sub _getline { return shift @{shift->{lines}}; }
package main;
sub fixture {
    my ($answer, @lines) = @_;
    return bless {name => 'test', answer => $answer, lines => \@lines}, 'ParserFixture';
}

# Wire strings are explicit, not produced by the encoder being tested.
my @values = (
    ['"ordinary"', 'ordinary'], ['""', ''], ['"two words"', 'two words'],
    ['"say \\"hello\\""', 'say "hello"'],
    ['"\\"boundary\\""', '"boundary"'],
    ['"one\\\\two\\\\\\\\three"', 'one\\two\\\\three'],
    ['"slash\\\\\\"quote"', 'slash\\"quote'],
    ['"Outlet \\#2"', 'Outlet #2'], ['"Outlet #3"', 'Outlet #3'],
    ['"it\'s literal"', "it's literal"],
    ['"literal\\\\n"', 'literal\\n'], ['unquoted\\ value', 'unquoted value'],
    ['"\200\377"', "\200\377"],
);
# Build high bytes explicitly without introducing non-ASCII source text.
$values[$#values][0] = '"' . "\200\377" . '"';
foreach my $case (@values) {
    my ($wire, $value) = @$case;
    my $tag = unpack('H*', $wire);
    equal(fixture("VAR test v $wire")->GetVar('v'), $value, "GetVar $tag");
    equal(fixture("VAR test v $wire")->Request('v'), $value, "Request $tag");
    equal(fixture("UPSDESC test $wire")->GetUPSDesc(), $value, "GetUPSDesc $tag");
    equal(fixture("DESC test v $wire")->GetDesc('v'), $value, "GetDesc $tag");
    equal(fixture("DESC test v $wire")->VarDesc('v'), $value, "VarDesc $tag");
    equal(fixture("CMDDESC test c $wire")->GetCmdDesc('c'), $value, "GetCmdDesc $tag");
    equal(fixture("CMDDESC test c $wire")->InstCmdDesc('c'), $value, "InstCmdDesc $tag");
    my $ups = fixture('BEGIN LIST UPS', "UPS test $wire", 'UPS second "other"', 'END LIST UPS')->ListUPS();
    check(ref($ups) eq 'HASH' && keys(%$ups) == 2 && $ups->{test} eq $value && $ups->{second} eq 'other', "ListUPS $tag");
    foreach my $kind ('VAR', 'RW') {
        my $method = $kind eq 'VAR' ? 'ListVar' : 'ListRW';
        my $got = fixture("BEGIN LIST $kind test", "$kind test v $wire", "END LIST $kind test")->$method();
        check(ref($got) eq 'HASH' && $got->{v} eq $value, "$method $tag");
    }
    my $enum = fixture('BEGIN LIST ENUM test v', "ENUM test v $wire", 'END LIST ENUM test v')->ListEnum('v');
    check(ref($enum) eq 'ARRAY' && @$enum == 1 && $enum->[0] eq $value, "ListEnum $tag");
    my $f = fixture("VAR test v $wire");
    $f->{vars} = {v => 'cached'};
    equal($f->FETCH('v'), $value, "tied FETCH consumer $tag");
}
# Token boundaries follow parseconf rather than shell concatenation.
foreach my $case (
    ['"first"second', 'first'], ['word"quote', 'word"quote'],
    ["'single'", "'single'"], ['word#comment', 'word'],
    ['word=other', 'word'], ['"hash#literal" # comment', 'hash#literal'],
    ['back\\n', 'backn'], ['"back\\n"', 'backn'],
    ["unquoted\\\ncontinued", 'unquotedcontinued']
) {
    equal(fixture('VAR test v ' . $case->[0])->GetVar('v'), $case->[1], 'NUT token boundary ' . unpack('H*', $case->[0]));
}
foreach my $case (
    ['GetVar', 'VAR test v "unterminated'],
    ['GetUPSDesc', 'UPSDESC test "unterminated'],
    ['GetDesc', 'DESC test v "unterminated'],
    ['GetCmdDesc', 'CMDDESC test c "unterminated']
) {
    my $f = fixture($case->[1]);
    my $method = $case->[0];
    check(!defined($f->$method('v')) && $f->Error() ne 'No error explanation available.', "$method incomplete token");
}
my $types = fixture('TYPE test v RW ENUM STRING:20');
equal($types->GetType('v'), 'RW ENUM STRING:20', 'GetType scalar flags');
equal($types->VarType('v'), 'RW ENUM STRING:20', 'VarType alias');
my $commands = fixture('BEGIN LIST CMD test', 'CMD test first', 'CMD test second', 'END LIST CMD test')->ListCmd();
check(ref($commands) eq 'ARRAY' && join(',', @$commands) eq 'first,second', 'ListCmd array');
my $clients = fixture('BEGIN LIST CLIENT test', 'CLIENT test 127.0.0.1', 'END LIST CLIENT test')->ListClient();
check(ref($clients) eq 'HASH' && $clients->{'127.0.0.1'} eq '127.0.0.1', 'ListClient hash');
my $ranges = fixture('BEGIN LIST RANGE test v', 'RANGE test v "1" "9"', 'RANGE test v 10 20', 'END LIST RANGE test v')->ListRange('v');
check(ref($ranges) eq 'ARRAY' && @$ranges == 2 && $ranges->[0]{min} eq '1' && $ranges->[1]{max} eq '20', 'ListRange array of min/max hashes');
my $empty = fixture('BEGIN LIST UPS', 'END LIST UPS')->ListUPS();
check(ref($empty) eq 'HASH' && !keys(%$empty), 'empty UPS list');
$empty = fixture('BEGIN LIST ENUM test v', 'END LIST ENUM test v')->ListEnum('v');
check(ref($empty) eq 'ARRAY' && !@$empty, 'empty enum');
foreach my $method ('GetVar', 'GetUPSDesc', 'GetDesc', 'GetCmdDesc', 'ListUPS', 'ListVar', 'ListRW', 'ListEnum', 'ListCmd', 'ListRange', 'ListClient') {
    foreach my $response (undef, 'ERR UNKNOWN-UPS', 'unexpected') {
        my $f = fixture($response);
        check(!defined($f->$method('v')) && $f->Error() ne 'No error explanation available.', "$method failure contract");
    }
}
my $mismatch = fixture('VAR test other "x"');
check(!defined($mismatch->GetVar('v')) && $mismatch->Error() =~ /Requested v, received other/, 'GetVar mismatch');
my $truncated = fixture('BEGIN LIST UPS', 'UPS test "ordinary"');
check(!defined($truncated->ListUPS()) && $truncated->Error() =~ /Network error/, 'truncated list failure');

package AuthFixture;
use vars qw(@ISA);
@ISA = qw(UPS::Nut);
sub _send {
    my ($self, $line) = @_;
    push @{$self->{sent}}, $line;
    return shift @{$self->{replies}};
}
package main;
my $auth = bless {sent => [], replies => ['OK', 'OK']}, 'AuthFixture';
check($auth->Authenticate('user name', 'pass"\\#word'), 'Authenticate success');
equal(join("\n", @{$auth->{sent}}), 'USERNAME "user name"' . "\n" . 'PASSWORD "pass\\"\\\\\\#word"', 'Authenticate encodes both arguments');
check($auth->Authenticate('other', 'other') && @{$auth->{sent}} == 2, 'Authenticate already authenticated');
foreach my $replies ([undef], ['ERR INVALID-ARGUMENT'], ['OK', 'ERR ACCESS-DENIED']) {
    $auth = bless {sent => [], replies => $replies}, 'AuthFixture';
    check(!defined($auth->Authenticate('user', 'pass')) && $auth->Error() ne 'No error explanation available.', 'Authenticate failure contract');
}
$auth = bless {sent => [], replies => []}, 'AuthFixture';
check(!defined($auth->Authenticate(undef, 'pass')) && !@{$auth->{sent}}, 'Authenticate missing credentials');

my $cwd = cwd();
my $tmp = ($ENV{TMPDIR} || '/tmp') . "/nut-perl-parser-$$";
my $created_tmp = mkdir($tmp, 0700);
die "mkdir $tmp: $!" unless $created_tmp;
my $child;
END {
    if ($child) { kill 'TERM', $child; waitpid($child, 0); }
    if ($created_tmp) {
        chdir($cwd) if defined $cwd;
        rmtree($tmp) if -d $tmp;
    }
}
sub write_file {
    my ($name, $text) = @_;
    my $fh = FileHandle->new($name, 'w') or die "open $name: $!";
    print $fh $text;
    close($fh) or die "close $name: $!";
}
chdir($tmp) or die "chdir: $!";
# Check include dispatch without creating a Windows-invalid filename.
write_file('include-escapes.conf', <<'CONF');
INCLUDE_REQUIRED "part \"\#\\name.conf"
CONF
my $include;
my $read_authconf = \&UPS::Nut::AuthConf::readAuthConfFile;
{
    local *UPS::Nut::AuthConf::readAuthConfFile = sub { $include = $_[1]; return (); };
    $read_authconf->('UPS::Nut::AuthConf', 'include-escapes.conf', 1);
}
equal($include, 'part "#\\name.conf', 'include path quote and backslash decoding');
# Relative includes have historically resolved against cwd.
write_file('part #name.conf', "PASSWORD=\"included\\\\n\"\n");
write_file('scope.conf', "PASSWORD=ignored\n");
write_file('auth.conf', <<'CONF');
USERNAME=global
PASSWORD=first
INCLUDE_REQUIRED "part \#na\me.conf" # trailing comment
[admin@localhost:12345] # section comment
PASSWORD = "say \"yes\" \\n # literal"
INCLUDE scope.conf
[@localhost:12345]
USERNAME='literal'
PASSWORD=unquoted\ value
[_global_defaults]
CERTPATH = "two\
lines"
CERTFILE = "escaped\#hash" # ignored
CERTIDENT_NAME = unquoted\
continuation
CONF
UPS::Nut::AuthConf->freeAuthConfList();
my @conf;
my $parsed = eval { @conf = UPS::Nut::AuthConf->readAuthConfFile('auth.conf', 1); 1; };
check($parsed, 'AuthConf escaped required include opens');
my $ac = UPS::Nut::AuthConf->getAuthConf('admin', 'localhost', 12345, \@conf);
equal($ac->{pass}, 'say "yes" \\n # literal', 'AuthConf quoted escapes/hash and section comments');
equal($ac->{user}, 'admin', 'AuthConf section user precedence');
equal($ac->{certpath}, 'twolines', 'AuthConf logical continuation');
equal($ac->{certfile}, 'escaped#hash', 'AuthConf escaped hash and trailing comment');
equal($ac->{certident}, 'unquotedcontinuation', 'AuthConf unquoted continuation');
my %args = $ac->to_nut_args();
equal($args{PASSWORD}, $ac->{pass}, 'AuthConf constructor arguments decoded once');
$ac = UPS::Nut::AuthConf->getAuthConf(undef, 'localhost', 12345, \@conf);
equal($ac->{user}, "'literal'", 'AuthConf apostrophes literal');
equal($ac->{pass}, 'unquoted value', 'AuthConf escaped unquoted space');
$ac = UPS::Nut::AuthConf->getAuthConf(undef, 'elsewhere', 3493, \@conf);
equal($ac->{pass}, 'included\\n', 'AuthConf escaped include filename decoded once');
my @missing = UPS::Nut::AuthConf->readAuthConfFile('absent.conf');
check(!@missing, 'AuthConf missing optional file');
my $ok = eval { UPS::Nut::AuthConf->readAuthConfFile('absent.conf', 1); 1; };
check(!$ok && $@ ne '', 'AuthConf missing required file');

# Load optional TLS support before starting the socket fixture watchdogs.
eval "require IO::Socket::SSL; 1";

# Exercise the real socket implementation, constructor and buffered getline.
my $listener = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Listen => 1, Proto => 'tcp', ReuseAddr => 1) or die "listen: $!";
my $port = $listener->sockport();
my @dialog = (
    ['LIST VAR test', ["BEGIN LIST VAR test\nVAR test v \"a\\", "\"b\\", "\\c\"\nEND LIST VAR test\n"]],
    ['GET VAR test v', ["VAR test v \"a\\", "\"b\\\\c\"\n"]],
    ['LIST UPS', ["BE", "GIN LIST UPS\nUPS test \"a\\", "\"b\\\\c\"\nUPS second \"two words\"\nEND LIST U", "PS\n"]],
    ['LOGOUT', ["OK Goodbye\n"]],
);
# Flush TAP before fork; normal child exit also works with Windows pseudofork.
$| = 1;
$child = fork();
die "fork: $!" unless defined $child;
if (!$child) {
    $created_tmp = 0; # Only the parent owns temporary-directory cleanup.
    my $status = eval {
        $SIG{ALRM} = sub { die "server timeout\n"; };
        alarm(15);
        my $sock = $listener->accept() or die "accept: $!";
        $sock->autoflush(1);
        foreach my $exchange (@dialog) {
            my $line = $sock->getline();
            die "unexpected request\n" unless defined $line && $line eq $exchange->[0] . "\n";
            foreach my $chunk (@{$exchange->[1]}) {
                print $sock $chunk;
                select(undef, undef, undef, 0.01);
            }
        }
        close($sock);
        1;
    };
    print STDERR $@ unless $status;
    exit($status ? 0 : 1);
}
close($listener);
$SIG{ALRM} = sub { die "client timeout\n"; };
alarm(15);
my $nut = UPS::Nut->new(HOST => '127.0.0.1', PORT => $port, NAME => 'test', USESSL => 0, TIMEOUT => 3);
check(defined($nut), 'localhost constructor');
if ($nut) {
    equal($nut->{vars}{v}, 'a"b\\c', 'constructor ListVar decoded');
    equal($nut->GetVar('v'), 'a"b\\c', 'fragmented GetVar');
    my $list = $nut->ListUPS();
    check(ref($list) eq 'HASH' && $list->{test} eq 'a"b\\c' && $list->{second} eq 'two words', 'fragmented and coalesced LIST');
    $nut->Logout();
}
waitpid($child, 0);
check($? == 0, 'localhost server completed');
$child = undef;
alarm(0);
print "1..$checks\n";
exit($failures ? 1 : 0);
