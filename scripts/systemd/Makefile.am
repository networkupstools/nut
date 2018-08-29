EXTRA_DIST = README

if HAVE_SYSTEMD

systemdsystemunit_DATA = \
        nut-driver-enumerator.service \
        nut-driver-enumerator.path \
        nut-driver@.service \
        nut-monitor.service \
        nut-server.service  \
        nut-driver.target   \
        nut.target

EXTRA_DIST += nut-driver.target nut.target

systemdshutdown_SCRIPTS = nutshutdown

libexec_SCRIPTS = ../upsdrvsvcctl/nut-driver-enumerator.sh

sbin_SCRIPTS = ../upsdrvsvcctl/upsdrvsvcctl

else
EXTRA_DIST += nut-driver@.service.in nut-monitor.service.in \
	nut-server.service.in nutshutdown.in nut-driver.target nut.target \
	nut-driver-enumerator.path.in nut-driver-enumerator.service.in
endif

