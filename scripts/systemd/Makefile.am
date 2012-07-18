EXTRA_DIST = README

if HAVE_SYSTEMD

systemdsystemunit_DATA = \
        nut-driver.service \
        nut-monitor.service \
        nut-server.service

systemdsystemshutdown_SCRIPTS = nutshutdown

else
EXTRA_DIST += nut-driver.service.in nut-monitor.service.in \
	nut-server.service.in nutshutdown.in
endif

