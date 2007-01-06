# don't know how different I can do this
%define majorver 2.0
%define version 2.0.1
%define relver 1
%define nutuser nutmon

# what version of RH are we building for?
%define redhat7 1
%define redhat6 0

# Options for Redhat version 6.x:
# rpm -ba|--rebuild --define "rh6 1"
%{?rh6:%define redhat7 0}
%{?rh6:%define redhat6 1}

# some systems dont have initrddir defined
%{?_initrddir:%define _initrddir /etc/rc.d/init.d}

Name: nut
Group: Applications/System
Summary: Multi-vendor UPS Monitoring Project Client Utilities
Version: %{version}
Release: %{relver}
Source: http://www.exploits.org/nut/release/%{majorver}/%{name}-%{version}.tar.gz
Copyright: GPL
BuildRoot: /var/tmp/%{name}-%{version}-root
Prereq: chkconfig fileutils
Obsoletes: nut-client
#
# configure file locations
# confdir etc are not really negotiable, so are not configurable here
%define STATEPATH  	/var/state/ups
%define CGIPATH   	/var/www/cgi-bin
%define MODELPATH	   /sbin
%define CONFPATH	   /etc/ups


%description
These programs are part of a developing project to monitor the assortment 
of UPSes that are found out there in the field. Many models have serial 
ports of some kind that allow some form of state checking. This
capability has been harnessed where possible to allow for safe shutdowns, 
live status tracking on web pages, and more.

This package includes the client utilities that are required to monitor a
UPS that the client host is powered from - either connected directly via
a serial port (in which case the main nut package needs to be installed on
this machine) or across the network (where another host on the network
monitors the UPS via serial cable and runs the main nut package to allow
clients to see the information).

%package server
Requires: nut
Summary: Multi-vendor UPS Monitoring Project server
Group: Applications/System
Requires: nut = %{version} patch

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
Group: Applications/System
Summary: CGI utils for Multi-vendor UPS Monitoring Project
Requires: gd >= 1.6
BuildRequires: gd-devel >= 1.6, libpng-devel

%description cgi
These programs are part of a developing project to monitor the assortment 
of UPSes that are found out there in the field. Many models have serial 
serial ports of some kind that allow some form of state checking. This
capability has been harnessed where possible to allow for safe shutdowns, 
live status tracking on web pages, and more.

This package adds the web CGI programs.   These can be installed on a
separate machine to the rest of the NUT package.

%prep
%setup -q
autoconf

%build
autoconf
CFLAGS="$RPM_OPT_FLAGS" ./configure \
	--prefix=/usr \
	--exec-prefix=/usr \
	--sysconfdir=%{CONFPATH} \
	--with-statepath=%{STATEPATH} \
	--with-drvpath=%{MODELPATH} \
	--with-cgipath=%{CGIPATH} \
	--with-cgi \
	--with-user=%{nutuser} \
	--with-group=%{nutuser} \
	--mandir=%{_mandir} \
	--enable-shared
make

%install
rm -rf %{buildroot}
#
# Build basic directories here - if they exist already then the
# installer doesn't try to create and chown them (the chown is
# a killer if we are building as non-root
mkdir -p %{buildroot}%{CONFPATH}
mkdir -p %{buildroot}%{MODELPATH}
mkdir -p %{buildroot}%{CGIPATH}
mkdir -p %{buildroot}%{STATEPATH}
mkdir -p %{buildroot}%{_mandir}
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/sbin
make DESTDIR=%{buildroot} install
make DESTDIR=%{buildroot} install-cgi

# move the *.sample config files to their real locations
# we don't need to worry about overwriting anything since
# they are marked as %config files within the package
for file in %{buildroot}%{CONFPATH}/*.sample
do
    mv $file %{buildroot}%{CONFPATH}/`basename $file .sample`
done

# install SYSV init stuff
mkdir -p %{buildroot}%{_initrddir}
install scripts/RedHat/upsd %{buildroot}%{_initrddir}
install scripts/RedHat/upsmon %{buildroot}%{_initrddir}
install scripts/RedHat/halt.patch %{buildroot}%{_initrddir}
mkdir -p %{buildroot}/etc/sysconfig
install scripts/RedHat/ups %{buildroot}/etc/sysconfig
##%if %{redhat7}
##install scripts/RedHat-6.0/upspowerdown.rh7 %{buildroot}%{_initrddir}/upspowerdown
##%else
##install scripts/RedHat-6.0/upspowerdown %{buildroot}%{_initrddir}
##%endif

%pre
/sbin/groupadd -r -f %{nutuser}
grep \^nutmon /etc/passwd >/dev/null
if [ $? -ne 0 ]; then
  /sbin/useradd -d /etc/ups -g %{nutuser} -M -r %{nutuser}
fi

%pre server
/sbin/groupadd -r -f %{nutuser}
grep \^nutmon /etc/passwd >/dev/null
if [ $? -ne 0 ]; then
  /sbin/useradd -d /etc/ups -g %{nutuser} -M -r %{nutuser}
fi

%pre cgi
/sbin/groupadd -r -f %{nutuser}
grep \^nutmon /etc/passwd >/dev/null
if [ $? -ne 0 ]; then
  /sbin/useradd -d /etc/ups -g %{nutuser} -M -r %{nutuser}
fi

%preun
# only do this if it is not an upgrade
if [ $1 -eq 0 ]
then 
   /sbin/chkconfig --del upsmon
   [ -f %{_initrddir}/upsmon ] && \
   	%{_initrddir}/upsmon stop
fi

%post
/sbin/chkconfig --add upsmon
# restart server if this is an upgrade
if [ $1 -gt 1 ]
then
   [ -f %{_initrddir}/upsmon ] && \
   	%{_initrddir}/upsmon restart
fi


%preun server
# only do this if it is not an upgrade
if [ $1 -eq 0 ]
then 
##   /sbin/chkconfig --del upspowerdown
   /sbin/chkconfig --del upsd
   [ -f %{_initrddir}/upsd ] && \
   	%{_initrddir}/upsd stop
fi

%post server
/usr/bin/patch -N -b -p0<%{_initrddir}/halt.patch
rm -f %{_initrddir}/halt.patch
##/sbin/chkconfig --add upspowerdown
/sbin/chkconfig --add upsd
# restart server if this is an upgrade
if [ $1 -gt 1 ]
then
   [ -f %{_initrddir}/upsd ] && \
   	%{_initrddir}/upsd restart
fi

%clean
rm -rf %{buildroot}

%files server
%defattr(-,root,root)
%{MODELPATH}
/usr/sbin/upsd
##%attr(755,root,root) %{_initrddir}/upspowerdown
%attr(755,root,root) %{_initrddir}/upsd
%attr(400,root,root) %{_initrddir}/halt.patch
%dir %attr(755,root,root) %{CONFPATH}
%config(noreplace) %attr(444,root,root) %{CONFPATH}/ups.conf
%config(noreplace) %attr(444,root,root) %{CONFPATH}/upsd.conf
%config(noreplace) %attr(400,root,root) %{CONFPATH}/upsd.users
%config(noreplace) %attr(644,root,root) /etc/sysconfig/ups
%{_mandir}/man8/apcsmart.8.gz
%{_mandir}/man8/belkin.8.gz
%{_mandir}/man8/bestups.8.gz
%{_mandir}/man8/newapc.8.gz
%{_mandir}/man8/nutupsdrv.8.gz
%{_mandir}/man8/powercom.8.gz
%{_mandir}/man8/upsd.8.gz
%{_mandir}/man8/upsdrvctl.8.gz
%{_mandir}/man5/ups.conf.5.gz
%{_mandir}/man5/upsd.conf.5.gz
%{_mandir}/man5/upsd.users.5.gz


%files
%defattr(-,root,root)
%doc ChangeLog COPYING AUTHORS INSTALL README docs/
%dir %attr(755,root,root) %{CONFPATH}
##%config(noreplace) %attr(644,root,root) %{CONFPATH}/hosts.conf
%config(noreplace) %attr(400,%{nutuser},%{nutuser}) %{CONFPATH}/upsmon.conf
%config(noreplace) %attr(400,%{nutuser},%{nutuser}) %{CONFPATH}/upssched.conf
%config(noreplace) %attr(644,root,root) /etc/sysconfig/ups
%dir %attr(755,%{nutuser},%{nutuser}) %{STATEPATH}
%attr(755,root,root) %{_initrddir}/upsmon
/usr/bin/upsc
/usr/bin/upscmd
/usr/bin/upsrw
/usr/bin/upslog
/usr/sbin/upsmon
/usr/sbin/upssched
%{_mandir}/man5/upsmon.conf.5.gz
%{_mandir}/man5/upssched.conf.5.gz
%{_mandir}/man8/nutupsdrv.8.gz
%{_mandir}/man8/upsc.8.gz
%{_mandir}/man8/upsrw.8.gz
%{_mandir}/man8/upslog.8.gz
%{_mandir}/man8/upscmd.8.gz
%{_mandir}/man8/upsmon.8.gz
%{_mandir}/man8/upssched.8.gz

%files cgi
%defattr(-,root,root)
%dir %attr(755,root,root) %{CONFPATH}
%config(noreplace) %attr(600,%{nutuser},root) %{CONFPATH}/upsset.conf
%config(noreplace) %attr(644,root,root) %{CONFPATH}/hosts.conf
%attr(644,root,root) %{CONFPATH}/upsstats.html
%attr(644,root,root) %{CONFPATH}/upsstats-single.html
%dir %{CGIPATH}
%{CGIPATH}/upsimage.cgi
%{CGIPATH}/upsset.cgi
%{CGIPATH}/upsstats.cgi

%changelog
* Fri Jan 31 2003 Antonino Albanese <al.an@monkeysweb.net>
- Updated startup files (don't know if they still works on 6x)
- Modified model path in /sbin cause upsdrvctl have to stay on
  an already mounted filesystem
- Create a system user and group %{nutuser} and compile everything
  --with-user=%{nutuser}
- no upspowerdown script anymore. I modify halt script directly
- added missing html files to nut-cgi

* Thu Feb  7 2002 Nigel Metheringham <Nigel.Metheringham@InTechnology.co.uk>
- Rearranged config files again
- Updated startup files (for RH 7x - rpm builds for eith 6x or 7x)
- More man pages
- Macrofied even more of the spec file
- stripped SNMP support for main line, will fully integrate soon

* Fri Feb  1 2002 Nigel Metheringham <Nigel.Metheringham@InTechnology.co.uk>
- Integrated SNMP support back in (from my branch)
- moved the config files around a little - default into samples subdir

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

