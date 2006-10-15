# Network UPS Tools: conf

INSTALL_0600 = $(INSTALL) -m 0600

SECFILES = upsmon.conf upsd.conf upsd.users
PUBFILES = upssched.conf ups.conf
CGIPUB   = hosts.conf upsset.conf upsstats.html upsstats-single.html

if WITH_CGI
 CGI_INSTALL = $(CGIPUB)
else
 CGI_INSTALL = 
endif

dist_noinst_DATA = $(SECFILES) $(PUBFILES) $(CGI_INSTALL)

# Automake does not support rename-on-install, so we do it "by hand". 

install-data-local:
	test -z "$(sysconfdir)" || $(mkdir_p) "$(DESTDIR)$(sysconfdir)"
	for f in $(SECFILES) ; do \
		$(INSTALL_0600) "$(srcdir)/$$f" "$(DESTDIR)$(sysconfdir)/$$f.sample"; \
	done
	for f in $(PUBFILES) ; do \
		$(INSTALL_DATA) "$(srcdir)/$$f" "$(DESTDIR)$(sysconfdir)/$$f.sample"; \
	done
	for f in $(CGI_INSTALL) ; do \
		$(INSTALL_DATA) "$(srcdir)/$$f" "$(DESTDIR)$(sysconfdir)/$$f.sample"; \
	done

uninstall-local:
	for f in $(SECFILES) ; do \
		rm -f "$(DESTDIR)$(sysconfdir)/$$f.sample"; \
	done
	for f in $(PUBFILES) ; do \
		rm -f "$(DESTDIR)$(sysconfdir)/$$f.sample"; \
	done
	for f in $(CGI_INSTALL) ; do \
		rm -f "$(DESTDIR)$(sysconfdir)/$$f.sample"; \
	done

