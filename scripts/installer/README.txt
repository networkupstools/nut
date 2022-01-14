This directory contains scripts and data used for NUT packaging
marketed earlier as Eaton IPSS Unix (or IPP for Unix, or UPP),
a freely available download. Most of the work was done on behalf
of Eaton by Frederic Bohe, Vaclav Krpec, Arnaud Quette and Jim Klimov.

This includes the package (tarball) creation script which relies on
presence of third-party library binaries in a $ARCH/libs directory,
and init-scripts from NUT source tree (originally expected as a "nut"
subdirectory), as well as an interactive installer script to set up
the package on a target deployment covering package (re-)installation,
initial device discovery, password setup, etc., and helper scripts
for status overview and shutdown handling.

The installer relied on "nutconf" tool (emulating dummy script for
tests provided here), which was added to NUT sources.

Note that heavy use of LD_LIBRARY_PATH in these scripts may become
counterproductive when the package is installed on a system that is
too many versions away from the intended target (e.g. mixing up the
symbols during dynamic linking), so while this contribution here is
published "as is", more work would be needed to make it useful in
modern environments. Helper scripts should be quickly useful though.
