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

default_timeout = 30
nut_debug_level_env = int(os.getenv('NUT_DEBUG_LEVEL', 0))
# Might hack here to debug PY script independently of upsset.cgi binary
nut_debug_level = nut_debug_level_env

def quote(value):
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"').replace('#', '\\#') + '"'


def request(binary, metadata, value='original', new_value=None):
    errors, commands, saved = [], [], []
    with tempfile.TemporaryDirectory(prefix='nut-upsset-') as directory, socket.socket() as server:
        server.bind(('127.0.0.1', 0))
        server.listen(1)
        server.settimeout(default_timeout)
        server_port_str = str(server.getsockname()[1])
        assert server_port_str is not None and server_port_str != "" and server_port_str != "None", "Server port is None or empty"
        target = 'dummy@127.0.0.1:' + server_port_str
        config = Path(directory)
        (config / 'upsset.conf').write_text('I_HAVE_SECURED_MY_CGI_DIRECTORY\n')
        (config / 'hosts.conf').write_text('MONITOR ' + target + ' "Test UPS"\n')
        (config / 'nutauth.conf').write_text('')
        (config / 'nutauth.conf').chmod(0o600)

        def serve():
            """ Mock upsd """
            try:
                if nut_debug_level > 0:
                    print("Server awaiting connection on port", server_port_str)
                connection, unused = server.accept()
                if nut_debug_level > 0:
                    print("Server accepted connection")
                with connection:
                    if nut_debug_level > 0:
                        print("Server using connection")
                    connection.settimeout(default_timeout)
                    with connection.makefile('rb') as stream:
                        for raw in stream:
                            command = raw.decode('ascii').rstrip('\r\n')
                            if nut_debug_level > 0:
                                print("Server got command:", command)
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
                                if nut_debug_level > 0:
                                    print("Server ended connection")
                                break
                            else:
                                raise AssertionError('Unexpected request: ' + command)
                            connection.sendall((reply + '\n').encode('ascii'))
                if nut_debug_level > 0:
                    print("Server thread ended without errors")
            except Exception as error:
                errors.append(error)
                if nut_debug_level > 0:
                    print("Server thread ended with errors")

        # Start mock upsd as a thread, get upsset.cgi to talk to it
        thread = threading.Thread(target=serve, daemon=True)
        thread.start()
        fields = {'username': 'test', 'password': 'test', 'monups': target,
                  'function': 'showsettings' if new_value is None else 'savesettings'}
        if new_value is not None:
            fields['UPSVAR_test.string'] = new_value
        body = urlencode(fields).encode('ascii') + b'\n'
        environment = {key: val for key, val in os.environ.items() if (key == 'NUT_DEBUG_LEVEL' or not key.startswith('NUT_'))}
        # Ensure CGI environment is sufficiently defined for predictable behavior
        # so upsset.cgi reliably parses POSTed form data and proceeds to contact
        # the mocked NUT server. Missing CONTENT_TYPE may cause it to ignore the
        # request body and skip any network activity, leading to a timeout here.
        environment.update(
            NUT_CONFPATH=directory,
            CONTENT_LENGTH=str(len(body)),
            REQUEST_METHOD='POST',
            CONTENT_TYPE='application/x-www-form-urlencoded',
            # Provide a minimal but complete CGI context; some servers/frameworks
            # always set these, and upsset.cgi/cgilib can rely on them in places.
            GATEWAY_INTERFACE='CGI/1.1',
            SERVER_PROTOCOL='HTTP/1.1',
            SERVER_SOFTWARE='nut-test',
            SERVER_NAME='localhost',
            SERVER_PORT=server_port_str,
            SCRIPT_NAME='/upsset.cgi',
            REQUEST_URI='/upsset.cgi',
            QUERY_STRING='',
            REMOTE_ADDR='127.0.0.1',
            HTTP_HOST='localhost'
        )
        try:
            try:
                result = subprocess.run([binary], input=body, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, env=environment, timeout=default_timeout)
            except (FileNotFoundError, PermissionError, OSError) as e:
                # Some build trees provide a libtool wrapper script (shell script)
                # which may not be directly executable in certain environments.
                # Retry explicitly via /bin/bash to ensure it launches.
                result = subprocess.run(['/usr/bin/env', 'bash', binary], input=body, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, env=environment, timeout=default_timeout)
        finally:
            thread.join(timeout=default_timeout + 5)
        assert not thread.is_alive(), 'Server did not finish'
        assert not errors, errors
        assert result.returncode == 0, (metadata, result.returncode, result.stderr.decode())
        output = result.stdout.decode('ascii')
        if nut_debug_level_env > 0:
            assert '</BODY></HTML>' in output
            assert output.rstrip().endswith('</pre>'), output
        else:
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
