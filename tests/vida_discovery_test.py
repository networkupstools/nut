#!/usr/bin/env python3
"""Exercise the production VIDA reconciler against changing kernel inventories.

No fake USB transport is used here: temporary sysfs-style objects, lifetimes,
HID bindings and real atomic files exercise discovery through its actual API.
Copyright (C) 2026 NUT contributors. GPL version 2 or later.
"""
import hashlib
import contextlib
import fcntl
import io
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

SOURCE = Path(os.environ.get('NUT_TOP_SRCDIR', Path(__file__).resolve().parents[1]))
MODULE_PATH = SOURCE / 'scripts/upsdrvsvcctl/nut-vida-discover.py.in'
LOADER = importlib.machinery.SourceFileLoader('vida_discovery', str(MODULE_PATH))
SPEC = importlib.util.spec_from_loader(LOADER.name, LOADER)
discovery = importlib.util.module_from_spec(SPEC)
LOADER.exec_module(discovery)
# Replay the exact descriptor from the native driver's admission contract.
HEADER = (SOURCE / 'drivers/vida-report-desc.h').read_text()
REPORT = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', HEADER))


class DiscoveryTests(unittest.TestCase):
    """Prove inventory changes produce a complete, idempotent NUT generation."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.usb = self.root / 'usb'
        self.hid = self.root / 'hid'
        self.usb.mkdir()
        self.hid.mkdir()
        self.state = self.root / 'state.json'
        self.config = self.root / 'administrator.conf'
        self.config.write_text('')
        self.output = self.root / 'ups.conf'
        self.sequence = 0

    def tearDown(self):
        self.temporary.cleanup()

    def add(self, port, report=REPORT):
        """Create a fresh USB object with a kernel-owned report descriptor."""
        self.sequence += 1
        device = self.usb / port
        device.mkdir()
        fields = {'idVendor': '0001', 'idProduct': '0000', 'authorized': '1',
                  'busnum': port.split('-')[0], 'devnum': str(self.sequence)}
        for name, value in fields.items():
            (device / name).write_text(value)
        child = device / (port + ':1.0') / ('0003:0001:0000.' + str(self.sequence))
        child.mkdir(parents=True)
        (child / 'report_descriptor').write_bytes(report)
        (self.hid / child.name).symlink_to(child, target_is_directory=True)
        return device

    def unbind_hid(self, port):
        """Model the native driver's interface claim, leaving its USB parent."""
        for link in self.hid.iterdir():
            if (self.usb / port).resolve() in link.resolve().parents:
                child = link.resolve()
                link.unlink()
                shutil.rmtree(child)

    def remove(self, port):
        self.unbind_hid(port)
        shutil.rmtree(self.usb / port)

    def run_scan(self, boot='boot-1'):
        return discovery.reconcile(self.usb, self.hid, boot, self.state, self.config, self.output)

    def test_zero_two_three_zero_and_reconnect_elsewhere(self):
        self.assertEqual(self.run_scan()['devices'], [])
        self.add('1-10')
        self.add('1-2')
        self.assertEqual(len(self.run_scan()['devices']), 2)
        self.add('2-4.3')
        result = self.run_scan()
        self.assertEqual(len(result['devices']), 3)
        self.assertIn('busport = "^004[.]003$"', self.output.read_text())
        for port in ['1-10', '1-2', '2-4.3']:
            self.remove(port)
        self.assertEqual(self.run_scan()['devices'], [])
        self.assertNotIn('driver =', self.output.read_text())
        self.add('3-9')
        self.add('2-6.1')
        result = self.run_scan()
        self.assertEqual({device['usb_path'] for device in result['devices']}, {'3-9', '2-6.1'})
        self.assertNotIn('armour_usb_001_010', self.output.read_text())

    def test_partial_removal_preserves_other_devices(self):
        self.add('1-10')
        self.add('1-2')
        self.run_scan()
        self.remove('1-10')
        result = self.run_scan()
        self.assertEqual([device['usb_path'] for device in result['devices']], ['1-2'])

    def test_same_connection_admission_survives_native_claim(self):
        self.add('1-10')
        first = self.run_scan()
        self.unbind_hid('1-10')
        second = self.run_scan()
        self.assertEqual(first['devices'], second['devices'])
        self.assertEqual(second['pending'], [])
        self.assertFalse(second['configuration_changed'])

    def test_old_address_or_boot_never_admits_an_unknown_replacement(self):
        device = self.add('1-10')
        self.run_scan()
        self.unbind_hid('1-10')
        (device / 'devnum').write_text('99')
        result = self.run_scan()
        self.assertEqual(result['devices'], [])
        self.assertEqual(result['pending'], ['1-10'])
        self.remove('1-10')
        self.add('1-10')
        self.run_scan()
        self.unbind_hid('1-10')
        self.assertEqual(self.run_scan(boot='boot-2')['devices'], [])

    def test_same_port_replacement_requires_fresh_descriptor(self):
        self.add('1-10')
        self.run_scan()
        self.remove('1-10')
        self.add('1-10', report=b'unrelated shared-ID device')
        self.assertEqual(self.run_scan()['devices'], [])

    def test_deauthorized_device_is_removed_then_readmitted(self):
        device = self.add('1-10')
        self.run_scan()
        (device / 'authorized').write_text('0')
        self.assertEqual(self.run_scan()['devices'], [])
        (device / 'authorized').write_text('1')
        self.assertEqual(len(self.run_scan()['devices']), 1)

    def test_report_not_ready_is_detected_on_a_later_scan(self):
        self.add('1-10', report=b'not-ready')
        self.assertEqual(self.run_scan()['devices'], [])
        report = next(self.hid.iterdir()) / 'report_descriptor'
        report.write_bytes(REPORT)
        self.assertEqual(len(self.run_scan()['devices']), 1)

    def test_shared_id_without_known_report_is_not_claimed(self):
        self.add('1-2', report=b'other model')
        result = self.run_scan()
        self.assertEqual(result['devices'], [])
        self.assertEqual(result['pending'], ['1-2'])

    def test_repeated_scan_does_not_rewrite_configuration(self):
        self.add('1-10')
        self.assertTrue(self.run_scan()['configuration_changed'])
        before = self.output.stat()
        for unused in range(20):
            del unused
            self.assertFalse(self.run_scan()['configuration_changed'])
        self.assertEqual(self.output.stat().st_ino, before.st_ino)
        self.assertEqual(self.output.stat().st_mtime_ns, before.st_mtime_ns)

    def test_manual_sections_and_file_mode_are_preserved(self):
        original = 'pollinterval = 3\n[manual]\n    driver = dummy-ups\n    port = other\n'
        self.config.write_text(original)
        self.config.chmod(0o640)
        self.add('1-10')
        self.run_scan()
        self.assertTrue(self.output.read_text().startswith(original))
        self.assertEqual(self.config.stat().st_mode & 0o777, 0o640)
        self.remove('1-10')
        self.run_scan()
        self.assertIn(original, self.output.read_text())

    def test_configuration_symlink_is_preserved(self):
        target = self.root / 'actual.conf'
        target.write_text('# administrator file\n')
        self.config.unlink()
        self.config.symlink_to(target)
        self.add('1-10')
        self.run_scan()
        self.assertTrue(self.config.is_symlink())
        self.assertEqual(target.read_text(), '# administrator file\n')
        self.assertIn('armour_usb_001_010', self.output.read_text())

    def test_malformed_block_and_manual_collision_do_not_write(self):
        self.add('1-10')
        for text in [discovery.BEGIN, discovery.END, discovery.BEGIN + discovery.BEGIN + discovery.END + discovery.END,
                     '[armour_usb_001_010]\n    driver = dummy-ups\n']:
            self.config.write_text(text)
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.run_scan()
            self.assertEqual(self.config.read_text(), text)
            self.assertFalse(self.state.exists())

    def test_inventory_error_does_not_remove_known_devices(self):
        self.add('1-10')
        self.run_scan()
        before = self.output.read_bytes()
        with patch.object(discovery, 'connection_identity', side_effect=PermissionError('denied')):
            with self.assertRaises(PermissionError):
                self.run_scan()
        self.assertEqual(self.output.read_bytes(), before)

    def test_object_disappearing_during_scan_is_handled(self):
        self.add('1-10')
        with patch.object(discovery, 'connection_identity', side_effect=FileNotFoundError()):
            self.assertEqual(self.run_scan()['devices'], [])

    def test_corrupt_state_fails_without_touching_configuration(self):
        self.config.write_text('# unchanged\n')
        self.state.write_text('broken JSON')
        with self.assertRaises(ValueError):
            self.run_scan()
        self.assertEqual(self.config.read_text(), '# unchanged\n')

    def test_concurrent_administrator_edit_is_never_overwritten(self):
        self.add('1-10')
        self.run_scan()
        writer = discovery.atomic_write
        edit = '\n[administrator_added]\n    driver = dummy-ups\n    port = other\n'

        def edit_during_publication(path, *args, **kwargs):
            if path == self.state:
                self.config.write_text(edit)
            return writer(path, *args, **kwargs)

        with patch.object(discovery, 'atomic_write', side_effect=edit_during_publication):
            self.run_scan()
        self.assertEqual(self.config.read_text(), edit)
        self.run_scan()
        self.assertIn('[administrator_added]', self.output.read_text())

    def test_input_cannot_be_the_generated_output_or_its_symlink(self):
        self.output.symlink_to(self.config)
        with self.assertRaises(ValueError):
            self.run_scan()
        self.assertEqual(self.config.read_text(), '')
        self.assertFalse(self.state.exists())

    def test_empty_inventory_without_loaded_hid_module(self):
        self.hid.rmdir()
        self.assertEqual(self.run_scan()['devices'], [])

    def test_missing_administrator_input_preserves_generation(self):
        self.add('1-10')
        self.run_scan()
        before = self.output.read_bytes()
        self.config.unlink()
        with self.assertRaises(FileNotFoundError):
            self.run_scan()
        self.assertEqual(self.output.read_bytes(), before)

    def test_native_and_discovery_identity_contracts_match(self):
        self.assertEqual(len(REPORT), 624)
        self.assertEqual(hashlib.sha256(REPORT).hexdigest(), discovery.REPORT_SHA256)

    def run_cli(self):
        """Exercise argument parsing, owner checks and the real lock acquisition."""
        boot = self.root / 'boot-id'
        boot.write_text('boot-1\n')
        args = ['--config', str(self.config), '--output', str(self.output),
                '--state', str(self.state), '--usb-root', str(self.usb),
                '--hid-root', str(self.hid), '--boot-id', str(boot)]
        with patch.object(discovery.grp, 'getgrnam', return_value=SimpleNamespace(gr_gid=os.getgid())), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return discovery.main(args)

    def test_cli_permissions_and_lock_contention(self):
        self.add('1-10')
        with self.state.with_suffix('.lock').open('w') as holder:
            fcntl.flock(holder, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.assertEqual(self.run_cli(), 0)
            self.assertFalse(self.output.exists())
            self.assertFalse(self.state.exists())
        self.assertEqual(self.run_cli(), 0)
        self.assertEqual(self.output.stat().st_mode & 0o777, 0o640)
        self.assertEqual(self.output.stat().st_gid, os.getgid())
        self.assertEqual(self.state.stat().st_mode & 0o777, 0o600)

    def test_cli_rejects_writable_state_directory_and_state_symlink(self):
        self.root.chmod(0o770)
        self.assertEqual(self.run_cli(), 1)
        self.assertFalse(self.output.exists())
        self.root.chmod(0o700)
        self.state.symlink_to(self.config)
        self.assertEqual(self.run_cli(), 1)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.config.read_text(), '')

    def test_failed_publication_preserves_generation_and_can_retry(self):
        self.add('1-10')
        self.run_scan()
        old = self.output.read_bytes()
        self.add('1-2')
        replace = discovery.os.replace

        def fail_output(source, destination):
            if Path(destination) == self.output:
                raise OSError('simulated publication failure')
            return replace(source, destination)

        with patch.object(discovery.os, 'replace', side_effect=fail_output):
            self.assertEqual(self.run_cli(), 1)
        self.assertEqual(self.output.read_bytes(), old)
        self.assertEqual(list(self.root.glob('.ups.conf.*')), [])
        self.assertEqual(self.run_cli(), 0)
        self.assertIn('armour_usb_001_002', self.output.read_text())

    def test_failed_state_sync_never_publishes_driver_configuration(self):
        self.add('1-10')
        with patch.object(discovery.os, 'fsync', side_effect=OSError('simulated sync failure')):
            self.assertEqual(self.run_cli(), 1)
        self.assertFalse(self.output.exists())
        self.assertFalse(self.state.exists())
        self.assertEqual(list(self.root.glob('.state.json.*')), [])
        self.assertEqual(self.run_cli(), 0)


if __name__ == '__main__':
    unittest.main()
