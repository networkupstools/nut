#!/usr/bin/env python3
"""Exercise upslog lifetime behaviour through its real executable and upsd."""

import sys

if sys.version_info < (3, 5):
    print("SKIP upslog-lifetime: Python 3.5 or newer is required")
    sys.exit(0)

import argparse
import getpass
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import tempfile
import time


def wait_for(predicate, seconds=10):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return True
        time.sleep(0.05)
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("builddir", type=Path)
    parser.add_argument("--exeext", default="")
    parser.add_argument("--serverless", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name == "nt" and not args.serverless:
        print("SKIP: multi-server integration requires concurrent upsd instances; "
              "Windows upsd is a singleton. Running serverless checks only.", flush=True)
        args.serverless = True
    build = args.builddir.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output.resolve()
    results, processes, logs = [], [], []
    scratch = tempfile.TemporaryDirectory(prefix="nit-upslog-")
    runtime = Path(scratch.name)
    env = os.environ.copy()
    for name in ("NUT_CONFPATH", "NUT_STATEPATH", "NUT_PIDPATH", "NUT_ALTPIDPATH"):
        env[name] = str(runtime)
    env["NUT_AUTHCONF_FILE"] = "none"
    env["LANG"] = env["LC_ALL"] = "C"
    env["TZ"] = "UTC"
    user = getpass.getuser()
    upslog = str(build / ("clients/upslog" + args.exeext))
    upsc = str(build / ("clients/upsc" + args.exeext))
    reserve = socket.socket()
    reserve.bind(("127.0.0.1", 0))
    refused = reserve.getsockname()[1]
    # Keep the port bound without listening throughout the serverless cases.
    def spec(name, port=refused):
        return "{}@127.0.0.1:{}".format(name, port)

    def canonical(text):
        return re.sub(r"@127\.0\.0\.1:[0-9]+", "@loopback", text)

    def observations(text):
        return {
            "records": [canonical(x) for x in text.splitlines()
                        if re.fullmatch(r"[^ @]+@127\.0\.0\.1:[0-9]+", x.strip())],
            "parsed": [canonical(x) for x in re.findall(r"Checking parse of '([^']+)'", text)],
            "logging": [canonical(x) for x in re.findall(r"logging status of (\S+) to", text)],
            "found": re.findall(r"FOUND: (\S+):", text),
            "queries": text.count("for currently served devices"),
        }

    def check(label, condition, details=None):
        item = {"case": label, "pass": bool(condition)}
        if details is not None:
            item["observed"] = details
        results.append(item)
        print("{}: {}".format("PASS" if condition else "FAIL", label), flush=True)

    def run(label, extra, names=None, parsed=None, code=0, error=None, interval="1", found=None):
        command = [upslog, "-F", "-D", "-u", user, "-p", str(runtime / label),
                   "-W", "1", "-d", "1", "-i", interval, "-f", "%UPSHOST%"] + extra
        result = subprocess.run(command, env=env, universal_newlines=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
        text = result.stdout + result.stderr
        (output / (label + ".log")).write_text(text)
        obs = observations(text)
        good = result.returncode == code
        if names is not None:
            good = good and obs["logging"] == [x + "@loopback" for x in names]
            good = good and obs["records"] == [x + "@loopback" for x in names]
        if parsed is not None:
            good = good and obs["parsed"] == [x + "@loopback" for x in parsed]
            good = good and obs["queries"] == parsed.count("*")
        if found is not None:
            good = good and obs["found"] == found
        if error is not None:
            good = good and error in text
        check(label, good, dict(exit_code=result.returncode, **obs))
        return text

    def server(label, names):
        location = runtime / label
        location.mkdir()
        server_env = env.copy()
        for key in ("NUT_CONFPATH", "NUT_STATEPATH", "NUT_PIDPATH", "NUT_ALTPIDPATH"):
            server_env[key] = str(location)
        (location / "ups.conf").write_text("".join(
            "[{}]\n driver = dummy-ups\n port = unused.dev\n".format(name) for name in names))
        (location / "upsd.users").write_text("")
        with socket.socket() as port_socket:
            port_socket.bind(("127.0.0.1", 0))
            port = port_socket.getsockname()[1]
        (location / "upsd.conf").write_text(
            "LISTEN 127.0.0.1 {}\nALLOW_NO_DEVICE true\n".format(port))
        log = (output / (label + ".log")).open("w")
        logs.append(log)
        proc = subprocess.Popen([str(build / ("server/upsd" + args.exeext)),
                                 "-F", "-D", "-u", user], env=server_env,
                                stdout=log, stderr=subprocess.STDOUT)
        processes.append(proc)
        def listed():
            reply = subprocess.run([upsc, "-l", "127.0.0.1:" + str(port)],
                                   env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   universal_newlines=True, timeout=3)
            return reply.returncode == 0 and set(reply.stdout.split()) == set(names)
        if not wait_for(listed):
            raise RuntimeError(label + " did not become ready")
        reply = subprocess.run([upsc, "-l", "127.0.0.1:" + str(port)],
                               env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               universal_newlines=True, check=True, timeout=3)
        return port, reply.stdout.split()

    try:
        run("legacy", ["-s", spec("one"), "-l", "-"], ["one"], ["one"])
        run("tuples", ["-m", spec("one") + ",-", "-m", spec("two") + ",-"],
            ["one", "two"], ["one", "two"])
        run("mixed", ["-m", spec("one") + ",-", "-s", spec("two"), "-l", "-"],
            ["one", "two"], ["one", "two"])
        shared = runtime / "shared.log"
        text = run("shared", ["-m", spec("one") + "," + str(shared),
                              "-m", spec("two") + "," + str(shared)])
        check("shared-file-records", shared.read_text().splitlines() == [spec("one"), spec("two")])
        check("failed-connection-teardown", text.count("FREEING:") == 2 and
              "Warning: initial connect failed" in text)
        run("missing-comma", ["-m", "one"], code=1, error="requires exactly 2 components")
        run("extra-component", ["-m", "one,-,extra"], code=1, error="requires exactly 2 components")
        run("empty-system", ["-m", ",-"], code=1, error="invalid UPS definition")
        run("missing-system-name", ["-m", "@127.0.0.1,-"], code=1, error="invalid UPS definition")
        if not args.serverless:
            normal, order = server("normal", ["alpha", "beta"])
            empty, _ = server("empty", [])
            single, _ = server("single", ["alpha"])
            reversed_order = list(reversed(order))
            wildcard = spec("*", normal)
            run("wildcard-head", ["-m", wildcard + ",-"], reversed_order,
                ["*"] + reversed_order[1:], found=order)
            run("wildcard-middle", ["-m", spec("one", normal) + ",-", "-m", wildcard + ",-"],
                ["one"] + reversed_order, ["one", "*"] + reversed_order, found=order)
            run("empty-only", ["-m", spec("*", empty) + ",-"], code=1,
                error="Queries failed to find anything")
            run("empty-then-system", ["-m", spec("*", empty) + ",-",
                                      "-m", spec("one", normal) + ",-"], ["one"], ["*"])
            # Characterise inherited list loss; a separate intentional wildcard fix
            # should update these expectations.
            run("stale-predecessor", ["-m", spec("*", empty) + ",-",
                                       "-m", spec("one", normal) + ",-",
                                       "-m", spec("*", empty) + ",-"], parsed=["*", "*"],
                code=1, error="Queries failed to find anything", found=[])
            run("stale-discovered", ["-m", spec("*", single) + ",-",
                                      "-m", spec("*", empty) + ",-"], parsed=["*", "*"],
                code=1, error="Queries failed to find anything", found=["alpha"])
            text = run("connected-teardown", ["-s", spec("alpha", normal), "-l", "-"],
                       ["alpha"], ["alpha"])
            check("connected-teardown-order", "loops have elapsed" in text and
                  "We logged out" in text and "FREEING:" in text and
                  text.index("loops have elapsed") < text.index("We logged out") <
                  text.index("FREEING:"))
            text = run("connected-disconnected", ["-s", spec("alpha", normal), "-l", "-"],
                       ["alpha"], ["alpha"], interval="31")
            check("disconnect-before-freeing", "We logged out" in text and
                  text.index("We logged out") < text.index("FREEING:"))
            if not args.exeext and os.name == "posix" and hasattr(signal, "SIGHUP"):
                target = runtime / "rotating.log"
                old = runtime / "rotating.old"
                log = (output / "rotation.log").open("w")
                logs.append(log)
                proc = subprocess.Popen([upslog, "-F", "-D", "-u", user, "-W", "1", "-i", "1",
                                         "-p", str(runtime / "rotate"), "-f", "%UPSHOST%",
                                         "-m", spec("alpha", normal) + "," + str(target),
                                         "-m", spec("beta", normal) + "," + str(target)],
                                        env=env, stdout=log, stderr=subprocess.STDOUT)
                processes.append(proc)
                def written():
                    return target.exists() and {spec("alpha", normal), spec("beta", normal)}.issubset(
                        target.read_text().splitlines())
                check("rotation-ready", wait_for(written))
                descriptors = Path("/proc/{}/fd".format(proc.pid))
                if descriptors.exists():
                    count = sum(1 for fd in descriptors.iterdir()
                                if os.path.realpath(str(fd)) == str(target))
                    check("shared-single-descriptor", count == 1, count)
                target.rename(old)
                proc.send_signal(signal.SIGHUP)
                check("rotation-new-records", wait_for(written))
                check("rotation-old-preserved", len(old.read_text().splitlines()) >= 2)
                proc.terminate()
                check("graceful-termination", proc.wait(timeout=5) == 0)
    finally:
        reserve.close()
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
        scratch.cleanup()
        (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    return int(any(not case["pass"] for case in results))


if __name__ == "__main__":
    raise SystemExit(main())
