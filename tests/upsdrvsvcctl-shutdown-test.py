# Test the configured wrapper with its absolute service commands bound to recorders.
import os
import shutil
import subprocess
import sys
import tempfile

if sys.version_info < (3, 5) or not shutil.which('bwrap'):
    print('SKIP upsdrvsvcctl-shutdown-test: requires Python 3.5+ and bubblewrap')
    sys.exit(0)

from pathlib import Path

bwrap = shutil.which('bwrap')


def script(path, body):
    path.write_text('#!/bin/sh\n' + body)
    path.chmod(0o755)


def matches_phases(lines, phases):
    for expected in phases:
        count = len(expected)
        if sorted(lines[:count]) != sorted(expected):
            return False
        lines = lines[count:]
    return not lines


with tempfile.TemporaryDirectory(prefix='nut-upsdrvsvcctl-') as tmp:
    fixture = Path(tmp)
    script(fixture / 'record', '''line=$0
for arg do
    line="$line\t$arg"
done
printf '%s\\n' "$line" >> /fixture/calls
case "$0" in /upsdrvctl) exit "$DRIVER_STATUS" ;; esac
''')
    script(fixture / 'enumerate', '''case "$*" in
    --list-services) printf '%s\\n' "$ALPHA" "$BETA" ;;
    '--get-service-for-device beta') echo "$BETA" ;;
    *) exit 1 ;;
esac
''')
    # Start from an empty root. Only the selected tools, libraries and
    # recorders are visible. No host service sockets or UPS devices are bound.
    sandbox = [bwrap, '--die-with-parent', '--unshare-all', '--new-session']
    for path in ('/lib', '/lib64', '/usr/lib', '/usr/lib64', '/etc/ld.so.cache',
                 '/bin/sh', '/bin/sleep'):
        if os.path.exists(path):
            sandbox.extend(['--ro-bind', path, path])
    if subprocess.call(sandbox + ['/bin/sh', '-c', ':'], env={}) != 0:
        print('SKIP upsdrvsvcctl-shutdown-test: cannot start the isolated shell')
        sys.exit(0)
    for path in ('/bin/systemctl', '/usr/sbin/svcadm', '/usr/sbin/svccfg', '/upsdrvctl'):
        sandbox.extend(['--ro-bind', str(fixture / 'record'), path])
    sandbox.extend(['--dev-bind', '/dev/null', '/dev/null', '--bind', tmp, '/fixture',
                    '--ro-bind', os.path.abspath(sys.argv[1]), '/upsdrvsvcctl'])

    failed = False
    for framework, unit, manager, stop, start in (
            ('systemd', 'nut-driver@{}.service', '/bin/systemctl', 'stop', 'start'),
            ('smf', 'svc:/system/power/nut-driver:{}', '/usr/sbin/svcadm',
             'disable -ts', 'enable -ts')):
        for names in ([], ['beta']):
            units = [unit.format(name) for name in (names or ['alpha', 'beta'])]
            # Each service phase runs in parallel. Order matters between phases.
            phases = []
            for action in (stop, 'shutdown', start):
                if action == 'shutdown':
                    phases.append([' '.join(['/upsdrvctl', action] + names)])
                else:
                    phases.append([manager + ' ' + action + ' ' + u for u in units])
                    if framework == 'smf':
                        phases.append([manager + ' clear ' + u for u in units])

            for options, driver_status in (([], 0), (['-t'], 0), ([], 23)):
                args = options + ['shutdown'] + names
                calls = fixture / 'calls'
                calls.write_text('')
                result = subprocess.run(sandbox + ['/upsdrvsvcctl'] + args,
                                        env={'SERVICE_FRAMEWORK': framework,
                                             'ALPHA': unit.format('alpha'),
                                             'BETA': unit.format('beta'),
                                             'UPSDRVCTL': '/upsdrvctl', 'PATH': '/bin',
                                             'ENUMERATOR': '/fixture/enumerate',
                                             'DRIVER_STATUS': str(driver_status)},
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        universal_newlines=True, timeout=30)
                recorded = [line.split('\t') for line in calls.read_text().splitlines()]
                displayed = result.stdout.splitlines()
                print(framework, args, 'exit', result.returncode,
                      'recorded', recorded, 'displayed', displayed)
                actual = [line.split(' ') for line in displayed] if options else recorded
                expected = [[line.split(' ') for line in phase] for phase in phases]
                if (result.returncode != driver_status or (options and recorded)
                        or 'not found' in result.stderr or 'No such file' in result.stderr
                        or not matches_phases(actual, expected)):
                    print('expected phases:', phases, file=sys.stderr)
                    print(result.stderr, file=sys.stderr)
                    failed = True

    sys.exit(1 if failed else 0)
