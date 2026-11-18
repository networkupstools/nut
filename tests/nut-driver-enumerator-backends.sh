#!/bin/sh
# Exercise the configured enumerator without access to the host's service
# managers. Run on Linux with bubblewrap and a configured enumerator path.
set -eu
nde=${1:?Usage: $0 /path/to/configured/nut-driver-enumerator.sh}
fixture="$(dirname "$0")/nut-driver-enumerator-test--backing.conf"
work="$(mktemp -d)"
trap 'rm -rf "$work"' 0
mkdir "$work/bin"

# Only these tools and the dynamic linker/libraries are visible in the
# disposable root. Service management commands share a recording stub.
cat > "$work/bin/manager" <<'STUB'
#!/bin/sh
set -eu
cmd=${0##*/}
{ printf '%s' "$cmd"; printf '\t%s' "$@"; printf '\n'; } >> /work/calls
case "$cmd:$1" in
    systemctl:enable)
        [ "$2" != nut-driver@realups2.service ] || [ ! -f /work/masked-source ] || exit 1
        if [ -f /work/reject-source ]; then
            case "$2" in nut-driver@realups2.service|nut-driver@MD5_5db8b3786b99a62c3872f9a3864dfed7.service) exit 1 ;; esac
        fi
        [ "$2" != "nut-driver@${REJECT}.service" ] || exit 1
        grep -Fx "$2" /work/instances >/dev/null || echo "$2" >> /work/instances ;;
    systemctl:is-enabled)
        [ "$2" = nut-driver@realups2.service ] && [ -f /work/masked-source ] && echo masked
        exit 1 ;;
    systemctl:show)
        # Disabled instances referenced by another loaded unit still appear.
        {
            cat /work/instances
            for f in /work/units/*.d/nut-driver-enumerator-generated.conf; do
                [ ! -f "$f" ] || sed -n 's/^After=//p' "$f" | tr ' ' '\n'
            done
        } | awk '/^nut-driver@/ && !seen[$0]++ { print "Id=" $0 }' ;;
    systemctl:disable)
        awk -v unit="$2" '$0 != unit' /work/instances > /work/remaining
        cat /work/remaining > /work/instances ;;
    systemctl:daemon-reload|systemctl:stop|systemctl:reset-failed) ;;
    systemctl:start)
        while read -r unit; do
            [ -s "/work/units/$unit.d/nut-driver-enumerator-generated-devicename.conf" ] || exit 1
        done < /work/instances
        echo "$3" >> /work/starts ;;
    systemctl:is-active) echo inactive ;;
    svccfg:-s)
        case "$3" in
            add)
                if [ -f /work/reject-source ]; then
                    case "$4" in realups2|MD5_5db8b3786b99a62c3872f9a3864dfed7) exit 1 ;; esac
                fi
                [ "$4" != "$REJECT" ] || exit 1
                echo "svc:/system/power/nut-driver:$4" >> /work/instances ;;
            delete)
                awk -v fmri="svc:/system/power/nut-driver:$4" '$0 != fmri' /work/instances > /work/remaining
                cat /work/remaining > /work/instances
                rm -f /work/nut-driver:"$4"-* ;;
            addpg)
                if [ "$5" = dependency ]; then
                    [ ! -f "/work/$2-$4" ] || exit 1
                    : > "/work/$2-$4"
                fi ;;
            delpg)
                rm -f "/work/$2-$4" "/work/$2-$4-grouping" "/work/$2-$4-entities" ;;
            setprop)
                case "$4" in
                    */grouping) echo "$7" > "/work/$2-${4%/*}-grouping" ;;
                    */SECTION_CHECKSUM*|*/DEVICE) echo "$7" > "/work/$2-${4##*/}" ;;
                    */entities)
                        target="$2-${4%/*}"
                        shift 7
                        values=""
                        while [ "$1" != ')' ]; do
                            values="${values}${values:+ }$1"
                            shift
                        done
                        echo "$values" > "/work/$target-entities" ;;
                esac ;;
        esac ;;
    svcs:-a) cat /work/instances ;;
    svcs:*) grep -Fx "svc:/system/power/$1" /work/instances ;;
    svcprop:-p)
        case "$2" in
            */entities|*/grouping) cat "/work/$3-${2%/*}-${2##*/}"; exit $? ;;
        esac
        case "$3" in
            svc:*) exit 1 ;;
            *) cat "/work/$3-${2##*/}" ;;
        esac ;;
    svcadm:*) ;;
    *) echo "Unexpected $cmd $*" >&2; exit 1 ;;
esac
STUB
chmod +x "$work/bin/manager"

run_nde() {
    action=${1-}
    set -- --die-with-parent --unshare-user --unshare-pid --clearenv \
        --dir /usr/bin --symlink usr/bin /bin --dir /usr/sbin \
        --dev /dev --proc /proc --tmpfs /tmp \
        --bind "$state" /work --ro-bind "$nde" /nde --ro-bind "$state/ups.conf" /ups.conf \
        --setenv PATH /usr/bin --setenv LANG C --setenv UPSCONF /ups.conf \
        --setenv NUT_CONFPATH /work --setenv SYSTEMD_CONFPATH /work/units \
        --setenv SERVICE_FRAMEWORK "$backend" --setenv REJECT "$reject" \
        --setenv AUTO_START yes
    for lib in /lib /lib64 /usr/lib /usr/lib64; do
        [ ! -d "$lib" ] || set -- "$@" --ro-bind "$lib" "$lib"
    done
    for tool in sh sed grep egrep awk tail head sort printf md5sum tr wc \
        date mkdir cat rm expr cut hostname which timeout basename dirname; do
        [ ! -x "/usr/bin/$tool" ] || set -- "$@" --ro-bind "/usr/bin/$tool" "/usr/bin/$tool"
    done
    for tool in bin/systemctl bin/svcs bin/svcprop sbin/svccfg sbin/svcadm; do
        set -- "$@" --ro-bind "$work/bin/manager" "/usr/$tool"
    done
    set -- "$@" /bin/sh /nde
    [ -z "$action" ] || set -- "$@" "$action"
    bwrap "$@"
}

check_dependencies() {
    device=$1
    shift
    case "$backend" in
        systemd)
            file="$state/units/nut-driver@$device.service.d/nut-driver-enumerator-generated.conf"
            if [ "$#" = 0 ]; then [ ! -f "$file" ]; return; fi
            deps=""
            for source do deps="${deps}${deps:+ }nut-driver@$source.service"; done
            printf 'Wants=%s\nAfter=%s\n' "$deps" "$deps" > "$state/expected"
            sed -n '/^Wants=/p; /^Requires=/p; /^After=/p' "$file" > "$state/actual" ;;
        smf)
            # Check individual arguments, including separate parentheses.
            pg=nut-driver-enumerator-generated
            awk -F '\t' -v target="nut-driver:$device" -v pg="$pg" \
                '$1 == "svccfg" && $3 == target && $4 == "setprop" &&
                ($5 == pg "/grouping" || $5 == pg "/entities")' "$state/calls" > "$state/actual"
            if [ "$#" = 0 ]; then [ ! -s "$state/actual" ]; return; fi
            {
                printf 'svccfg\t-s\t%s\tsetprop\t%s/grouping\t=\tastring:\trequire_any\n' "nut-driver:$device" "$pg"
                printf 'svccfg\t-s\t%s\tsetprop\t%s/entities\t=\tfmri:\t(' "nut-driver:$device" "$pg"
                for source do printf '\tsvc:/system/power/nut-driver:%s' "$source"; done
                printf '\t)\n'
            } > "$state/expected" ;;
    esac
    diff -u "$state/expected" "$state/actual"
}

for backend in systemd smf; do
    for reject in '' realups2; do
        state="$work/$backend-${reject:-plain}"
        mkdir "$state"
        cp "$fixture" "$state/ups.conf"
        : > "$state/instances"
        result=0
        run_nde > "$state/stdout" 2> "$state/stderr" || result=$?
        if [ "$result" != 42 ]; then cat "$state/stderr" >&2; exit 1; fi
        second=realups2
        [ -z "$reject" ] || second=MD5_5db8b3786b99a62c3872f9a3864dfed7
        for action in create reconfigure; do
            if [ "$action" = reconfigure ]; then
                : > "$state/calls"
                : > "$state/starts"
                result=0
                run_nde --reconfigure > "$state/stdout" 2> "$state/stderr" || result=$?
                if [ "$result" != 42 ]; then cat "$state/stderr" >&2; exit 1; fi
            fi
            check_dependencies clone-1 realups
            check_dependencies clone-outlet-1 realups
            check_dependencies failover realups "$second"
            check_dependencies file
        done
        if [ "$backend" = systemd ]; then
            sort "$state/instances" > "$state/expected"
            sort "$state/starts" > "$state/actual"
            diff -u "$state/expected" "$state/actual"
        fi
        : > "$state/calls"
        # Changing a clone's backing port must regenerate its existing
        # unit and resolve the already registered (possibly hashed) source.
        sed '/^\[clone-1\]/,/^$/s/port = usbhid-ups-realups$/port = usbhid-ups-realups2/' \
            "$fixture" > "$state/ups.conf"
        result=0
        run_nde > "$state/stdout" 2> "$state/stderr" || result=$?
        if [ "$result" != 42 ]; then cat "$state/stderr" >&2; exit 1; fi
        check_dependencies clone-1 "$second"
        run_nde > "$state/stdout" 2> "$state/stderr"
        # A dependent may already be running with one usable source when
        # another service fails registration and succeeds on the next run.
        : > "$state/reject-source"
        result=0
        run_nde --reconfigure > "$state/stdout" 2> "$state/stderr" || result=$?
        [ "$result" = 13 ]
        rm "$state/reject-source"
        : > "$state/calls"
        result=0
        run_nde > "$state/stdout" 2> "$state/stderr" || result=$?
        if [ "$result" != 42 ]; then cat "$state/stderr" >&2; exit 1; fi
        check_dependencies failover realups "$second"
        run_nde > "$state/stdout" 2> "$state/stderr"
        # Editing a referenced backing section must re-enable its service,
        # even when systemd still reports its disabled unit as loaded.
        sed '/^\[realups\]/a\
    serial = changed' "$state/ups.conf" > "$state/changed"
        cp "$state/changed" "$state/ups.conf"
        : > "$state/calls"
        result=0
        run_nde > "$state/stdout" 2> "$state/stderr" || result=$?
        if [ "$result" != 42 ]; then cat "$state/stderr" >&2; exit 1; fi
        if [ "$backend" = systemd ]; then
            grep -Fx nut-driver@realups.service "$state/instances" >/dev/null
            awk -F '\t' '$1=="systemctl" && $2=="start" && $4=="nut-driver@realups.service" { started=1 } END { exit !started }' "$state/calls"
        fi
        run_nde > "$state/stdout" 2> "$state/stderr"

        if [ "$backend" = systemd ] && [ -z "$reject" ]; then
            # A mask on the plain unit must not be bypassed by enabling
            # a hash. Removing the mask restores the missing edge.
            : > "$state/masked-source"
            result=0
            run_nde --reconfigure > "$state/stdout" 2> "$state/stderr" || result=$?
            [ "$result" = 13 ]
            check_dependencies failover realups
            if grep -F MD5_ "$state/instances" >/dev/null; then exit 1; fi
            rm "$state/masked-source"
            result=0
            run_nde > "$state/stdout" 2> "$state/stderr" || result=$?
            if [ "$result" != 42 ]; then cat "$state/stderr" >&2; exit 1; fi
            check_dependencies failover realups realups2
        fi

        # A proxy can require the local driver but only want networking.
        echo 'DEPREQ_DRV_LOCAL_SYSTEMD=Requires' > "$state/nut-driver-enumerator.conf"
        cat >> "$state/ups.conf" <<'PROXY'
[proxy]
    driver = dummy-ups
    port = realups@localhost
PROXY
        result=0
        run_nde > "$state/stdout" 2> "$state/stderr" || result=$?
        if [ "$result" != 42 ]; then cat "$state/stderr" >&2; exit 1; fi
        case "$backend" in
            systemd)
                file="$state/units/nut-driver@proxy.service.d/nut-driver-enumerator-generated.conf"
                sed -n '/^Wants=/p; /^Requires=/p; /^After=/p' "$file" > "$state/actual"
                printf '%s\n' Wants=network.target Requires=nut-driver@realups.service \
                    'After=network.target nut-driver@realups.service' > "$state/expected"
                diff -u "$state/expected" "$state/actual" ;;
            smf)
                pg="$state/nut-driver:proxy-nut-driver-enumerator-generated"
                [ "$(cat "$pg-grouping")" = optional_all ]
                [ "$(cat "$pg-entities")" = svc:/network/loopback:default ]
                [ "$(cat "$pg-drivers-grouping")" = require_any ]
                [ "$(cat "$pg-drivers-entities")" = svc:/system/power/nut-driver:realups ] ;;
        esac
        run_nde > "$state/stdout" 2> "$state/stderr"
        echo "PASS $backend with ${reject:-no} rejected instance name"
    done
done
