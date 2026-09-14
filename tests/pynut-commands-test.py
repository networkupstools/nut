# NOTE: No shebang, not marked executable. The Python interpreter setup and
# execution is driven by NUT tests/Makefile.am goals. Works for Py2 and Py3.
#
# Regression checks for PyNUT command enumeration (no running server needed).
# Run with PYTHONPATH pointing to the configured scripts/python/module directory.

import socket
import unittest

import PyNUT


class TranscriptSocket(object):
    """Only release a response after its exact request has been sent."""

    def __init__(self, exchanges, chunk_size=50):
        self.exchanges = exchanges
        self.chunk_size = chunk_size
        self.sent = []
        self.pending = b''
        self.read_error = None

    def sendall(self, request):
        index = len(self.sent)
        self.sent.append(request)
        expected, response = self.exchanges[index]
        if request != expected:
            raise AssertionError('Expected %r, received %r' % (expected, request))
        if isinstance(response, Exception):
            raise response
        self.pending += response

    def recv(self, size):
        if self.read_error is not None and not self.pending:
            raise self.read_error
        size = min(size, self.chunk_size)
        result, self.pending = self.pending[:size], self.pending[size:]
        return result


def command_list(names):
    return (b'BEGIN LIST CMD dummy\n' +
            b''.join([b'CMD dummy ' + name + b'\n' for name in names]) +
            b'END LIST CMD dummy\n')


def description_reply(name, description):
    return b'CMDDESC dummy ' + name + b' "' + description + b'"\n'


class CommandTests(unittest.TestCase):
    def client(self, exchanges, chunk_size=50):
        transport = TranscriptSocket(exchanges, chunk_size)
        client = PyNUT.PyNUTClient(connect_now=False)
        client._PyNUTClient__srv_handler = transport
        return client, transport

    def assert_requests(self, transport):
        # GetUPSCommands catches even AssertionError during CMDDESC, so check
        # the complete transcript outside the client's exception handler too.
        self.assertEqual(transport.sent, [pair[0] for pair in transport.exchanges])
        self.assertEqual(transport.pending, b'')

    def test_descriptions(self):
        pairs = [
            (b'load.off', b'Turn off the load immediately'),
            (b'test.panel.start', b'Start testing the UPS panel'),
            (b'upstream.load.off', b'Turn off the load immediately'),
            (b'outlet.1.load.off', b'Turn off this outlet'),
            (b'driver.reload-or-error', b'Reload driver configuration'),
            (b'test.panel.stop', b''),
            (b'experimental.example', b'Description unavailable'),
        ]
        exchanges = [(b'LIST CMD dummy\n', command_list([p[0] for p in pairs]))]
        for name, description in pairs:
            exchanges.append((b'GET CMDDESC dummy ' + name + b'\n',
                              description_reply(name, description)))
        for chunk_size in (1, 7, 50):
            client, transport = self.client(exchanges, chunk_size)
            commands = client.GetUPSCommands('dummy')
            self.assert_requests(transport)
            self.assertEqual(commands, dict(pairs))
            for name, description in commands.items():
                self.assertEqual(type(name), bytes)
                self.assertEqual(type(description), bytes)
            # NUT-Monitor's Qt variants sort the keys and decode both fields.
            labels = ['%s\n%s' % (name.decode('ascii'), commands[name].decode('ascii'))
                      for name in sorted(commands.keys())]
            self.assertTrue('test.panel.start\nStart testing the UPS panel' in labels)

    def test_description_fallbacks(self):
        name = b'test.panel.start'
        for response in (b'ERR CMD-NOT-SUPPORTED\n', b'ERR DATA-STALE\n',
                         b'WRONG response\n', b'CMDDESC\n',
                         b'CMDDESC dummy test.panel.start missing-quotes\n'):
            exchanges = [
                (b'LIST CMD dummy\n', command_list([name, b'load.off'])),
                (b'GET CMDDESC dummy test.panel.start\n', response),
                (b'GET CMDDESC dummy load.off\n',
                 description_reply(b'load.off', b'Turn off the load immediately')),
            ]
            client, transport = self.client(exchanges)
            self.assertEqual(client.GetUPSCommands('dummy'),
                             {name: name, b'load.off': b'Turn off the load immediately'})
            self.assert_requests(transport)

    def test_list_error(self):
        client, transport = self.client([(b'LIST CMD dummy\n', b'ERR UNKNOWN-UPS\n')])
        try:
            client.GetUPSCommands('dummy')
        except PyNUT.PyNUTError as error:
            self.assertEqual(str(error), 'ERR UNKNOWN-UPS')
        else:
            self.fail('LIST CMD error did not propagate')
        self.assert_requests(transport)

    def test_list_transport_errors(self):
        for response in (b'', b'BEGIN LIST CMD dummy\nCMD dummy load.off\n'):
            client, transport = self.client([(b'LIST CMD dummy\n', response)])
            self.assertRaises(EOFError, client.GetUPSCommands, 'dummy')
            self.assert_requests(transport)
        client, transport = self.client([(b'LIST CMD dummy\n', b'')])
        transport.read_error = socket.timeout('read timed out')
        self.assertRaises(socket.timeout, client.GetUPSCommands, 'dummy')
        self.assert_requests(transport)
        client, transport = self.client([(b'LIST CMD dummy\n', socket.error('send failed'))])
        self.assertRaises(socket.error, client.GetUPSCommands, 'dummy')
        self.assert_requests(transport)

    def test_description_transport_errors(self):
        name = b'load.off'
        for response, read_error in ((b'', None), (b'', socket.timeout('read timed out')),
                                     (socket.error('send failed'), None)):
            exchanges = [
                (b'LIST CMD dummy\n', command_list([name])),
                (b'GET CMDDESC dummy load.off\n', response),
            ]
            client, transport = self.client(exchanges)
            transport.read_error = read_error
            self.assertEqual(client.GetUPSCommands('dummy'), {name: name})
            self.assert_requests(transport)

    def test_leftover_response(self):
        # A coalesced LIST header/body leaves buffered data after the first
        # line. Repeated calls must consume it without losing response boundaries.
        name = b'load.off'
        exchanges = [
            (b'LIST CMD dummy\n', command_list([name])),
            (b'GET CMDDESC dummy load.off\n', description_reply(name, b'Load off')),
        ] * 2 + [(b'LIST UPS\n', b'BEGIN LIST UPS\nUPS dummy "Test device"\nEND LIST UPS\n')]
        client, transport = self.client(exchanges)
        for unused in range(2):
            self.assertEqual(client.GetUPSCommands('dummy'), {name: b'Load off'})
        self.assertEqual(client.GetUPSList(), {b'dummy': b'Test device'})
        self.assert_requests(transport)
        self.assertEqual(client._PyNUTClient__recv_leftover, b'')


if __name__ == '__main__':
    unittest.main()
