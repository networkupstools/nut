# Run with Python 3.5+ and the path to a built upsset.cgi. No UPS is needed.
import sys

if sys.version_info < (3, 5):
    print('SKIP upsset-string-test: Python 3.5 or newer is required')
    sys.exit(0)

import html
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
from urllib.parse import urlencode


def quote(value):
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"').replace('#', '\\#') + '"'


def request(binary, metadata, value='original', new_value=None):
    errors, commands, saved = [], [], []
    with tempfile.TemporaryDirectory(prefix='nut-upsset-') as directory, socket.socket() as server:
        server.bind(('127.0.0.1', 0))
        server.listen(1)
        server.settimeout(10)
        target = 'dummy@127.0.0.1:' + str(server.getsockname()[1])
        config = Path(directory)
        (config / 'upsset.conf').write_text('I_HAVE_SECURED_MY_CGI_DIRECTORY\n')
        (config / 'hosts.conf').write_text('MONITOR ' + target + ' "Test UPS"\n')
        (config / 'nutauth.conf').write_text('')
        (config / 'nutauth.conf').chmod(0o600)

        def serve():
            try:
                connection, unused = server.accept()
                with connection:
                    connection.settimeout(10)
                    with connection.makefile('rb') as stream:
                        for raw in stream:
                            command = raw.decode('ascii').rstrip('\r\n')
                            commands.append(command)
                            if command == 'STARTTLS':
                                reply = 'ERR FEATURE-NOT-SUPPORTED'
                            elif command == 'LIST RW dummy':
                                reply = 'BEGIN LIST RW dummy\nRW dummy test.string "original"\nEND LIST RW dummy'
                            elif command == 'GET DESC dummy test.string':
                                reply = 'DESC dummy test.string "Test string"'
                            elif command == 'GET TYPE dummy test.string':
                                reply = 'TYPE dummy test.string RW ' + quote(metadata)
                            elif command == 'GET VAR dummy test.string':
                                reply = 'VAR dummy test.string ' + quote(value)
                            elif command in ('USERNAME test', 'PASSWORD test'):
                                reply = 'OK'
                            elif new_value is not None and command == 'SET VAR dummy test.string ' + quote(new_value):
                                saved.append(new_value)
                                reply = 'OK'
                            elif command == 'LOGOUT':
                                connection.sendall(b'OK Goodbye\n')
                                break
                            else:
                                raise AssertionError('Unexpected request: ' + command)
                            connection.sendall((reply + '\n').encode('ascii'))
            except Exception as error:
                errors.append(error)

        thread = threading.Thread(target=serve, daemon=True)
        thread.start()
        fields = {'username': 'test', 'password': 'test', 'monups': target,
                  'function': 'showsettings' if new_value is None else 'savesettings'}
        if new_value is not None:
            fields['UPSVAR_test.string'] = new_value
        body = urlencode(fields).encode('ascii') + b'\n'
        environment = {key: val for key, val in os.environ.items() if not key.startswith('NUT_')}
        environment.update(NUT_CONFPATH=directory, CONTENT_LENGTH=str(len(body)),
                           REQUEST_METHOD='POST')
        try:
            result = subprocess.run([binary], input=body, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, env=environment, timeout=10)
        finally:
            thread.join(timeout=11)
        assert not thread.is_alive(), 'Server did not finish'
        assert not errors, errors
        assert result.returncode == 0, (metadata, result.returncode, result.stderr.decode())
        output = result.stdout.decode('ascii')
        assert output.rstrip().endswith('</BODY></HTML>'), output
        assert 'Error:' not in output, output
        if new_value is None:
            assert 'GET TYPE dummy test.string' in commands, commands
        else:
            assert 'USERNAME test' in commands and 'PASSWORD test' in commands, commands
        return output, saved


def main():
    binary = os.path.abspath(sys.argv[1])
    # This upper boundary fits long on both 32-bit and 64-bit builds.
    for length in (1, 127, 128, 255, 256, 2147483647):
        output, unused = request(binary, 'STRING:' + str(length))
        assert 'NAME="UPSVAR_test.string" VALUE="original" SIZE="%d"' % length in output, output
        print('PASS STRING:%d' % length, flush=True)
    for length in ('', '0', '-1', 'junk', '128junk', ' 128', '128 ', '9' * 80):
        output, unused = request(binary, 'STRING:' + length)
        assert 'Unknown type' in output, output
        assert 'NAME="UPSVAR_test.string"' not in output, output
        print('PASS invalid STRING:%r' % length, flush=True)
    output, unused = request(binary, 'NUMBER')
    assert 'SIZE="20"' in output, output
    print('PASS NUMBER', flush=True)
    value = '<&"\'\\#' + 'x' * 122
    assert len(value) == 128
    output, saved = request(binary, 'STRING:128', new_value=value)
    assert saved == [value] and 'Updated 1 setting.' in output, (saved, output)
    output, unused = request(binary, 'STRING:128', value=saved[0])
    assert 'VALUE="' + html.escape(value, quote=True).replace('&#x27;', '&#39;') + '" SIZE="128"' in output, output
    print('PASS 128-character save/readback and HTML encoding', flush=True)
    output, saved = request(binary, 'STRING:256', new_value='x' * 257)
    assert not saved and 'No settings changed.' in output, (saved, output)
    print('PASS existing 256-character submission limit', flush=True)


if __name__ == '__main__':
    main()
