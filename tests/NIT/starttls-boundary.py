#!/usr/bin/env python3
"""Exercise the upsd plaintext-to-TLS parser boundary."""

import argparse
import getpass
import os
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import tempfile
import time


def run(command, **kwargs):
    print("Run: {}".format(" ".join(str(item) for item in command)), flush=True)
    return subprocess.run(command, check=True, **kwargs)


def receive_line(stream, timeout=4.0):
    stream.settimeout(timeout)
    data = bytearray()
    while not data.endswith(b"\n"):
        block = stream.recv(1)
        if not block:
            raise AssertionError("connection closed before newline: {!r}".format(bytes(data)))
        data.extend(block)
        if len(data) > 4096:
            raise AssertionError("response line exceeded test bound")
    return bytes(data)


def write(path, content, mode=None):
    path.write_text(content, encoding="ascii")
    if mode is not None:
        path.chmod(mode)


class BoundaryTests:
    def __init__(self, build, exeext, output):
        self.build = build
        self.upsd = build / ("server/upsd" + exeext)
        self.upsc = build / ("clients/upsc" + exeext)
        self.scratch = Path(tempfile.mkdtemp(prefix="nit-starttls-"))
        self.output = output.resolve() if output else self.scratch
        self.output.mkdir(parents=True, exist_ok=True)
        self.failures = []
        self.processes = []
        self.logs = []
        self.env = os.environ.copy()
        for name in ("NUT_CONFPATH", "NUT_STATEPATH", "NUT_PIDPATH", "NUT_ALTPIDPATH"):
            self.env[name] = str(self.scratch)
        self.env["LANG"] = self.env["LC_ALL"] = "C"
        self.env["TZ"] = "UTC"
        write(self.scratch / "upsd.users", "")
        write(self.scratch / "ups.conf", "")
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            self.port = reserve.getsockname()[1]

        probe = subprocess.run([str(self.upsd), "-Dh"], env=self.env,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               universal_newlines=True, check=False)
        description = probe.stdout
        if "OpenSSL" in description:
            self.backend = "openssl"
        elif "NSS" in description:
            self.backend = "nss"
        else:
            self.backend = "none"
        self.client_validation = "; with client certificate validation" in description
        print("Runtime directory: {}\nLogs: {}\nTLS backend: {}\nClient certificate validation: {}".format(
            self.scratch, self.output, self.backend,
            "yes" if self.client_validation else "no"), flush=True)

    def check(self, label, condition, detail=""):
        print("{}: {}{}".format("PASS" if condition else "FAIL", label,
                                " ({})".format(detail) if detail else ""), flush=True)
        if not condition:
            self.failures.append(label)

    def prepare_tls(self):
        certs = self.scratch / "certs"
        ca_dir = certs / "ca-dir"
        certs.mkdir()
        ca_dir.mkdir()
        write(certs / "ca.conf", """\
[req]
distinguished_name = dn
x509_extensions = extensions
prompt = no
[dn]
commonName = NUT STARTTLS test CA
[extensions]
authorityKeyIdentifier = keyid:always,issuer
basicConstraints = critical,CA:TRUE
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
""")
        write(certs / "server.ext", """\
authorityKeyIdentifier = keyid,issuer
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
subjectKeyIdentifier = hash
subjectAltName = DNS:localhost,IP:127.0.0.1
""")
        write(certs / "client.ext", """\
authorityKeyIdentifier = keyid,issuer
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = clientAuth
subjectKeyIdentifier = hash
""")
        run(["openssl", "genrsa", "-out", str(certs / "ca.key"), "2048"])
        run(["openssl", "req", "-x509", "-new", "-key", str(certs / "ca.key"),
             "-sha256", "-days", "1", "-out", str(certs / "ca.crt"),
             "-config", str(certs / "ca.conf")])
        for name in ("server", "client"):
            run(["openssl", "req", "-new", "-newkey", "rsa:2048", "-nodes",
                 "-keyout", str(certs / (name + ".key")),
                 "-out", str(certs / (name + ".req")),
                 "-subj", "/CN={}".format("localhost" if name == "server" else "NUT test client")])
            run(["openssl", "x509", "-req", "-in", str(certs / (name + ".req")),
                 "-CA", str(certs / "ca.crt"), "-CAkey", str(certs / "ca.key"),
                 "-CAcreateserial", "-days", "1", "-sha256",
                 "-extfile", str(certs / (name + ".ext")),
                 "-out", str(certs / (name + ".crt"))])
        ca_hash = subprocess.check_output(
            ["openssl", "x509", "-hash", "-noout", "-in", str(certs / "ca.crt")],
            universal_newlines=True).strip()
        shutil.copyfile(certs / "ca.crt", ca_dir / (ca_hash + ".0"))
        for name in ("server", "client"):
            pem = (certs / (name + ".crt")).read_bytes() + (certs / "ca.crt").read_bytes() + (certs / (name + ".key")).read_bytes()
            (certs / (name + ".pem")).write_bytes(pem)
            (certs / (name + ".pem")).chmod(0o600)
        self.ca = certs / "ca.crt"
        self.ca_dir = ca_dir
        self.client_pem = certs / "client.pem"
        self.server_pem = certs / "server.pem"
        if self.backend == "nss":
            self.prepare_nss(certs)
            auth = """\
SSLBACKEND = "nss"
CERTPATH = "{}"
CERTIDENT_NAME = "NUT test client"
CERTIDENT_PASS = "nut-test-password"
CERTHOST = "localhost"
CERTVERIFY = on
FORCESSL = on
""".format(self.client_db)
        else:
            auth = """\
CERTPATH = "{}"
CERTFILE = "{}"
CERTVERIFY = on
FORCESSL = on
""".format(self.ca_dir, self.client_pem)
        write(self.scratch / "nutauth.conf", auth, 0o600)

    def prepare_nss(self, certs):
        password = certs / "nss-password"
        write(password, "nut-test-password", 0o600)
        self.server_db = certs / "server-db"
        self.client_db = certs / "client-db"
        for name, database, trust in (
                ("server", self.server_db, "CT,C,C"),
                ("client", self.client_db, "TC,,")):
            database.mkdir()
            run(["certutil", "-N", "-d", str(database), "-f", str(password)])
            run(["certutil", "-A", "-d", str(database), "-f", str(password),
                 "-n", "NUT STARTTLS test CA", "-t", trust, "-a",
                 "-i", str(certs / "ca.crt")])
            bundle = certs / (name + ".p12")
            nickname = "NUT test {}".format(name)
            run(["openssl", "pkcs12", "-export", "-out", str(bundle),
                 "-inkey", str(certs / (name + ".key")),
                 "-in", str(certs / (name + ".crt")),
                 "-certfile", str(certs / "ca.crt"), "-name", nickname,
                 "-passout", "file:" + str(password)])
            run(["pk12util", "-i", str(bundle), "-d", str(database),
                 "-k", str(password), "-w", str(password)])

    def configure_server(self, tls, certrequest):
        config = "LISTEN 127.0.0.1 {}\nALLOW_NO_DEVICE true\n".format(self.port)
        if tls:
            if self.backend == "nss":
                config += "CERTPATH \"{}\"\nCERTIDENT \"NUT test server\" \"nut-test-password\"\n".format(
                    self.server_db)
            else:
                config += "CERTFILE \"{}\"\nCERTPATH \"{}\"\n".format(
                    self.server_pem, self.ca_dir)
            if self.client_validation:
                config += "CERTREQUEST {}\n".format(certrequest)
        write(self.scratch / "upsd.conf", config)

    def start_server(self, label, tls, certrequest):
        self.configure_server(tls, certrequest)
        log = (self.output / ("upsd-" + label + ".log")).open("w")
        self.logs.append(log)
        command = [str(self.upsd), "-DDDDDD", "-F", "-4", "-u", getpass.getuser()]
        print("Start: {}".format(command), flush=True)
        process = subprocess.Popen(command, env=self.env, stdout=log,
                                   stderr=subprocess.STDOUT, universal_newlines=True)
        self.processes.append(process)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise AssertionError("upsd exited during startup; see {}".format(log.name))
            try:
                probe = socket.create_connection(("127.0.0.1", self.port), timeout=0.1)
                probe.close()
                return process, Path(log.name)
            except OSError:
                time.sleep(0.05)
        raise AssertionError("upsd did not become ready; see {}".format(log.name))

    def stop_server(self, process):
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=4)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)
        for log in self.logs:
            log.flush()

    def connect(self):
        stream = socket.create_connection(("127.0.0.1", self.port), timeout=2)
        stream.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        return stream

    def tls_context(self, client_certificate=True):
        context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=str(self.ca))
        context.check_hostname = True
        if client_certificate:
            context.load_cert_chain(str(self.client_pem))
        return context

    def case(self, label, action, tls=True, certrequest=2):
        process, log_path = self.start_server(label, tls, certrequest)
        try:
            action()
        except Exception as error:
            self.check(label, False, "{}: {}".format(type(error).__name__, error))
        finally:
            self.stop_server(process)
        return log_path

    def plaintext_pipeline(self):
        def action():
            stream = self.connect()
            stream.sendall(b"PROTVER\nVER\n")
            self.check("plaintext pipelining keeps command boundaries",
                       receive_line(stream) == b"1.3\n" and
                       receive_line(stream).startswith(b"Network UPS Tools upsd "))
            stream.close()
        self.case("plaintext-pipeline", action, self.backend != "none")

    def normal_tls(self):
        def action():
            plain = self.connect()
            plain.sendall(b"STARTTLS\n")
            ack = receive_line(plain)
            secure = self.tls_context().wrap_socket(plain, server_hostname="localhost")
            secure.sendall(b"PROTVER\n")
            response = receive_line(secure)
            self.check("verified STARTTLS accepts a clean TLS command",
                       ack == b"OK STARTTLS\n" and response == b"1.3\n" and
                       bool(secure.getpeercert()))
            secure.close()
        self.case("normal-tls", action)

    def tls_without_client_certificate(self):
        def action():
            plain = self.connect()
            plain.sendall(b"STARTTLS\n")
            ack = receive_line(plain)
            secure = self.tls_context(False).wrap_socket(plain, server_hostname="localhost")
            secure.sendall(b"PROTVER\n")
            response = receive_line(secure)
            self.check("STARTTLS works when client certificates are not requested",
                       ack == b"OK STARTTLS\n" and response == b"1.3\n")
            secure.close()
        self.case("no-client-certificate", action, certrequest=0)

    def complete_tail(self):
        def action():
            plain = self.connect()
            plain.sendall(b"STARTTLS\nPROTVER\n")
            ack = receive_line(plain)
            secure = self.tls_context().wrap_socket(plain, server_hostname="localhost")
            secure.settimeout(0.3)
            try:
                unsolicited = secure.recv(1)
            except socket.timeout:
                unsolicited = None
            secure.sendall(b"VER\n")
            response = receive_line(secure)
            self.check("buffered plaintext command is not dispatched after STARTTLS",
                       ack == b"OK STARTTLS\n" and unsolicited is None and
                       response.startswith(b"Network UPS Tools upsd "),
                       "unsolicited={!r} response={!r}".format(unsolicited, response))
            secure.close()
        self.case("complete-tail", action)

    def split_state(self):
        def action():
            plain = self.connect()
            plain.sendall(b"STARTTLS\nPROT")
            ack = receive_line(plain)
            secure = self.tls_context().wrap_socket(plain, server_hostname="localhost")
            secure.sendall(b"VER\n")
            response = receive_line(secure)
            secure.sendall(b"PROTVER\n")
            followup = receive_line(secure)
            self.check("plaintext parser state cannot combine with TLS bytes",
                       ack == b"OK STARTTLS\n" and
                       response.startswith(b"Network UPS Tools upsd ") and
                       followup == b"1.3\n", "first={!r}".format(response))
            secure.close()
        self.case("split-state", action)

    def rejected_handshake(self):
        def action():
            rejected = False
            detail = "no rejection observed"
            plain = self.connect()
            plain.sendall(b"STARTTLS\nUSERNAME injected\nUSERNAME second\n")
            ack = receive_line(plain)
            context = self.tls_context(False)
            try:
                secure = context.wrap_socket(plain, server_hostname="localhost")
            except socket.timeout:
                detail = "TLS handshake timed out"
            except (OSError, ssl.SSLError) as error:
                rejected = True
                detail = "{}: {}".format(type(error).__name__, error)
            else:
                try:
                    secure.settimeout(1)
                    secure.sendall(b"PROTVER\n")
                    response = secure.recv(1)
                except socket.timeout:
                    detail = "TLS application read timed out"
                except (OSError, ssl.SSLError) as error:
                    rejected = True
                    detail = "{}: {}".format(type(error).__name__, error)
                else:
                    if response:
                        detail = "received TLS application data {!r}".format(response)
                    else:
                        rejected = True
                        detail = "peer closed the TLS connection"
                finally:
                    secure.close()
            self.check("required client certificate handshake is rejected",
                       ack == b"OK STARTTLS\n" and rejected, detail)

        log_path = self.case("rejected-handshake", action)
        log = log_path.read_text(encoding="utf-8", errors="replace")
        self.check("rejected handshake dispatches no buffered plaintext state",
                   "Calling command handler for USERNAME" not in log)

    def stock_client(self):
        def action():
            completed = subprocess.run(
                [str(self.upsc), "-A", str(self.scratch / "nutauth.conf"),
                 "-l", "127.0.0.1:{}".format(self.port)],
                env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                universal_newlines=True, timeout=10, check=False)
            self.check("stock upsc completes verified STARTTLS", completed.returncode == 0,
                       "returncode={} stderr={!r}".format(completed.returncode, completed.stderr))
        self.case("stock-client", action)

    def no_tls_fallback(self):
        def action():
            stream = self.connect()
            stream.sendall(b"STARTTLS\nPROTVER\n")
            error = receive_line(stream)
            response = receive_line(stream)
            self.check("no-TLS STARTTLS rejection preserves plaintext operation",
                       error == b"ERR FEATURE-NOT-SUPPORTED\n" and response == b"1.3\n")
            stream.close()
        self.case("no-tls-fallback", action, False)

    def execute(self):
        if self.backend in ("openssl", "nss"):
            self.prepare_tls()
            self.plaintext_pipeline()
            self.normal_tls()
            self.tls_without_client_certificate()
            self.complete_tail()
            self.split_state()
            if self.client_validation:
                self.rejected_handshake()
            else:
                print("SKIP: required client certificate rejection is not compiled in", flush=True)
            self.stock_client()
        elif self.backend == "none":
            self.plaintext_pipeline()
            self.no_tls_fallback()
        return bool(self.failures)

    def close(self):
        for process in reversed(self.processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=2)
        for log in self.logs:
            log.close()
        if self.output != self.scratch:
            shutil.rmtree(self.scratch)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("builddir", type=Path)
    parser.add_argument("--exeext", default="")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    tests = BoundaryTests(args.builddir.resolve(), args.exeext, args.output)
    try:
        failed = tests.execute()
    finally:
        tests.close()
    print("{} failed checks".format(len(tests.failures)), flush=True)
    return failed


if __name__ == "__main__":
    raise SystemExit(main())
