#!/usr/bin/env python
# Test the byte contract of LIST UPS without a running server.
# SPDX-License-Identifier: GPL-3.0-or-later

import itertools
import unittest

import PyNUT


class RecordingSocket(object):
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.sent = []

    def sendall(self, data):
        self.sent.append(data)

    def recv(self, size):
        if not self.chunks:
            return b''
        data = self.chunks.pop(0)
        if len(data) > size:
            self.chunks.insert(0, data[size:])
        return data[:size]

    def close(self):
        pass


def listing(rows):
    return b'BEGIN LIST UPS\n' + rows + b'END LIST UPS\n'


class UPSListTest(unittest.TestCase):
    def client(self, chunks):
        client = PyNUT.PyNUTClient(connect_now=False)
        transport = RecordingSocket(chunks)
        client._PyNUTClient__srv_handler = transport
        return client, transport

    def check_description(self, encoded, expected):
        response = listing(b'UPS dummy "' + encoded + b'"\n')
        client, transport = self.client([response])
        result = client.GetUPSList()
        self.assertEqual(result, {b'dummy': expected})
        self.assertEqual(type(list(result.keys())[0]), type(b''))
        self.assertEqual(type(result[b'dummy']), type(b''))
        self.assertEqual(transport.sent, [b'LIST UPS\n'])

    def test_ordinary(self):
        for desc in [b'Office UPS', b'', b'UPS', b'  Office UPS  ']:
            self.check_description(desc, desc)

    def test_quotes(self):
        for encoded, expected in [
            (b'Office \\"Main\\" UPS', b'Office "Main" UPS'),
            (b'\\"Office', b'"Office'),
            (b'Office\\"', b'Office"'),
            (b'\\"', b'"'),
            (b'\\"\\"', b'""'),
        ]:
            self.check_description(encoded, expected)

    def test_backslashes_and_literals(self):
        for encoded, expected in [
            (b'\\\\', b'\\'),
            (b'\\\\\\\\', b'\\\\'),
            (b'\\\\\\"', b'\\"'),
            (b'\\"\\\\', b'"\\'),
            (b'\\\\n \\\\t \\\\x41', b'\\n \\t \\x41'),
            (b"Owner's \\#1 = UPS; $HOME `a`", b"Owner's #1 = UPS; $HOME `a`"),
            (b'\\n', b'n'),
        ]:
            self.check_description(encoded, expected)

    def test_escape_combinations(self):
        # Literal encoded/decoded atoms, including adjacent escape boundaries.
        atoms = [(b' ', b' '), (b'\\"', b'"'),
                 (b'\\\\', b'\\'), (b'\\#', b'#')]
        for size in range(1, 5):
            for parts in itertools.product(atoms, repeat=size):
                self.check_description(b''.join(p[0] for p in parts),
                                       b''.join(p[1] for p in parts))

    def test_bytes(self):
        for value in range(0x20, 0x80):
            byte = chr(value).encode('latin-1')
            encoded = b'\\' + byte if byte in (b'"', b'\\', b'#') else byte
            self.check_description(encoded, byte)
        # Synthetic wire bytes: upsd's config parser discards these, but the
        # client must not introduce a Unicode decoding policy for descriptions.
        self.check_description(b'\x80\xff\\"\xc3\xa9', b'\x80\xff"\xc3\xa9')

    def test_multiple_entries_and_names(self):
        response = listing(b'UPS UPS_1-a.b "Office \\"Main\\""\n'
                           b'UPS second ""\n')
        client, transport = self.client([response, response])
        self.assertEqual(client.GetUPSList(),
                         {b'UPS_1-a.b': b'Office "Main"', b'second': b''})
        names = client.GetUPSNames()
        self.assertEqual(type(names), list)
        self.assertEqual(sorted(names), ['UPS_1-a.b', 'second'])
        for name in names:
            self.assertEqual(type(name), type(b''.decode('ascii')))
        self.assertEqual(transport.sent, [b'LIST UPS\n', b'LIST UPS\n'])

    def test_empty_list(self):
        client, transport = self.client([listing(b''), listing(b'')])
        self.assertEqual(client.GetUPSList(), {})
        self.assertEqual(client.GetUPSNames(), [])

    def test_fragmentation(self):
        response = listing(b'UPS dummy "A\\\\\\"B\\#C"\n')
        expected = {b'dummy': b'A\\"B#C'}
        for split in range(1, len(response)):
            client, transport = self.client([response[:split], response[split:]])
            self.assertEqual(client.GetUPSList(), expected)
        client, transport = self.client([response[i:i+1] for i in range(len(response))])
        self.assertEqual(client.GetUPSList(), expected)

    def test_coalesced_responses(self):
        # First recv includes part of the next response; __read_until must
        # retain it through both the header and the end-of-list reads.
        response = listing(b'UPS d "\\""\n')
        client, transport = self.client([response + listing(b'') + b'OK\n'])
        self.assertEqual(client.GetUPSList(), {b'd': b'"'})
        self.assertEqual(client.GetUPSList(), {})
        self.assertEqual(client._PyNUTClient__read_until(b'\n'), b'OK\n')
        self.assertEqual(transport.chunks, [])

    def test_consumers(self):
        response = listing(b'UPS dummy "Office \\"Main\\""\n')
        client, transport = self.client([response + b'OK\n' + response +
            b'BEGIN LIST CLIENT dummy\nCLIENT dummy 127.0.0.1\nEND LIST CLIENT dummy\n'])
        self.assertEqual(client.DeviceLogin('dummy'), 'OK')
        self.assertEqual(client.ListClients('dummy'), {b'dummy': [b'127.0.0.1']})
        self.assertEqual(transport.sent, [b'LIST UPS\n', b'LOGIN dummy\n',
                                         b'LIST UPS\n', b'LIST CLIENT dummy\n'])

    def test_list_clients_fallback(self):
        response = listing(b'UPS dummy "\\""\n')
        client, transport = self.client([response + b'ERR INVALID-ARGUMENT\n' + response +
            b'BEGIN LIST CLIENT dummy\nEND LIST CLIENT dummy\n'])
        self.assertEqual(client.ListClients(), {})

    def test_unknown_ups(self):
        response = listing(b'UPS dummy "\\""\n')
        for method in ['DeviceLogin', 'ListClients']:
            client, transport = self.client([response])
            try:
                getattr(client, method)('missing')
            except PyNUT.PyNUTError as exc:
                self.assertEqual(str(exc), 'ERR UNKNOWN-UPS')
            else:
                self.fail('Expected PyNUTError')
            self.assertEqual(transport.sent, [b'LIST UPS\n'])

    def test_server_error(self):
        client, transport = self.client([b'ERR ACCESS-DENIED\n'])
        try:
            client.GetUPSList()
        except PyNUT.PyNUTError as exc:
            self.assertEqual(str(exc), 'ERR ACCESS-DENIED')
        else:
            self.fail('Expected PyNUTError')

    def test_eof(self):
        for response in [b'', b'BEGIN LIST UPS\nUPS dummy "unfinished']:
            client, transport = self.client([response])
            self.assertRaises(EOFError, client.GetUPSList)

    def test_malformed_rows(self):
        for row in [b'UPS dummy unquoted\n', b'UPS dummy "raw "quote""\n',
                    b'UPS dummy "desc" "extra"\n', b'UPS dummy "missing close\n',
                    b'UPS dummy "dangling\\"\n']:
            client, transport = self.client([listing(row)])
            self.assertRaises(ValueError, client.GetUPSList)

    def test_ignored_lines(self):
        client, transport = self.client([listing(b'OTHER line\n\nUPS dummy "Office"\n')])
        self.assertEqual(client.GetUPSList(), {b'dummy': b'Office'})


if __name__ == '__main__':
    unittest.main()
