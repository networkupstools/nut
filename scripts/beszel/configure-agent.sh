#!/bin/sh
# Configure an existing systemd-managed Beszel agent to collect NUT data.

set -eu

usage() {
	cat <<'EOF'
Usage: configure-agent.sh [--server HOST] [--interval DURATION]
                          [--devices NAME[,NAME...]] [--apply]

Check an existing NUT server and print a Beszel agent systemd drop-in.
With --apply, install the drop-in and restart an active Beszel agent.
Requires NUT 2.8.5 or newer and a Beszel hub and agent with NUT support.

Options:
  --server HOST       NUT server hostname or host:port (default: localhost)
  --interval DURATION Beszel collection interval (default: 60s)
  --devices NAMES     Comma-separated UPS names (default: discover all)
  --apply             Install the drop-in; requires root
  -h, --help          Show this help
EOF
}

die() {
	printf 'configure-agent.sh: %s\n' "$*" >&2
	exit 1
}

server=localhost
interval=60s
devices=
apply=false

while [ "$#" -gt 0 ]; do
	case "$1" in
		--server|--interval|--devices)
			[ "$#" -ge 2 ] || die "missing value for $1"
			case "$1" in
				--server) server=$2 ;;
				--interval) interval=$2 ;;
				--devices) devices=$2 ;;
			esac
			shift 2
			;;
		--apply) apply=true; shift ;;
		-h|--help) usage; exit 0 ;;
		*) die "unknown option: $1" ;;
	esac
done

# Keep systemd Environment values literal; do not allow quotes, spaces or
# control characters into the generated unit file.
case "$server" in
	''|*[!A-Za-z0-9.:-]*|[!A-Za-z0-9]*) die 'invalid NUT server' ;;
esac
case "$interval" in
	''|*[!A-Za-z0-9]*) die 'invalid interval' ;;
esac
printf '%s\n' "$interval" | grep -Eq '^[1-9][0-9]*(ms|s|m|h)$' || die 'interval must be a positive duration such as 60s'
if [ -n "$devices" ]; then
	case "$devices" in
		*[!A-Za-z0-9_.,-]*) die 'invalid comma-separated UPS names' ;;
	esac
	printf '%s\n' "$devices" | grep -Eq '^[A-Za-z0-9_][A-Za-z0-9_.-]*(,[A-Za-z0-9_][A-Za-z0-9_.-]*)*$' || die 'invalid comma-separated UPS names'
fi

command -v upsc >/dev/null 2>&1 || die 'upsc is not installed; configure NUT first'
if [ -n "$devices" ]; then
	old_ifs=$IFS
	IFS=,
	for device in $devices; do
		if ! output=$(upsc -j "$device@$server" 2>&1); then
			die "cannot query $device@$server: $output"
		fi
		printf '%s\n' "$output" | grep -Eq '^[[:space:]]*\{' || die "upsc did not return JSON for $device@$server"
	done
	IFS=$old_ifs
else
	if ! output=$(upsc -L -j "$server" 2>&1); then
		die "cannot query NUT device list at $server: $output"
	fi
	printf '%s\n' "$output" | grep -Eq '^[[:space:]]*\{' || die 'upsc did not return JSON; NUT 2.8.5 or newer is required'
fi

write_dropin() {
	printf '[Service]\n'
	printf 'Environment="NUT_SERVER=%s"\n' "$server"
	printf 'Environment="NUT_INTERVAL=%s"\n' "$interval"
	if [ -n "$devices" ]; then
		printf 'Environment="NUT_DEVICES=%s"\n' "$devices"
	fi
}

if [ "$apply" = false ]; then
	write_dropin
	exit 0
fi

[ "$(id -u)" -eq 0 ] || die '--apply requires root'
command -v systemctl >/dev/null 2>&1 || die '--apply requires systemd'
systemctl cat beszel-agent.service >/dev/null 2>&1 || die 'beszel-agent.service is not installed'

unit_dir=/etc/systemd/system/beszel-agent.service.d
unit_file=$unit_dir/nut.conf
install -d -m 755 "$unit_dir"
tmp_file=$(mktemp "$unit_dir/.nut.conf.XXXXXX") || die 'cannot create temporary drop-in'
trap 'rm -f "$tmp_file"' 0
trap 'exit 1' 1 2 15
write_dropin > "$tmp_file"
chmod 644 "$tmp_file"

if [ -e "$unit_file" ] || [ -L "$unit_file" ]; then
	if cmp -s "$tmp_file" "$unit_file"; then
		printf 'Beszel NUT drop-in is already current: %s\n' "$unit_file"
		exit 0
	fi
	die "$unit_file already exists with different contents; review it before changing it"
fi

mv "$tmp_file" "$unit_file"
trap - 0 1 2 15
systemctl daemon-reload
if systemctl is-active --quiet beszel-agent.service; then
	systemctl restart beszel-agent.service
	printf 'Updated and restarted beszel-agent.service using %s\n' "$unit_file"
else
	printf 'Installed %s; beszel-agent.service is stopped, so start it when ready\n' "$unit_file"
fi
