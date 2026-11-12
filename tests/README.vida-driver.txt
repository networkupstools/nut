VIDA driver regression tests
===========================

Configure an out-of-tree build following `docs/developers.txt`, with
`--with-drivers=nutdrv_vida --with-usb=libusb-1.0`. Run `make check` from
that build directory. No connected UPS, root access or installed NUT
configuration is needed. These tests never issue power-control commands.

`vida_utest` compiles the production driver and protocol parser, replacing
only the USB boundary and NUT publication callbacks. It checks:

* All published variables against explicit expected values, including their
  units and decimal formatting; no invented power or runtime measurements.
* All 256 status-bit combinations, including alarms, UPS type and beeper
  state, followed by a healthy sample to check clearing and repeated use.
* Ten device-acquisition failures with ownership and cleanup assertions,
  plus initialization failure and platforms without kernel detach support.
* Seven response failures at each of the four query positions. A partial
  poll must become stale without publishing a mixed sample, release the
  connection, and recover on a subsequent complete poll.
* Physical selectors, ambiguous matches, port paths, changed USB addresses,
  strict descriptor admission, malformed records and numeric boundaries.
* Literal captured responses in `vida-recordings.h`. These independent
  fixtures check decoding and parsing without synthesizing the input using
  production framing code. Synthetic frames exercise adversarial cases.

`vida-cli-test` runs the real driver main() and shutdown command handling
through `vida-no-usb-driver`, a non-installed executable whose USB enumeration
is always empty and whose open/transfer functions abort. It verifies a
failure exit status for both default and explicit unsupported shutdown
sequences. This check requires Python 3 and a POSIX account database.
The common `driver_shutdown_utest` separately checks framework return codes.

Compiler qualification should include separate GCC and Clang build trees
with `--enable-warnings=medium --enable-Werror`. Run `make distcheck-light`
before submission, particularly after changing source/distribution lists.
These are reproducible tests of production logic, not substitutes for
hardware validation of firmware, transport timing or electrical behavior.
