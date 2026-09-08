#!/usr/bin/env python
# Test NUT protocol and configuration quoting without external services.
# SPDX-License-Identifier: GPL-3.0-or-later

import os
import shutil
import tempfile
import unittest

import PyNUT
from test_upslist import RecordingSocket


class ReplySocket(RecordingSocket):
    """ Only supply a reply after its exact request has been observed. """
    def __init__(self, exchanges):
        RecordingSocket.__init__(self, [])
        self.exchanges = list(exchanges)

    def sendall(self, data):
        self.sent.append(data)
        if not self.exchanges:
            raise AssertionError('Unexpected request: %r' % data)
        request, reply = self.exchanges.pop(0)
        if request != data:
            raise AssertionError('Expected %r, received %r' % (request, data))
        self.chunks.append(reply)

    def send(self, data):
        self.sendall(data)
        return len(data)


class ProtocolTest(unittest.TestCase):
    def client(self, exchanges):
        client = PyNUT.PyNUTClient(connect_now=False)
        transport = ReplySocket(exchanges)
        client._PyNUTClient__srv_handler = transport
        return client, transport

    def check_value(self, encoded, expected):
        cases = [
            ('GetVariableDescription', ('dummy', 'ups.id'),
             b'GET DESC dummy ups.id\n',
             b'DESC dummy ups.id "' + encoded + b'"\n',
             expected.decode('ascii')),
            ('GetEnumList', ('dummy', 'ups.id'),
             b'LIST ENUM dummy ups.id\n',
             b'BEGIN LIST ENUM dummy ups.id\nENUM dummy ups.id "' + encoded +
             b'"\nEND LIST ENUM dummy ups.id\n', [expected.decode('ascii')]),
            ('GetUPSVars', ('dummy',), b'LIST VAR dummy\n',
             b'BEGIN LIST VAR dummy\nVAR dummy ups.id "' + encoded +
             b'"\nEND LIST VAR dummy\n', {b'ups.id': expected}),
            ('GetRWVars', ('dummy',), b'LIST RW dummy\n',
             b'BEGIN LIST RW dummy\nRW dummy ups.id "' + encoded +
             b'"\nEND LIST RW dummy\n', {b'ups.id': expected}),
        ]
        for method, args, request, reply, wanted in cases:
            client, transport = self.client([(request, reply)])
            result = getattr(client, method)(*args)
            self.assertEqual(result, wanted, method)
            if isinstance(result, dict):
                self.assertEqual(type(list(result.keys())[0]), type(b''))
                self.assertEqual(type(result[b'ups.id']), type(b''))
            elif isinstance(result, list):
                self.assertEqual(type(result[0]), type(b''.decode('ascii')))
            else:
                self.assertEqual(type(result), type(b''.decode('ascii')))
            self.assertEqual(transport.exchanges, [])

    def test_ordinary_readers(self):
        for value in [b'Office UPS', b'', b'  spaced  ']:
            self.check_value(value, value)

    def test_escaped_readers(self):
        for encoded, expected in [
            (b'Office \\"Main\\" \\#1', b'Office "Main" #1'),
            (b'\\\\n \\\\t \\\\x41', b'\\n \\t \\x41'),
            (b'\\\\\\"\\\\', b'\\"\\'),
            (b'\\q', b'q'),
            (b"Owner's #1 = UPS", b"Owner's #1 = UPS"),
        ]:
            self.check_value(encoded, expected)

    def test_command_descriptions(self):
        client, transport = self.client([
            (b'LIST CMD dummy\n', b'BEGIN LIST CMD dummy\n'
             b'CMD dummy test.panel.start\nCMD dummy test.panel.stop\n'
             b'END LIST CMD dummy\n'),
            (b'GET CMDDESC dummy test.panel.start\n',
             b'CMDDESC dummy test.panel.start "Start \\"panel\\" \\#1 \\\\n"\n'),
            (b'GET CMDDESC dummy test.panel.stop\n',
             b'CMDDESC dummy test.panel.stop "Stop panel"\n'),
        ])
        result = client.GetUPSCommands('dummy')
        self.assertEqual(result, {b'test.panel.start': b'Start "panel" #1 \\n',
                                  b'test.panel.stop': b'Stop panel'})
        for key, value in result.items():
            self.assertEqual(type(key), type(b''))
            self.assertEqual(type(value), type(b''))
        self.assertEqual(transport.exchanges, [])

    def test_command_description_fallback(self):
        client, transport = self.client([
            (b'LIST CMD dummy\n', b'BEGIN LIST CMD dummy\n'
             b'CMD dummy test.panel.start\nEND LIST CMD dummy\n'),
            (b'GET CMDDESC dummy test.panel.start\n', b'ERR UNKNOWN-COMMAND\n'),
        ])
        self.assertEqual(client.GetUPSCommands('dummy'),
                         {b'test.panel.start': b'test.panel.start'})
        self.assertEqual(transport.exchanges, [])

    def test_range_text(self):
        client, transport = self.client([
            (b'LIST RANGE dummy battery.charge.low\n',
             b'BEGIN LIST RANGE dummy battery.charge.low\n'
             b'RANGE dummy battery.charge.low "001" "010.50"\n'
             b'RANGE dummy battery.charge.low "20" "100"\n'
             b'END LIST RANGE dummy battery.charge.low\n'),
        ])
        result = client.GetRangeList('dummy', 'battery.charge.low')
        self.assertEqual(result, [{'min': '001', 'max': '010.50'},
                                  {'min': '20', 'max': '100'}])
        for limits in result:
            for value in limits.values():
                self.assertEqual(type(value), type(b''.decode('ascii')))

    def test_server_errors(self):
        for method, args, request in [
            ('GetVariableDescription', ('dummy', 'ups.id'), b'GET DESC dummy ups.id\n'),
            ('GetEnumList', ('dummy', 'ups.id'), b'LIST ENUM dummy ups.id\n'),
            ('GetRangeList', ('dummy', 'ups.id'), b'LIST RANGE dummy ups.id\n'),
            ('GetUPSVars', ('dummy',), b'LIST VAR dummy\n'),
            ('GetRWVars', ('dummy',), b'LIST RW dummy\n'),
            ('GetUPSCommands', ('dummy',), b'LIST CMD dummy\n'),
        ]:
            client, transport = self.client([(request, b'ERR UNKNOWN-UPS\n')])
            try:
                getattr(client, method)(*args)
            except PyNUT.PyNUTError as exc:
                self.assertEqual(str(exc), 'ERR UNKNOWN-UPS')
            else:
                self.fail('Expected PyNUTError from ' + method)


class AuthConfTest(unittest.TestCase):
    def setUp(self):
        PyNUT.AuthConf.freeAuthConfList()
        PyNUT.AuthConf.setDebug(False)
        self.directory = tempfile.mkdtemp(prefix='nut-authconf-')

    def tearDown(self):
        PyNUT.AuthConf.freeAuthConfList()
        shutil.rmtree(self.directory)

    def write_config(self, name, contents):
        filename = os.path.join(self.directory, name)
        with open(filename, 'w') as stream:
            stream.write(contents)
        return filename

    def read_config(self, contents):
        filename = self.write_config('nutauth.conf', contents)
        PyNUT.AuthConf.readAuthConfFile(filename, fatal_errors=True)
        return PyNUT.AuthConf.getAuthConf(host='localhost', port=3493)

    def test_value_syntax(self):
        for source, expected in [
            ('ordinary', 'ordinary'), ('""', ''),
            ('"Office \\"Main\\" \\#1"', 'Office "Main" #1'),
            ('"Owner\'s #1" # comment', "Owner's #1"),
            ("'literal'", "'literal'"),
            ('"\\\\n \\\\t \\\\x41"', '\\n \\t \\x41'),
            ('unquoted\\ space', 'unquoted space'),
            ('escaped\\#hash # comment', 'escaped#hash'),
            ('"equals=value"', 'equals=value'),
            ('left=right', 'left'),
            ('"first"second', 'first'),
            ('mid"quote', 'mid"quote'),
            ('one\\\ntwo', 'onetwo'),
            ('"one\\\ntwo"', 'onetwo'),
        ]:
            PyNUT.AuthConf.freeAuthConfList()
            result = self.read_config('PASSWORD=' + source + '\n')
            self.assertEqual(result.password, expected, source)

    def test_defaults_and_section_precedence(self):
        result = self.read_config('USERNAME=global\nPASSWORD=global\n'
            'CERTPATH="/global path"\n[@localhost:3493] # host defaults\n'
            'PASSWORD=host\n[alice@localhost:3493]\nUSERNAME=ignored\n'
            'PASSWORD=user\n')
        self.assertEqual(result.password, 'host')
        result = PyNUT.AuthConf.getAuthConf(user='alice', host='localhost', port=3493)
        self.assertEqual(result.user, 'alice')
        self.assertEqual(result.password, 'user')
        self.assertEqual(result.certpath, '/global path')

    def test_included_filenames(self):
        self.write_config('include "q"#1\\part.conf', 'PASSWORD="from include"\n')
        encoded = os.path.join(self.directory, 'include \\"q\\"\\#1\\\\part.conf')
        result = self.read_config('INCLUDE_REQUIRED "' + encoded + '" # comment\n')
        self.assertEqual(result.password, 'from include')
        PyNUT.AuthConf.freeAuthConfList()
        self.write_config("owner's file.conf", 'CERTPATH="included path"\n')
        encoded = os.path.join(self.directory, "owner's\\ file.conf")
        result = self.read_config('INCLUDE ' + encoded + '\nPASSWORD=local\n')
        self.assertEqual(result.certpath, 'included path')
        self.assertEqual(result.password, 'local')

    def test_optional_and_required_missing_include(self):
        missing = os.path.join(self.directory, 'missing.conf')
        result = self.read_config('INCLUDE "' + missing + '"\nPASSWORD=local\n')
        self.assertEqual(result.password, 'local')
        PyNUT.AuthConf.freeAuthConfList()
        self.assertRaises(PyNUT.PyNUTError, self.read_config,
                          'INCLUDE_REQUIRED "' + missing + '"\n')

    def connect(self, login, password, transport):
        original = PyNUT.socket.create_connection
        PyNUT.socket.create_connection = lambda *args, **kwargs: transport
        try:
            return PyNUT.PyNUTClient(login=login, password=password,
                                     use_ssl=False, tracking=None)
        finally:
            PyNUT.socket.create_connection = original

    def test_authenticate_decoded_credentials(self):
        result = self.read_config('USERNAME="Office \\"Main\\" \\#1"\n'
                                  'PASSWORD="path\\\\next \\"key\\" \\#2"\n')
        transport = ReplySocket([
            (b'USERNAME "Office \\"Main\\" \\#1"\n', b'OK\n'),
            (b'PASSWORD "path\\\\next \\"key\\" \\#2"\n', b'OK\n'),
        ])
        client = self.connect(result.user, result.password, transport)
        self.assertEqual(transport.exchanges, [])
        client.disconnect()

    def test_authenticate_ordinary_and_errors(self):
        transport = ReplySocket([(b'USERNAME "user"\n', b'ERR ACCESS-DENIED\n')])
        try:
            self.connect('user', 'pass', transport)
        except PyNUT.PyNUTError as exc:
            self.assertEqual(str(exc), 'ERR ACCESS-DENIED')
        else:
            self.fail('Expected username error')
        self.assertEqual(transport.exchanges, [])
        for reply in [b'OK\n', b'ERR ACCESS-DENIED\n', b'ERR INVALID-ARGUMENT\n']:
            transport = ReplySocket([
                (b'USERNAME "user"\n', b'OK\n'),
                (b'PASSWORD "pass"\n', reply),
            ])
            if reply == b'OK\n':
                client = self.connect('user', 'pass', transport)
                client.disconnect()
            else:
                try:
                    self.connect('user', 'pass', transport)
                except PyNUT.PyNUTError as exc:
                    self.assertEqual(str(exc), reply[:-1].decode('ascii'))
                else:
                    self.fail('Expected authentication error')
            self.assertEqual(transport.exchanges, [])

    def test_credential_line_breaks_rejected(self):
        # A line-oriented argument must not become more than one request.
        for value in ['first\nsecond', 'first\rsecond']:
            for login, password in [(value, 'pass'), ('user', value)]:
                transport = RecordingSocket([b'OK\n', b'OK\n'])
                self.assertRaises(ValueError, self.connect, login, password, transport)
                self.assertEqual(transport.sent,
                                 [] if login == value else [b'USERNAME "user"\n'])


if __name__ == '__main__':
    unittest.main()
