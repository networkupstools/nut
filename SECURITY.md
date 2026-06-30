# Security Policy

For end-users: see also the [Notes on securing NUT](docs/security.txt) chapter
in NUT documentation. Pay attention to file system permissions, not abusing
the ability to run daemons as a specified user account (especially `root`),
and try to ensure you set up (and then require) SSL protection of networked
communications.

Please do not shoot yourselves in the feet with simple mistakes!

## Supported Versions

The upstream Network UPS Tools (NUT) project deals with on-going development
of the code base. The numbered releases are snapshots of the `master` branch
with a special ritual to coordinate the release across several repositories,
finalize documentation, update version data, and assign a `git tag`.

* At this time there are no "sustaining" branches to track older releases
  nor to backport any urgent fixes into them. This may change in the future.

The upstream project does not currently promote any binary artifacts intended
for end-user consumption on most platforms.

* We do release source `dist` tarballs which can be used for builds without
  git and autotools generation. The ones published for releases are signed.
  They are typically used by packagers or their CI systems, and by end-users
  who build their own software, but for whatever reason chose to not track
  the newest upstream code (from git).
* Some NUT CI jobs do produce packages, primarily to cover the platforms where
  it happens and to make sure we track all deliverable files correctly -- so
  installation of those and possible conflicts with the rest of the system
  are up to the users who would risk to use them.
* There is a special case with NUT for Windows builds, where an archive of
  binaries built and pre-installed by AppVeyor CI is published and can be
  unpacked in any location chosen by end-users to evaluate the progress of
  N4W feature development. There are currently no (MSI) packages produced.

End-user distribution packages are the responsibility of the corresponding
Operating System, third-party packaging projects, or ultimate virtual or
physical appliance images which ship them. They typically involve older
upstream release snapshots (sometimes still a decade old), augmented with
packager-specific patches (whether to integrate with the requirements of the
target operating environment, or to backport some fixes from newer releases).

To address issues in such packages, including requests to backport fixes
from the upstream development, their maintainers and distributors should
be the primary point of contact.

## Reporting a Vulnerability

We welcome any reports which can help make the NUT ecosystem better and safer!

If you think you have found a security vulnerability, please *DO NOT* disclose
it publicly until we have had a chance to fix it, and coordinate updates with
whichever downstream projects we can reach. More specifically, please do not
report security vulnerabilities using GitHub issues or the mailing list!

To responsibly report a suspected or proven vulnerability with the NUT code
base, please use https://github.com/networkupstools/nut/security/advisories/new
to post a new report with the problem description, reproduction details,
estimated impact for end-users (ease of exploitation, danger to their
systems, NUT versions involved if possible). Suggestions about the root
cause and remedies are also welcome.

Please disclose if AI was involved in the discovery process or preparation
of the report, we prefer to keep track of how much the new technologies help,
and how much of the finished work is done/revised by a human.

It may be possible that we receive several reports on the same subject
before a solution and advisory are published, we will try our best to
figure out how to co-credit all reporters when time comes.

The project is maintained by enthusiasts without a commercial interest
in their spare time, so there is no bounty program (beside a heartfelt
thanks, and kudos in eventual attribution when the security advisory gets
published), nor any guaranteed speed of response, other than a best effort
to react as soon as possible.
