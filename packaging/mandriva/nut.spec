%define name		nut
%define version		2.0.1
%define release		2mdk
%define nutuser		ups

%define STATEPATH  	/var/state/ups
%define CGIPATH   	/var/www/cgi-bin
%define DRVPATH   	/sbin
%define CONFPATH	%{_sysconfdir}/ups

Name:		%{name}
Version:	%{version}
Release:	%{release}
License:	GPL
Epoch:		1

Group:		System/Configuration/Hardware
Summary:	Network UPS Tools Client Utilities
Source:		%{name}-%{version}.tar.bz2
Source1:	upsd
Source2:	upsmon
Patch0:		nut-2.0.1-lib64.patch.bz2
URL:		http://random.networkupstools.org

BuildRoot:	%{_tmppath}/%{name}-%{version}-root
Prereq:		chkconfig fileutils rpm-helper >= 0.8
BuildRequires:	freetype2-devel libjpeg-devel libpng-devel xpm-devel
BuildRequires:	net-snmp-devel autoconf2.5 libusb-devel

%description
These programs are part of a developing project to monitor the assortment 
of UPSes that are found out there in the field. Many models have serial 
ports of some kind that allow some form of state checking. This
capability has been harnessed where possible to allow for safe shutdowns, 
live status tracking on web pages, and more.

This package includes the client utilities that are required to monitor a
UPS that the client host is powered from - either connected directly via
a serial port (in which case the nut-server package needs to be installed on
this machine) or across the network (where another host on the network
monitors the UPS via serial cable and runs the main nut package to allow
clients to see the information).


%package server
Summary:	Network UPS Tools server
Group:		System/Servers
Requires:	nut = %{epoch}:%{version}-%{release}
Prereq:		rpm-helper >= 0.8

%description server
These programs are part of a developing project to monitor the assortment 
of UPSes that are found out there in the field. Many models have serial 
serial ports of some kind that allow some form of state checking. This
capability has been harnessed where possible to allow for safe shutdowns, 
live status tracking on web pages, and more.

This package is the main NUT upsd daemon and the associated per-UPS-model
drivers which talk to the UPSes.  You also need to install the base NUT
package.


%package cgi
Group:		Monitoring
Summary:	CGI utils for NUT
Requires:	apache
BuildRequires:	libgd-devel >= 2.0.5, libpng-devel
Prereq:		rpm-helper >= 0.8
Conflicts:	apcupsd

%description cgi
These programs are part of a developing project to monitor the assortment 
of UPSes that are found out there in the field. Many models have serial 
serial ports of some kind that allow some form of state checking. This
capability has been harnessed where possible to allow for safe shutdowns, 
live status tracking on web pages, and more.

This package adds the web CGI programs.   These can be installed on a
separate machine to the rest of the NUT package.


%package devel
Group:		Monitoring
Summary:	Development for NUT Client
BuildRequires:	libgd-devel >= 2.0.5, libpng-devel
Prereq:		rpm-helper >= 0.8

%description devel
This package contains the development header files and libraries
necessary to develop NUT client applications.


%prep
%setup -q
%patch0 -p1 -b .lib64
env WANT_AUTOCONF_2_5=1 autoconf
%patch1 -p1

%build
%serverbuild
%configure2_5x \
	--with-cgi \
	--with-statepath=%{STATEPATH} \
	--with-drvpath=%{DRVPATH} \
	--with-cgipath=%{CGIPATH} \
	--with-gd-libs \
	--with-user=%{nutuser} \
	--with-group=%{nutuser} \
	--enable-shared \
	--sysconfdir=%{CONFPATH}
# workaround buggy parrallel build:
make all usb snmp

%install
rm -rf %{buildroot}
#
# Build basic directories here - if they exist already then the
# installer doesn't try to create and chown them (the chown is
# a killer if we are building as non-root
mkdir -p %{buildroot}%{CONFPATH}
mkdir -p %{buildroot}%{STATEPATH}
mkdir -p %{buildroot}%{DRVPATH}
mkdir -p %{buildroot}%{CGIPATH}
mkdir -p %{buildroot}%{_mandir}
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_sbindir}
make DESTDIR=%{buildroot} install
make DESTDIR=%{buildroot} install-conf
make DESTDIR=%{buildroot} install-cgi-conf
#make DESTDIR=%{buildroot} install-all-drivers
make DESTDIR=%{buildroot} install-lib
make DESTDIR=%{buildroot} install-cgi
make DESTDIR=%{buildroot} install-usb
make DESTDIR=%{buildroot} install-snmp

# install SYSV init stuff
mkdir -p %{buildroot}%{_initrddir}
mkdir -p %{buildroot}%{DRVPATH}
install %SOURCE1 %{buildroot}/%{_initrddir}
install %SOURCE2 %{buildroot}/%{_initrddir}
#install drivers/dummycons %{buildroot}/%{DRVPATH}

# move the *.sample config files to their real locations
# we don't need to worry about overwriting anything since
# they are marked as %config files within the package
for file in %{buildroot}%{CONFPATH}/*.sample
do
    mv $file %{buildroot}%{CONFPATH}/`basename $file .sample`
done

mv %{buildroot}%{CONFPATH}/upsmon.conf %{buildroot}%{CONFPATH}/upsmon.conf.sample
perl -pi -e 's/# RUN_AS_USER nutmon/RUN_AS_USER ups/g' %{buildroot}%{CONFPATH}/upsmon.conf.sample

cp -af data/driver.list docs/

%pre
# Create an UPS user.
%_pre_useradd ups %{STATEPATH} /bin/false
%_pre_groupadd ups ups
%_pre_groupadd tty ups
%_pre_groupadd usb ups

%preun
# only do this if it is not an upgrade
%_preun_service upsmon

%post
%_post_service upsmon

%postun
# Only do this if it is not an upgrade
if [ ! -f %_sbindir/upsd ]; then
   %_postun_userdel ups
fi

%pre	server
# Create an UPS user. We do not use the buggy macro %_pre_groupadd anymore.
%_pre_useradd ups %{STATEPATH} /bin/false
%_pre_groupadd ups ups
%_pre_groupadd tty ups
%_pre_groupadd usb ups

%preun	server
%_preun_service upsd || :

%post	server
%_post_service upsd || :


%postun	server
# Only do this if it is not an upgrade
if [ ! -f %_sbindir/upsmon ]; then
   %_postun_userdel ups
fi

%clean
rm -rf %{buildroot}

%files	server
%defattr(-,root,root)
%{DRVPATH}/*
%{_sbindir}/upsd
%config %attr(744,root,root) %{_initrddir}/upsd
%dir %attr(755,root,root) %{CONFPATH}
%config(noreplace) %attr(644,root,root) %{CONFPATH}/ups.conf
%config(noreplace) %attr(640,root,ups) %{CONFPATH}/upsd.users
%config(noreplace) %attr(640,root,ups) %{CONFPATH}/upsd.conf
%{_datadir}/cmdvartab
%{_datadir}/driver.list
%{_bindir}/libupsclient-config
%{_libdir}/pkgconfig/libupsclient.pc
%{_mandir}/man5/ups.conf.5.bz2
%{_mandir}/man5/upsd.conf.5.bz2
%{_mandir}/man5/upsd.users.5.bz2
%{_mandir}/man8/belkin.8.bz2
%{_mandir}/man8/belkinunv.8.bz2
%{_mandir}/man8/bestups.8.bz2
%{_mandir}/man8/bestuferrups.8.bz2
%{_mandir}/man8/cyberpower.8.bz2
%{_mandir}/man8/cpsups.8.bz2
%{_mandir}/man8/everups.8.bz2
%{_mandir}/man8/etapro.8.bz2
%{_mandir}/man8/genericups.8.bz2
%{_mandir}/man8/isbmex.8.bz2
%{_mandir}/man8/liebert.8.bz2
%{_mandir}/man8/masterguard.8.bz2
%{_mandir}/man8/mge-utalk.8.bz2
%{_mandir}/man8/apcsmart.8.bz2
%{_mandir}/man8/nutupsdrv.8.bz2
%{_mandir}/man8/oneac.8.bz2
%{_mandir}/man8/powercom.8.bz2
%{_mandir}/man8/snmp-ups.8.bz2
%{_mandir}/man8/tripplite.8.bz2
%{_mandir}/man8/tripplitesu.8.bz2
%{_mandir}/man8/victronups.8.bz2
%{_mandir}/man8/upsd.8.bz2
%{_mandir}/man8/upsdrvctl.8.bz2
%{_mandir}/man8/mge-shut.8.bz2
%{_mandir}/man8/energizerups.8.bz2
%{_mandir}/man8/safenet.8.bz2
%{_mandir}/man8/hidups.8.bz2
%{_mandir}/man8/usbhid-ups.8.bz2
%{_mandir}/man8/bestfcom.8.bz2
%{_mandir}/man8/metasys.8.bz2

%files
%defattr(-,root,root)
%doc ChangeLog COPYING AUTHORS INSTALL MAINTAINERS NEWS README UPGRADING docs
%dir %attr(755,root,root) %{CONFPATH}
%config %attr(744,root,root) %{_initrddir}/upsmon
%config(noreplace) %attr(640,root,ups) %{CONFPATH}/upssched.conf
%attr(640,root,ups) %{CONFPATH}/upsmon.conf.sample
%dir %attr(750,ups,ups) %{STATEPATH}
%_bindir/upsc
%_bindir/upscmd
%_bindir/upsrw
%_bindir/upslog
%_sbindir/upsmon
%_sbindir/upssched
%{_mandir}/man5/upsmon.conf.5.bz2
%{_mandir}/man5/upssched.conf.5.bz2
%{_mandir}/man8/upsc.8.bz2
%{_mandir}/man8/upscmd.8.bz2
%{_mandir}/man8/upsrw.8.bz2
%{_mandir}/man8/upslog.8.bz2
%{_mandir}/man8/upsmon.8.bz2
%{_mandir}/man8/upssched.8.bz2
%{_mandir}/man8/upsset.cgi.8.bz2

%files cgi
%defattr(-,root,root)
%dir %attr(755,root,root) %{CONFPATH}
# The apache user will have to read this 3 files
%config(noreplace) %attr(644,root,root) %{CONFPATH}/hosts.conf
%config(noreplace) %attr(644,root,root) %{CONFPATH}/upsset.conf
%config(noreplace) %attr(644,root,root) %{CONFPATH}/upsstats.html
%config(noreplace) %attr(644,root,root) %{CONFPATH}/upsstats-single.html
%{CGIPATH}/upsimage.cgi
%{CGIPATH}/upsset.cgi
%{CGIPATH}/upsstats.cgi
%{_mandir}/man5/hosts.conf.5.bz2
%{_mandir}/man5/upsstats.html.5.bz2
%{_mandir}/man5/upsset.conf.5.bz2
%{_mandir}/man8/upsimage.cgi.8.bz2
%{_mandir}/man8/upsset.cgi.8.bz2
%{_mandir}/man8/upsstats.cgi.8.bz2

%files devel
%defattr(-,root,root)
%{_includedir}/*.h
%{_libdir}/libupsclient.a
%{_mandir}/man3/upscli_*.3.bz2

%changelog
* Thu Jun 23 2005 Arnaud Quette <arnaud.quette@free.fr> 2.0.2-1mdk
- remove Michel Bouissou's patch1 at it has been merged upstream

* Tue Mar 22 2005 Gwenole Beauchesne <gbeauchesne@mandrakesoft.com> 2.0.1-2mdk
- lib64 fixes (again)

* Mon Feb 28 2005 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1:2.0.1-1mdk
- 2.0.1
- adapt patchs from Michel Bouissou

* Thu Feb 10 2005 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1:2.0.0-4mdk
- upsd poweroff script clean: remove the sleep and let the halt script continue
- add upsd service status option
- add Michel Bouissou's patch

* Fri Oct 22 2004 Gwenole Beauchesne <gbeauchesne@mandrakesoft.com> 2.0.0-3mdk
- lib64 fixes

* Tue Oct 05 2004 Thierry Vignaud <tvignaud@mandrakesoft.com> 2.0.0-2mdk
- workaround buggy parrallel build
- package again driver list
- patch 2: fix compiling

* Fri Mar 26 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 2.0.0-1mdk
- 2.0.0

* Wed Mar 24 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.2-1mdk
- 1.4.2 final (with no changements from previous pre2 release)

* Tue Mar 16 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.2-0.pre2.1mdk
- New release with security and kernel 2.6 fixs
- Change URL

* Thu Mar  4 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-10mdk
- Clean remove (bis repetita)

* Wed Mar  3 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-9mdk
- Clean remove

* Wed Mar  3 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-8mdk
- Force remove old init scripts even if they have been customised

* Tue Feb 24 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-7mdk
- Add Epoch required by new rpm

* Fri Jan 23 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-6mdk
- Orthographical correction for i18n team

* Tue Jan 20 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-5mdk
- Fix initscripts (Bug report from Henning Kulander)

* Mon Jan 19 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-4mdk
- Fix upsd initscript

* Tue Jan  6 2004 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-2mdk
- Add dummycons driver and correction in upsd script

* Tue Dec  9 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-1mdk
- New release

* Mon Dec 08 2003 Oden Eriksson <oden.eriksson@kvikkjokk.net> 1.4.1-0.7mdk
- buildrequires net-snmp-devel

* Tue Dec  5 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-0.6mdk
- More clean

* Tue Dec  4 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-0.5mdk
- Clean the init script upsd

* Tue Dec  2 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-0.4mdk
- pre4 release
- The init scripts read now some parameters directly from upsmon.conf
- Remove patch0 and add the parameters snmp and hidups to make instead

* Wed Nov 26 2003 Thierry Vignaud <tvignaud@mandrakesoft.com> 1.4.1-0.3mdk
- fix packaging in order to enable update
- fix server update when service is down

* Thu Nov 20 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.1-0.pre3.1mdk
- New release
- Remove bad require in devel package
- Adapt patch 0 to new release and add the snmp-ups driver
- patch 1 to add parseconf.h in the devel package

* Fri Oct 31 2003 Thierry Vignaud <tvignaud@mandrakesoft.com> 1.4.0-3mdk
- patch 0: support ups connected through usb

* Tue Oct 21 2003 Gwenole Beauchesne <gbeauchesne@mandrakesoft.com> 1.4.0-2mdk
- fix deps

* Wed Jul 30 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.4.0-1mdk
- New stable tree: 1.4.0 released

* Tue Jul 29 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.2.3-2mdk
- Add cgi conflict with apcupsd wich is using the same file name for its cgi

* Wed Jul 23 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.2.3-1mdk
- New release
- Change gid of upssched.conf, upsd.conf & upsd.users

* Fri Jan 24 2003 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.2.1-4mdk
- Requires

* Sat Dec 28 2002 Stefan van der Eijk <stefan@eijk.nu> 1.2.1-2mdk
- BuildRequires

* Fri Dec 20 2002 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.2.1-1mdk
- New Release

* Thu Nov 14 2002 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.2.0-1mdk
- New Release
- Do not use the buggy macro %_pre_groupadd anymore
- Create the devel package

* Thu Aug 29 2002 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.0.0-4mdk
- TODO

* Mon Aug 26 2002 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.0.0-3mdk
- Change STATEPATH, change owner of upsd.* files and modify init scripts

* Mon Aug 26 2002 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.0.0-2mdk
- Add the user ups to the group tty and usb with rpmhelper.
- Add new runlevel scripts
- Remove upspowerdown: now supported in the Mandrake halt init script.

* Wed Aug 21 2002 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 1.0.0-1mdk
- New release
- Use rpm-helper
- Add some new manuals to %files

* Mon Jul 22 2002 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 0.50.0-1mdk
- New release
- Add upssched-cmd in the doc directory

* Wed Feb 07 2002 Arnaud de Lorbeau <adelorbeau@mandrakesoft.com> 0.45.3-2mdk
- Specfile adaptations for Mandrake Linux.

* Wed Oct 24 2001 Peter Bieringer <pb@bieringer.de> (0.45.3pre1)
- Take man path given by rpm instead of hardwired
- Add some missing man pages to %files

* Wed Feb 07 2001 Karl O. Pinc <kop@meme.com> (0.44.3-pre2)
- Cgi package buildrequires gd >= 1.6
- Added man pages for apcsmart and powercom models

* Tue Dec 05 2000 <Nigel.Metheringham@InTechnology.co.uk> (0.44.2)
- Made cgi package standalone (needs no other parts of NUT)
- Moved some configs into cgi
- Shared hosts.conf between cgi & main

* Fri Nov 24 2000 <Nigel.Metheringham@InTechnology.co.uk> (0.44.2)
- Moved models to be more FHS compliant and make sure they are there
  if everything but root is unmounted
- Moved a few things around

* Mon Aug 21 2000 <Nigel.Metheringham@Vdata.co.uk> (0.44.1)
- Added new model drivers into rpm list
- Made it wildcard more stuff so this doesn't need to be
  maintained for every little change.
  ** NOTE this breaks things if modelpath isn't distinct **

* Mon Jul 17 2000 <Nigel.Metheringham@Vdata.co.uk> (0.44.0)
- Fixed some problems in the spec file
- Dropped the older changelog entries since there is some
  intermediate history thats been missed.
- Added new model drivers into rpm list
- Updated descriptions somewhat

