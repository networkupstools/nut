#!/bin/sh

# This is a CI test for nut-driver-enumerator-test.sh
# using chosen SHELL_PROGS (passed from caller envvars)

cd "${REPO_DIR}/tests" || exit
BUILDDIR="`pwd`"
SRCDIR="`pwd`"
export BUILDDIR SRCDIR

# Travis uses Ubuntu 14.04 which does not yet have systemd (16.04 should)
# The self-tests do not (yet) use the actual OS framework, so just let them run
if [ ! -x /bin/systemctl ] ; then
    SERVICE_FRAMEWORK="selftest"
    export SERVICE_FRAMEWORK
fi

printf "\n=== `date -u` : Will test nut-driver-enumerator interpreted by: "
if [ -n "${SHELL_PROGS}" ] ; then
    echo "$SHELL_PROGS"
else
    echo "default system '/bin/sh'"
fi

# Set this to enable verbose tracing
case "${CI_TRACE-}" in
    [Yy][Ee][Ss]|[Oo][Nn]|[Tt][Rr][Uu][Ee])
        ls -la
        DEBUG=trace ; export DEBUG
        ;;
esac

$CI_TIME ./nut-driver-enumerator-test.sh
