EXTRA_DIST = README

if HAVE_SYSTEMD

systemdsystemunit_DATA = \
        nut-driver.service \
        nut-monitor.service \
        nut-server.service

EXTRA_DIST += nut-driver.target

# Oops, we mis-picked provisioning of nut-driver.target from another
# branch some time ago. FIXME: Remove this dummy line when the real
# branch with this file is merged in the same codebase!
nut-driver.target:
	@touch "$@"

systemdshutdown_SCRIPTS = nutshutdown

else
EXTRA_DIST += nut-driver.service.in nut-monitor.service.in \
	nut-server.service.in nutshutdown.in
endif

