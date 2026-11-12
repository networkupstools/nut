#!/usr/bin/env python3
"""Load packaged units into systemd's dry-run manager, without starting units.

The real manager resolves templates, drop-ins and startup transactions. This
tests its effective ordering and environment-file configuration, including
negative controls with missing packaged drop-ins. No host service is touched.
Copyright 2026 NUT contributors; GPLv2+.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


BUILD = Path(sys.argv.pop(1)).resolve()
MANAGER = next((path for path in [shutil.which('systemd'), '/usr/lib/systemd/systemd',
                                '/lib/systemd/systemd'] if path and Path(path).is_file()), None)


@unittest.skipUnless(MANAGER and os.geteuid() != 0, 'systemd dry-run needs a local manager and an unprivileged account')
class ServiceTests(unittest.TestCase):
    """Prove the shipped service graph admits discovery before existing drivers."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='nut-vida-units-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        source = BUILD / 'scripts/systemd'
        for item in source.iterdir():
            if item.suffix in ('.target', '.timer', '.service'):
                shutil.copyfile(str(item), str(self.root / item.name))
            elif item.is_dir() and item.name.endswith('.service.d'):
                shutil.copytree(str(item), str(self.root / item.name))
        for name in ['basic', 'sysinit', 'network', 'local-fs', 'shutdown']:
            (self.root / (name + '.target')).write_text('[Unit]\nDefaultDependencies=no\n')

    def enable(self, target, names):
        directory = self.root / (target + '.wants')
        directory.mkdir(exist_ok=True)
        for name in names:
            template = 'nut-driver@.service' if name.startswith('nut-driver@') else name
            (directory / name).symlink_to('../' + template)

    def load(self, drivers, enumerator='nut-driver-enumerator-daemon.service'):
        self.enable('nut.target', ['nut-vida-discovery.service', 'nut-vida-discovery.timer',
                                   enumerator, 'nut-server.service'])
        self.enable('nut-driver.target', drivers)
        env = dict(os.environ, SYSTEMD_UNIT_PATH=str(self.root), SYSTEMD_GENERATOR_PATH='/dev/null',
                   SYSTEMD_LOG_LEVEL='warning', LC_ALL='C')
        result = subprocess.run([MANAGER, '--test', '--system', '--unit=nut.target'],
                                env=env, capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('ordering cycle', result.stderr.lower())
        units = {}
        for match in re.finditer(r'^\s*-> Unit ([^:\n]+):\n(.*?)(?=^\s*-> Unit |^-> By jobs:|\Z)',
                                 result.stdout, re.MULTILINE | re.DOTALL):
            units[match.group(1)] = match.group(2)
        jobs = set(re.findall(r'Action: (\S+) -> start', result.stdout))
        self.assertIn('nut-vida-discovery.service', jobs)
        for name in drivers + [enumerator, 'nut-server.service']:
            self.assertIn(name, jobs)
            self.assertIn(name, units)
        return units

    def check_consumers(self, units, names):
        for name in names:
            self.assertRegex(units[name], r'After: nut-vida-discovery\.service\s')
            self.assertRegex(units[name], r'EnvironmentFile: -[^\n]*/nut-vida-discovery\.conf')

    def test_existing_instances_wait_for_initial_discovery(self):
        names = ['nut-driver@first.service', 'nut-driver@second.service']
        units = self.load(names)
        self.check_consumers(units, names + ['nut-server.service', 'nut-driver-enumerator-daemon.service'])

    def test_empty_start_and_oneshot_enumerator(self):
        units = self.load([], 'nut-driver-enumerator.service')
        self.check_consumers(units, ['nut-server.service', 'nut-driver-enumerator.service'])

    def test_missing_driver_dropin_negative_control(self):
        shutil.rmtree(str(self.root / 'nut-driver@.service.d'))
        units = self.load(['nut-driver@existing.service'])
        with self.assertRaises(AssertionError):
            self.check_consumers(units, ['nut-driver@existing.service'])

    def test_missing_server_environment_negative_control(self):
        shutil.rmtree(str(self.root / 'nut-server.service.d'))
        units = self.load([])
        with self.assertRaises(AssertionError):
            self.check_consumers(units, ['nut-server.service'])


if __name__ == '__main__':
    unittest.main()
