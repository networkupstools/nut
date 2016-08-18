Format: 1.0
Source: nut
Binary: nut, nut-server, nut-client, nut-cgi, nut-snmp, nut-ipmi, nut-xml, nut-powerman-pdu, nut-doc, libupsclient4, libupsclient-dev, libnutclient0, libnutclient-dev, python-nut, nut-monitor, libups-nut-perl
Architecture: any all
Version: 2.7.4-11
Maintainer: Arnaud Quette <aquette@debian.org>
Uploaders: Laurent Bigonville <bigon@debian.org>
Homepage: http://www.networkupstools.org/
Standards-Version: 3.9.6
Vcs-Browser: http://anonscm.debian.org/gitweb/?p=collab-maint/nut.git;a=summary
Vcs-Git: git://anonscm.debian.org/collab-maint/nut.git
Testsuite: autopkgtest
Build-Depends: debhelper (>= 8.1.3), cdbs (>= 0.4.122~), autotools-dev, dh-autoreconf, dh-systemd (>= 1.14), libgd-dev | libgd2-xpm-dev | libgd2-noxpm-dev, libsnmp-dev | libsnmp9-dev, libusb-dev (>= 0.1.8), libneon27-gnutls-dev | libneon27-dev, libpowerman0-dev (>= 2.3.3), libwrap0-dev (>= 7.6), python (>= 2.6.6-3~), libfreeipmi-dev (>= 0.8.5) [!hurd-i386], libipmimonitoring-dev (>= 1.1.5-2) [!hurd-i386], libnss3-dev, libtool, libltdl-dev, liblua5.1-0-dev, lua5.1
Build-Depends-Indep: asciidoc (>= 8.6.3), docbook-xsl, dblatex (>= 0.2.5), libxml2-utils
Package-List:
 libnutclient-dev deb libdevel optional arch=any
 libnutclient0 deb libs optional arch=any
 libnutscan-dev deb libdevel optional arch=any
 libnutscan1 deb libs optional arch=any
 libups-nut-perl deb perl optional arch=all
 libupsclient-dev deb libdevel optional arch=any
 libupsclient4 deb libs optional arch=any
 nut deb metapackages optional arch=all
 nut-cgi deb admin optional arch=any
 nut-client deb admin optional arch=any
 nut-doc deb doc optional arch=all
 nut-ipmi deb admin optional arch=linux-any,kfreebsd-any
 nut-monitor deb admin optional arch=all
 nut-powerman-pdu deb admin extra arch=any
 nut-server deb admin optional arch=any
 nut-snmp deb admin optional arch=any
 nut-xml deb admin optional arch=any
 python-nut deb python optional arch=all
# DEBTRANSFORM-TAR: nut-2.7.4-DMF+daisychain-cb7c7ea8b0d.tar.gz
