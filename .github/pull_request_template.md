<!-- Comment:
* Please revise the docs/developers.txt for coding style suggestions and
  other considerations applicable to NUT codebase contributions, as well
  as for which text documents to update. See also docs/developer-guide.txt
  for general points on NUT architecture and design.

* Please note that we require "Signed-Off-By" tags in each Git Commit
  message, to conform to the common DCO (Developer Certificate of Origin)
  as posted in LICENSE-DCO at root of NUT codebase as well as published
  at https://developercertificate.org/

* The checklist below is more of a reminder of steps to take and "dangers"
  to look out for. PRs to update this template are also welcome :)

* Local build iterations can be augmented with the ci_build.sh script.
-->

## General points

- [ ] Described the changes in the PR submission or a separate issue, e.g.
  known published or discovered protocols, applicable hardware (expected
  compatible and actually tested/developed against), limitations, etc.

- [ ] There may be multiple commits in the PR, aligned and commented with
  a functional change. Notably, coding style changes better belong in a
  separate PR, but certainly in a dedicated commit to simplify reviews
  of "real" changes in the other commits. Similarly for typo fixes in
  comments or text documents.

- [ ] Please star NUT on GitHub, this helps with sponsorships! ;)

## Frequent "underwater rocks" for driver addition/update PRs

- [ ] Revised existing driver families and added a sub-driver if applicable
  (`nutdrv_qx`, `usbhid-ups`...) or added a brand new driver in the other
  case.

- [ ] Did not extend obsoleted drivers with new hardware support features
  (notably `blazer` and other single-device family drivers for Qx protocols,
  except the new `nutdrv_qx` which should cover them all).

- [ ] For updated existing device drivers, bumped the `DRIVER_VERSION` macro
  or its equivalent.

<!-- Comment:
  Some sub-drivers have `SUBDRIVER_VERSION` or customized names like
  e.g. `MEGATEC_VERSION` in `drivers/nutdrv_qx_megatec.c`
-->

- [ ] For USB devices (HID or not), revised that the driver uses unique
  VID/PID combinations, or raised discussions when this is not the case
  (several vendors do use same interface chips for unrelated protocols).

- [ ] For new USB devices, built and committed the changes for the
  `scripts/upower/95-upower-hid.hwdb` file

- [ ] Proposed NUT data mapping is aligned with existing `docs/nut-names.txt`
  file. If the device exposes useful data points not listed in the file, the
  `experimental.*` namespace can be used as documented there, and discussion
  should be raised on the NUT Developers mailing list to standardize the new
  concept.

- [ ] Updated `data/driver.list.in` if applicable (new tested device info)
<!-- Comment:
Also note below, a point about PR posting for NUT DDL
-->

## Frequent "underwater rocks" for general C code PRs

- [ ] Did not "blindly assume" default integer type sizes and value ranges,
  structure layout and alignment in memory, endianness (layout of bytes and
  bits in memory for multi-byte numeric types), or use of generic `int` where
  language or libraries dictate the use of `size_t` (or `ssize_t` sometimes).

<!-- Comment:
* NOTE: Casting and/or pragmas (support detected at compile time,
  see `m4/ax_c_pragmas.m4`) to silence warnings may be acceptable,
  but only if coupled with range checks or similar actions.
-->

- [ ] Progress and errors are handled with `upsdebugx()`, `upslogx()`,
  `fatalx()` and related methods, not with direct `printf()` or `exit()`.
  Similarly, NUT helpers are used for error-checked memory allocation and
  string operations (except where customized error handling is needed,
  such as unlocking device ports, etc.)

- [ ] Coding style (including whitespace for indentations) follows precedent
  in the code of the file, and examples/guide in `docs/developers.txt` file.

- [ ] For newly added files, the `Makefile.am` recipes were updated and the
  `make distcheck` target passes.

## General documentation updates

- [ ] Updated `docs/acknowledgements.txt` (for vendor-backed device support)

- [ ] Added or updated manual page information in `docs/man/*.txt` files
  and corresponding recipe lists in `docs/man/Makefile.am` for new pages

- [ ] Passed `make spellcheck`, updated spell-checking dictionary in the
  `docs/nut.dict` file if needed (did not remove any words -- the `make`
  rule printout in case of changes suggests how to maintain it).

## Additional work may be needed after posting this PR

- [ ] Propose a PR for NUT DDL with detailed device data dumps from tests
  against real hardware (the more models, the better).

- [ ] Address NUT CI farm build failures for the PR: testing on numerous
  platforms and toolkits can expose issues not seen on just one system.

<!-- Comment:
* One frequent "offence" is the appearance of unexpected (not git-ignored)
  or modification during build of files tracked in Git.

* Another frequent issue is not tracking newly introduced file names in
  `EXTRA_DIST` of the `Makefile.am` (and for `*.in` templates -- of rules
  in the `configure.ac` script) so the `make distcheck` fails.

* Avoid using GNU-specific constructs in the `Makefile.am`, even if that
  means cumbersome ways to build a target. This should not happen in mere
  driver updates, however.

* Also some third-party libraries or OS headers and method argument types
  and counts can differ -- necessitating m4 code for `configure` script
  probing, and `ifdef`, `typedef`, etc. in C code to adapt to the build
  environment (precedents available in NUT codebase). In extreme cases,
  you may need to spin up a VM or container to reproduce those issues
  and iterate on a fix locally; see `docs/config-prereqs.txt` and
  `docs/ci-farm-lxc-setup.txt` for notes taken during preparation of
  the multi-platform NUT CI farm.
-->

- [ ] Revise suggestions from LGTM.COM analysis about "new issues" with
  the changed codebase.

<!-- Comment:
  Take them with a grain of salt, especially with regard to things like
  architecture-dependent range checks, but many of the complaints from
  the tool are indeed useful.
-->
