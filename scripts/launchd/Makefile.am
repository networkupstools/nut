EXTRA_DIST = README.adoc

if WITH_LAUNCHD

launchdaemon_DATA = \
        org.networkupstools.upsd.plist \
        org.networkupstools.upsmon.plist

### TODO: 'plutil -lint'

else
EXTRA_DIST += \
        org.networkupstools.upsd.plist.in \
        org.networkupstools.upsmon.plist.in
endif

CLEANFILES = \
        org.networkupstools.upsd.plist \
        org.networkupstools.upsmon.plist
