#!/usr/bin/env python3
"""Run the real driver CLI against a transport that cannot open USB.

Copyright 2026 NUT contributors; GPLv2+.
"""
import os
import pwd
import subprocess
import sys
import tempfile


def main():
    binary = os.path.abspath(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix='nut-vida-cli-') as state:
        env = {key: value for key, value in os.environ.items()
               if not key.startswith('NUT_') and key not in ('NOTIFY_SOCKET', 'WATCHDOG_PID', 'WATCHDOG_USEC')}
        env.update(NUT_CONFPATH=state, NUT_STATEPATH=state, NUT_ALTPIDPATH=state,
                   NUT_PIDPATH=state, NUT_QUIET_INIT_UPSNOTIFY='true')
        base = [binary, '-s', 'no-hardware', '-x', 'port=auto', '-x', 'bus=^001$',
                '-x', 'busport=^010$', '-u', pwd.getpwuid(os.getuid()).pw_name, '-D']
        # Check default and explicit command-sequence entry to the real main().
        for options in [[], ['-x', 'sdcommands=shutdown.default']]:
            process = subprocess.Popen(base + options + ['-k'], env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       universal_newlines=True)
            try:
                out, err = process.communicate(timeout=15)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate()
            if (process.returncode != 1 or 'shutdown not supported' not in err
                    or 'shutdown request was successful' in err):
                raise AssertionError((options, process.returncode, out, err))
    print('VIDA CLI: both unsupported shutdown paths failed correctly; USB inaccessible')


if __name__ == '__main__':
    main()
