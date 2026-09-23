#!/usr/bin/env python3
"""Exercise repeater freshness through real dummy-ups, upsd and upsc binaries."""

import argparse
import getpass
import os
from pathlib import Path
import shutil
import socket
import socketserver
import subprocess
import tempfile
import threading
import time


class Source(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, port=0):
        super().__init__(("127.0.0.1", port), Reply)
        self.mode = "normal"
        self.value = "100"
        self.requests = 0
        self.lock = threading.Lock()
        threading.Thread(target=self.serve_forever, daemon=True).start()

    def change(self, mode, value="100"):
        with self.lock:
            self.mode, self.value = mode, value
            return self.requests


class Reply(socketserver.StreamRequestHandler):
    def handle(self):
        try:
            for line in self.rfile:
                if line.strip() == b"STARTTLS":
                    self.wfile.write(b"ERR FEATURE-NOT-SUPPORTED\n")
                    continue
                if line.strip() != b"LIST VAR source":
                    self.wfile.write(b"ERR UNKNOWN-COMMAND\n")
                    continue
                with self.server.lock:
                    mode, value = self.server.mode, self.server.value
                    self.server.requests += 1
                print("source: LIST {} value={}".format(mode, value), flush=True)
                if mode == "drop":
                    return
                if mode == "stale":
                    self.wfile.write(b"ERR DATA-STALE\n")
                    continue
                self.wfile.write(b"BEGIN LIST VAR source\n")
                self.wfile.write(b'VAR source ups.status "OL"\n')
                if mode == "short":
                    self.wfile.write(b"VAR source battery.charge\n")
                    return
                self.wfile.write(('VAR source battery.charge "{}"\n'.format(value)).encode("ascii"))
                if mode == "partial":
                    return
                if mode == "error":
                    self.wfile.write(b"ERR DATA-STALE\n")
                    continue
                self.wfile.write(b'VAR source driver.name "must-not-overwrite"\n')
                self.wfile.write(b"END LIST VAR source\n")
        except (OSError, ValueError):
            pass


def wait_for(predicate, seconds=15):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return True
        time.sleep(0.2)
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("builddir", type=Path)
    parser.add_argument("--exeext", default="")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    build = args.builddir.resolve()
    # Keep paths short for the driver Unix-domain socket, as in nit.sh.
    scratch = Path(tempfile.mkdtemp(prefix="nit-repeater-"))
    output = args.output.resolve() if args.output else scratch
    output.mkdir(parents=True, exist_ok=True)
    print("Runtime directory: {}\nLogs: {}".format(scratch, output), flush=True)
    source = Source()
    processes, logs, failures = [], [], []
    env = os.environ.copy()
    for name in ("NUT_CONFPATH", "NUT_STATEPATH", "NUT_PIDPATH", "NUT_ALTPIDPATH"):
        env[name] = str(scratch)
    env["NUT_AUTHCONF_FILE"] = str(scratch / "nutauth.conf")
    env["LANG"] = env["LC_ALL"] = "C"
    env["TZ"] = "UTC"
    user = getpass.getuser()

    def start(binary, name, extra):
        log = (output / (name + ".log")).open("w")
        logs.append(log)
        command = [str(build / (binary + args.exeext)), "-D", "-F", "-u", user] + extra
        print("Start: {}".format(command), flush=True)
        proc = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        processes.append(proc)
        return proc

    def check(label, condition):
        print("{}: {}".format("PASS" if condition else "FAIL", label), flush=True)
        if not condition:
            failures.append(label)

    def query(device="repeater", variable="battery.charge"):
        result = subprocess.run([str(build / ("clients/upsc" + args.exeext)),
                                 "{}@127.0.0.1:{}".format(device, port), variable],
                                env=env, capture_output=True, text=True, timeout=5)
        return result.returncode, result.stdout.strip(), result.stderr.strip()

    def fresh(value):
        return query()[:2] == (0, value)

    def stale():
        result = query()
        return result[0] != 0 and "Data stale" in result[2]

    try:
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            port = reserve.getsockname()[1]
        upstream = source.server_address[1]
        (scratch / "upsd.conf").write_text("LISTEN 127.0.0.1 {}\nMAXAGE 60\n".format(port))
        (scratch / "upsd.users").write_text("")
        (scratch / "nutauth.conf").write_text("")
        (scratch / "nutauth.conf").chmod(0o600)
        (scratch / "control.dev").write_text("ups.status: OL\nbattery.charge: 77\n")
        config = "pollinterval = 1\n"
        for name in ("repeater", "strict", "relaxed"):
            config += "[{}]\n driver = dummy-ups\n port = source@127.0.0.1:{}\n authconf = none\n".format(name, upstream)
            if name == "relaxed":
                config += " repeater_disable_strict_start\n"
        for name, mode in (("once", "dummy-once"), ("loop", "dummy-loop")):
            config += "[{}]\n driver = dummy-ups\n port = control.dev\n mode = {}\n".format(name, mode)
        (scratch / "ups.conf").write_text(config)
        source.shutdown()
        source.server_close()
        strict = start("drivers/dummy-ups", "strict", ["-a", "strict"])
        check("strict startup rejects connection refusal", wait_for(lambda: strict.poll() is not None) and strict.returncode != 0)
        relaxed = start("drivers/dummy-ups", "relaxed", ["-a", "relaxed"])
        server = start("server/upsd", "upsd", [])
        for name in ("once", "loop"):
            start("drivers/dummy-ups", name, ["-a", name])
        check("relaxed startup remains stale and running",
              wait_for(lambda: "Data stale" in query("relaxed")[2]) and relaxed.poll() is None)
        source = Source(upstream)
        check("relaxed startup recovers", wait_for(lambda: query("relaxed")[:2] == (0, "100")))
        relaxed.terminate()
        relaxed.wait(timeout=5)
        source.change("partial")
        incomplete = start("drivers/dummy-ups", "strict-incomplete", ["-a", "strict"])
        check("strict startup rejects incomplete LIST",
              wait_for(lambda: incomplete.poll() is not None, 5) and incomplete.returncode != 0)
        if incomplete.poll() is None:
            incomplete.terminate()
            incomplete.wait(timeout=5)
        source.change("normal")
        driver = start("drivers/dummy-ups", "repeater", ["-a", "repeater"])
        check("complete LIST is fresh", wait_for(lambda: fresh("100")))
        check("upstream driver metadata is not copied", query("repeater", "driver.name")[:2] == (0, "dummy-ups"))
        for number, mode in enumerate(("drop", "partial", "stale", "error", "drop", "partial", "short"), 1):
            before = source.change(mode, str(20 + number))
            check(mode + " reached upstream", wait_for(lambda: source.requests > before))
            check(mode + " marks downstream stale", wait_for(stale, 8))
            print("Client result: {}".format(query()), flush=True)
            # Hold the fault across multiple update/reconnect attempts.
            before = source.requests
            check(mode + " keeps retrying", wait_for(lambda: source.requests >= before + 2, 8))
            check(mode + " stays stale while driver lives", stale() and driver.poll() is None)
            for name in ("once", "loop"):
                check(mode + " leaves " + name + " fresh", query(name)[:2] == (0, "77"))
            value = str(90 - number)
            source.change("normal", value)
            check(mode + " recovers new data without restart", wait_for(lambda: fresh(value)) and driver.poll() is None)
        check("downstream server stayed running", server.poll() is None)
    finally:
        for proc in reversed(processes):
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=5)
        for log in logs:
            log.close()
        source.shutdown()
        source.server_close()
        if output != scratch:
            shutil.rmtree(scratch)
    print("{} failed checks".format(len(failures)), flush=True)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
