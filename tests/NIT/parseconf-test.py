#!/usr/bin/env python
# Exact values through configuration, dummy-ups, upsd and upsc.
# Copyright (C) 2026 Network UPS Tools project
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import print_function

import json
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
    process = subprocess.Popen([upsc] + list(args), stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    # Python 2.6 also runs NIT; communicate(timeout=...) is newer.
    timer = threading.Timer(10, process.kill)
    timer.start()
    try:
        output, error = process.communicate()
    finally:
        timer.cancel()
        timer.join()
    if process.returncode:
        raise AssertionError('upsc %r returned %d: %r' %
                             (args, process.returncode, error))
    # Fixtures are ASCII. Normalize only the platform's output line ending.
    return output.decode('ascii').replace('\r\n', '\n')


def equal(actual, expected):
    if actual != expected:
        raise AssertionError('got %r, expected %r' % (actual, expected))


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
    if sys.argv[1] == 'prepare':
        prepare(sys.argv[2])
    else:
        check(sys.argv[1], sys.argv[2])
