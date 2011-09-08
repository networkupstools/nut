EXTRA_DIST = README

if HAVE_SYSTEMD
%.service: %.service.in
	sed -e 's,@sbindir\@,$(sbindir),g' $< > $@

nutshutdown: nutshutdown.in
	sed -e 's,@sbindir\@,$(sbindir),g' $< > $@

systemdsystemunit_DATA = \
        nut-driver.service \
        nut-monitor.service \
        nut-server.service

systemdsystemshutdown_SCRIPTS = nutshutdown

else
EXTRA_DIST += nut-driver.service.in nut-monitor.service.in \
	nut-server.service.in nutshutdown.in
endif

