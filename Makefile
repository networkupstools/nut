# Network UPS Tools: top level

# directory definitions
prefix		= /usr/local/ups
exec_prefix	= ${prefix}

BINDIR		= $(DESTDIR)${exec_prefix}/bin
SBINDIR		= $(DESTDIR)${exec_prefix}/sbin
CONFPATH	= $(DESTDIR)${prefix}/etc
CGIPATH		= $(DESTDIR)${prefix}/cgi-bin
DRVPATH		= $(DESTDIR)${prefix}/bin

INSTALLDIRS	= $(CONFPATH) $(DRVPATH) $(BINDIR) $(SBINDIR)

SUBDIRS	= common drivers server clients
CFLAGS	= -I../include -O -Wall -Wsign-compare

# Permissions for executables
INSTALLPERMS = 0755
INSTALLCMD = /usr/bin/install -c

all: build

build:
	@for i in $(SUBDIRS); do \
		echo $$i/; cd $$i; $(MAKE) || exit 1; cd ..; \
	done

clean:
	@for i in $(SUBDIRS) lib; do \
		cd $$i; $(MAKE) clean; cd ..;\
	done
	@rm -f scripts/hotplug/libhidups

distclean: clean
	-cp Makefile.dist Makefile
	-rm -rf autom4te.cache
	-rm -f include/config.h include/config.h.in~
	-rm -f config.status config.cache config.log
	@for i in $(SUBDIRS) conf man data lib; do \
		rm $$i/Makefile; \
	done

install: install-dirs install-bin install-man install-data

install-bin:
	@for i in $(SUBDIRS); do \
		cd $$i; $(MAKE) install; cd ..; \
	done

install-man:
	@cd man; $(MAKE) install; cd ..;

install-conf:
	cd conf; $(MAKE) install; cd ..

install-data:
	cd data; $(MAKE) install; cd ..

install-dirs:
	@for d in $(INSTALLDIRS); do \
		if (test ! -d $$d) then \
			./install-sh -d $$d || exit 1; \
		fi \
	done

# the target changes based on whether you did configure --with-cgi or not
cgi: build-cgi-fake

build-cgi-fake:
	@echo "Error: configured without CGI support."
	@echo "Run 'configure --with-cgi' before doing 'make cgi'."

build-cgi:
	@cd clients; $(MAKE) cgi; cd ..;

install-cgi: cgi install-cgi-dir install-dirs install-cgi-bin install-cgi-man install-cgi-html

install-cgi-dir:
	if (test ! -d $(CGIPATH)) then \
		./install-sh -d $(CGIPATH) || exit 1; \
	fi \

install-cgi-bin:
	@cd clients; $(MAKE) install-cgi-bin; cd ..;

install-cgi-man:
	@cd man; $(MAKE) install-cgi-man; cd ..;

install-cgi-conf:
	@cd conf; $(MAKE) install-cgi-conf; cd ..;

install-cgi-html:
	@cd data/html; $(MAKE) install-cgi-html; cd ../..;

install-lib:
	@cd clients; $(MAKE) install-lib; cd ..;
	@cd man; $(MAKE) install-lib-man; cd ..;
	@cd lib; $(MAKE) install-lib; cd ..;

usb: build-usb

build-usb:
	@cd drivers; $(MAKE) hidups; cd ..;
	@cd drivers; $(MAKE) newhidups; cd ..;
	@cd drivers; $(MAKE) energizerups; cd ..;
	@cd drivers; $(MAKE) bcmxcp_usb; cd ..;

install-usb:
	@cd drivers; $(MAKE) install-usb; cd ..;
	@cd man; $(MAKE) install-usb-man; cd ..;

snmp: build-snmp

build-snmp:
	@cd drivers; $(MAKE) snmp-ups; cd ..;

install-snmp: snmp install-snmp-man install-snmp-mgr

install-snmp-mgr:
	@cd drivers; $(MAKE) install-snmp; cd ..;

install-snmp-man:
	@cd man; $(MAKE) install-snmp-man; cd ..;

# This is only used to set the "official" version before it goes out the door.
#
rhver = $(shell echo $(SETVER) | sed s/\-//g)
setver:
	@if (test -z "$(SETVER)") then \
		echo "SETVER not defined"; \
		exit; \
	fi;
	@echo "Setting version to $(SETVER)";
	@if (test -f "include/version.h") then \
		echo "#define UPS_VERSION \"$(SETVER)\"" > include/version.h; \
	else \
		echo "include/version.h not found"; \
		exit; \
	fi;
	@if (test -f packaging/RedHat/nut.spec.in) then \
		cat packaging/RedHat/nut.spec.in | sed s/@NUT-VERSION@/$(rhver)/g > packaging/RedHat/nut.spec; \
	else \
		echo "nut.spec.in not found"; \
		exit ; \
	fi
	@echo $(SETVER) > include/version;
