EXTRA_DIST = README \
    avahi/nut.service.in \
    HP-UX/nut-drvctl \
    HP-UX/nut-drvctl.sh \
    HP-UX/nut-upsd \
    HP-UX/nut-upsd.sh \
    HP-UX/nut-upsmon \
    HP-UX/nut-upsmon.sh \
    logrotate/nutlogd \
    misc/nut.bash_completion \
    misc/osd-notify \
    perl/Nut.pm \
    RedHat/halt.patch \
    RedHat/README \
    RedHat/ups \
    RedHat/upsd \
    RedHat/upsd.in \
    RedHat/upsmon \
    RedHat/upsmon.in \
    Solaris8/S99upsmon \
    subdriver/gen-usbhid-subdriver.sh \
    subdriver/gen-snmp-subdriver.sh \
    ufw/README \
    upower/95-upower-hid.rules \
    Windows/halt.c \
    Windows/Makefile

SUBDIRS = augeas devd hotplug python systemd udev Solaris upsdrvsvcctl
if WITH_SNMP
if WITH_NEON
  SUBDIRS += DMF
endif
endif
