#!/usr/bin/env python3
"""Exercise the real C config reader and enumerator without starting services.

Copyright 2026 NUT contributors; GPLv2+.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    """Both consumers must select the same inventory and reject missing input."""
    build = Path(sys.argv[1])
    commands = [[str(build / 'drivers/upsdrvctl'), '-l'],
                ['sh', str(build / 'scripts/upsdrvsvcctl/nut-driver-enumerator.sh'),
                 '--list-devices']]
    with tempfile.TemporaryDirectory(prefix='nut-upsconf-') as temporary:
        root = Path(temporary)
        with (root / 'ups.conf').open('w') as stream:
            stream.write('[manual_ups]\n driver = dummy-ups\n port = auto\n')
        generated = root / 'generated inventory.conf'
        with generated.open('w') as stream:
            stream.write('[generated_ups]\n driver = dummy-ups\n port = auto\n')
        env = {key: value for key, value in os.environ.items() if not key.startswith('NUT_')}
        env.update(NUT_CONFPATH=str(root), NUT_QUIET_INIT_NDE_WARNING='1',
                   SERVICE_FRAMEWORK='systemd', SYSTEMD_CONFPATH=str(root))
        # Do not inherit a caller's selector; UPSCONF predates NUT_UPSCONF in NDE.
        env.pop('UPSCONF', None)
        for override, expected in [(None, 'manual_ups'), ('', 'manual_ups'),
                                   (str(generated), 'generated_ups'),
                                   (str(root / 'missing'), None)]:
            env.pop('NUT_UPSCONF', None)
            if override is not None:
                env['NUT_UPSCONF'] = override
            for command in commands:
                process = subprocess.Popen(command, env=env, universal_newlines=True,
                                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    out, err = process.communicate(timeout=15)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.communicate()
                result = (command, process.returncode, out, err)
                if expected is None:
                    if process.returncode == 0 or 'manual_ups' in out:
                        raise AssertionError(result)
                else:
                    names = [line for line in out.splitlines()
                             if line in ('manual_ups', 'generated_ups')]
                    if process.returncode or names != [expected]:
                        raise AssertionError(result)
    print('Config override: C reader and enumerator passed all 8 cases')


if __name__ == '__main__':
    main()
