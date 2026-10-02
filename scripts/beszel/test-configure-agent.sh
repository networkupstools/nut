#!/bin/sh
# Exercise the non-privileged preview and validation paths with a mock upsc.

set -eu

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
test_dir=$(mktemp -d) || exit 1
trap 'rm -r "$test_dir"' 0
trap 'exit 1' 1 2 15

cat > "$test_dir/upsc" <<'EOF'
#!/bin/sh
case "${MOCK_UPSC_MODE:-json}:$*" in
	'json:-L -j localhost') printf '{"apc":"APC UPS"}\n' ;;
	'json:-j apc@localhost') printf '{"ups.status":"OL"}\n' ;;
	'devices_only:-j apc@localhost') printf '{"ups.status":"OL"}\n' ;;
	'legacy:-L -j localhost') printf 'Usage: upsc [options]\n' ;;
	'failure:-L -j localhost') exit 1 ;;
	*) exit 2 ;;
esac
EOF
chmod 755 "$test_dir/upsc"
PATH="$test_dir:$PATH"
export PATH

sh "$script_dir/configure-agent.sh" > "$test_dir/actual"
printf '[Service]\nEnvironment="NUT_SERVER=localhost"\nEnvironment="NUT_INTERVAL=60s"\n' > "$test_dir/expected"
diff -u "$test_dir/expected" "$test_dir/actual"

sh "$script_dir/configure-agent.sh" --devices apc --interval 2m > "$test_dir/actual"
printf '[Service]\nEnvironment="NUT_SERVER=localhost"\nEnvironment="NUT_INTERVAL=2m"\nEnvironment="NUT_DEVICES=apc"\n' > "$test_dir/expected"
diff -u "$test_dir/expected" "$test_dir/actual"

# Configured devices do not require device-list permission on the NUT server.
MOCK_UPSC_MODE=devices_only; export MOCK_UPSC_MODE
sh "$script_dir/configure-agent.sh" --devices apc > "$test_dir/actual"
MOCK_UPSC_MODE=json; export MOCK_UPSC_MODE

expect_failure() {
	if "$@" > "$test_dir/actual" 2>&1; then
		printf 'Expected failure: %s\n' "$*" >&2
		exit 1
	fi
}

expect_failure sh "$script_dir/configure-agent.sh" --interval 0s
expect_failure sh "$script_dir/configure-agent.sh" --devices 'apc;other'
expect_failure sh "$script_dir/configure-agent.sh" --server 'localhost
[Service]'
MOCK_UPSC_MODE=legacy; export MOCK_UPSC_MODE
expect_failure sh "$script_dir/configure-agent.sh"
MOCK_UPSC_MODE=failure; export MOCK_UPSC_MODE
expect_failure sh "$script_dir/configure-agent.sh"

printf 'configure-agent.sh tests passed\n'
