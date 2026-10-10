#!/usr/bin/env python3
"""Attachment protocol integration and exact client compatibility exchanges.

Run with an existing NIT sandbox: attachment-test.py CLIENT PORT USER PASSWORD.
The client executable can also be checked alone with attachment-test.py CLIENT.
Only loopback sockets and the supplied disposable sandbox are used.
"""
import os
import socket
import ssl
import subprocess
import sys
import threading
import time


def expect(got, wanted):
    if got != wanted:
        raise AssertionError("got %r, expected %r" % (got, wanted))


def exchange(client, mode, dialog, success=True, output=None, tls_cert=None):
    """Run the real C/C++ transport against a bounded scripted TCP peer."""
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(12)
    failures = []

    def peer():
        try:
            connection, _ = listener.accept()
            connection.settimeout(12)
            if tls_cert:
                expect(connection.recv(64), b"STARTTLS\n")
                connection.sendall(b"OK STARTTLS\n")
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.load_cert_chain(tls_cert)
                connection = context.wrap_socket(connection, server_side=True)
            with connection:
                stream = connection.makefile("rb", buffering=0)
                for command, reply in dialog:
                    request = stream.readline()
                    while request in (b"STARTTLS\n", b"PROTVER\n", b"NETVER\n"):
                        connection.sendall(b"ERR FEATURE-NOT-SUPPORTED\n" if request == b"STARTTLS\n" else b"1.3\n")
                        request = stream.readline()
                    expect(request, (command + "\n").encode("ascii"))
                    if reply is None:
                        # The operation may have completed: lose its reply.
                        connection.shutdown(socket.SHUT_RDWR)
                        return
                    if isinstance(reply, list):
                        for part in reply:
                            connection.sendall(part.encode("ascii"))
                            time.sleep(0.01)
                    else:
                        connection.sendall(reply.encode("ascii"))
                expect(stream.readline(), b"")
        except Exception as error:
            failures.append(error)

    worker = threading.Thread(target=peer)
    worker.daemon = True
    worker.start()
    try:
        result = subprocess.run([client, mode, str(listener.getsockname()[1])],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=15, universal_newlines=True)
        worker.join(13)
        if worker.is_alive():
            raise AssertionError("scripted peer did not finish")
        if failures:
            raise failures[0]
        expect(result.returncode, 0 if success else 1)
        if output is not None:
            expect(result.stdout.strip(), output)
    finally:
        listener.close()


def client_checks(client):
    for mode in ("attach", "login", "c-api-attach"):
        exchange(client, mode, [("ATTACH dummy", "OK\n")])
        exchange(client, mode, [("ATTACH dummy", "ERR UNKNOWN-COMMAND\n"),
                                ("LOGIN dummy", "OK\n")])
        for error in ("ERR ACCESS-DENIED\n", "ERR ALREADY-ATTACHED\n",
                      "ERR UNKNOWN-COMMAND trailing\n", None):
            exchange(client, mode, [("ATTACH dummy", error)], mode == "c-api-attach")
    for mode in ("detach", "logout", "c-api-detach"):
        exchange(client, mode, [("DETACH", "OK Goodbye\n")])
        exchange(client, mode, [("DETACH", "ERR UNKNOWN-COMMAND\n"),
                                ("LOGOUT", "OK Goodbye\n")])
        exchange(client, mode, [("DETACH", ["ERR UNKNOWN-", "COMMAND\n"]),
                                ("LOGOUT", "OK Goodbye\n")])
        for error in ("ERR ACCESS-DENIED\n", "ERR UNKNOWN-COMMAND trailing\n", None):
            exchange(client, mode, [("DETACH", error)], mode == "c-api-detach")
    # Implicit C cleanup retains its legacy command and single-drain behaviour.
    for reply in ("OK Goodbye\n", "ERR UNKNOWN-", "ERR ACCESS-DENIED\n", None):
        exchange(client, "c-detach", [("LOGOUT", reply)])
    for mode in ("count", "numlogins", "c-api-count"):
        exchange(client, mode, [("GET NUMATTACH dummy", "NUMATTACH dummy 2\n")], output="2")
        for error in ("ERR INVALID-ARGUMENT\n", "ERR UNKNOWN-COMMAND\n"):
            exchange(client, mode, [("GET NUMATTACH dummy", error),
                                    ("GET NUMLOGINS dummy", "NUMLOGINS dummy 2\n")], output="2")
        if mode != "c-api-count":
            exchange(client, mode, [("GET NUMATTACH dummy", "ERR ACCESS-DENIED\n")], False)
    for mode, query in (("c-query-count", "NUMATTACH"),
                        ("c-query-lower-count", "numattach")):
        for error, code in (("INVALID-ARGUMENT", "26"), ("UNKNOWN-COMMAND", "25"),
                            ("INVALID-ARGUMENT trailing", "42"),
                            ("UNKNOWN-COMMAND-OTHER", "42"),
                            ("ACCESS-DENIED", "6"), ("UNKNOWN-UPS", "4")):
            exchange(client, mode, [("GET " + query + " dummy", "ERR " + error + "\n"),
                                    ("LOGOUT", "OK Goodbye\n")], False, output=code)
    # Existing query error mapping is unchanged.
    exchange(client, "c-query-legacy-count",
             [("GET NUMLOGINS dummy", "ERR UNKNOWN-COMMAND-OTHER\n"),
              ("LOGOUT", "OK Goodbye\n")], False, output="25")
    print("PASS: C/C++ preferred and legacy APIs, exact fallback, errors and lost replies")


class Connection:
    def __init__(self, port):
        self.socket = socket.create_connection(("127.0.0.1", port), 5)
        self.stream = self.socket.makefile("rb")

    def query(self, command):
        self.socket.sendall((command + "\n").encode("ascii"))
        return self.stream.readline().decode("ascii").rstrip("\n")

    def close(self):
        self.stream.close()
        self.socket.close()


def server_checks(port, user, password, ups="dummy"):
    observer = Connection(port)
    def count(command="NUMATTACH"):
        reply = observer.query("GET %s %s" % (command, ups))
        prefix = command + " " + ups + " "
        if not reply.startswith(prefix):
            raise AssertionError(reply)
        return int(reply[len(prefix):])

    def authenticate(connection, supplied_password=password):
        expect(connection.query("USERNAME " + user), "OK")
        expect(connection.query("PASSWORD " + supplied_password), "OK")

    def await_count(wanted, timeout=5):
        deadline = time.time() + timeout
        while count() != wanted:
            if time.time() >= deadline:
                raise AssertionError("attachment count did not return to %d" % wanted)
            time.sleep(0.05)
        expect(count("NUMLOGINS"), wanted)

    try:
        help_words = observer.query("HELP").split()
        assert "ATTACH" in help_words and "DETACH" in help_words
        assert "LOGIN" not in help_words and "LOGOUT" not in help_words
        baseline = count()
        expect(count("NUMLOGINS"), baseline)
        expect(observer.query("ATTACH " + ups), "ERR USERNAME-REQUIRED")
        expect(observer.query("GET NUMATTACH missing"), "ERR UNKNOWN-UPS")
        for command in ("ATTACH", "LOGIN"):
            for finish in ("DETACH", "LOGOUT", None):
                connection = Connection(port)
                try:
                    authenticate(connection)
                    expect(connection.query(command), "ERR INVALID-ARGUMENT")
                    expect(connection.query(command + " missing"), "ERR UNKNOWN-UPS")
                    expect(connection.query(command + " " + ups), "OK")
                    await_count(baseline + 1)
                    expect(connection.query("ATTACH " + ups), "ERR ALREADY-ATTACHED")
                    expect(connection.query("LOGIN " + ups), "ERR ALREADY-LOGGED-IN")
                    await_count(baseline + 1)
                    if finish:
                        expect(connection.query(finish + " " + ups), "ERR INVALID-ARGUMENT")
                        expect(connection.query(finish), "OK Goodbye")
                        expect(connection.stream.readline(), b"")
                finally:
                    connection.close()
                # Windows upsd currently expires abruptly closed sockets
                # through its idle timeout; explicit teardown is immediate.
                await_count(baseline, 75 if finish is None and os.name == "nt" else 5)
            connection = Connection(port)
            try:
                authenticate(connection, password + "-incorrect")
                expect(connection.query(command + " " + ups), "ERR ACCESS-DENIED")
                await_count(baseline)
            finally:
                connection.close()
        print("PASS: real upsd aliases, HELP, credentials, duplicate errors and teardown counts")
    finally:
        observer.close()


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[2] == "--tls":
        for reply in ("OK Goodbye\n", "ERR UNKNOWN-", "ERR ACCESS-DENIED\n", None):
            dialog = [("LOGOUT", reply)]
            exchange(os.path.abspath(sys.argv[1]), "c-detach-tls", dialog, tls_cert=sys.argv[3])
        print("PASS: C implicit LOGOUT over TLS, incomplete reply, errors and lost replies")
        sys.exit(0)
    client_checks(os.path.abspath(sys.argv[1]))
    if len(sys.argv) >= 5:
        server_checks(int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5] if len(sys.argv) > 5 else "dummy")
