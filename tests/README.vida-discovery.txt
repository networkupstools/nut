VIDA discovery and configuration regression tests
=================================================

See `README.vida-driver.txt` for native driver tests. On Linux, configure
with the VIDA driver, libusb 1.0, systemd unit installation and Python 3.8
or newer. `make check` then also runs:

* `vida-discovery-test`: temporary sysfs and configuration fixtures exercise
  zero, one and multiple devices, moved ports, disconnect/reconnect, cached
  identity, malformed state, administrator edits, locking, permissions,
  and failure/recovery during atomic state and configuration publication.
* `vida-service-test`: the real systemd manager in dry-run mode resolves
  the packaged units and drop-ins. It checks initial discovery ordering
  and the shared environment file with existing driver instances and with
  an empty inventory, for both enumerator modes. Removing packaged drop-ins
  must fail the assertions. This test requires an available systemd binary
  and a non-root account; otherwise unittest explicitly reports a skip.
  It never starts units, runs unit commands, or changes host services.
* `upsconf-override-test`: the real `upsdrvctl` and shell enumerator consume
  default, empty, explicit and missing `NUT_UPSCONF` paths. This generic test
  requires Python 3 on POSIX and remains enabled without VIDA discovery or USB.
* `upsconf_maxpath_utest`: the C parser rejects an oversized override using
  the platform's actual `NUT_PATH_MAX`, without depending on exec environment
  limits, Python, USB or discovery.

Run `make check-vida-integration` for `make check` plus the configure matrix.
The matrix uses private build directories and inherits the current compiler
and dependency selection. It checks the actual generated installation lists
for the helper, service, timer and all consumer drop-ins in these cases:
selected VIDA driver, other driver, explicitly disabled discovery, missing
systemd, missing Python, rejected Python minimum-version probe, missing USB,
and explicit activation with unmet prerequisites. With pkg-config available
it also checks non-Linux host admission using the native toolchain; this is
not a cross-platform runtime test. The disabled-discovery and no-USB cells
also build and execute the generic configuration regressions.

The version-probe shim tests rejection only. Minimum-version compatibility
must additionally use a real Python 3.8 interpreter, for example by setting
both `--with-python=/path/to/python3.8` and
`--with-python3=/path/to/python3.8` in a separate build and running the tests.
Keep a second build with a current Python 3 interpreter. Do not claim Python
3.8 qualification from a patched version string or syntax inspection.

The existing `ci_build.sh` entry point accepts
`BUILD_TYPE=default-tgt:check-vida-integration` on a Linux runner with the
discovery prerequisites. No privileged runner or hardware is required.
Systemd graph checks must run as a non-root user to qualify that
part of the matrix. Run `make distcheck-light` to check archive completeness.

These tests complement physical unplug/replug and cold-start qualification.
Fixtures cannot prove kernel USB notification delivery, real service startup,
or admission of a firmware/report-descriptor variant not actually observed.
