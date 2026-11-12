#!/usr/bin/env python3
"""Exercise actual configure/install selection in isolated build directories.

Run via make check-vida-integration in a Linux build with discovery enabled.
Each cell has a private build directory; its options inherit the caller's
compiler and dependency selection. No system files or services are changed.
Copyright 2026 NUT contributors; GPLv2+.
"""
import concurrent.futures
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile


def main():
    source, build = (Path(value).resolve() for value in sys.argv[1:3])
    make = shlex.split(sys.argv[3])
    configured = subprocess.check_output([str(build / 'config.status'), '--config'], text=True)
    args = [value for value in shlex.split(configured)
            if value not in ('-C', '--config-cache')
            and not value.startswith(('--cache-file=', '--srcdir=',
                                      '--enable-vida-discovery', '--disable-vida-discovery'))]
    env = dict(os.environ)
    # A parent parallel make jobserver cannot be inherited through these pipes.
    env.pop('MAKEFLAGS', None)
    env.pop('MFLAGS', None)
    # Make exports derived ccache settings which were not necessarily present
    # at configure time. Explicit original settings remain in config.status.
    for key in list(env):
        if key.startswith('CCACHE_'):
            del env[key]
    with tempfile.TemporaryDirectory(prefix='nut-vida-configure-') as temporary:
        root = Path(temporary)
        shim = root / 'rejected-python-probe'
        shim.write_text('#!' + sys.executable + '\nimport os,sys\n'
                        'if len(sys.argv) > 2 and "sys.version_info < (3, 8)" in sys.argv[2]:\n'
                        '    sys.exit(1)\n'
                        'os.execv(' + repr(sys.executable) + ', [' + repr(sys.executable) + '] + sys.argv[1:])\n')
        shim.chmod(0o755)
        cases = [
            ('selected', [], True, True),
            ('other-driver', ['--with-drivers=usbhid-ups'], True, False),
            ('disabled', ['--disable-vida-discovery'], True, False),
            ('no-systemd', ['--with-systemdsystemunitdir=no'], True, False),
            ('no-python', ['--without-python', '--without-python3'], True, False),
            ('rejected-version-probe', ['--with-python3=' + str(shim)], True, False),
            ('no-usb', ['--without-usb', '--with-drivers=dummy-ups'], True, False),
            ('require-unavailable', ['--with-drivers=usbhid-ups', '--enable-vida-discovery'], False, False),
        ]
        # This is a configure admission probe, not a cross-platform execution
        # claim: the native compiler and dependencies deliberately remain in use.
        pkgconfig = env.get('PKG_CONFIG') or shutil.which('pkg-config')
        if pkgconfig:
            paths = subprocess.check_output(shlex.split(pkgconfig) + ['--variable=pc_path', 'pkg-config'], text=True).strip()
            cases.append(('non-linux-admission', ['--host=x86_64-unknown-freebsd',
                          'PKG_CONFIG_LIBDIR=' + paths], True, False))

        def run(case):
            name, extra, success, enabled = case
            directory = root / name
            directory.mkdir()
            cache = build / 'config.cache' if name == 'selected' else root / 'selected/config.cache'
            if cache.exists() and name != 'non-linux-admission':
                shutil.copyfile(str(cache), str(directory / 'config.cache'))
            with (directory / 'configure.log').open('w') as log:
                result = subprocess.run([str(source / 'configure'), '-C'] + args + extra,
                                        cwd=str(directory), env=env, stdout=log, stderr=subprocess.STDOUT)
            output = (directory / 'configure.log').read_text()
            if (result.returncode == 0) != success:
                raise AssertionError((name, result.returncode, output[-6000:]))
            if not success:
                if 'VIDA discovery requires Linux, systemd units, nutdrv_vida and Python 3.8 or newer' not in output:
                    raise AssertionError((name, 'failed for the wrong reason', output[-6000:]))
            else:
                for folder, variable, expected in [
                        ('scripts/upsdrvsvcctl', 'libexec_SCRIPTS', ['nut-vida-discover.py']),
                        ('scripts/systemd', 'systemdsystemunit_DATA', ['nut-vida-discovery.service', 'nut-vida-discovery.timer']),
                        ('scripts/systemd', 'nobase_systemdsystemunit_DATA', [
                            unit + '.d/nut-vida-discovery.conf' for unit in ['nut-driver@.service',
                            'nut-server.service', 'nut-driver-enumerator.service', 'nut-driver-enumerator-daemon.service']])]:
                    recipe = 'print-vida-selection:\n\t@echo "$(' + variable + ')"\n'
                    selected = subprocess.check_output(make + ['-s', '-f', 'Makefile', '-f', '-', 'print-vida-selection'],
                                                       input=recipe, text=True, cwd=str(directory / folder), env=env).split()
                    for item in expected:
                        if (item in selected) != enabled:
                            raise AssertionError((name, variable, item, selected))
                if name in ('disabled', 'no-usb'):
                    with (directory / 'generic-config-test.log').open('w') as log:
                        checked = subprocess.run(make + ['-j' + str(os.cpu_count() or 1),
                            'upsconf-override-test', 'upsconf_maxpath_utest'],
                            cwd=str(directory / 'tests'), env=env, stdout=log, stderr=subprocess.STDOUT)
                    if checked.returncode:
                        raise AssertionError((name, 'generic configuration regression failed',
                            (directory / 'generic-config-test.log').read_text()[-6000:]))
                    subprocess.run([str(directory / 'tests/upsconf_maxpath_utest')], env=env, check=True)
            print(json.dumps({'case': name, 'configure_rc': result.returncode, 'discovery_installed': enabled}), flush=True)

        run(cases[0])
        with concurrent.futures.ThreadPoolExecutor(max_workers=len(cases) - 1) as pool:
            list(pool.map(run, cases[1:]))


if __name__ == '__main__':
    main()
