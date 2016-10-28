EXTRA_DIST = README

if HAVE_SYSTEMD

systemdsystemunit_DATA = \
        nut-driver-enumerator.service \
        nut-driver@.service \
        nut-monitor.service \
        nut-server.service  \
        nut-driver.target

systemdshutdown_SCRIPTS = nutshutdown

libexec_SCRIPTS = nut-driver-enumerator.sh

else
EXTRA_DIST += nut-driver@.service.in nut-monitor.service.in \
	nut-server.service.in nutshutdown.in nut-driver.target \
	nut-driver-enumerator.sh.in nut-driver-enumerator.service.in
endif

