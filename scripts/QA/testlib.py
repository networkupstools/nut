from __future__ import print_function
#
#    testlib.py quality assurance test script
#    Copyright (C) 2008-2016 Canonical Ltd.
#
#    This library is free software; you can redistribute it and/or
#    modify it under the terms of the GNU Library General Public
#    License as published by the Free Software Foundation; either
#    version 2 of the License.
#
#    This library is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#    Library General Public License for more details.
#
#    You should have received a copy of the GNU Library General Public
#    License along with this program.  If not, see
#    <http://www.gnu.org/licenses/>.
#

'''Common classes and functions for package tests.'''

import crypt
import glob
import grp
import gzip
import os
import os.path
import pwd
import random
import re
import shutil
import socket
import string
import subprocess
import sys
import tempfile
import time
import unittest

from stat import ST_SIZE

# Don't make python-pexpect mandatory
try:
    import pexpect
except ImportError:
    pass

import warnings
warnings.filterwarnings('ignore', message=r'.*apt_pkg\.TagFile.*', category=DeprecationWarning)
try:
    import apt_pkg
    # cope with apt_pkg api changes.
    if 'init_system' in dir(apt_pkg):
        apt_pkg.init_system()
    else:
        apt_pkg.InitSystem()
except:
    # On non-Debian system, fall back to simple comparison without debianisms
    class apt_pkg(object):
        @staticmethod
        def version_compare(one, two):
            list_one = one.split('.')
            list_two = two.split('.')
            while len(list_one) > 0 and len(list_two) > 0:
                try:
                    if int(list_one[0]) > int(list_two[0]):
                        return 1
                    if int(list_one[0]) < int(list_two[0]):
                        return -1
                except:
                    # ugh, non-numerics in the version, fall back to
                    # string comparison, which will be wrong for e.g.
                    # 3.2 vs 3.16rc1
                    if list_one[0] > list_two[0]:
                        return 1
                    if list_one[0] < list_two[0]:
                        return -1
                list_one.pop(0)
                list_two.pop(0)
            return 0

        @staticmethod
        def VersionCompare(one, two):
            return apt_pkg.version_compare(one, two)

bogus_nxdomain = "208.69.32.132"

# http://www.chiark.greenend.org.uk/ucgi/~cjwatson/blosxom/2009-07-02-python-sigpipe.html
# This is needed so that the subprocesses that produce endless output
# actually quit when the reader goes away.
import signal
def subprocess_setup():
    # Python installs a SIGPIPE handler by default. This is usually not what
    # non-Python subprocesses expect.
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)


class TimedOutException(Exception):
    def __init__(self, value="Timed Out"):
        self.value = value

    def __str__(self):
        return repr(self.value)


def _restore_backup(path):
    pathbackup = path + '.autotest'
    if os.path.exists(pathbackup):
        shutil.move(pathbackup, path)


def _save_backup(path):
    pathbackup = path + '.autotest'
    if os.path.exists(path) and not os.path.exists(pathbackup):
        shutil.copy2(path, pathbackup)
        # copy2 does not copy ownership, so do it here.
        # Reference: http://docs.python.org/library/shutil.html
        a = os.stat(path)
        os.chown(pathbackup, a[4], a[5])


def config_copydir(path):
    if os.path.exists(path) and not os.path.isdir(path):
        raise OSError("'%s' is not a directory" % (path))
    _restore_backup(path)

    pathbackup = path + '.autotest'
    if os.path.exists(path):
        shutil.copytree(path, pathbackup, symlinks=True)


def config_replace(path, contents, append=False):
    '''Replace (or append) to a config file'''
    _restore_backup(path)
    if os.path.exists(path):
        _save_backup(path)
        if append:
            with open(path) as fh:
                contents = fh.read() + contents
    with open(path, 'w') as fh:
        fh.write(contents)


def config_comment(path, field):
    _save_backup(path)
    contents = ""
    with open(path) as fh:
        for line in fh:
            if re.search("^\s*%s\s*=" % (field), line):
                line = "#" + line
            contents += line

    with open(path + '.new', 'w') as new_fh:
        new_fh.write(contents)
    os.rename(path + '.new', path)


def config_set(path, field, value, spaces=True):
    _save_backup(path)
    contents = ""
    if spaces:
        setting = '%s = %s\n' % (field, value)
    else:
        setting = '%s=%s\n' % (field, value)
    found = False
    with open(path) as fh:
        for line in fh:
            if re.search("^\s*%s\s*=" % (field), line):
                found = True
                line = setting
            contents += line
    if not found:
        contents += setting

    with open(path + '.new', 'w') as new_config:
        new_config.write(contents)
    os.rename(path + '.new', path)


def config_patch(path, patch, depth=1):
    '''Patch a config file'''
    _restore_backup(path)
    _save_backup(path)

    handle, name = mkstemp_fill(patch)
    rc = subprocess.call(['/usr/bin/patch', '-p%s' % depth, path], stdin=handle, stdout=subprocess.PIPE)
    os.unlink(name)
    if rc != 0:
        raise Exception("Patch failed")


def config_restore(path):
    '''Rename a replaced config file back to its initial state'''
    _restore_backup(path)


def timeout(secs, f, *args):
    def handler(signum, frame):
        raise TimedOutException()

    old = signal.signal(signal.SIGALRM, handler)
    result = None
    signal.alarm(secs)
    try:
        result = f(*args)
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, old)

    return result


def require_nonroot():
    if os.geteuid() == 0:
        print("This series of tests should be run as a regular user with sudo access, not as root.", file=sys.stderr)
        sys.exit(1)


def require_root():
    if os.geteuid() != 0:
        print("This series of tests should be run with root privileges (e.g. via sudo).", file=sys.stderr)
        sys.exit(1)


def require_sudo():
    if os.geteuid() != 0 or os.environ.get('SUDO_USER', None) is None:
        print("This series of tests must be run under sudo.", file=sys.stderr)
        sys.exit(1)
    if os.environ['SUDO_USER'] == 'root':
        print('Please run this test using sudo from a regular user. (You ran sudo from root.)', file=sys.stderr)
        sys.exit(1)


def random_string(length, lower=False):
    '''Return a random string, consisting of ASCII letters, with given
    length.'''

    s = ''
    selection = string.ascii_letters
    if lower:
        selection = string.ascii_lowercase
    maxind = len(selection) - 1
    for l in range(length):
        s += selection[random.randint(0, maxind)]
    return s


def mkstemp_fill(contents, suffix='', prefix='testlib-', dir=None):
    '''As tempfile.mkstemp does, return a (file, name) pair, but with
    prefilled contents.'''

    handle, name = tempfile.mkstemp(suffix=suffix, prefix=prefix, dir=dir)
    os.close(handle)
    handle = open(name, "w+")
    handle.write(contents)
    handle.flush()
    handle.seek(0)

    return handle, name


def create_fill(path, contents, mode=0o644):
    '''Safely create a page'''
    # make the temp file in the same dir as the destination file so we
    # don't get invalid cross-device link errors when we rename
    handle, name = mkstemp_fill(contents, dir=os.path.dirname(path))
    handle.close()
    os.rename(name, path)
    os.chmod(path, mode)


def login_exists(login):
    '''Checks whether the given login exists on the system.'''

    try:
        pwd.getpwnam(login)
        return True
    except KeyError:
        return False


def group_exists(group):
    '''Checks whether the given login exists on the system.'''

    try:
        grp.getgrnam(group)
        return True
    except KeyError:
        return False


def is_empty_file(path):
    '''test if file is empty, returns True if so'''
    with open(path) as fh:
        return (len(fh.read()) == 0)


def recursive_rm(dirPath, contents_only=False):
    '''recursively remove directory'''
    names = os.listdir(dirPath)
    for name in names:
        path = os.path.join(dirPath, name)
        if os.path.islink(path) or not os.path.isdir(path):
            os.unlink(path)
        else:
            recursive_rm(path)
    if not contents_only:
        os.rmdir(dirPath)


def check_pidfile(exe, pidfile):
    '''Checks if pid in pidfile is running'''
    if not os.path.exists(pidfile):
        return False

    # get the pid
    try:
        with open(pidfile, 'r') as fd:
            pid = fd.readline().rstrip('\n')
    except:
        return False

    return check_pid(exe, pid)


def check_pid(exe, pid):
    '''Checks if pid is running'''
    cmdline = "/proc/%s/cmdline" % (str(pid))
    if not os.path.exists(cmdline):
        return False

    # get the command line
    try:
        with open(cmdline, 'r') as fd:
            tmp = fd.readline().split('\0')
    except:
        return False

    # this allows us to match absolute paths or just the executable name
    if re.match('^' + exe + '$', tmp[0]) or \
       re.match('.*/' + exe + '$', tmp[0]) or \
       re.match('^' + exe + ': ', tmp[0]) or \
       re.match('^\(' + exe + '\)', tmp[0]):
        return True

    return False


def check_port(port, proto, ver=4):
    '''Check if something is listening on the specified port.
       WARNING: for some reason this does not work with a bind mounted /proc
    '''
    assert (port >= 1)
    assert (port <= 65535)
    assert (proto.lower() == "tcp" or proto.lower() == "udp")
    assert (ver == 4 or ver == 6)

    fn = "/proc/net/%s" % (proto)
    if ver == 6:
        fn += str(ver)

    rc, report = cmd(['cat', fn])
    assert (rc == 0)

    hport = "%0.4x" % port

    if re.search(': [0-9a-f]{8}:%s [0-9a-f]' % str(hport).lower(), report.lower()):
        return True
    return False


def get_arch():
    '''Get the current architecture'''
    rc, report = cmd(['uname', '-m'])
    assert (rc == 0)
    return report.strip()


def get_memory():
    '''Gets total ram and swap'''
    meminfo = "/proc/meminfo"
    memtotal = 0
    swaptotal = 0
    if not os.path.exists(meminfo):
        return (False, False)

    try:
        fd = open(meminfo, 'r')
        for line in fd.readlines():
            splitline = line.split()
            if splitline[0] == 'MemTotal:':
                memtotal = int(splitline[1])
            elif splitline[0] == 'SwapTotal:':
                swaptotal = int(splitline[1])
        fd.close()
    except:
        return (False, False)

    return (memtotal, swaptotal)


def is_running_in_vm():
    '''Check if running under a VM'''
    # add other virtualization environments here
    for search in ['QEMU Virtual CPU']:
        rc, report = cmd_pipe(['dmesg'], ['grep', search])
        if rc == 0:
            return True
    return False


def ubuntu_release():
    '''Get the Ubuntu release'''
    f = "/etc/lsb-release"
    try:
        size = os.stat(f)[ST_SIZE]
    except:
        return "UNKNOWN"

    if size > 1024 * 1024:
        raise IOError('Could not open "%s" (too big)' % f)

    with open("/etc/lsb-release", 'r') as fh:
        lines = fh.readlines()

    pat = re.compile(r'DISTRIB_CODENAME')
    for line in lines:
        if pat.search(line):
            return line.split('=')[1].rstrip('\n').rstrip('\r')

    return "UNKNOWN"


def cmd(command, input=None, stderr=subprocess.STDOUT, stdout=subprocess.PIPE, stdin=None, timeout=None):
    '''Try to execute given command (array) and return its stdout, or return
    a textual error if it failed.'''

    try:
        sp = subprocess.Popen(command, stdin=stdin, stdout=stdout, stderr=stderr, close_fds=True, preexec_fn=subprocess_setup, universal_newlines=True)
    except OSError as e:
        return [127, str(e)]

    out, outerr = sp.communicate(input)
    # Handle redirection of stdout
    if out is None:
        out = ''
    # Handle redirection of stderr
    if outerr is None:
        outerr = ''
    return [sp.returncode, out + outerr]


def cmd_pipe(command1, command2, input=None, stderr=subprocess.STDOUT, stdin=None):
    '''Try to pipe command1 into command2.'''
    try:
        sp1 = subprocess.Popen(command1, stdin=stdin, stdout=subprocess.PIPE, stderr=stderr, close_fds=True)
        sp2 = subprocess.Popen(command2, stdin=sp1.stdout, stdout=subprocess.PIPE, stderr=stderr, close_fds=True)
    except OSError as e:
        return [127, str(e)]

    out = sp2.communicate(input)[0]
    return [sp2.returncode, out]


def cwd_has_enough_space(cdir, total_bytes):
    '''Determine if the partition of the current working directory has 'bytes'
       free.'''
    rc, df_output = cmd(['df'])
    if rc != 0:
        result = 'df failed, got exit code %d, expected %d\n' % (rc, 0)
        raise OSError(result)
    if rc != 0:
        return False

    kb = total_bytes / 1024

    mounts = dict()
    for line in df_output.splitlines():
        if '/' not in line:
            continue
        tmp = line.split()
        mounts[tmp[5]] = int(tmp[3])

    cdir = os.getcwd()
    while cdir != '/':
        if cdir not in mounts:
            cdir = os.path.dirname(cdir)
            continue
        return kb < mounts[cdir]

    return kb < mounts['/']


def get_md5(filename):
    '''Gets the md5sum of the file specified'''

    (rc, report) = cmd(["/usr/bin/md5sum", "-b", filename])
    expected = 0
    assert (expected == rc)

    return report.split(' ')[0]


def dpkg_compare_installed_version(pkg, check, version):
    '''Gets the version for the installed package, and compares it to the
       specified version.
    '''
    (rc, report) = cmd(["/usr/bin/dpkg", "-s", pkg])
    assert (rc == 0)
    assert ("Status: install ok installed" in report)
    installed_version = ""
    for line in report.splitlines():
        if line.startswith("Version: "):
            installed_version = line.split()[1]

    assert (installed_version != "")

    (rc, report) = cmd(["/usr/bin/dpkg", "--compare-versions", installed_version, check, version])
    assert (rc == 0 or rc == 1)
    if rc == 0:
        return True
    return False


def _run_apt_command(pkg_list, apt_cmd='install'):
    env = os.environ.copy()
    env['DEBIAN_FRONTEND'] = 'noninteractive'
    # debugging version, but on precise doesn't actually run dpkg
    # command = ['apt-get', '-y', '--force-yes', '-o', 'Dpkg::Options::=--force-confold', '-o', 'Debug::pkgDPkgPM=true', apt_cmd]
    command = ['apt-get', '-y', '--force-yes', '-o', 'Dpkg::Options::=--force-confold', apt_cmd]
    command.extend(pkg_list)
    rc, report = cmd(command)
    return rc, report


# note: for the following install_* functions, you probably want the
# versions in the TestlibCase class (see below)
def install_builddeps(src_pkg):
    rc, report = _run_apt_command([src_pkg], 'build-dep')
    assert(rc == 0)


def install_package(package):
    rc, report = _run_apt_command([package], 'install')
    assert(rc == 0)


def install_packages(pkg_list):
    rc, report = _run_apt_command(pkg_list, 'install')
    assert(rc == 0)


def prepare_source(source, builder, cached_src, build_src, patch_system):
    '''Download and unpack source package, installing necessary build depends,
       adjusting the permissions for the 'builder' user, and returning the
       directory of the unpacked source. Patch system can be one of:
       - cdbs
       - dpatch
       - quilt
       - quiltv3
       - None (not the string)

       This is normally used like this:

       def setUp(self):
           ...
           self.topdir = os.getcwd()
           self.cached_src = os.path.join(os.getcwd(), "source")
           self.tmpdir = tempfile.mkdtemp(prefix='testlib', dir='/tmp')
           self.builder = testlib.TestUser()
           testlib.cmd(['chgrp', self.builder.login, self.tmpdir])
           os.chmod(self.tmpdir, 0775)

       def tearDown(self):
           ...
           self.builder = None
           self.topdir = os.getcwd()
           if os.path.exists(self.tmpdir):
               testlib.recursive_rm(self.tmpdir)

       def test_suite_build(self):
           ...
           build_dir = testlib.prepare_source('foo', \
                                         self.builder, \
                                         self.cached_src, \
                                         os.path.join(self.tmpdir, \
                                           os.path.basename(self.cached_src)),
                                         "quilt")
           os.chdir(build_dir)

           # Example for typical build, adjust as necessary
           print("")
           print("  make clean")
           rc, report = testlib.cmd(['sudo', '-u', self.builder.login, 'make', 'clean'])

           print("  configure")
           rc, report = testlib.cmd(['sudo', '-u', self.builder.login, './configure', '--prefix=%s' % self.tmpdir, '--enable-debug'])

           print("  make (will take a while)")
           rc, report = testlib.cmd(['sudo', '-u', self.builder.login, 'make'])

           print("  make check (will take a while)",)
           rc, report = testlib.cmd(['sudo', '-u', self.builder.login, 'make', 'check'])
           expected = 0
           result = 'Got exit code %d, expected %d\n' % (rc, expected)
           self.assertEqual(expected, rc, result + report)

        def test_suite_cleanup(self):
            ...
            if os.path.exists(self.cached_src):
                testlib.recursive_rm(self.cached_src)

       It is up to the caller to clean up cached_src and build_src (as in the
       above example, often the build_src is in a tmpdir that is cleaned in
       tearDown() and the cached_src is cleaned in a one time clean-up
       operation (eg 'test_suite_cleanup()) which must be run after the build
       suite test (obviously).
       '''

    # Make sure we have a clean slate
    assert (os.path.exists(os.path.dirname(build_src)))
    assert (not os.path.exists(build_src))

    cdir = os.getcwd()
    if os.path.exists(cached_src):
        shutil.copytree(cached_src, build_src)
        os.chdir(build_src)
    else:
        # Only install the build dependencies on the initial setup
        install_builddeps(source)
        os.makedirs(build_src)
        os.chdir(build_src)

        # These are always needed
        pkgs = ['build-essential', 'dpkg-dev', 'fakeroot']
        install_packages(pkgs)

        rc, report = cmd(['apt-get', 'source', source])
        assert (rc == 0)
        shutil.copytree(build_src, cached_src)
        print("(unpacked %s)" % os.path.basename(glob.glob('%s_*.dsc' % source)[0]), end=' ')

    unpacked_dir = os.path.join(build_src, glob.glob('%s-*' % source)[0])

    # Now apply the patches. Do it here so that we don't mess up our cached
    # sources.
    os.chdir(unpacked_dir)
    assert (patch_system in ['cdbs', 'dpatch', 'quilt', 'quiltv3', None])
    if patch_system is not None and patch_system != "quiltv3":
        if patch_system == "quilt":
            os.environ.setdefault('QUILT_PATCHES', 'debian/patches')
            rc, report = cmd(['quilt', 'push', '-a'])
            assert (rc == 0)
        elif patch_system == "cdbs":
            rc, report = cmd(['./debian/rules', 'apply-patches'])
            assert (rc == 0)
        elif patch_system == "dpatch":
            rc, report = cmd(['dpatch', 'apply-all'])
            assert (rc == 0)

    cmd(['chown', '-R', '%s:%s' % (builder.uid, builder.gid), build_src])
    os.chdir(cdir)

    return unpacked_dir


def get_changelog_version(source_dir):
    '''Extract a package version from a changelog'''
    package_version = ""

    changelog_file = os.path.join(source_dir, "debian/changelog")

    if os.path.exists(changelog_file):
        changelog = open(changelog_file, 'r')
        header = changelog.readline().split()
        package_version = header[1].strip('()')

    return package_version


def _aa_status():
    '''Get aa-status output'''
    exe = "/usr/sbin/aa-status"
    assert (os.path.exists(exe))
    if os.geteuid() == 0:
        return cmd([exe])
    return cmd(['sudo', exe])


def is_apparmor_loaded(path):
    '''Check if profile is loaded'''
    rc, report = _aa_status()
    if rc != 0:
        return False

    for line in report.splitlines():
        if line.endswith(path):
            return True
    return False


def is_apparmor_confined(path):
    '''Check if application is confined'''
    rc, report = _aa_status()
    if rc != 0:
        return False

    for line in report.splitlines():
        if re.search('%s \(' % path, line):
            return True
    return False


def check_apparmor(path, first_ubuntu_release, is_running=True):
    '''Check if path is loaded and confined for everything higher than the
       first Ubuntu release specified.

       Usage:
        rc, report = testlib.check_apparmor('/usr/sbin/foo', 8.04, is_running=True)
        if rc < 0:
            return self._skipped(report)

        expected = 0
        result = 'Got exit code %d, expected %d\n' % (rc, expected)
        self.assertEqual(expected, rc, result + report)
     '''
    global manager
    rc = -1

    if manager.lsb_release["Release"] < first_ubuntu_release:
        return (rc, "Skipped apparmor check")

    if not os.path.exists('/sbin/apparmor_parser'):
        return (rc, "Skipped (couldn't find apparmor_parser)")

    rc = 0
    msg = ""
    if not is_apparmor_loaded(path):
        rc = 1
        msg = "Profile not loaded for '%s'" % path

    # this check only makes sense it the 'path' is currently executing
    if is_running and rc == 0 and not is_apparmor_confined(path):
        rc = 1
        msg = "'%s' is not running in enforce mode" % path

    return (rc, msg)


def get_gcc_version(gcc, full=True):
    gcc_version = 'none'
    if not gcc.startswith('/'):
        gcc = '/usr/bin/%s' % (gcc)
    if os.path.exists(gcc):
        gcc_version = 'unknown'
        lines = cmd([gcc, '-v'])[1].strip().splitlines()
        version_lines = [x for x in lines if x.startswith('gcc version')]
        if len(version_lines) == 1:
            gcc_version = " ".join(version_lines[0].split()[2:])
    if not full:
        return gcc_version.split()[0]
    return gcc_version


def is_kdeinit_running():
    '''Test if kdeinit is running'''
    # applications that use kdeinit will spawn it if it isn't running in the
    # test. This is a problem because it does not exit. This is a helper to
    # check for it.
    rc, report = cmd(['ps', 'x'])
    if 'kdeinit4 Running' not in report:
        print("kdeinit not running (you may start/stop any KDE application then run this script again)", file=sys.stderr)
        return False
    return True


def get_pkgconfig_flags(libs=[]):
    '''Find pkg-config flags for libraries'''
    assert (len(libs) > 0)
    rc, pkg_config = cmd(['pkg-config', '--cflags', '--libs'] + libs)
    expected = 0
    if rc != expected:
        print('Got exit code %d, expected %d\n' % (rc, expected), file=sys.stderr)
    assert(rc == expected)
    return pkg_config.split()


def cap_to_name(cap_num):
    '''given an integer, return the capability name'''
    rc, output = cmd(['capsh', '--decode=%x' % cap_num])
    expected = 0
    if rc != expected:
        print('capsh: got exit code %d, expected %d\n' % (rc, expected), file=sys.stderr)
    cap_name = output.strip().split('=')[1]
    return cap_name


def enumerate_capabilities():
    i = 0
    cap_list = []
    done = False
    while not done:
        cap_name = cap_to_name(pow(2, i))
        if cap_name == str(i):
            done = True
        else:
            cap_list.append(cap_name)
            i += 1
        if i > 64:
            done = True
    return cap_list


class TestDaemon:
    '''Helper class to manage daemons consistently'''
    def __init__(self, init):
        '''Setup daemon attributes'''
        self.initscript = init

    def start(self):
        '''Start daemon'''
        rc, report = cmd([self.initscript, 'start'])
        expected = 0
        result = 'Got exit code %d, expected %d\n' % (rc, expected)
        time.sleep(2)
        if expected != rc:
            return (False, result + report)

        if "fail" in report:
            return (False, "Found 'fail' in report\n" + report)

        return (True, "")

    def stop(self):
        '''Stop daemon'''
        rc, report = cmd([self.initscript, 'stop'])
        expected = 0
        result = 'Got exit code %d, expected %d\n' % (rc, expected)
        if expected != rc:
            return (False, result + report)

        if "fail" in report:
            return (False, "Found 'fail' in report\n" + report)

        return (True, "")

    def reload(self):
        '''Reload daemon'''
        rc, report = cmd([self.initscript, 'force-reload'])
        expected = 0
        result = 'Got exit code %d, expected %d\n' % (rc, expected)
        if expected != rc:
            return (False, result + report)

        if "fail" in report:
            return (False, "Found 'fail' in report\n" + report)

        return (True, "")

    def restart(self):
        '''Restart daemon'''
        (res, str) = self.stop()
        if not res:
            return (res, str)

        (res, str) = self.start()
        if not res:
            return (res, str)

        return (True, "")

    def force_restart(self):
        '''Restart daemon even if already stopped'''
        (res, str) = self.stop()

        (res, str) = self.start()
        if not res:
            return (res, str)

        return (True, "")

    def status(self):
        '''Check daemon status'''
        rc, report = cmd([self.initscript, 'status'])
        expected = 0
        result = 'Got exit code %d, expected %d\n' % (rc, expected)
        if expected != rc:
            return (False, result + report)

        if "fail" in report:
            return (False, "Found 'fail' in report\n" + report)

        return (True, "")


class TestlibManager(object):
    '''Singleton class used to set up per-test-run information'''
    def __init__(self):
        # Set glibc aborts to dump to stderr instead of the tty so test output
        # is more sane.
        os.environ.setdefault('LIBC_FATAL_STDERR_', '1')

        # check verbosity
        self.verbosity = False
        if (len(sys.argv) > 1 and '-v' in sys.argv[1:]):
            self.verbosity = True

        # Load LSB release file
        self.lsb_release = dict()
        if not os.path.exists('/usr/bin/lsb_release') and not os.path.exists('/bin/lsb_release'):
            raise OSError("Please install 'lsb-release'")
        for line in subprocess.Popen(['lsb_release', '-a'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True).communicate()[0].splitlines():
            field, value = line.split(':', 1)
            value = value.strip()
            field = field.strip()
            # Convert numerics
            try:
                value = float(value)
            except:
                pass
            self.lsb_release.setdefault(field, value)

        # FIXME: hack OEM releases into known-Ubuntu versions
        if self.lsb_release['Distributor ID'] == "HP MIE (Mobile Internet Experience)":
            if self.lsb_release['Release'] == 1.0:
                self.lsb_release['Distributor ID'] = "Ubuntu"
                self.lsb_release['Release'] = 8.04
            else:
                raise OSError("Unknown version of HP MIE")

        # FIXME: hack to assume a most-recent release if we're not
        # running under Ubuntu.
        if self.lsb_release['Distributor ID'] not in ["Ubuntu", "Linaro"]:
            self.lsb_release['Release'] = 10000
        # Adjust Linaro release to pretend to be Ubuntu
        if self.lsb_release['Distributor ID'] in ["Linaro"]:
            self.lsb_release['Distributor ID'] = "Ubuntu"
            self.lsb_release['Release'] -= 0.01

        # Load arch
        if not os.path.exists('/usr/bin/dpkg'):
            machine = cmd(['uname', '-m'])[1].strip()
            if machine.endswith('86'):
                self.dpkg_arch = 'i386'
            elif machine.endswith('_64'):
                self.dpkg_arch = 'amd64'
            elif machine.startswith('arm'):
                self.dpkg_arch = 'armel'
            else:
                raise ValueError("Unknown machine type '%s'" % (machine))
        else:
            self.dpkg_arch = cmd(['dpkg', '--print-architecture'])[1].strip()

        # Find kernel version
        self.kernel_is_ubuntu = False
        self.kernel_version_signature = None
        self.kernel_version = cmd(["uname", "-r"])[1].strip()
        versig = '/proc/version_signature'
        if os.path.exists(versig):
            self.kernel_is_ubuntu = True
            self.kernel_version_signature = open(versig).read().strip()
            self.kernel_version_ubuntu = self.kernel_version
        elif os.path.exists('/usr/bin/dpkg'):
            # this can easily be inaccurate but is only an issue for Dapper
            rc, out = cmd(['dpkg', '-l', 'linux-image-%s' % (self.kernel_version)])
            if rc == 0:
                self.kernel_version_signature = out.strip().split('\n').pop().split()[2]
                self.kernel_version_ubuntu = self.kernel_version_signature
        if self.kernel_version_signature is None:
            # Attempt to fall back to something for non-Debian-based
            self.kernel_version_signature = self.kernel_version
            self.kernel_version_ubuntu = self.kernel_version
        # Build ubuntu version without hardware suffix
        try:
            self.kernel_version_ubuntu = "-".join([x for x in self.kernel_version_signature.split(' ')[1].split('-') if re.search('^[0-9]', x)])
        except:
            pass

        # Find gcc version
        self.gcc_version = get_gcc_version('gcc')

        # Find libc
        self.path_libc = [x.split()[2] for x in cmd(['ldd', '/bin/ls'])[1].splitlines() if x.startswith('\tlibc.so.')][0]

        # Report self
        if self.verbosity:
            kernel = self.kernel_version_ubuntu
            if kernel != self.kernel_version_signature:
                kernel += " (%s)" % (self.kernel_version_signature)
            print("Running test: '%s' distro: '%s %.2f' kernel: '%s' arch: '%s' uid: %d/%d SUDO_USER: '%s')" % (
                sys.argv[0],
                self.lsb_release['Distributor ID'],
                self.lsb_release['Release'],
                kernel,
                self.dpkg_arch,
                os.geteuid(), os.getuid(),
                os.environ.get('SUDO_USER', '')), file=sys.stdout)
            sys.stdout.flush()

        # Additional heuristics
        # if os.environ.get('SUDO_USER', os.environ.get('USER', '')) in ['mdeslaur']:
        #    sys.stdout.write("Replying to Marc Deslauriers in http://launchpad.net/bugs/%d: " % random.randint(600000, 980000))
        #    sys.stdout.flush()
        #    time.sleep(0.5)
        #    sys.stdout.write("destroyed\n")
        #    time.sleep(0.5)

    def hello(self, msg):
        print("Hello from %s" % (msg), file=sys.stderr)
# The central instance
manager = TestlibManager()


class TestlibCase(unittest.TestCase):
    def __init__(self, *args):
        '''This is called for each TestCase test instance, which isn't much better
           than SetUp.'''

        unittest.TestCase.__init__(self, *args)

        # Attach to and duplicate dicts from manager singleton
        self.manager = manager
        # self.manager.hello(repr(self) + repr(*args))
        self.my_verbosity = self.manager.verbosity
        self.lsb_release = self.manager.lsb_release
        self.dpkg_arch = self.manager.dpkg_arch
        self.kernel_version = self.manager.kernel_version
        self.kernel_version_signature = self.manager.kernel_version_signature
        self.kernel_version_ubuntu = self.manager.kernel_version_ubuntu
        self.kernel_is_ubuntu = self.manager.kernel_is_ubuntu
        self.gcc_version = self.manager.gcc_version
        self.path_libc = self.manager.path_libc

    def version_compare(self, one, two):
        if 'version_compare' in dir(apt_pkg):
            return apt_pkg.version_compare(one, two)
        else:
            return apt_pkg.VersionCompare(one, two)

    def assertFileType(self, filename, filetype, strict=True):
        '''Checks the file type of the file specified'''

        (rc, report, out) = self._testlib_shell_cmd(["/usr/bin/file", "-b", filename])
        out = out.strip()
        expected = 0
        # Absolutely no idea why this happens on Hardy
        if self.lsb_release['Release'] == 8.04 and rc == 255 and len(out) > 0:
            rc = 0
        result = 'Got exit code %d, expected %d:\n%s\n' % (rc, expected, report)
        self.assertEqual(expected, rc, result)

        if strict:
            filetype = '^%s$' % (filetype)
        else:
            # accept if the beginning of the line matches
            filetype = '^%s' % (filetype)
        result = 'File type reported by file: [%s], expected regex: [%s]\n' % (out, filetype)
        self.assertNotEqual(None, re.search(filetype, out), result)

    def yank_commonname_from_cert(self, certfile):
        '''Extract the commonName from a given PEM'''
        rc, out = cmd(['openssl', 'asn1parse', '-in', certfile])
        if rc == 0:
            ready = False
            for line in out.splitlines():
                if ready:
                    return line.split(':')[-1]
                if ':commonName' in line:
                    ready = True
        return socket.getfqdn()

    def announce(self, text):
        if self.my_verbosity:
            print("(%s) " % (text), file=sys.stdout, end='')
            sys.stdout.flush()

    def make_clean(self):
        rc, output = self.shell_cmd(['make', 'clean'])
        self.assertEqual(rc, 0, output)

    def get_makefile_compiler(self):
        # Find potential compiler name
        compiler = 'gcc'
        if os.path.exists('Makefile'):
            for line in open('Makefile'):
                if line.startswith('CC') and '=' in line:
                    items = [x.strip() for x in line.split('=')]
                    if items[0] == 'CC':
                        compiler = items[1]
                        break
        return compiler

    def make_target(self, target, expected=0):
        '''Compile a target and report output'''

        compiler = self.get_makefile_compiler()
        rc, output = self.shell_cmd(['make', target])
        self.assertEqual(rc, expected, 'rc(%d)!=%d:\n' % (rc, expected) + output)
        self.assertTrue('%s ' % (compiler) in output, 'Expected "%s":' % (compiler) + output)
        return output

    # call as   return testlib.skipped()
    def _skipped(self, reason=""):
        '''Provide a visible way to indicate that a test was skipped'''
        if reason != "":
            reason = ': %s' % (reason)
        self.announce("skipped%s" % (reason))
        return False

    def _testlib_shell_cmd(self, args, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT):
        argstr = "'" + "', '".join(args).strip() + "'"
        rc, out = cmd(args, stdin=stdin, stdout=stdout, stderr=stderr)
        report = 'Command: ' + argstr + '\nOutput:\n' + out
        return rc, report, out

    def shell_cmd(self, args, stdin=None):
        return cmd(args, stdin=stdin)

    def assertShellExitEquals(self, expected, args, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, msg=""):
        '''Test a shell command matches a specific exit code'''
        rc, report, out = self._testlib_shell_cmd(args, stdin=stdin, stdout=stdout, stderr=stderr)
        result = 'Got exit code %d, expected %d\n' % (rc, expected)
        self.assertEqual(expected, rc, msg + result + report)

    # make sure exit value is in a list of expected values
    def assertShellExitIn(self, expected, args, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, msg=""):
        '''Test a shell command matches a specific exit code'''
        rc, report, out = self._testlib_shell_cmd(args, stdin=stdin, stdout=stdout, stderr=stderr)
        result = 'Got exit code %d, expected one of %s\n' % (rc, ', '.join(map(str, expected)))
        self.assertIn(rc, expected, msg + result + report)

    def assertShellExitNotEquals(self, unwanted, args, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, msg=""):
        '''Test a shell command doesn't match a specific exit code'''
        rc, report, out = self._testlib_shell_cmd(args, stdin=stdin, stdout=stdout, stderr=stderr)
        result = 'Got (unwanted) exit code %d\n' % rc
        self.assertNotEqual(unwanted, rc, msg + result + report)

    def assertShellOutputContains(self, text, args, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, msg="", invert=False, expected=None):
        '''Test a shell command contains a specific output'''
        rc, report, out = self._testlib_shell_cmd(args, stdin=stdin, stdout=stdout, stderr=stderr)
        result = 'Got exit code %d.  Looking for text "%s"\n' % (rc, text)
        if not invert:
            self.assertTrue(text in out, msg + result + report)
        else:
            self.assertFalse(text in out, msg + result + report)
        if expected is not None:
            result = 'Got exit code %d. Expected %d (%s)\n' % (rc, expected, " ".join(args))
            self.assertEqual(rc, expected, msg + result + report)

    def assertShellOutputEquals(self, text, args, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, msg="", invert=False, expected=None):
        '''Test a shell command matches a specific output'''
        rc, report, out = self._testlib_shell_cmd(args, stdin=stdin, stdout=stdout, stderr=stderr)
        result = 'Got exit code %d. Looking for exact text "%s" (%s)\n' % (rc, text, " ".join(args))
        if not invert:
            self.assertEqual(text, out, msg + result + report)
        else:
            self.assertNotEqual(text, out, msg + result + report)
        if expected is not None:
            result = 'Got exit code %d. Expected %d (%s)\n' % (rc, expected, " ".join(args))
            self.assertEqual(rc, expected, msg + result + report)

    def _word_find(self, report, content, invert=False):
        '''Check for a specific string'''
        if invert:
            warning = 'Found "%s"\n' % content
            self.assertTrue(content not in report, warning + report)
        else:
            warning = 'Could not find "%s"\n' % content
            self.assertTrue(content in report, warning + report)

    def _test_sysctl_value(self, path, expected, msg=None, exists=True):
        sysctl = '/proc/sys/%s' % (path)
        self.assertEqual(exists, os.path.exists(sysctl), sysctl)
        value = None
        if exists:
            with open(sysctl) as sysctl_fd:
                value = int(sysctl_fd.read())
            report = "%s is not %d: %d" % (sysctl, expected, value)
            if msg:
                report += " (%s)" % (msg)
            self.assertEqual(value, expected, report)
        return value

    def set_sysctl_value(self, path, desired):
        sysctl = '/proc/sys/%s' % (path)
        self.assertTrue(os.path.exists(sysctl), "%s does not exist" % (sysctl))
        with open(sysctl, 'w') as sysctl_fh:
            sysctl_fh.write(str(desired))
        self._test_sysctl_value(path, desired)

    def kernel_at_least(self, introduced):
        return self.version_compare(self.kernel_version_ubuntu,
                                    introduced) >= 0

    def kernel_claims_cve_fixed(self, cve):
        changelog = "/usr/share/doc/linux-image-%s/changelog.Debian.gz" % (self.kernel_version)
        if os.path.exists(changelog):
            for line in gzip.open(changelog):
                if cve in line and "revert" not in line and "Revert" not in line:
                    return True
        return False

    def install_builddeps(self, src_pkg):
        rc, report = _run_apt_command([src_pkg], 'build-dep')
        self.assertEqual(0, rc, 'Failed to install build-deps for %s\nOutput:\n%s' % (src_pkg, report))

    def install_package(self, package):
        rc, report = _run_apt_command([package], 'install')
        self.assertEqual(0, rc, 'Failed to install package %s\nOutput:\n%s' % (package, report))

    def install_packages(self, pkg_list):
        rc, report = _run_apt_command(pkg_list, 'install')
        self.assertEqual(0, rc, 'Failed to install packages %s\nOutput:\n%s' % (','.join(pkg_list), report))


class TestGroup:
    '''Create a temporary test group and remove it again in the dtor.'''

    def __init__(self, group=None, lower=False):
        '''Create a new group'''

        self.group = None
        if group:
            if group_exists(group):
                raise ValueError('group name already exists')
        else:
            while(True):
                group = random_string(7, lower=lower)
                if not group_exists(group):
                    break

        assert subprocess.call(['groupadd', group]) == 0
        self.group = group
        g = grp.getgrnam(self.group)
        self.gid = g[2]

    def __del__(self):
        '''Remove the created group.'''

        if self.group:
            rc, report = cmd(['groupdel', self.group])
            assert rc == 0


class TestUser:
    '''Create a temporary test user and remove it again in the dtor.'''

    def __init__(self, login=None, home=True, group=None, uidmin=None, lower=False, shell=None):
        '''Create a new user account with a random password.

        By default, the login name is random, too, but can be explicitly
        specified with 'login'. By default, a home directory is created, this
        can be suppressed with 'home=False'.'''

        self.login = None

        if os.geteuid() != 0:
            raise ValueError("You must be root to run this test")

        if login:
            if login_exists(login):
                raise ValueError('login name already exists')
        else:
            while(True):
                login = 't' + random_string(7, lower=lower)
                if not login_exists(login):
                    break

        self.salt = random_string(2)
        self.password = random_string(8, lower=lower)
        self.crypted = crypt.crypt(self.password, self.salt)

        creation = ['useradd', '-p', self.crypted]
        if home:
            creation += ['-m']
        if group:
            creation += ['-G', group]
        if uidmin:
            creation += ['-K', 'UID_MIN=%d' % uidmin]
        if shell:
            creation += ['-s', shell]
        creation += [login]
        assert subprocess.call(creation) == 0
        # Set GECOS
        assert subprocess.call(['usermod', '-c', 'Buddy %s' % (login), login]) == 0

        self.login = login
        p = pwd.getpwnam(self.login)
        self.uid   = p[2]
        self.gid   = p[3]
        self.gecos = p[4]
        self.home  = p[5]
        self.shell = p[6]

    def __del__(self):
        '''Remove the created user account.'''

        if self.login:
            # sanity check the login name so we don't accidentally wipe too much
            if len(self.login) > 3 and '/' not in self.login:
                subprocess.call(['rm', '-rf', '/home/' + self.login, '/var/mail/' + self.login])
            rc, report = cmd(['userdel', '-f', self.login])
            assert rc == 0

    def add_to_group(self, group):
        '''Add user to the specified group name'''
        rc, report = cmd(['usermod', '-G', group, self.login])
        if rc != 0:
            print(report)
        assert rc == 0


class AddUser:
    '''Create a temporary test user and remove it again in the dtor.'''

    def __init__(self, login=None, home=True, encrypt_home=False, group=None, lower=False, shell=None):
        '''Create a new user account with a random password.

        By default, the login name is random, too, but can be explicitly
        specified with 'login'. By default, a home directory is created, this
        can be suppressed with 'home=False'.

        This class differs from the TestUser class in that the adduser/deluser
        tools are used rather than the useradd/user/del tools. The adduser
        program is the only commandline program that can be used to add a new
        user with an encrypted home directory. It is possible that the AddUser
        class may replace the TestUser class in the future.'''

        self.login = None

        if os.geteuid() != 0:
            raise ValueError("You must be root to run this test")

        if login:
            if login_exists(login):
                raise ValueError('login name already exists')
        else:
            while(True):
                login = 't' + random_string(7, lower=True)
                if not login_exists(login):
                    break

        self.password = random_string(8, lower=lower)

        creation = ['adduser', '--quiet']

        if not home:
            creation += ['--no-create-home']
        elif encrypt_home:
            creation += ['--encrypt-home']

        if shell:
            creation += ['--shell', shell]

        creation += ['--gecos', 'Buddy %s' % (login), login]

        child = pexpect.spawn(creation.pop(0), creation, timeout=5)
        assert child.expect('Enter new UNIX password:') == 0
        child.sendline(self.password)
        assert child.expect('Retype new UNIX password:') == 0
        child.sendline(self.password)

        child.wait()
        child.close()
        assert child.exitstatus == 0
        assert child.signalstatus is None

        self.login = login

        if group:
            assert self.add_to_group(group) == 0

        p = pwd.getpwnam(self.login)
        self.uid   = p[2]
        self.gid   = p[3]
        self.gecos = p[4]
        self.home  = p[5]
        self.shell = p[6]

    def __del__(self):
        '''Remove the created user account.'''

        if self.login:
            # sanity check the login name so we don't accidentally wipe too much
            rc, report = cmd(['deluser', '--remove-home', self.login])
            assert rc == 0

    def add_to_group(self, group):
        '''Add user to the specified group name'''
        rc, report = cmd(['adduser', self.login, group])
        if rc != 0:
            print(report)
        assert rc == 0


# Timeout handler using alarm() from John P. Speno's Pythonic Avocado
class TimeoutFunctionException(Exception):
    """Exception to raise on a timeout"""
    pass


class TimeoutFunction:
    def __init__(self, function, timeout):
        self.timeout = timeout
        self.function = function

    def handle_timeout(self, signum, frame):
        raise TimeoutFunctionException()

    def __call__(self, *args, **kwargs):
        old = signal.signal(signal.SIGALRM, self.handle_timeout)
        signal.alarm(self.timeout)
        try:
            result = self.function(*args, **kwargs)
        finally:
            signal.signal(signal.SIGALRM, old)
        signal.alarm(0)
        return result


def main():
    print("hi")
    unittest.main()
