#
# spec file for package nut.spec
#
# Copyright (c) 2015 SUSE LINUX GmbH, Nuernberg, Germany.
# Copyright (c) 2016-2018 Eaton EEIC.
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
%define systemdsystemdutildir %(pkg-config --variable=systemdutildir systemd)
%define systemdshutdowndir %(pkg-config --variable=systemdshutdowndir systemd)

Name:           nut
Version:        2.7.4
Release:        12
Summary:        Network UPS Tools Core (Uninterruptible Power Supply Monitoring)
License:        GPL-2.0+
Group:          Hardware/UPS
Url:            http://www.networkupstools.org/
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

BuildRequires:  avahi-devel
# To fix end-of-line encoding:
BuildRequires:  dos2unix
BuildRequires:  freeipmi-devel
BuildRequires:  gcc-c++
BuildRequires:  gd-devel
BuildRequires:  libtool
BuildRequires:  libtool-ltdl-devel
BuildRequires:  libusb-devel
BuildRequires:  net-snmp-devel
BuildRequires:  pkg-config
BuildRequires:  python
# LUA 5.1 or 5.2 is known ok for us, both are modern in current distros (201609xx)
BuildRequires:  lua-devel
# TODO: Make sure how this is named to use in CentOS/RHEL (may be not in core but EPEL repos)
# The pycparser is required to rebuild DMF files, but those pre-built
# copies in the git repo/tarball "should" be in sync with original
# C files, so we don't require regeneration for packaging. Also the
# Jenkins NUT-master job should have verified this.
#BuildRequires:  python-pycparser

%if 0%{?suse_version}
BuildRequires:  apache2-devel
BuildRequires:  dbus-1-glib-devel
BuildRequires:  libcppunit-devel
BuildRequires:  libneon-devel
BuildRequires:  libopenssl-devel
BuildRequires:  systemd-rpm-macros
BuildRequires:  powerman-devel
BuildRequires:  tcpd-devel
# TODO: For doc build: move out of opensuse
###BuildRequires:  asciidoc
BuildRequires:  dblatex
BuildRequires:  libxslt-tools
%endif
BuildRequires:  asciidoc

%if 0%{?centos_version}
BuildRequires:  cppunit-devel
BuildRequires:  dbus-glib-devel
BuildRequires:  httpd-devel
BuildRequires:  neon-devel
BuildRequires:  openssl-devel
BuildRequires:  tcp_wrappers-devel
BuildRequires:  libxslt
%endif

%if 0%{?rhel_version}>=7
BuildRequires:  dbus-glib-devel
BuildRequires:  httpd-devel
BuildRequires:  libusb
BuildRequires:  neon
BuildRequires:  openssl-devel
BuildRequires:  tcp_wrappers-devel
BuildRequires:  libxslt
%endif

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
	--with-snmp_dmf_lua\
	--with-dev\
	--with-ipmi \
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
	--with-dmfsnmp-regenerate=no --with-dmfnutscan-regenerate=no --with-dmfsnmp-validate=no --with-dmfnutscan-validate=no

(cd tools; python nut-snmpinfo.py)

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
mkdir %{buildroot}/bin
mv %{buildroot}%{_bindir}/upssched-cmd %{buildroot}/bin/upssched-cmd
# Rename web pages to not conflict with apache2-example-pages or user home page:
mkdir %{buildroot}%{HTMLPATH}/nut %{buildroot}%{CGIPATH}/nut
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
%doc AUTHORS COPYING ChangeLog MAINTAINERS NEWS README UPGRADING docs/*.txt docs/cables
/bin/*
%{_sysconfdir}/bash_completion.d/*
%{_sysconfdir}/logrotate.d/*
%{_bindir}/*
%exclude %{_bindir}/nut-scanner-reindex-dmfsnmp
%{_datadir}/nut
%exclude %{_datadir}/nut/dmfnutscan
%exclude %{_datadir}/nut/dmfsnmp
%exclude %{_datadir}/nut/dmfnutscan.d
%exclude %{_datadir}/nut/dmfsnmp.d
%{_mandir}/man5/*.*
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
%{_bindir}/nut-scanner-reindex-dmfsnmp
%{_mandir}/man8/netxml-ups*.*
%{_mandir}/man8/snmp-ups*.*
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
%{_sbindir}/gen-snmp-subdriver.sh

%files -n libupsclient1
%defattr(-,root,root)
%{_libdir}/*.so.*

%files cgi
%defattr(-,root,root)
%{CGIPATH}/nut
%{HTMLPATH}/nut
%config(noreplace) %{CONFPATH}/upsstats-single.html
%config(noreplace) %{CONFPATH}/upsstats.html

%files devel
%defattr(-,root,root)
%{_includedir}/*.h
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc
%{_mandir}/man3/*.*

%changelog
