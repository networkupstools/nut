#!/bin/sh

# This script allows to enumerate UPSes in order to produce the
# individual service unit instances for each defined configuration.
# It assumes the user has adequate permissions to inspect and create
# services (e.g. is a root or has proper RBAC profiles to do so).
# TODO: Complete the sample basic support for Solaris SMF (needs the
# service definitions similar to systemd NUT units to be added first).
#
# \Author:  Jim Klimov <EvgenyKlimov@eaton.com>

[ -z "${UPSCONF-}" ] && \
    UPSCONF="@sysconfdir@/ups.conf"

if [ -z "${SERVICE_FRAMEWORK-}" ] ; then
    [ -x /usr/sbin/svcadm ] && [ -x /usr/sbin/svccfg ] && [ -x /usr/bin/svcs ] && \
        SERVICE_FRAMEWORK="smf"
    [ -z "${SERVICE_FRAMEWORK-}" ] && \
        [ -x /bin/systemctl ] && \
        SERVICE_FRAMEWORK="systemd"
fi

# List of configured UPSes in the config-file
UPSLIST_FILE=""
# List of configured service instances for UPS drivers
UPSLIST_SVCS=""
hook_registerInstance=""
hook_unregisterInstance=""
hook_listInstances=""

case "${SERVICE_FRAMEWORK-}" in
    smf)
        hook_registerInstance="smf_registerInstance"
        hook_unregisterInstance="smf_unregisterInstance"
        hook_listInstances="smf_listInstances"
        ;;
    systemd)
        hook_registerInstance="systemd_registerInstance"
        hook_unregisterInstance="systemd_unregisterInstance"
        hook_listInstances="systemd_listInstances"
        ;;
    "")
        echo "Error detecting the service-management framework on this OS" >&2
        exit 1
        ;;
    *)
        echo "Error: User provided an unknown service-management framework '$SERVICE_FRAMEWORK'" >&2
        exit 1
        ;;
esac

common_isFiled() {
    [ -n "$UPSLIST_FILE" ] && \
    for UPSF in $UPSLIST_FILE ; do
        [ "$1" = "$UPSF" ] && return 0
    done
    return 1
}

common_isRegistered() {
    [ -n "$UPSLIST_SVCS" ] && \
    for UPSS in $UPSLIST_SVCS ; do
        [ "$1" = "$UPSS" ] && return 0
    done
    return 1
}

smf_registerInstance() {
    echo "Solaris SMF support was recognized but currently is not implemented" >&2
}
smf_unregisterInstance() {
    echo "Solaris SMF support was recognized but currently is not implemented" >&2
}
smf_listInstances() {
    /usr/bin/svcs -a -H -o fmri | egrep '/nut-driver:' | sed 's/^.*://' | sort -n
}

systemd_registerInstance() {
    /bin/systemctl enable 'nut-driver@'"$1"
}
systemd_unregisterInstance() {
    /bin/systemctl stop 'nut-driver@'"$1" || false
    /bin/systemctl disable 'nut-driver@'"$1"
}
systemd_listInstances() {
    /bin/systemctl show 'nut-driver@*' -p Id | egrep '=nut-driver' | sed -e 's/^.*@//' -e 's/\.service$//' | sort -n
}

################# MAIN PROGRAM

if [ -s "$UPSCONF" ] ; then
    UPSLIST_FILE="`egrep '^[ \t]*\[.*\][ \t]*$' "$UPSCONF" | sed 's,^[ \t]*\[\(.*\)\][ \t]*$,\1,' | sort -n`" || UPSLIST_FILE=""
    if [ -z "$UPSLIST_FILE" ] ; then
        echo "Error reading the '$UPSCONF' file or it does not declare any device configurations" >&2
    fi
else
    echo "The '$UPSCONF' file does not exist or is empty" >&2
fi

UPSLIST_SVCS="`$hook_listInstances`" || UPSLIST_SVCS=""
if [ -z "$UPSLIST_SVCS" ] ; then
    echo "Error reading the list of service instances for UPS drivers, or none are defined" >&2
fi

if [ -n "$UPSLIST_FILE" ]; then
    for UPSF in $UPSLIST_FILE ; do
        if ! common_isRegistered "$UPSF" ; then
            echo "Adding new ${SERVICE_FRAMEWORK} service instance for power device [${UPSF}]..." >&2
            $hook_registerInstance "$UPSF"
        fi
    done

    UPSLIST_SVCS="`$hook_listInstances`" || UPSLIST_SVCS=""
    if [ -z "$UPSLIST_SVCS" ] ; then
        echo "Error reading the list of service instances for UPS drivers, or none are defined" >&2
    fi
fi

if [ -n "$UPSLIST_SVCS" ]; then
    for UPSS in $UPSLIST_SVCS ; do
        if ! common_isFiled "$UPSS" ; then
            echo "Dropping old ${SERVICE_FRAMEWORK} service instance for power device [${UPSS}] which is no longer in config file..." >&2
            $hook_unregisterInstance "$UPSS"
        fi
    done
fi

UPSLIST_SVCS="`$hook_listInstances`" || UPSLIST_SVCS=""
if [ -n "$UPSLIST_SVCS" ] ; then
    echo "=== The currently defined service instances are:"
    echo "$UPSLIST_SVCS"
else
    echo "Error reading the list of service instances for UPS drivers, or none are defined" >&2
fi

if [ -n "$UPSLIST_FILE" ] ; then
    echo "=== The currently defined configurations in '$UPSCONF' are:"
    echo "$UPSLIST_FILE"
fi
