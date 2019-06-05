# Network UPS Tools: conf

INSTALL_0600 = $(INSTALL) -m 0600

SECFILES = upsd.conf.sample upsd.users.sample
PUBFILES = nut.conf.sample ups.conf.sample
CGIPUB = hosts.conf.sample upsset.conf.sample upsstats.html.sample	\
 upsstats-single.html.sample

if WITH_CGI
 CGI_INSTALL = $(CGIPUB)
else
 CGI_INSTALL = 
endif

dist_sysconf_DATA = $(SECFILES) $(PUBFILES) $(CGI_INSTALL)
nodist_sysconf_DATA = upssched.conf.sample upsmon.conf.sample

SPELLCHECK_SRC = $(dist_sysconf_DATA) \
 upssched.conf.sample.in upsmon.conf.sample.in

spellcheck spellcheck-interactive spellcheck-sortdict:
	$(MAKE) -f $(top_builddir)/docs/Makefile SPELLCHECK_SRC="$(SPELLCHECK_SRC)" $@
