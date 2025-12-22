#
# spec file for package nut.spec
#
# Copyright (c) 2015 SUSE LINUX GmbH, Nuernberg, Germany.
# Copyright (c) 2016-2018 Eaton EEIC.
# Copyright (c) 2025 by Jim Klimov <jimklimov+nut@gmail.com>
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

# NOTE: Evaluations in percent-round-parentheses below happen in the
# build area already populated with packages according to "Required:"
# lines. We can test for OS feature availability here if needed (e.g.
# to decide if we deliver a sub-package with certain dependencies),
# but can not decide that we can/must install something and then we
# would have the needed OS capability.

%define LIBEXECPATH	%{_libexecdir}/ups

# Requires httpd(-devel?) or apache2(-devel?) to be present in this distro:
%define apache_serverroot_data %(%{_sbindir}/apxs2 -q datadir || %{_sbindir}/apxs -q PREFIX || true)
# FIXME: is naming correct for both versions?
%define apache_serverroot_cgi %(%{_sbindir}/apxs2 -q cgidir || %{_sbindir}/apxs -q cgidir || true)

%if "0%{?apache_serverroot_cgi}" == "0" || 0%(echo '%{apache_serverroot_cgi}' | grep -E '^%{_datadir}' >/dev/null && echo 1 || echo 0) > 0
# Spec-var is undefined or empty, or matches the pattern triggering
#   E: arch-dependent-file-in-usr-share (Badness: 590)
# Dump nut-cgi artifacts under our own locations, so end-users can
# integrate them later.
%define CGIPATH		%{LIBEXECPATH}/cgi-bin
%else
%define CGIPATH		%{apache_serverroot_cgi}/nut
%endif

%if "0%{?apache_serverroot_data}" == "0" || 0%(test x'%{apache_serverroot_data}' = x'^%{_datadir}' && echo 1 || echo 0) > 0
%define HTMLPATH	%{_datadir}/nut/htdocs
%else
# Rename web pages location to not conflict with apache2-example-pages
# or user home page:
%define HTMLPATH	%{apache_serverroot_data}/nut
%endif

%define MODELPATH	%{LIBEXECPATH}/driver
%define STATEPATH	%{_localstatedir}/lib/ups
### Note: this is /etc/nut in Debian version
%define CONFPATH	%{_sysconfdir}/ups
# RPM on OpenSUSE goes:
#   DOCDIR=/home/abuild/rpmbuild/BUILD/nut-2.8.4.428-build/BUILDROOT/usr/share/doc/packages/nut
%define DOCPATH		%{_docdir}/nut

### FIXME: Detect properly?
# W: suse-filelist-forbidden-udev-userdirs /etc/udev/rules.d/62-nut-usbups.rules is not allowed in SUSE
#    This directory is for user files, use /usr/lib/udev/rules.d
%define UDEVRULEPATH	%(test -d /usr/lib/udev && echo /usr/lib/udev || echo "%{_sysconfdir}/udev")

### FIXME: Detect properly?
# W: suse-filelist-forbidden-bashcomp-userdirs /etc/bash_completion.d/nut.bash_completion is not allowed in SUSE
#    This directory is for user files, use /usr/share/bash-completion/completions/
%define BASHCOMPLETIONPATH	%(test -d /usr/share/bash-completion/completions && echo /usr/share/bash-completion/completions || echo "%{_sysconfdir}/bash_completion.d")

%define NUT_USER		upsd
%define NUT_GROUP		daemon
%define LBRACE (
%define RBRACE )
%define QUOTE "
%define BACKSLASH \\
# Collect all devices listed in ups-nut-device.fdi:
%define USBHIDDRIVERS    %(zcat %{SOURCE0} | tr a-z A-Z | fgrep -a -A1 USBHID-UPS | sed -n 's/.*ATTR{IDVENDOR}==%{QUOTE}%{BACKSLASH}%{LBRACE}[^%{QUOTE}]*%{BACKSLASH}%{RBRACE}%{QUOTE}, ATTR{IDPRODUCT}==%{QUOTE}%{BACKSLASH}%{LBRACE}[^%{QUOTE}]*%{BACKSLASH}%{RBRACE}%{QUOTE}, MODE=.*/modalias%{LBRACE}usb:v%{BACKSLASH}1p%{BACKSLASH}2d*dc*dsc*dp*ic*isc*ip*%{RBRACE}/p' | tr '%{BACKSLASH}n' ' ')
%define USBNONHIDDRIVERS %(zcat %{SOURCE0} | tr a-z A-Z | fgrep -a -A1 _USB       | sed -n 's/.*ATTR{IDVENDOR}==%{QUOTE}%{BACKSLASH}%{LBRACE}[^%{QUOTE}]*%{BACKSLASH}%{RBRACE}%{QUOTE}, ATTR{IDPRODUCT}==%{QUOTE}%{BACKSLASH}%{LBRACE}[^%{QUOTE}]*%{BACKSLASH}%{RBRACE}%{QUOTE}, MODE=.*/modalias%{LBRACE}usb:v%{BACKSLASH}1p%{BACKSLASH}2d*dc*dsc*dp*ic*isc*ip*%{RBRACE}/p' | tr '%{BACKSLASH}n' ' ')

# Collect systemd related paths so we can package files there:
%define systemdsystemunitdir %(pkg-config --variable=systemdsystemunitdir systemd)
%define systemdsystempresetdir %(pkg-config --variable=systemdsystempresetdir systemd || pkg-config --variable=systemdsystempresetdir libsystemd)
%define systemdtmpfilesdir %(pkg-config --variable=tmpfilesdir systemd || pkg-config --variable=tmpfilesdir libsystemd)
%define systemdsystemdutildir %(pkg-config --variable=systemdutildir systemd)
%define systemdshutdowndir %(pkg-config --variable=systemdshutdowndir systemd)

# % define NUT_SYSTEMD_UNITS_SERVICE_TARGET	% (cd scripts/systemd && ls -1 *.{service,target,path,timer}{,.in} | sed 's,.in$,,' | sort | uniq)
%define NUT_SYSTEMD_UNITS_SERVICE_TARGET	nut-driver-enumerator.service nut-driver.target nut-driver@.service nut-logger.service nut-monitor.service nut-server.service nut-sleep.service nut-udev-settle.service nut.target nut-driver-enumerator.path
# Most deployments do not want these by default:
%define NUT_SYSTEMD_UNITS_UNCOMMON_NDE	nut-driver-enumerator-daemon-activator.path nut-driver-enumerator-daemon-activator.service nut-driver-enumerator-daemon.service

%define NUT_SYSTEMD_UNITS_PRESET	%(cd scripts/systemd && ls -1 *.preset{,.in} | sed 's,.in$,,' | sort | uniq)

# Not all distros have certain packages (or their equivalents/aliases),
# NOTE: No use searching remote repos for what might be or not be available
# there; we have to use rpm queries based on whatever did get installed
# according to Requires lines below (in turn according to our declaration
# of what is shipped by this or that distro/release), to decide whether we
# deliver certain sub-packages - and set NUTPKG_WITH_<DEPNAME> at that time.
# For version-specific checks note that some are directly digited, others
# are off by two or four digits (e.g. 0810 = a "8.10" release), see
# https://en.opensuse.org/openSUSE:Build_Service_cross_distribution_howto

# Does this NUT branch have DMF feature code?
%define NUTPKG_WITH_DMF	%( test -d scripts/DMF && echo 1 || echo 0 )

# FIXME: Find a smarter way to set those from main codebase recipes...
# Something like `git grep 'version-info' '*.am'` ?
%define SO_MAJOR_LIBUPSCLIENT	7
%define SO_MAJOR_LIBNUTCLIENT	2
%define SO_MAJOR_LIBNUTCLIENTSTUB	1
%define SO_MAJOR_LIBNUTSCAN	4
%define SO_MAJOR_LIBNUTCONF	0

# If not published, nutconf is built with a statically linked library variant
%define NUTPKG_WITH_LIBNUTCONF	0

Name:           nut
# NOTE: OBS should rewrite this:
Version:        2.8.4
Release:        1
Summary:        Network UPS Tools Core (Uninterruptible Power Supply Monitoring)
License:        GPL-2.0+
Group:          Hardware/UPS
Url:            https://www.networkupstools.org/
Source0:        %{name}-%{version}.tar.gz

Requires:       %{_bindir}/bash
Requires:       %{_bindir}/sh
Requires:       %{_sbindir}/sh
Requires:       %{_bindir}/chown
Requires:       %{_bindir}/chgrp
Requires:       %{_bindir}/chmod
###BuildRequires:  % {_sbindir}/chroot
Requires:       %{_bindir}/rm
Requires:       %{_bindir}/fgrep
Requires:       %{_bindir}/grep
Requires:       %{_bindir}/pgrep
Requires:       %{_bindir}/pkill
Requires:       %{_bindir}/readlink
Requires:       usbutils
#%if 0 % {?suse_version}
Requires(post): udev
#%endif
#Requires(post): group(% {NUT_GROUP})
#Requires(post): user(% {NUT_USER})
Requires(postun):	%{_bindir}/sh

BuildRoot:      %{_tmppath}/%{name}-%{version}-build

Recommends:	logrotate

# To fix end-of-line encoding:
BuildRequires:  dos2unix

%if ! 0%{?rhel_version}
# For man page aliases
# https://en.opensuse.org/openSUSE:Packaging_Conventions_RPM_Macros#fdupes
BuildRequires:  fdupes
%endif

%if ( ! 0%{?rhel_version} )  &&  ( ! 0%{?rhel} )
# Not sure why claimed absent in RHEL7 (even with Fedora/EPEL repo layer added)
%define NUTPKG_WITH_AVAHI	1
BuildRequires:  avahi-devel
%else
%define NUTPKG_WITH_AVAHI	0
%endif

%if ( ! 0%{?rhel_version} )  &&  ( ! 0%{?rhel} )  &&  ( 0%{?sle_version}>=150000 || ! 0%{?sle_version} )  &&  ( 0%{?suse_version}>=1300 || ! 0%{?suse_version} )
# Not sure why claimed absent in RHEL (even with Fedora/EPEL repo layer added)
%define NUTPKG_WITH_FREEIPMI	1
BuildRequires:  (libfreeipmi-devel or freeipmi-devel)
%else
%define NUTPKG_WITH_FREEIPMI	0
%endif

%if ( 0%{?rhel_version}>=800 || ! 0%{?rhel_version} )  &&  ( 0%{?rhel}>=8 || ! 0%{?rhel} )
# Not in RHEL7
%define NUTPKG_WITH_LIBGD	1
BuildRequires:  gd-devel
%else
%define NUTPKG_WITH_LIBGD	0
%endif

BuildRequires:  gcc-c++
BuildRequires:  libtool

%if ( 0%{?rhel_version}>=800 || ! 0%{?rhel_version} )  &&  ( 0%{?rhel}>=8 || ! 0%{?rhel} )
# Not in RHEL7
%define NUTPKG_WITH_LTDL	1
BuildRequires:  libtool-ltdl-devel
%else
%define NUTPKG_WITH_LTDL	0
%endif

# libusb-0.1 or libusb-1.0:
BuildRequires:  (libusb-devel or libusbx-devel)
#!Prefer:       libusbx-devel
BuildRequires:  net-snmp-devel
BuildRequires:  libxml2-devel
BuildRequires:  pkg-config
# Maybe older Pythons are also okay, but were not tested for ages
BuildRequires:  (python >= 2.6 or python3 or python2)

%if 0%{?NUTPKG_WITH_DMF}
# Toggle decided above based on variant of NUT source codebase
# LUA 5.1 or 5.2 is known ok for us, both are modern in current distros (201609xx)
BuildRequires:  lua-devel

# TODO: Make sure how this is named to use in CentOS/RHEL (may be not in core but EPEL repos)
# The pycparser is required to rebuild DMF files, but those pre-built
# copies in the git repo/tarball "should" be in sync with original
# C files, so we don't require regeneration for packaging. Also the
# Jenkins NUT-master job should have verified this.
#BuildRequires:  python-pycparser
%endif

# Variant names in different distros
# For some platforms we may have to fiddle with distro-named macros like
#   % if 0 % {?centos_version}
#   % if 0 % {?suse_version}
#   % if 0 % {?rhel_version}>=700
# and whatnot

%if ( (0%{?rhel_version}>0 && 0%{?rhel_version}<800) || ! 0%{?rhel_version} )  &&  ( (0%{?centos_version}>0 && 0%{?centos_version}!=800) || ! 0%{?centos_version} )
# Strange that this is not present in RHEL (even with Fedora EPEL repos attached)
# and that it is resolvable in CentOS 7, 9, 10 but not 8...
# We only need this to learn paths from apxs tool, so no NUTPKG_WITH_* toggle
BuildRequires:  (httpd-devel or apache2-devel)
%endif

BuildRequires:  (dbus-1-glib-devel or dbus-glib-devel)

%if ( ! 0%{?rhel_version} )  &&  ( ! 0%{?rhel} )
# Strange that this is not present in RHEL (even with Fedora EPEL repos attached)
BuildRequires:  (libcppunit-devel or cppunit-devel)
%endif

# Obsoleted/away in newer distros
%if ( (0%{?rhel_version}>0 && 0%{?rhel_version}<800) || ! 0%{?rhel_version} )  &&  ( (0%{?centos_version}>0 && 0%{?centos_version}<800) || ! 0%{?centos_version} )  &&  ( (0%{?fedora_version}>0 && 0%{?fedora_version}<=27) || ! 0%{?fedora_version} )
# Note that per https://en.opensuse.org/openSUSE:Build_Service_cross_distribution_howto
# there was "fedora_version" until some time before 36, when the macro became "fedora";
# similarly for "rhel_version <= 700" vs. "rhel == 8"...
%define NUTPKG_WITH_TCPWRAP	1
BuildRequires:  (tcpd-devel or tcp_wrappers-devel)
%else
%define NUTPKG_WITH_TCPWRAP	0
%endif

# May be plain "neon" and "libusb" in RHEL7 or older?
# OBS: This may need `osc meta prjconf` to `Prefer:` one
# certain variant in case several hits happen on a builder:
BuildRequires:  (libneon-devel or neon-devel or neon)
#!Prefer:       (libneon-devel or neon-devel)
BuildRequires:  (libopenssl-devel or openssl-devel or openssl)
#!Prefer:       (libopenssl-devel or openssl-devel)

%if ( ! 0%{?rhel_version} )  &&  ( ! 0%{?rhel} )  &&  ( (0%{?centos_version}>0 && 0%{?centos_version}<1000) || ! 0%{?centos_version} )  &&  ( (0%{?fedora_version}>0 && 0%{?fedora_version}<4200) || ! 0%{?fedora_version} )  &&  ( (0%{?fedora}>0 && 0%{?fedora}<42) || ! 0%{?fedora} )
# Strange that this is not present in RHEL (even with Fedora EPEL repos attached)
# NOTE: fedora_version=99 seems to be rawhide; currently it says this package
# is not known (so likely a post-42 release would be more specific later),
# CentOS10 also does not like the name.
%define NUTPKG_WITH_POWERMAN	1
BuildRequires:  powerman-devel
%else
%define NUTPKG_WITH_POWERMAN	0
%endif

%if ( 0%{?suse_version}>0 || ! 0%{?suse_version} )  &&  (0%{?centos_version}>=800 || ! 0%{?centos_version} )  &&  ( ! 0%{?rhel_version} )  &&  ( ! 0%{?rhel} )
# Strange that this is not present in RHEL (even with Fedora EPEL repos attached)
# But it also complains about epel-rpm-macros when this is added though.
BuildRequires:  systemd-rpm-macros
%endif

%if ( 0%{?rhel_version}>=800 || ! 0%{?rhel_version} )  &&  ( 0%{?rhel}>=8 || ! 0%{?rhel} )
# Only needed for PDF generation, we do not package that now
#BuildRequires:  dblatex

BuildRequires:  (libxslt-tools or libxslt)
BuildRequires:  asciidoc
%endif

%if 0%{?opensuse_version}
# Package provides driver for USB HID UPSes, but people can live with hal addon:
Enhances:       %{USBHIDDRIVERS}
# Package provides the only avalailable driver for other USB UPSes:
Supplements:    %{USBNONHIDDRIVERS}
%systemd_requires
%endif

%description
Core package of Network UPS Tools.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

%package drivers-net
Summary:        Network UPS Tools - Extra Networking Drivers (for Network Monitoring)
Group:          Hardware/UPS
Requires:       %{name} = %{version}

%description drivers-net
Networking drivers for the Network UPS Tools. You will need them
together with nut to provide UPS networking support.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

%package -n libupsclient%{SO_MAJOR_LIBUPSCLIENT}
Summary:        Network UPS Tools Library (Uninterruptible Power Supply Monitoring)
Group:          System/Libraries

%description -n libupsclient%{SO_MAJOR_LIBUPSCLIENT}
Shared library for the Network UPS Tools, used by its and third-party C clients.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

%package -n libnutclient%{SO_MAJOR_LIBNUTCLIENT}
Summary:        Network UPS Tools Library (Uninterruptible Power Supply Monitoring)
Group:          System/Libraries

%description -n libnutclient%{SO_MAJOR_LIBNUTCLIENT}
Shared library for the Network UPS Tools, used by its and third-party C++ clients.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

%package -n libnutclientstub%{SO_MAJOR_LIBNUTCLIENTSTUB}
Summary:        Network UPS Tools Library (Uninterruptible Power Supply Monitoring)
Group:          System/Libraries

%description -n libnutclientstub%{SO_MAJOR_LIBNUTCLIENTSTUB}
Shared stub library for the Network UPS Tools with memory-backed configurations,
primarily used by tests and mocks with its and third-party C++ clients.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

%if 0%{?NUTPKG_WITH_LTDL}
%package -n libnutscan%{SO_MAJOR_LIBNUTSCAN}
Summary:        Network UPS Tools Library (Uninterruptible Power Supply Monitoring)
Group:          System/Libraries

%description -n libnutscan%{SO_MAJOR_LIBNUTSCAN}
Shared library for the Network UPS Tools, used by its nut-scanner and nutconf tools,
and possibly third-party C clients, integrations or tools.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

%if 0%{?NUTPKG_WITH_LIBNUTCONF} > 0
# If not published, nutconf is built with a statically linked library variant
%package -n libnutconf%{SO_MAJOR_LIBNUTCONF}
Summary:        Network UPS Tools Library (Uninterruptible Power Supply Monitoring)
Group:          System/Libraries

%description -n libnutconf%{SO_MAJOR_LIBNUTCONF}
Shared library for the Network UPS Tools, used by its nutconf tool,
and possibly third-party C++ clients, integrations or tools.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.
%endif
%endif

%if 0%{?NUTPKG_WITH_LIBGD}
%package cgi
Summary:        Network UPS Tools Web Server Support (UPS Status Pages)
Group:          Hardware/UPS
Requires:       %{name} = %{version}

%description cgi
Web server support package for the Network UPS Tools.

Predefined URL is http://localhost/nut/index.html

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.
%endif

%package monitor
Summary:        Network UPS Tools Web Server Support (GUI client)
Group:          Hardware/UPS
Requires:       %{name} = %{version}
Requires:       python-base
BuildRequires:  (python >= 2.6 or python3 or python2)
BuildArch:      noarch

%description monitor
Graphical user interface client for the Network UPS Tools,
written in Python.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

%package devel
Summary:        Network UPS Tools (Uninterruptible Power Supply Monitoring)
Group:          Development/Libraries/C and C++
Requires:       %{name} = %{version}
Requires:       openssl-devel

%description devel
Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

%prep
%setup -q

# Note: NOT configure macro, due to override of --sysconfdir and --datadir
# values just for the recipe part but not for whole specfile
%build
# May be pre-populated if building from tarball, or derived from git
# Otherwise let the SPEC version reflect in NUT self-identification
if [ ! -s VERSION_DEFAULT ] && [ ! -e .git ] && [ -n '%{version}' ] ; then
	echo NUT_VERSION_DEFAULT='%{version}' > VERSION_DEFAULT
fi

sh autogen.sh
./configure --disable-static --with-pic \
	--prefix=%{_prefix}\
	--bindir=%{_bindir}\
	--sbindir=%{_sbindir}\
	--libdir=%{_libdir}\
	--libexecdir=%{LIBEXECPATH}\
	--sysconfdir=%{CONFPATH}\
	--datadir=%{_datadir}/nut\
	--docdir=%{DOCPATH}\
	--with-ssl --with-openssl\
	--with-libltdl=yes\
	--with-cgi=auto\
	--with-serial\
	--with-usb\
	--with-snmp\
	--with-neon\
	--with-dev\
	--with-ipmi=auto\
	--with-powerman=auto\
	--with-doc=man=dist-auto\
	--with-htmlpath=%{HTMLPATH}\
	--with-cgipath=%{CGIPATH}\
	--with-statepath=%{STATEPATH}\
	--with-drvpath=%{MODELPATH}\
	--with-user=%{NUT_USER}\
	--with-group=%{NUT_GROUP} \
	--with-udev-dir=%{UDEVRULEPATH} \
	--enable-option-checking=fatal\
	--with-systemdsystemunitdir --with-systemdshutdowndir \
	--with-augeas-lenses-dir=/usr/share/augeas/lenses/dist \
%if 0%{?NUTPKG_WITH_LIBNUTCONF} > 0
	--with-dev-libnutconf\
%endif
%if 0%{?NUTPKG_WITH_DMF}
	--with-snmp_dmf_lua\
	--with-dmfsnmp-regenerate=no --with-dmfnutscan-regenerate=no --with-dmfsnmp-validate=no --with-dmfnutscan-validate=no\
%endif
	--enable-keep_nut_report_feature\
	--enable-strip\
	--enable-check-NIT

### via Make now ### (cd tools; python nut-snmpinfo.py)

make %{?_smp_mflags}
PORT=$(sed -n 's/#define PORT //p' config.log)
if test "$PORT" = 3493 ; then
    PORT=nut
fi

%check
make %{?_smp_mflags} check

%install
make DESTDIR=%{buildroot} install %{?_smp_mflags}
mkdir -p %{buildroot}%{STATEPATH}/upssched
# SuSE rc
mkdir -p %{buildroot}%{_sbindir}
mkdir -p %{buildroot}%{_sysconfdir}/logrotate.d
# Avoid W: incoherent-logrotate-file /etc/logrotate.d/nutlogd
install -m 644 scripts/logrotate/nutlogd %{buildroot}%{_sysconfdir}/logrotate.d/nut
# As (currently) hard-coded in that file above
mkdir -p %{buildroot}/var/log
rename .sample "" %{buildroot}%{_sysconfdir}/ups/*.sample
find %{buildroot} -type f -name "*.la" -delete -print
mkdir -p %{buildroot}%{BASHCOMPLETIONPATH}
install -m0644 scripts/misc/nut.bash_completion %{buildroot}%{BASHCOMPLETIONPATH}/
install -m0755 scripts/subdriver/gen-snmp-subdriver.sh %{buildroot}%{_sbindir}/
# TODO: Detect path from chosen interpreter or NUT build config files?
# Avoid W: non-executable-script /usr/lib/python3.6/site-packages/PyNUT.py 644 /usr/bin/python...
# Not really relevant for the module (not directly runnable, but has the shebang just in case)
chmod +x %{buildroot}/usr/lib*/python*/*-packages/*.py
if [ x"%{systemdtmpfilesdir}" != x ]; then
    # Deliver these dirs by packaging:
    sed 's,^\(. %{STATEPATH}\(/upssched\)*\( .*\)*\)$,#PACKAGED#\1,' -i %{buildroot}%{systemdtmpfilesdir}/nut-common-tmpfiles.conf
fi
# Use deterministic script interpreters:
find %{buildroot} -type f -name '*.sh' -o -name '*.py' -o -name '*.pl' | \
while read F ; do
    if head -1 "$F" | grep bin/env >/dev/null ; then
        F_SHEBANG="`head -1 \"$F\"`"

        F_SHELL_SHORT="`echo \"$F_SHEBANG\" | sed -e 's,^.*bin/env *,,' -e 's, .*$,,'`" \
        && [ -n "$F_SHELL_SHORT" ] \
        || { echo "WARNING: Failed to extract an interpreter from shebang '${F_SHEBANG}'" >&2 ; continue ; }

        F_SHELL_PATH="`command -v \"$F_SHELL_SHORT\"`" \
        && [ -n "$F_SHELL_PATH" ] && [ -x "$F_SHELL_PATH" ] \
        || { echo "WARNING: Failed to find executable path to interpreter '${F_SHELL_SHORT}' from shebang '${F_SHEBANG}'" >&2 ; continue; }

        echo "REWRITING shebang from '$F_SHEBANG' to '#!${F_SHELL_PATH}' in '$F'" >&2
        sed '1 s,^.*$,#!'"${F_SHELL_PATH}," -i "$F"
    fi
done
# create symlinks for man pages; skip man1 (not used with pkgconfig
# capable builds), and man7 (one page there):
%fdupes -s %{buildroot}/%{_mandir}/man3
%fdupes -s %{buildroot}/%{_mandir}/man5
%fdupes -s %{buildroot}/%{_mandir}/man8

%pre
usr/sbin/groupadd -r -g %{NUT_GROUP} 2>/dev/null || :
usr/sbin/useradd -r -g %{NUT_GROUP} -s /bin/false \
  -c "UPS daemon" -d /sbin %{NUT_USER} 2>/dev/null || :
%if "x%{?systemdsystemunitdir}" == "x"
%else
%service_add_pre %{NUT_SYSTEMD_UNITS_SERVICE_TARGET} %{NUT_SYSTEMD_UNITS_UNCOMMON_NDE}
%endif

%post
# Be sure that all files are owned by a dedicated user.
# Some systems do not have the users or groups during rpmbuild
# pre/port tests, so we neuter faults with a warning echo.
# Some systems struggle with "chown USER:GROUP" so we separate
# them into two commands here:
bin/chown -R %{NUT_USER} %{STATEPATH} || echo "WARNING: Could not secure state path '%{STATEPATH}'" >&2
bin/chgrp -R %{NUT_GROUP} %{STATEPATH} || echo "WARNING: Could not secure state path '%{STATEPATH}'" >&2
# Be sure that all files are owned by a dedicated user.
bin/chown %{NUT_USER} %{CONFPATH}/upsd.conf %{CONFPATH}/upsmon.conf %{CONFPATH}/upsd.users || echo "WARNING: Could not secure config files in path '%{CONFPATH}'" >&2
bin/chgrp root %{CONFPATH}/upsd.conf %{CONFPATH}/upsmon.conf %{CONFPATH}/upsd.users || echo "WARNING: Could not secure config files in path '%{CONFPATH}'" >&2
bin/chmod 600 %{CONFPATH}/upsd.conf %{CONFPATH}/upsmon.conf %{CONFPATH}/upsd.users || echo "WARNING: Could not secure config files in path '%{CONFPATH}'" >&2
# And finally trigger udev to set permissions according to newly installed rules files.
if [ -x /sbin/udevadm ] ; then /sbin/udevadm trigger --subsystem-match=usb --property-match=DEVTYPE=usb_device ; fi
%if "x%{?systemdtmpfilesdir}" == "x"
%else
%tmpfiles_create nut-common-tmpfiles.conf
%endif
%if "x%{?systemdsystemunitdir}" == "x"
%else
%service_add_post %{NUT_SYSTEMD_UNITS_SERVICE_TARGET} %{NUT_SYSTEMD_UNITS_UNCOMMON_NDE}
%endif

%preun
%if "x%{?systemdsystemunitdir}" == "x"
:
%else
%service_del_preun %{NUT_SYSTEMD_UNITS_SERVICE_TARGET} %{NUT_SYSTEMD_UNITS_UNCOMMON_NDE}
%endif

%postun
%if "x%{?systemdsystemunitdir}" == "x"
:
%else
%service_del_postun %{NUT_SYSTEMD_UNITS_SERVICE_TARGET} %{NUT_SYSTEMD_UNITS_UNCOMMON_NDE}
%endif

%post -n libupsclient%{SO_MAJOR_LIBUPSCLIENT} -p /sbin/ldconfig

%postun -n libupsclient%{SO_MAJOR_LIBUPSCLIENT} -p /sbin/ldconfig

%post -n libnutclient%{SO_MAJOR_LIBNUTCLIENT} -p /sbin/ldconfig

%postun -n libnutclient%{SO_MAJOR_LIBNUTCLIENT} -p /sbin/ldconfig

%post -n libnutclientstub%{SO_MAJOR_LIBNUTCLIENTSTUB} -p /sbin/ldconfig

%postun -n libnutclientstub%{SO_MAJOR_LIBNUTCLIENTSTUB} -p /sbin/ldconfig

%if 0%{?NUTPKG_WITH_LTDL} > 0
%post -n libnutscan%{SO_MAJOR_LIBNUTSCAN} -p /sbin/ldconfig

%postun -n libnutscan%{SO_MAJOR_LIBNUTSCAN} -p /sbin/ldconfig

%if 0%{?NUTPKG_WITH_LIBNUTCONF} > 0
%post -n libnutconf%{SO_MAJOR_LIBNUTCONF} -p /sbin/ldconfig

%postun -n libnutconf%{SO_MAJOR_LIBNUTCONF} -p /sbin/ldconfig
%endif
%endif

%files
%defattr(-,root,root)
%doc AUTHORS COPYING LICENSE-DCO LICENSE-GPL2 LICENSE-GPL3 ChangeLog MAINTAINERS NEWS.adoc README.adoc UPGRADING.adoc docs/*.adoc docs/*.txt docs/cables/*.txt
# List the (system) dirs we impact but do not own for the package
#% dir % {BASHCOMPLETIONPATH}
#% dir % {_sysconfdir}/logrotate.d
#% dir % {_bindir}
#% dir % {_sbindir}
#% dir % {_datadir}
#% dir % {_docdir}
#% dir % {DOCPATH}
# NOTE: Currently this only delivers libupsclient-config.1
#  and only if not building with pkg-config available:
#% dir % {_mandir}/man1
#% dir % {_mandir}/man3
#% dir % {_mandir}/man5
#% dir % {_mandir}/man7
#% dir % {_mandir}/man8
#% dir % {_libexecdir}
# FIXME: Detect from logrotate properties (or our scriptlet file)?
#% dir /var/log
%if "x%{?systemdsystemunitdir}" == "x"
%else
%dir %{systemdsystemunitdir}
%endif
%if "x%{?systemdsystempresetdir}" == "x"
%else
%dir %{systemdsystempresetdir}
%endif
%if "x%{?systemdtmpfilesdir}" == "x"
%else
%dir %{systemdtmpfilesdir}
%endif
%if "x%{?systemdshutdowndir}" == "x"
%else
%dir %{systemdshutdowndir}
%endif
# List the file patterns to install from proto area
%{BASHCOMPLETIONPATH}/*
%config(noreplace) %{_sysconfdir}/logrotate.d/*
%{_bindir}/*
%if 0%{?NUTPKG_WITH_DMF}
%exclude %{_bindir}/nut-scanner-reindex-dmfsnmp
%endif
%{_datadir}/nut
%if 0%{?NUTPKG_WITH_DMF}
%exclude %{_datadir}/nut/dmfnutscan
%exclude %{_datadir}/nut/dmfsnmp
%exclude %{_datadir}/nut/dmfnutscan.d
%exclude %{_datadir}/nut/dmfsnmp.d
%endif
%{_mandir}/man5/*.*
%{_mandir}/man7/*.*
%{_mandir}/man8/*.*
%exclude %{_mandir}/man8/netxml-ups*.*
%exclude %{_mandir}/man8/snmp-ups*.*
%dir %{LIBEXECPATH}
%{_sbindir}/*
%dir %{UDEVRULEPATH}
%dir %{UDEVRULEPATH}/rules.d
### FIXME: if under /etc ### % config(noreplace) % {UDEVRULEPATH}/rules.d/*.rules
%{UDEVRULEPATH}/rules.d/*.rules
%config(noreplace) %{CONFPATH}/hosts.conf
%config(noreplace) %attr(600,%{NUT_USER},root) %{CONFPATH}/upsd.conf
%config(noreplace) %attr(600,%{NUT_USER},root) %{CONFPATH}/upsd.users
%config(noreplace) %attr(600,%{NUT_USER},root) %{CONFPATH}/upsmon.conf
%dir %{CONFPATH}
%config(noreplace) %{CONFPATH}/nut.conf
%config(noreplace) %{CONFPATH}/ups.conf
%config(noreplace) %{CONFPATH}/upsset.conf
%config(noreplace) %{CONFPATH}/upssched.conf
%dir %{MODELPATH}
%{MODELPATH}/*
%exclude %{MODELPATH}/snmp-ups
%exclude %{MODELPATH}/netxml-ups
%exclude %{_sbindir}/gen-snmp-subdriver.sh
%attr(770,%{NUT_USER},%{NUT_GROUP}) %{STATEPATH}
%attr(770,%{NUT_USER},%{NUT_GROUP}) %{STATEPATH}/upssched
%if "x%{?systemdsystemunitdir}" == "x"
%else
%{systemdsystemunitdir}/*
%endif
%if "x%{?systemdsystempresetdir}" == "x"
%else
%{systemdsystempresetdir}/*
%endif
%if "x%{?systemdtmpfilesdir}" == "x"
%else
%{systemdtmpfilesdir}/*
%endif
%if "x%{?systemdshutdowndir}" == "x"
%else
%{systemdshutdowndir}/nutshutdown
%endif
%{_datadir}/augeas/lenses/dist/nuthostsconf.aug
%{_datadir}/augeas/lenses/dist/nutnutconf.aug
%{_datadir}/augeas/lenses/dist/nutupsconf.aug
%{_datadir}/augeas/lenses/dist/nutupsdconf.aug
%{_datadir}/augeas/lenses/dist/nutupsdusers.aug
%{_datadir}/augeas/lenses/dist/nutupsmonconf.aug
%{_datadir}/augeas/lenses/dist/nutupsschedconf.aug
%{_datadir}/augeas/lenses/dist/nutupssetconf.aug
%{_datadir}/augeas/lenses/dist/tests/test_nut.aug
%dir %{_datadir}/augeas
%dir %{_datadir}/augeas/lenses
%dir %{_datadir}/augeas/lenses/dist
%dir %{_datadir}/augeas/lenses/dist/tests
%{LIBEXECPATH}/nut-driver-enumerator.sh
# Exclude whatever other packages bring, some rpmbuild versions seem to dump
# everything into the base package and then complain about duplicates/conflicts:
### libupsclient7 etc
%exclude %{_libdir}/*.so.*
### nut-cgi
%exclude %{CGIPATH}
%exclude %{HTMLPATH}
%exclude %{CONFPATH}/*.html
### nut-monitor
# TODO: Actually package NUT-Monitor app and scripts where available
# TODO: Detect path from chosen interpreter or NUT build config files?
%exclude /usr/lib*/python*/*-packages/*
### nut-devel
%exclude %{_includedir}/*.h
%exclude %{_libdir}/*.so
%exclude %{_libdir}/pkgconfig/*.pc
%exclude %{_mandir}/man3/*.*
%exclude %{LIBEXECPATH}/sockdebug


%files drivers-net
%defattr(-,root,root)
%{MODELPATH}/snmp-ups
%{MODELPATH}/netxml-ups
%{_mandir}/man8/netxml-ups*.*
%{_mandir}/man8/snmp-ups*.*
%{_sbindir}/gen-snmp-subdriver.sh
%if 0%{?NUTPKG_WITH_DMF}
%dir %{_datadir}/nut/dmfsnmp
%{_datadir}/nut/dmfsnmp/*.dmf
%{_datadir}/nut/dmfsnmp/*.xsd
%dir %{_datadir}/nut/dmfsnmp.d
%{_datadir}/nut/dmfsnmp.d/*.dmf
%endif

%files -n libupsclient%{SO_MAJOR_LIBUPSCLIENT}
%defattr(-,root,root)
%{_libdir}/libupsclient.so.*

%files -n libnutclient%{SO_MAJOR_LIBNUTCLIENT}
%defattr(-,root,root)
%{_libdir}/libnutclient.so.*

%files -n libnutclientstub%{SO_MAJOR_LIBNUTCLIENTSTUB}
%defattr(-,root,root)
%{_libdir}/libnutclientstub.so.*

%if 0%{?NUTPKG_WITH_LTDL}
%files -n libnutscan%{SO_MAJOR_LIBNUTSCAN}
%defattr(-,root,root)
%{_libdir}/libnutscan.so.*
%if 0%{?NUTPKG_WITH_DMF}
%{_bindir}/nut-scanner-reindex-dmfsnmp
%dir %{_datadir}/nut/dmfnutscan
%{_datadir}/nut/dmfnutscan/*.dmf
%{_datadir}/nut/dmfnutscan/*.xsd
%dir %{_datadir}/nut/dmfnutscan.d
%{_datadir}/nut/dmfnutscan.d/*.dmf
%endif

%if 0%{?NUTPKG_WITH_LIBNUTCONF} > 0
%files -n libnutconf%{SO_MAJOR_LIBNUTCONF}
%defattr(-,root,root)
%{_libdir}/libnutconf.so.*
%endif
%endif

%if 0%{?NUTPKG_WITH_LIBGD}
%files cgi
%defattr(-,root,root)
%dir %{CGIPATH}
%dir %{HTMLPATH}
%{CGIPATH}/*
%{HTMLPATH}/*
%config(noreplace) %{CONFPATH}/upsstats-single.html
%config(noreplace) %{CONFPATH}/upsstats.html
%endif

%files monitor
%defattr(-,root,root)
# TODO: Actually package NUT-Monitor app and scripts where available
# TODO: Detect path from chosen interpreter or NUT build config files?
#  Maybe specify python version and location using RPM macros, see e.g.
#    https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/9/html/installing_and_using_dynamic_programming_languages/assembly_packaging-python-3-rpms_installing-and-using-dynamic-programming-languages
# Note we MAY be dual-packaging for several python versions and locations.
/usr/lib*/python*/*-packages/*

%files devel
%defattr(-,root,root)
%{_includedir}/*.h
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc
# FIXME: Alias man page files... use symlinks or what?
# [  139s] nut-devel.x86_64: W: files-duplicate /usr/share/man/man3/nutclient_execute_device_command.3.gz /usr/share/man/man3/nutclient_get_device_command_description.3.gz:/usr/share/man/man3/nutclient_get_device_commands.3.gz:/usr/share/man/man3/nutclient_has_device_command.3.gz
# [  139s] nut-devel.x86_64: W: files-duplicate /usr/share/man/man3/nutclient_get_device_num_logins.3.gz /usr/share/man/man3/nutclient_authenticate.3.gz:/usr/share/man/man3/nutclient_device_forced_shutdown.3.gz:/usr/share/man/man3/nutclient_device_login.3.gz:/usr/share/man/man3/nutclient_device_master.3.gz:/usr/share/man/man3/nutclient_logout.3.gz
# [  139s] nut-devel.x86_64: W: files-duplicate /usr/share/man/man3/nutclient_get_devices.3.gz /usr/share/man/man3/nutclient_get_device_description.3.gz:/usr/share/man/man3/nutclient_has_device.3.gz
# [  139s] nut-devel.x86_64: W: files-duplicate /usr/share/man/man3/nutclient_set_device_variable_value.3.gz /usr/share/man/man3/nutclient_get_device_rw_variables.3.gz:/usr/share/man/man3/nutclient_get_device_variable_description.3.gz:/usr/share/man/man3/nutclient_get_device_variable_values.3.gz:/usr/share/man/man3/nutclient_get_device_variables.3.gz:/usr/share/man/man3/nutclient_has_device_variable.3.gz:/usr/share/man/man3/nutclient_set_device_variable_values.3.gz
# [  139s] nut-devel.x86_64: W: files-duplicate /usr/share/man/man3/nutclient_tcp_get_timeout.3.gz /usr/share/man/man3/nutclient_tcp_create_client.3.gz:/usr/share/man/man3/nutclient_tcp_disconnect.3.gz:/usr/share/man/man3/nutclient_tcp_is_connected.3.gz:/usr/share/man/man3/nutclient_tcp_reconnect.3.gz:/usr/share/man/man3/nutclient_tcp_set_timeout.3.gz
%{_mandir}/man3/*.*
%{LIBEXECPATH}/sockdebug

%changelog
