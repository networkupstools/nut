#!/usr/bin/python
#
#    test-nut.py quality assurance test script
#    Copyright (C) 2008-2011 Arnaud Quette <aquette@debian.org>
#    Copyright (C) 2012-2013 Jamie Strandboge <jamie@canonical.com>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License version 3,
#    as published by the Free Software Foundation.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

'''
  *** IMPORTANT ***
  DO NOT RUN ON A PRODUCTION SERVER.
  *** IMPORTANT ***

  How to run (up to natty):
    $ sudo apt-get -y install nut
    $ sudo ./test-nut.py -v

  How to run (oneiric+):
    $ sudo apt-get -y install nut-server nut-client
    $ sudo ./test-nut.py -v

  NOTE:
    - NUT architecture (helps understanding):
      http://www.networkupstools.org/docs/developer-guide.chunked/ar01s02.html#_the_layering

    - These tests only validate the NUT software framework itself (communication
      between the drivers, server and client layers ; events propagation and
      detection). The critical part of NUT, Ie the driver layer which
      communicate with actual devices, can only be tested with real hardware!

    - These tests use the NUT simulation driver (dummy-ups) to emulate real
      hardware behavior, and generate events (power failure, low battery, ...).

    - NUT QA INFORMATION:
      - NUT provides "make check" and "make distcheck" in its source distribution
      - NUT provides Quality Assurance information, to track all efforts:
        http://www.networkupstools.org/nut-qa.html

  TODO:
    - in-tree testing mode, to test NUT from source compilation without install
      (currently using the installed packages)
    - improve test duration, by reworking NutTestCommon._setUp() and the way
      daemons are started (ie, always)
    - more events testing (upsmon / upssched)
    - test syslog and wall output
    - test UPS redundancy
    - test Powerchain (once available!)
    - test AppArmor (once available!)
    - add hardware testing as Private tests?
    - load a .dev file, and test a full output
'''

# QRT-Packages: netcat-openbsd
# QRT-Alternates: nut-server nut
# QRT-Alternates: nut-client nut
# nut-dev is needed for the dummy driver on hardy
# QRT-Alternates: nut-dev
# QRT-Privilege: root
# QRT-Depends:


import unittest, sys, os, time
import tempfile
import testlib

use_private = True
try:
    from private.qrt.nut import PrivateNutTest
except ImportError:
    class PrivateNutTest(object):
        '''Empty class'''
    print >>sys.stdout, "Skipping private tests"


class NutTestCommon(testlib.TestlibCase):
    '''Common functions'''

    initscript    = ["/etc/init.d/nut"]
    hosts_file    = "/etc/hosts"
    powerdownflag = "/etc/killpower"
    shutdowncmd   = "/tmp/shutdowncmd"
    powerdownflag = "/etc/killpower"
    notifyscript  = "/tmp/nutifyme"
    notifylog     = "/tmp/notify.log"
    systemd_present = True
    nut_driver_target_present = False

    def _is_systemd_init():
        ''' Read the "/proc/1/exe" symlink to know which init is running'''
        init_bin = "/proc/1/exe"
        if os.readlink( init_bin ) == "/usr/lib/systemd":
          return True
        else:
          return False

    def _setUp(self):
        '''Set up prior to each test_* function'''
        '''We generate a NUT config using the dummmy-ups driver
           and standard settings for local monitoring
        '''
        ##if self.lsb_release['Release'] >= 13.04:
        ##self.initscript = ["/etc/init.d/nut-server", "/etc/init.d/nut-client"]
        # if so, instantiate driver unit ("systemctl enable nut-driver@dummy-dev1")
        # Check if it's a systemd system, otherwise it's init sysV
        systemd_present = self._is_systemd_init()

        # Check if nut-driver.target exists
        rc, report = testlib.cmd(['systemctl', 'list-unit-files', 'nut-driver.target'])
        if report:
            nut_driver_target_present = True
            self.initscript = ["nut-driver.target", "nut-server", "nut-monitor"]
        else:
            self.initscript = ["nut-driver", "nut-server", "nut-monitor"]
        self.tmpdir = ""
        self.rundir = "/var/run/nut"
        testlib.cmd(['/bin/rm -f' + self.powerdownflag])

        testlib.config_replace('/etc/nut/ups.conf', '''
[dummy-dev1]
	driver = dummy-ups
	port = dummy.dev
	desc = "simulation device"
		''')

        if self.nut_driver_target_present:
            testlib.cmd(['systemctl', 'enable', 'nut-driver@dummy-dev1'])

        if self.lsb_release['Release'] <= 8.04:
            testlib.config_replace('/etc/nut/upsd.conf', '''
ACL dummy-net 127.0.0.1/8
ACL dummy-net2 ::1/64
ACL all 0.0.0.0/0
ACCEPT dummy-net dummy-net2
REJECT all
            ''')
        else:
            testlib.config_replace('/etc/nut/upsd.conf', '''# just to touch the file''')

        extra_cfgs = ''
        if self.lsb_release['Release'] <= 8.04:
            extra_cfgs = '''        allowfrom = dummy-net dummy-net2
'''
        testlib.config_replace('/etc/nut/upsd.users', '''
[admin]
        password = dummypass
        actions = SET
        instcmds = ALL
%s
[monuser]
        password  = dummypass
        upsmon master
%s        ''' %(extra_cfgs, extra_cfgs))

        testlib.config_replace('/etc/nut/upsmon.conf', '''
MONITOR dummy-dev1@localhost 1 monuser dummy-pass master
MINSUPPLIES 1
SHUTDOWNCMD "/usr/bin/touch ''' + self.shutdowncmd + '"\n'
'''POWERDOWNFLAG ''' + self.powerdownflag + '\n'
'''
NOTIFYCMD ''' + self.notifyscript + '\n'
'''
NOTIFYFLAG ONLINE     SYSLOG+EXEC
NOTIFYFLAG ONBATT     SYSLOG+EXEC
NOTIFYFLAG LOWBATT    SYSLOG+EXEC
NOTIFYFLAG FSD        SYSLOG+EXEC
# NOTIFYFLAG COMMOK     SYSLOG+EXEC
# NOTIFYFLAG COMMBAD    SYSLOG+EXEC
NOTIFYFLAG SHUTDOWN   SYSLOG+EXEC
# NOTIFYFLAG REPLBATT   SYSLOG+EXEC
# NOTIFYFLAG NOCOMM     SYSLOG+EXEC
# NOTIFYFLAG NOPARENT   SYSLOG+EXEC

# Shorten test duration by:
# Speeding up polling frequency
POLLFREQ 2
# And final wait delay
FINALDELAY 0
'''
)

        testlib.create_fill(self.notifyscript, '''
#! /bin/bash
echo "$*" > ''' + self.notifylog + '\n', mode=0755)

        # dummy-ups absolutely needs a data file, even if empty
        testlib.config_replace('/etc/nut/dummy.dev', '''
ups.mfr: Dummy Manufacturer
ups.model: Dummy UPS
ups.status: OL
# Set a big enough timer to avoid value reset, due to reading loop
TIMER 600
		''')
 
        testlib.config_replace('/etc/nut/nut.conf', '''MODE=standalone''')

        # Add known friendly IP names for localhost v4 and v6
        # FIXME: find a way to determine if v4 / v6 are enabled, and a way to
        # get v4 / v6 names
        testlib.config_replace(self.hosts_file, '''#
127.0.0.1	localv4
::1	localv6
''', append=True)

        if self.lsb_release['Release'] <= 8.04:
            testlib.config_replace('/etc/default/nut', '''#
START_UPSD=yes
UPSD_OPTIONS=""
START_UPSMON=yes
UPSMON_OPTIONS=""
''', append=False)

        # Start the framework
        self._restart()

    def _tearDown(self):
        '''Clean up after each test_* function'''
        self._stop()
        time.sleep(2)
        if self.nut_driver_target_present:
            testlib.cmd(['systemctl', 'disable', 'nut-driver@dummy-dev1'])
        os.unlink('/etc/nut/ups.conf')
        os.unlink('/etc/nut/upsd.conf')
        os.unlink('/etc/nut/upsd.users')
        os.unlink('/etc/nut/upsmon.conf')
        os.unlink('/etc/nut/dummy.dev')
        os.unlink('/etc/nut/nut.conf')
        testlib.config_restore('/etc/nut/ups.conf')
        testlib.config_restore('/etc/nut/upsd.conf')
        testlib.config_restore('/etc/nut/upsd.users')
        testlib.config_restore('/etc/nut/upsmon.conf')
        testlib.config_restore('/etc/nut/dummy.dev')
        testlib.config_restore('/etc/nut/nut.conf')
        if os.path.exists(self.notifyscript):
            os.unlink(self.notifyscript)
        if os.path.exists(self.shutdowncmd):
            os.unlink(self.shutdowncmd)
        testlib.config_restore(self.hosts_file)
        ##if self.lsb_release['Release'] <= 8.04:
        ##    testlib.config_restore('/etc/default/nut')

        if os.path.exists(self.tmpdir):
            testlib.recursive_rm(self.tmpdir)

        # this is needed because of the potentially hung upsd process in the
        # CVE-2012-2944 test
        testlib.cmd(['killall', 'upsd'])
        testlib.cmd(['killall', '-9', 'upsd'])

    def _start(self):
        '''Start NUT'''
        for initscript in self.initscript:
            rc, report = testlib.cmd(['systemctl', 'start', initscript])
            expected = 0
            result = 'Got exit code %d, expected %d\n' % (rc, expected)
            self.assertEquals(expected, rc, result + report)
            time.sleep(2)

    def _stop(self):
        '''Stop NUT'''
        for initscript in self.initscript:
            rc, report = testlib.cmd(['systemctl', 'stop', initscript])
            expected = 0
            result = 'Got exit code %d, expected %d\n' % (rc, expected)
            self.assertEquals(expected, rc, result + report)

    def _reload(self):
        '''Reload NUT'''
        for initscript in self.initscript:
            rc, report = testlib.cmd(['systemctl', 'force-reload', initscript])
            # TODO: replace with 'reload-or-try-restart'
            expected = 0
            result = 'Got exit code %d, expected %d\n' % (rc, expected)
            self.assertEquals(expected, rc, result + report)

    def _restart(self):
        '''Restart NUT'''
        self._stop()
        time.sleep(2)
        self._start()

    def _status(self):
        '''NUT Status'''
        for initscript in self.initscript:
            rc, report = testlib.cmd(['systemctl', 'status', initscript])
            expected = 0
            ##if self.lsb_release['Release'] <= 8.04:
            ##    self._skipped("init script does not support status command")
            ##    expected = 1
            result = 'Got exit code %d, expected %d\n' % (rc, expected)
            self.assertEquals(expected, rc, result + report)

    def _testDaemons(self, daemons):
        '''Daemons running'''
        for d in daemons:
            # A note on the driver pid file: its name is
            # <ups.conf section name>-<driver name>.pid
            # ex: dummy-dev1-dummy-ups.pid
            if d == 'dummy-ups' :
                pidfile = os.path.join(self.rundir, 'dummy-ups-dummy-dev1.pid')
            else :
                pidfile = os.path.join(self.rundir, d + '.pid')
            warning = "Could not find pidfile '" + pidfile + "'"
            self.assertTrue(os.path.exists(pidfile), warning)
            self.assertTrue(testlib.check_pidfile(d, pidfile), d + ' is not running')

    def _nut_setvar(self, var, value):
        '''Test upsrw'''
        rc, report = testlib.cmd(['/bin/upsrw', '-s', var + '=' + value,
            '-u', 'admin' , '-p', 'dummypass', 'dummy-dev1@localhost'])
        self.assertTrue(rc == 0, 'upsrw: ' + report)
        return rc,report


class BasicTest(NutTestCommon, PrivateNutTest):
    '''Test basic NUT functionalities'''

    def setUp(self):
        '''Setup mechanisms'''
        NutTestCommon._setUp(self)

    def tearDown(self):
        '''Shutdown methods'''
        NutTestCommon._tearDown(self)

    def test_daemons_service(self):
        '''Test daemons using "service status"'''
        self._status()

    def test_daemons_pid(self):
        '''Test daemons using PID files'''
        daemons = [ 'dummy-ups', 'upsd', 'upsmon' ]
        self._testDaemons(daemons)

    def test_upsd_IPv4(self):
        '''Test upsd IPv4 reachability'''
        rc, report = testlib.cmd(['/bin/upsc', '-l', 'localv4'])
        self.assertTrue('dummy-dev1' in report, 'dummy-dev1 should be present in device(s) listing: ' + report)

    def test_upsd_IPv6(self):
        '''Test upsd IPv6 reachability'''
        rc, report = testlib.cmd(['/bin/upsc', '-l', 'localv6'])
        self.assertTrue('dummy-dev1' in report, 'dummy-dev1 should be present in device(s) listing: ' + report)

    def test_upsc_device_list(self):
        '''Test NUT client interface (upsc): device(s) listing'''
        rc, report = testlib.cmd(['/bin/upsc', '-L'])
        self.assertTrue('dummy-dev1: simulation device' in report, 'dummy-dev1 should be present in device(s) listing: ' + report)

    def _test_upsc_status(self):
        '''Test NUT client interface (upsc): data access'''
        rc, report = testlib.cmd(['/bin/upsc', 'dummy-dev1', 'ups.status'])
        self.assertTrue('OL' in report, 'UPS Status: ' + report + 'should be OL')

    #def test_upsc_powerchain(self):
    #    '''Test NUT client interface (upsc): Powerchain(s) listing'''
    #    rc, report = testlib.cmd(['/bin/upsc', '-p'])
    # Result == Main ; dummy-dev1 ; $hostname
    #    self.assertTrue('dummy-dev1' in report, 'dummy-dev1 should be present in device(s) listing: ' + report)

    def test_upsrw(self):
        '''Test upsrw'''
        # Set ups.status to OB (On Battery)...
        self._nut_setvar('ups.model', 'Test')
        time.sleep(2)
        # and check the result on the client side
        rc, report = testlib.cmd(['/bin/upsc', 'dummy-dev1@localhost', 'ups.model'])
        self.assertTrue('Test' in report, 'UPS Model: ' + report + 'should be Test')

    # FIXME: need a simulation counterpart, not yet implemented
    #def test_upscmd(self):
    #    '''Test upscmd'''

    def test_upsmon_notif(self):
        '''Test upsmon notifications'''
        # Set ups.status to OB (On Battery)...
        self._nut_setvar('ups.status', 'OB')
        time.sleep(1)
        # and check the result on the client side
        rc, report = testlib.cmd(['/bin/upsc', 'dummy-dev1@localhost', 'ups.status'])
        self.assertTrue('OB' in report, 'UPS Status: ' + report + 'should be OB')

    def test_upsmon_shutdown(self):
        '''Test upsmon basic shutdown (single UPS, low battery status)'''
        self._nut_setvar('ups.status', 'OB LB')
        time.sleep(2)
        # and check the result on the client side
        rc, report = testlib.cmd(['/bin/upsc', 'dummy-dev1@localhost', 'ups.status'])
        self.assertTrue('OB LB' in report, 'UPS Status: ' + report + 'should be OB LB')
        # FIXME: improve with a 2 sec loop * 5 tries
        time.sleep(3)
        # Check for powerdownflag and shutdowncmd (needed for halt!)
        # FIXME: replace by a call to 'upsmon -K'
        self.assertTrue(os.path.exists(self.powerdownflag), 'POWERDOWNFLAG has not been set!')
        self.assertTrue(os.path.exists(self.shutdowncmd), 'SHUTDOWNCMD has not been executed!')

    def test_CVE_2012_2944(self):
        '''Test CVE-2012-2944'''
        self.tmpdir = tempfile.mkdtemp(dir='/tmp', prefix="testlib-")
        # First send bad input. We need to do this in a script because python
        # functions don't like our embedded NULs
        script = os.path.join(self.tmpdir, 'script.sh')
        contents = '''#!/bin/sh
printf '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\n' | nc -q 1 127.0.0.1 3493
sleep 1
dd if=/dev/urandom count=64 | nc -q 1 127.0.0.1 3493
'''
        testlib.create_fill(script, contents, mode=0755)
        rc, report = testlib.cmd([script])

        # It should not have crashed. Let's see if it did
        self._testDaemons(['upsd'])
        self.assertTrue('ERR UNKNOWN-COMMAND' in report, "Could not find 'ERR UNKNOWN-COMMAND' in:\n%s" % report)

        # This CVE may also result in a hung upsd. Try to kill it, if it is
        # still around, it is hung
        testlib.cmd(['killall', 'upsd'])
        pidfile = os.path.join(self.rundir, 'upsd.pid')
        self.assertFalse(os.path.exists(pidfile), "Found %s" % pidfile)
        self.assertFalse(testlib.check_pidfile('upsd', pidfile), 'upsd is hung')
        #subprocess.call(['bash'])

# FIXME
#class AdvancedTest(NutTestCommon, PrivateNutTest):
#    '''Test advanced NUT functionalities'''

if __name__ == '__main__':

    suite = unittest.TestSuite()
    # more configurable
    if (len(sys.argv) == 1 or sys.argv[1] == '-v'):
        suite.addTest(unittest.TestLoader().loadTestsFromTestCase(BasicTest))

    # Pull in private tests
    #if use_private:
    #    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(MyPrivateTest))

    else:
        print '''Usage:
  test-nut.py [-v]             basic tests
'''
        sys.exit(1)
    rc = unittest.TextTestRunner(verbosity=2).run(suite)
    if not rc.wasSuccessful():
        sys.exit(1)
