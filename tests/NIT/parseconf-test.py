#!/usr/bin/env python
# Exact values through configuration, dummy-ups, upsd and upsc.
# nit.sh selects $PYTHON; keep this script compatible with Python 2.6+ and 3.
# Copyright (C) 2026 Network UPS Tools project
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import print_function

import json
import math
import os
import subprocess
import sys
import threading


# Input is NUT syntax; expected values are literal decoded strings.
VALUES = [
    ('Outlet #1', 'Outlet'),
    ('"Outlet \\#2"', 'Outlet #2'),
    ('"Outlet #3"', 'Outlet #3'),
    ('Outlet \\#4', 'Outlet #4'),
    ('"\\"edge\\" C:\\\\ups #5"', '"edge" C:\\ups #5'),
    ('one\\ two \\n \\t \\x41', 'one two n t x41'),
    ("'single quotes'", "'single quotes'"),
    ('"a=b:c  two  spaces"', 'a=b:c  two  spaces'),
    ('prefix\\\nsuffix', 'prefixsuffix'),
    ('""', None),  # dummy-ups deletes variables assigned an empty value.
    ('"a\\\\#b"', 'a\\#b'),
    ('"a\\\\\\"b"', 'a\\"b'),
    ('Outlet\\ \\#4', 'Outlet #4'),
]
DESCRIPTION_INPUT = '"Parser #1: \\"quoted\\" C:\\\\ups\'s"'
DESCRIPTION = 'Parser #1: "quoted" C:\\ups\'s'


def prepare(confpath):
    path = os.path.join(confpath, 'ups.conf')
    with open(path, 'r') as source:
        config = source.read()
    original = 'desc = "Crash Dummy"'
    if config.count(original) != 1:
        raise AssertionError('expected one fresh NIT dummy configuration')
    with open(path, 'w') as target:
        target.write(config.replace(original, 'desc = ' + DESCRIPTION_INPUT))
    with open(os.path.join(confpath, 'dummy.seq'), 'w') as target:
        target.write('ups.status: OL\n')
        for index, (value, expected) in enumerate(VALUES):
            if expected is None:
                target.write('outlet.%d.desc: before empty\n' % (index + 1))
            target.write('outlet.%d.desc: %s\n' % (index + 1, value))
        target.write('TIMER 60\n')


def query(upsc, *args):
    timeout = float(os.environ.get('NIT_PARSECONF_TIMEOUT', '60'))
    if timeout <= 0 or math.isnan(timeout) or math.isinf(timeout):
        raise ValueError('NIT_PARSECONF_TIMEOUT must be positive finite seconds')
    command = [upsc] + list(args)
    print('START: %r (watchdog %g seconds)' % (command, timeout))
    sys.stdout.flush()
    process = subprocess.Popen(command, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    expired = threading.Event()

    def expire():
        if process.poll() is None:
            try:
                process.kill()
            except OSError:
                # The child may have exited between poll() and kill().
                if process.poll() is None:
                    raise
            else:
                expired.set()

    # Python 2.6 also runs NIT; communicate(timeout=...) is newer.
    timer = threading.Timer(timeout, expire)
    timer.start()
    try:
        output, error = process.communicate()
    finally:
        timer.cancel()
        timer.join()
    if expired.is_set():
        raise AssertionError('%r timed out after %g seconds; watchdog killed '
                             'the process: stdout=%r stderr=%r' %
                             (command, timeout, output, error))
    if process.returncode:
        raise AssertionError('%r returned %d: stdout=%r stderr=%r' %
                             (command, process.returncode, output, error))
    # Fixtures are ASCII. Normalize only the platform's output line ending.
    return output.decode('ascii').replace('\r\n', '\n')


def equal(actual, expected):
    if actual != expected:
        raise AssertionError('got %r, expected %r' % (actual, expected))


def self_test():
    original = os.environ.get('NIT_PARSECONF_TIMEOUT')
    child = ('import sys, time; sys.stdout.write("output\\n"); '
             'sys.stdout.flush(); sys.stderr.write("diagnostic\\n"); '
             'sys.stderr.flush(); ')
    try:
        os.environ['NIT_PARSECONF_TIMEOUT'] = '5'
        equal(query(sys.executable, '-c', child + 'time.sleep(2)'), 'output\n')
        for action, timeout, message in [('sys.exit(7)', '5', 'returned 7'),
                                        ('time.sleep(2)', '1', 'timed out after 1 seconds')]:
            os.environ['NIT_PARSECONF_TIMEOUT'] = timeout
            try:
                query(sys.executable, '-c', child + action)
            except AssertionError as error:
                text = str(error)
                if (message not in text
                        or 'stdout=%r' % b'output\n' not in text
                        or 'stderr=%r' % b'diagnostic\n' not in text):
                    raise
            else:
                raise AssertionError('expected query failure: ' + message)
        for timeout in ['0', '-1', 'nan', 'inf']:
            os.environ['NIT_PARSECONF_TIMEOUT'] = timeout
            try:
                query(sys.executable, '-c', 'pass')
            except ValueError:
                pass
            else:
                raise AssertionError('accepted invalid watchdog: ' + timeout)
    finally:
        if original is None:
            del os.environ['NIT_PARSECONF_TIMEOUT']
        else:
            os.environ['NIT_PARSECONF_TIMEOUT'] = original
    print('PASS: query output, process failure, watchdog and timeout validation')


def check(upsc, port):
    host = '127.0.0.1:' + port
    ups = 'dummy@' + host
    plain = query(upsc, ups).splitlines()
    data = json.loads(query(upsc, '-j', ups))
    for index, (value, expected) in enumerate(VALUES):
        key = 'outlet.%d.desc' % (index + 1)
        if expected is None:
            equal([line for line in plain if line.startswith(key + ':')], [])
            equal(key in data, False)
            print('PASS: empty value removes %s from text and JSON lists' % key)
            continue
        equal([line for line in plain if line.startswith(key + ':')],
              [key + ': ' + expected])
        equal(data[key], expected)
        equal(query(upsc, ups, key), expected + '\n')
        equal(json.loads(query(upsc, '-j', ups, key)), expected)
        print('PASS: %s = %r (text and JSON, list and single value)' %
              (key, expected))

    names = query(upsc, '-l', host).splitlines()
    equal(json.loads(query(upsc, '-j', '-l', host)), names)
    descriptions = json.loads(query(upsc, '-j', '-L', host))
    equal(sorted(descriptions), sorted(names))
    equal(descriptions['dummy'], DESCRIPTION)
    equal([line for line in query(upsc, '-L', host).splitlines()
           if line.startswith('dummy:')], ['dummy: ' + DESCRIPTION])
    # These read-only clients do not LOGIN to the UPS.
    equal(query(upsc, '-c', ups).splitlines(), [])
    equal(json.loads(query(upsc, '-j', '-c', ups)), [])
    print('PASS: configured description, UPS lists and empty client lists')


if __name__ == '__main__':
    if sys.argv[1] == 'self-test':
        self_test()
    elif sys.argv[1] == 'prepare':
        prepare(sys.argv[2])
    else:
        check(sys.argv[1], sys.argv[2])
