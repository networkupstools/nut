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
        (root / 'ups.conf').write_text('[manual_ups]\n driver = dummy-ups\n port = auto\n')
        generated = root / 'generated inventory.conf'
        generated.write_text('[generated_ups]\n driver = dummy-ups\n port = auto\n')
        env = dict(os.environ, NUT_CONFPATH=str(root), NUT_QUIET_INIT_NDE_WARNING='1')
        # Do not inherit a caller's selector; UPSCONF predates NUT_UPSCONF in NDE.
        env.pop('UPSCONF', None)
        for override, expected in [(None, 'manual_ups'), ('', 'manual_ups'),
                                   (str(generated), 'generated_ups'),
                                   (str(root / 'missing'), None), ('x' * 65536, None)]:
            env.pop('NUT_UPSCONF', None)
            if override is not None:
                env['NUT_UPSCONF'] = override
            for command in commands:
                result = subprocess.run(command, env=env, text=True, capture_output=True,
                                        timeout=15)
                if expected is None:
                    assert result.returncode != 0, result
                    assert 'manual_ups' not in result.stdout, result
                else:
                    assert result.returncode == 0, result
                    names = [line for line in result.stdout.splitlines()
                             if line in ('manual_ups', 'generated_ups')]
                    assert names == [expected], result
    print('Config override: C reader and enumerator passed all 10 cases')


if __name__ == '__main__':
    main()
