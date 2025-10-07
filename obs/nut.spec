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


%define apache_serverroot %(%{_sbindir}/apxs2 -q datadir 2>/dev/null || %{_sbindir}/apxs -q PREFIX)
%define CGIPATH		%{apache_serverroot}/cgi-bin
%define HTMLPATH	%{apache_serverroot}/htdocs
%define MODELPATH	%{_libexecdir}/ups/driver
%define STATEPATH	%{_localstatedir}/lib/ups
%define CONFPATH	%{_sysconfdir}/ups
### Note: this is /etc/nut in Debian version
%define USER		upsd
%define GROUP		daemon
%define LBRACE (
%define RBRACE )
%define QUOTE "
%define BACKSLASH \\
# Collect all devices listed in ups-nut-device.fdi:
%define USBHIDDRIVERS    %(zcat %{SOURCE0} | tr a-z A-Z | fgrep -a -A1 USBHID-UPS | sed -n 's/.*ATTR{IDVENDOR}==%{QUOTE}%{BACKSLASH}%{LBRACE}[^%{QUOTE}]*%{BACKSLASH}%{RBRACE}%{QUOTE}, ATTR{IDPRODUCT}==%{QUOTE}%{BACKSLASH}%{LBRACE}[^%{QUOTE}]*%{BACKSLASH}%{RBRACE}%{QUOTE}, MODE=.*/modalias%{LBRACE}usb:v%{BACKSLASH}1p%{BACKSLASH}2d*dc*dsc*dp*ic*isc*ip*%{RBRACE}/p' | tr '%{BACKSLASH}n' ' ')
%define USBNONHIDDRIVERS %(zcat %{SOURCE0} | tr a-z A-Z | fgrep -a -A1 _USB       | sed -n 's/.*ATTR{IDVENDOR}==%{QUOTE}%{BACKSLASH}%{LBRACE}[^%{QUOTE}]*%{BACKSLASH}%{RBRACE}%{QUOTE}, ATTR{IDPRODUCT}==%{QUOTE}%{BACKSLASH}%{LBRACE}[^%{QUOTE}]*%{BACKSLASH}%{RBRACE}%{QUOTE}, MODE=.*/modalias%{LBRACE}usb:v%{BACKSLASH}1p%{BACKSLASH}2d*dc*dsc*dp*ic*isc*ip*%{RBRACE}/p' | tr '%{BACKSLASH}n' ' ')
%define systemdsystemunitdir %(pkg-config --variable=systemdsystemunitdir systemd)
%define systemdsystempresetdir %(pkg-config --variable=systemdsystempresetdir systemd || pkg-config --variable=systemdsystempresetdir libsystemd)
%define systemdtmpfilesdir %(pkg-config --variable=systemdtmpfilesdir systemd || pkg-config --variable=systemdtmpfilesdir libsystemd)
%define systemdsystemdutildir %(pkg-config --variable=systemdutildir systemd)
%define systemdshutdowndir %(pkg-config --variable=systemdshutdowndir systemd)

%define NUTPKG_WITH_DMF	0

# Not all distros have it
%define NUTPKG_WITH_FREEIPMI	%( (yum search freeipmi-devel | grep -E '^(lib)?freeipmi-devel\.' && exit ; dnf search freeipmi-devel | grep -E '^(lib)?freeipmi-devel\.' && exit ; zypper search -s freeipmi-devel | grep -E '(lib)?freeipmi-devel' && exit ; urpmq --sources freeipmi-devel && exit ; pkcon search name freeipmi-devel | grep -E '(Available|Installed).*freeipmi-devel' && exit;) >&2 && echo 1 || echo 0)
%define NUTPKG_WITH_POWERMAN	%( (yum search powerman-devel | grep -E '^(lib)?powerman-devel\.' && exit ; dnf search powerman-devel | grep -E '^(lib)?powerman-devel\.' && exit ; zypper search -s powerman-devel | grep -E '(lib)?powerman-devel' && exit ; urpmq --sources powerman-devel && exit ; pkcon search name powerman-devel | grep -E '(Available|Installed).*powerman-devel' && exit;) >&2 && echo 1 || echo 0)
%define NUTPKG_WITH_AVAHI	%( (yum search avahi-devel | grep -E '^(lib)?avahi-devel\.' && exit ; dnf search avahi-devel | grep -E '^(lib)?avahi-devel\.' && exit ; zypper search -s avahi-devel | grep -E '(lib)?avahi-devel' && exit ; urpmq --sources avahi-devel && exit ; pkcon search name avahi-devel | grep -E '(Available|Installed).*avahi-devel' && exit;) >&2 && echo 1 || echo 0)
%define NUTPKG_WITH_TCPWRAP	%( (yum search tcp_wrappers-devel | grep -E '^(lib)?tcp_wrappers-devel\.' && exit ; dnf search tcp_wrappers-devel | grep -E '^(lib)?tcp_wrappers-devel\.' && exit ; zypper search -s tcp_wrappers-devel | grep -E '(lib)?tcp_wrappers-devel' && exit ; urpmq --sources tcp_wrappers-devel && exit ; pkcon search name tcp_wrappers-devel | grep -E '(Available|Installed).*tcp_wrappers-devel' && exit;) >&2 && echo 1 || echo 0)

Name:           nut
# NOTE: OBS should rewrite this:
Version:        2.8.4
Release:        1
Summary:        Network UPS Tools Core (Uninterruptible Power Supply Monitoring)
License:        GPL-2.0+
Group:          Hardware/UPS
Url:            https://www.networkupstools.org/
Source0:        %{name}-%{version}.tar.gz

Requires:       %{_bindir}/fgrep
Requires:       %{_bindir}/grep
Requires:       %{_bindir}/pgrep
Requires:       %{_bindir}/pkill
Requires:       %{_bindir}/readlink
Requires:       usbutils
%if 0%{?suse_version}
Requires(post): udev
%endif
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

# To fix end-of-line encoding:
BuildRequires:  dos2unix

%if 0%{?NUTPKG_WITH_AVAHI}
BuildRequires:  avahi-devel
%endif

%if 0%{?NUTPKG_WITH_FREEIPMI}
BuildRequires:  (libfreeipmi-devel or freeipmi-devel)
%endif

BuildRequires:  gcc-c++
BuildRequires:  gd-devel
BuildRequires:  libtool
BuildRequires:  libtool-ltdl-devel
# libusb-0.1 or libusb-1.0:
BuildRequires:  (libusb-devel or libusbx-devel)
#!Prefer:       libusbx-devel
BuildRequires:  net-snmp-devel
BuildRequires:  pkg-config
# Maybe older Pythons are also okay, but were not tested for ages
BuildRequires:  (python >= 2.6 or python3 or python2)

%if 0%{?NUTPKG_WITH_DMF}
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
#   % if 0 % {?rhel_version}>=7
# and whatnot

BuildRequires:  (httpd-devel or apache2-devel)
BuildRequires:  (dbus-1-glib-devel or dbus-glib-devel)

%if 0%{?rhel_version}>=8 || ! 0%{?rhel_version}
BuildRequires:  (libcppunit-devel or cppunit-devel)
%endif

%if 0%{?NUTPKG_WITH_TCPWRAP}
BuildRequires:  (tcpd-devel or tcp_wrappers-devel)
%endif

# May be plain "neon" and "libusb" in RHEL7 or older?
BuildRequires:  (libneon-devel or neon-devel or neon)
#!Prefer:       (libneon-devel or neon-devel)
BuildRequires:  (libopenssl-devel or openssl-devel or openssl)
#!Prefer:       (libopenssl-devel or openssl-devel)

%if 0%{?NUTPKG_WITH_POWERMAN}
BuildRequires:  powerman-devel
%endif

%if 0%{?suse_version}
BuildRequires:  systemd-rpm-macros
# Only needed for PDF generation, we do not package that now
#BuildRequires:  dblatex
%endif

BuildRequires:  (libxslt-tools or libxslt)
BuildRequires:  asciidoc

%if %{defined opensuse_version}
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

%package -n libupsclient1
Summary:        Network UPS Tools Library (Uninterruptible Power Supply Monitoring)
Group:          System/Libraries

%description -n libupsclient1
Shared library for the Network UPS Tools.

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS hardware.

Detailed information about supported hardware can be found in
%{_docdir}/nut.

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

%package gui
Summary:        Network UPS Tools Web Server Support (GUI client)
Group:          Hardware/UPS
Requires:       %{name} = %{version}
BuildRequires:  (python >= 2.6 or python3 or python2)

%description gui
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
sh autogen.sh
./configure --disable-static --with-pic \
	--prefix=%{_prefix}\
	--bindir=%{_bindir}\
	--sbindir=%{_sbindir}\
	--libdir=%{_libdir}\
	--libexecdir=%{_libexecdir}\
	--sysconfdir=%{CONFPATH}\
	--datadir=%{_datadir}/nut\
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
	--with-user=%{USER}\
	--with-group=%{GROUP} \
	--with-udev-dir=%{_sysconfdir}/udev \
	--enable-option-checking=fatal\
	--with-systemdsystemunitdir --with-systemdshutdowndir \
	--with-augeas-lenses-dir=/usr/share/augeas/lenses/dist \
%if 0%{?NUTPKG_WITH_DMF}
	--with-snmp_dmf_lua\
	--with-dmfsnmp-regenerate=no --with-dmfnutscan-regenerate=no --with-dmfsnmp-validate=no --with-dmfnutscan-validate=no
%endif

### via Make now ### (cd tools; python nut-snmpinfo.py)

make %{?_smp_mflags}
PORT=$(sed -n 's/#define PORT //p' config.log)
if test "$PORT" = 3493 ; then
    PORT=nut
fi

%install
make DESTDIR=%{buildroot} install %{?_smp_mflags}
mkdir -p %{buildroot}%{STATEPATH}
# SuSE rc
mkdir -p %{buildroot}%{_sbindir}
mkdir -p %{buildroot}%{_sysconfdir}/logrotate.d
install -m 644 scripts/logrotate/nutlogd %{buildroot}%{_sysconfdir}/logrotate.d/
mkdir -p %{buildroot}%{STATEPATH}
rename .sample "" %{buildroot}%{_sysconfdir}/ups/*.sample
mkdir -p %{buildroot}/bin
mv %{buildroot}%{_bindir}/upssched-cmd %{buildroot}/bin/upssched-cmd
# Rename web pages to not conflict with apache2-example-pages or user home page:
mkdir -p %{buildroot}%{HTMLPATH}/nut %{buildroot}%{CGIPATH}/nut
mv %{buildroot}%{HTMLPATH}/*.{html,png} %{buildroot}%{HTMLPATH}/nut/
mv %{buildroot}%{CGIPATH}/*.cgi %{buildroot}%{CGIPATH}/nut
find %{buildroot} -type f -name "*.la" -delete -print
mkdir -p %{buildroot}%{_sysconfdir}/bash_completion.d
install -m0644 scripts/misc/nut.bash_completion %{buildroot}%{_sysconfdir}/bash_completion.d/
install -m0755 scripts/subdriver/gen-snmp-subdriver.sh %{buildroot}%{_sbindir}/

%pre
usr/sbin/useradd -r -g %{GROUP} -s /bin/false \
  -c "UPS daemon" -d /sbin %{USER} 2>/dev/null || :
%if %{defined opensuse_version}
%service_add_pre nut-driver@.service nut-server.service nut-monitor.service nut-driver.target nut.target
%endif

%post
# Be sure that all files are owned by a dedicated user.
bin/chown -R %{USER}:%{GROUP} %{STATEPATH}
# Be sure that all files are owned by a dedicated user.
bin/chown %{USER}:root %{CONFPATH}/upsd.conf %{CONFPATH}/upsmon.conf %{CONFPATH}/upsd.users
bin/chmod 600 %{CONFPATH}/upsd.conf %{CONFPATH}/upsmon.conf %{CONFPATH}/upsd.users
# And finally trigger udev to set permissions according to newly installed rules files.
/sbin/udevadm trigger --subsystem-match=usb --property-match=DEVTYPE=usb_device
%if %{defined opensuse_version}
%service_add_post nut-driver@.service nut-server.service nut-monitor.service nut-driver-enumerator.service nut-driver.target nut.target
%endif

%preun
%if %{defined opensuse_version}
%service_del_preun nut-driver@.service nut-server.service nut-monitor.service nut-driver-enumerator.service nut-driver.target nut.target
%endif

%postun
%if %{defined opensuse_version}
%service_del_postun nut-driver@.service nut-server.service nut-monitor.service nut-driver-enumerator.service nut-driver.target nut.target
%endif

%post -n libupsclient1 -p /sbin/ldconfig

%postun -n libupsclient1 -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc AUTHORS COPYING LICENSE-DCO LICENSE-GPL2 LICENSE-GPL3 ChangeLog MAINTAINERS NEWS.adoc README.adoc UPGRADING.adoc docs/*.adoc docs/*.txt docs/cables
/bin/*
%{_sysconfdir}/bash_completion.d/*
%{_sysconfdir}/logrotate.d/*
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
%dir %{_libexecdir}/ups
%{_sbindir}/*
%dir %{_sysconfdir}/udev
%dir %{_sysconfdir}/udev/rules.d
%config(noreplace) %{_sysconfdir}/udev/rules.d/*.rules
%config(noreplace) %{CONFPATH}/hosts.conf
%config(noreplace) %attr(600,%{USER},root) %{CONFPATH}/upsd.conf
%config(noreplace) %attr(600,%{USER},root) %{CONFPATH}/upsd.users
%config(noreplace) %attr(600,%{USER},root) %{CONFPATH}/upsmon.conf
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
%attr(700,%{USER},%{GROUP}) %{STATEPATH}
%{systemdsystemunitdir}/*
%{systemdsystempresetdir}/*
%{systemdtmpfilesdir}/*
%{systemdshutdowndir}/nutshutdown
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
%{_libexecdir}/nut-driver-enumerator.sh

%files drivers-net
%defattr(-,root,root)
%{MODELPATH}/snmp-ups
%{MODELPATH}/netxml-ups
%{_mandir}/man8/netxml-ups*.*
%{_mandir}/man8/snmp-ups*.*
%{_sbindir}/gen-snmp-subdriver.sh
%if 0%{?NUTPKG_WITH_DMF}
%{_bindir}/nut-scanner-reindex-dmfsnmp
%dir %{_datadir}/nut/dmfnutscan
%dir %{_datadir}/nut/dmfsnmp
%{_datadir}/nut/dmfnutscan/*.dmf
%{_datadir}/nut/dmfsnmp/*.dmf
%{_datadir}/nut/dmfnutscan/*.xsd
%{_datadir}/nut/dmfsnmp/*.xsd
%dir %{_datadir}/nut/dmfnutscan.d
%dir %{_datadir}/nut/dmfsnmp.d
%{_datadir}/nut/dmfnutscan.d/*.dmf
%{_datadir}/nut/dmfsnmp.d/*.dmf
%endif

%files -n libupsclient1
%defattr(-,root,root)
%{_libdir}/*.so.*

%files cgi
%defattr(-,root,root)
%{CGIPATH}/nut
%{HTMLPATH}/nut
%config(noreplace) %{CONFPATH}/upsstats-single.html
%config(noreplace) %{CONFPATH}/upsstats.html

%files gui
%defattr(-,root,root)
# TODO: NUT-Monitor where available
# TODO: Detect path from chosen interpreter or NUT build config files?
/usr/lib/python*/*-packages/*

%files devel
%defattr(-,root,root)
%{_includedir}/*.h
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc
%{_mandir}/man3/*.*
%{_libexecdir}/sockdebug

%changelog
