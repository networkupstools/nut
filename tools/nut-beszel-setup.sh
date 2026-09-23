#!/bin/sh
################################################################################
#
# nut-beszel-setup
#   Setup script for integrating NUT with Beszel monitoring.
#   Installs NUT, auto-discovers UPS/PDU devices, configures the NUT server,
#   and installs the Beszel agent with NUT data collection enabled.
#
#   Target: Debian/Ubuntu (apt-based)
#
#   Usage:
#     sudo ./nut-beszel-setup.sh [options]
#
#   Options:
#     --beszel-hub <url>     Beszel hub WebSocket URL (default: ws://localhost:8090)
#     --beszel-token <token> Beszel agent auth token
#     --nut-server <host>    NUT server hostname (default: localhost)
#     --nut-interval <sec>   NUT data refresh interval in seconds (default: 60)
#     --skip-install         Skip package installation (assume NUT is installed)
#     --skip-agent           Skip Beszel agent installation
#     --dry-run              Show what would be done without making changes
#
################################################################################

set -eu

# Defaults
BESZEL_HUB="ws://localhost:8090"
BESZEL_TOKEN=""
NUT_SERVER="localhost"
NUT_INTERVAL=60
SKIP_INSTALL=false
SKIP_AGENT=false
DRY_RUN=false

# Colors (if tty)
if [ -t 1 ]; then
	RED='\033[0;31m'
	GREEN='\033[0;32m'
	YELLOW='\033[1;33m'
	NC='\033[0m'
else
	RED=''
	GREEN=''
	YELLOW=''
	NC=''
fi

info() { printf "${GREEN}[INFO]${NC} %s\n" "$*"; }
warn() { printf "${YELLOW}[WARN]${NC} %s\n" "$*"; }
err() { printf "${RED}[ERROR]${NC} %s\n" "$*" >&2; }
die() { err "$*"; exit 1; }

run() {
	if [ "$DRY_RUN" = true ]; then
		printf "  [dry-run] %s\n" "$*"
	else
		"$@"
	fi
}

# Parse arguments
while [ $# -gt 0 ]; do
	case "$1" in
		--beszel-hub)
			BESZEL_HUB="$2"
			shift 2
			;;
		--beszel-token)
			BESZEL_TOKEN="$2"
			shift 2
			;;
		--nut-server)
			NUT_SERVER="$2"
			shift 2
			;;
		--nut-interval)
			NUT_INTERVAL="$2"
			shift 2
			;;
		--skip-install)
			SKIP_INSTALL=true
			shift
			;;
		--skip-agent)
			SKIP_AGENT=true
			shift
			;;
		--dry-run)
			DRY_RUN=true
			shift
			;;
		-h|--help)
			sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
			exit 0
			;;
		*)
			die "Unknown option: $1 (use --help for usage)"
			;;
	esac
done

# Check for root
if [ "$(id -u)" -ne 0 ] && [ "$DRY_RUN" = false ]; then
	die "This script must be run as root (use sudo)"
fi

# Check OS
if [ "$DRY_RUN" = false ]; then
	if [ ! -f /etc/os-release ]; then
		die "Cannot detect OS (missing /etc/os-release)"
	fi
	. /etc/os-release
	case "$ID" in
		debian|ubuntu)
			info "Detected ${NAME} ${VERSION_ID}"
			;;
		*)
			warn "Unsupported OS: ${ID}. This script is designed for Debian/Ubuntu."
			warn "Continuing, but package names may differ."
			;;
	esac
fi

################################################################################
# Step 1: Install NUT packages
################################################################################
if [ "$SKIP_INSTALL" = false ]; then
	info "Installing NUT packages..."
	if [ "$DRY_RUN" = false ]; then
		apt-get update -qq
		DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
			nut \
			nut-server \
			nut-client \
			nut-scanner
		info "NUT packages installed."
	else
		run apt-get update -qq
		run apt-get install -y nut nut-server nut-client nut-scanner
	fi
else
	info "Skipping NUT installation (--skip-install)"
fi

################################################################################
# Step 2: Auto-discover UPS/PDU devices
################################################################################
info "Scanning for UPS/PDU devices..."

NUT_CONF_DIR="/etc/nut"
UPS_CONF="${NUT_CONF_DIR}/ups.conf"
UPS_MON_CONF="${NUT_CONF_DIR}/upsmon.conf"
UPSD_CONF="${NUT_CONF_DIR}/upsd.conf"
UPSD_USER="nut"
UPSD_GROUP="nut"

DISCOVERED=""

if [ "$DRY_RUN" = false ]; then
	# Try nut-scanner with multiple methods
	info "Running nut-scanner (USB, SNMP, NUT, Avahi)..."
	SCANNER_OUTPUT=""

	# USB scan
	if command -v nut-scanner-usb >/dev/null 2>&1; then
		info "  Scanning USB..."
		SCANNER_OUTPUT="${SCANNER_OUTPUT}$(nut-scanner-usb 2>/dev/null || true)"
	fi

	# NUT scan (existing NUT servers on network)
	if command -v nut-scanner-nut >/dev/null 2>&1; then
		info "  Scanning NUT (network)..."
		SCANNER_OUTPUT="${SCANNER_OUTPUT}$(nut-scanner-nut 2>/dev/null || true)"
	fi

	# SNMP scan
	if command -v nut-scanner-snmp >/dev/null 2>&1; then
		info "  Scanning SNMP..."
		SCANNER_OUTPUT="${SCANNER_OUTPUT}$(nut-scanner-snmp 2>/dev/null || true)"
	fi

	# Avahi/mDNS scan
	if command -v nut-scanner-avahi >/dev/null 2>&1; then
		info "  Scanning Avahi/mDNS..."
		SCANNER_OUTPUT="${SCANNER_OUTPUT}$(nut-scanner-avahi 2>/dev/null || true)"
	fi

	# Parse discovered devices
	# nut-scanner output format:
	#   nut_scanner: device found
	#   nut_scanner : [usb]    001:005: USB connected UPS (vendor product busid)
	#   nut_scanner : [nut]    host:port: NUT UPS (driver)
	#   nut_scanner : [snmp]   host: MIB-based UPS (driver)

	DISCOVERED=$(echo "$SCANNER_OUTPUT" | grep -E '^\s*nut_scanner\s*:' | grep -v 'device found' || true)

	if [ -z "$DISCOVERED" ]; then
		warn "No UPS/PDU devices discovered automatically."
		warn "You will need to manually configure ${UPS_CONF}."
		warn "See: https://networkupstools.org/stable.html"
	else
		info "Discovered devices:"
		echo "$DISCOVERED" | sed 's/^/  /'
	fi
else
	info "[dry-run] Would run nut-scanner to discover devices"
fi

################################################################################
# Step 3: Configure NUT server
################################################################################
info "Configuring NUT server..."

if [ "$DRY_RUN" = false ]; then
	# Create nut user/group if not present (package usually does this)
	if ! id "$UPSD_USER" >/dev/null 2>&1; then
		useradd --system --no-create-home --shell /usr/sbin/nologin "$UPSD_USER" 2>/dev/null || true
		groupadd "$UPSD_GROUP" 2>/dev/null || true
		usermod -g "$UPSD_GROUP" "$UPSD_USER" 2>/dev/null || true
	fi

	# Ensure config directory exists
	mkdir -p "$NUT_CONF_DIR"

	# Generate ups.conf from discovered devices if not already configured
	if [ ! -s "$UPS_CONF" ] || ! grep -q '^\[' "$UPS_CONF" 2>/dev/null; then
		info "Generating ${UPS_CONF}..."
		{
			echo "# NUT UPS configuration"
			echo "# Generated by nut-beszel-setup.sh on $(date -u +%Y-%m-%dT%H:%M:%SZ)"
			echo ""
			if [ -n "$DISCOVERED" ]; then
				# Parse each discovered device and generate config
				echo "$DISCOVERED" | while IFS= read -r line; do
					# Extract device info from scanner output
					# Format varies by scanner type
					driver=$(echo "$line" | sed -n 's/.*(\([^)]*\)).*/\1/p' | head -1)
					[ -z "$driver" ] && driver="usbhid-ups"
					port=$(echo "$line" | sed -n 's/.*:\s*\([^:()]*\).*/\1/p' | head -1)
					[ -z "$port" ] && port="auto"
					name="ups1"
					echo "[$name]"
					echo "  driver=$driver"
					echo "  port=$port"
					echo "  desc=Auto-discovered UPS"
					echo ""
				done
			else
				# Fallback: dummy UPS for testing
				echo "[dummy]"
				echo "  driver=dummy-ups"
				echo "  port=none"
				echo "  desc=Dummy UPS (replace with real device)"
				echo ""
			fi
		} > "$UPS_CONF"
		chmod 640 "$UPS_CONF"
		chown root:"$UPSD_GROUP" "$UPS_CONF" 2>/dev/null || true
	else
		info "Existing ${UPS_CONF} found, leaving unchanged."
	fi

	# Configure upsd.conf
	if [ ! -f "$UPSD_CONF" ]; then
		info "Generating ${UPSD_CONF}..."
		cat > "$UPSD_CONF" <<EOF
# NUT server configuration
# Generated by nut-beszel-setup.sh
UPSSET_VAR DEFAULT_POLLinterval 2
MAXAGE 5+5
DATALIMIT 250
TIMERFD yes

LISTEN 127.0.0.1
LISTEN ::1
EOF
		chmod 640 "$UPSD_CONF"
		chown root:"$UPSD_GROUP" "$UPSD_CONF" 2>/dev/null || true
	fi

	# Configure upsmon.conf
	if [ ! -f "$UPS_MON_CONF" ]; then
		info "Generating ${UPS_MON_CONF}..."
		cat > "$UPS_MON_CONF" <<EOF
# NUT upsmon configuration
# Generated by nut-beszel-setup.sh
NOCOMM
NOTIFYFLAG ONBATTERY WRN
NOTIFYFLAG ONBATTERY SYS Wall
NOTIFYFLAG ONBATTERY EXEC
NOTIFYFLAG OFFBATTERY EXEC
NOTIFYFLAG ONLINE EXEC
NOTIFYFLAG LOWBATT WRN
NOTIFYFLAG LOWBATT SYS Wall
NOTIFYFLAG LOWBATT EXEC
NOTIFYFLAG FSD EXEC
NOTIFYFLAG DSBATTS EXEC
NOTIFYFLAG COMMLOST EXEC
NOTIFYFLAG COMMOK EXEC
NOTIFYFLAG SHUTDOWN EXEC
NOTIFYFLAG ONLINE EXEC
NOTIFYFLAG BATTFAIL EXEC
NOTIFYFLAG BATTCHG EXEC
NOTIFYFLAG NOBATT EXEC
NOTIFYFLAG CAL START EXEC
NOTIFYFLAG CAL END EXEC
NOTIFYFLAG DELAYUP EXEC
NOTIFYFLAG DELAYDOWN EXEC
NOTIFYFLAG REPLBATT EXEC
NOTIFYFLAG OVERLOAD WRN
NOTIFYFLAG OVERLOAD SYS Wall
NOTIFYFLAG OVERLOAD EXEC
NOTIFYFLAG TRIMMING EXEC
NOTIFYFLAG BOOSTING EXEC
NOTIFYFLAG NOINPUT EXEC
NOTIFYFLAG OUTPUTLOW EXEC
NOTIFYFLAG OUTPUTHIGH EXEC
NOTIFYFLAG PARAFAIL EXEC
NOTIFYFLAG USBDROPPED EXEC
NOTIFYFLAG USBRECONNECTED EXEC
EOF
		chmod 640 "$UPS_MON_CONF"
		chown root:"$UPSD_GROUP" "$UPS_MON_CONF" 2>/dev/null || true
	fi

	# Create upsd.users for Beszel agent access
	UPSD_USERS="${NUT_CONF_DIR}/upsd.users"
	if [ ! -f "$UPSD_USERS" ]; then
		info "Creating ${UPSD_USERS} for Beszel agent access..."
		cat > "$UPSD_USERS" <<EOF
# NUT user access configuration
# Generated by nut-beszel-setup.sh

[upsuser]
  password = beszel-monitor
  opts = monitor

[admin]
  password = $(head -c 16 /dev/urandom | base64 | tr -d '=+/' | head -c 16)
  opts = notify
EOF
		chmod 640 "$UPSD_USERS"
		chown root:"$UPSD_GROUP" "$UPSD_USERS" 2>/dev/null || true
		info "Admin password generated (see ${UPSD_USERS})"
	fi

	# Create upsd.conf auth file for upsc
	AUTHCONF="${NUT_CONF_DIR}/upscmd.conf"
	if [ ! -f "$AUTHCONF" ]; then
		info "Creating ${AUTHCONF}..."
		cat > "$AUTHCONF" <<EOF
# NUT upsc authentication
# Generated by nut-beszel-setup.sh
upsuser:beszel-monitor
EOF
		chmod 640 "$AUTHCONF"
		chown root:"$UPSD_GROUP" "$AUTHCONF" 2>/dev/null || true
	fi

	# Enable and start NUT services
	info "Enabling NUT services..."
	systemctl enable nut-server 2>/dev/null || true
	systemctl enable nut-upsmon 2>/dev/null || true
	systemctl restart nut-server 2>/dev/null || true
	systemctl restart nut-upsmon 2>/dev/null || true
	info "NUT services started."
else
	run systemctl enable nut-server
	run systemctl restart nut-server
fi

################################################################################
# Step 4: Install and configure Beszel agent
################################################################################
if [ "$SKIP_AGENT" = false ]; then
	info "Setting up Beszel agent..."

	BESZEL_AGENT_DIR="/opt/beszel"
	BESZEL_AGENT_BIN="${BESZEL_AGENT_DIR}/beszel-agent"
	BESZEL_SYSTEMD_UNIT="/etc/systemd/system/beszel-agent.service"

	if [ "$DRY_RUN" = false ]; then
		# Create agent directory
		mkdir -p "$BESZEL_AGENT_DIR"

		# Download Beszel agent if not present
		if [ ! -x "$BESZEL_AGENT_BIN" ]; then
			info "Downloading Beszel agent..."
			ARCH=$(dpkg --print-architecture)
			case "$ARCH" in
				amd64) GOARCH="amd64" ;;
				arm64) GOARCH="arm64" ;;
				armhf) GOARCH="arm" ;;
				*) GOARCH="amd64" ;;
			esac

			# Get latest release version
			BESZEL_VERSION=$(curl -sL "https://api.github.com/repos/henrygd/beszel/releases/latest" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1)
		[ -z "$BESZEL_VERSION" ] && BESZEL_VERSION="v0.20.0"
			BESZEL_URL="https://github.com/henrygd/beszel/releases/download/${BESZEL_VERSION}/beszel-agent_Linux_${GOARCH}"

			info "  Version: ${BESZEL_VERSION}"
			info "  URL: ${BESZEL_URL}"
			curl -sL -o "$BESZEL_AGENT_BIN" "$BESZEL_URL" || die "Failed to download Beszel agent"
			chmod +x "$BESZEL_AGENT_BIN"
		else
			info "Beszel agent already present at ${BESZEL_AGENT_BIN}"
		fi

		# Create systemd unit for Beszel agent
		info "Creating systemd unit..."
		cat > "$BESZEL_SYSTEMD_UNIT" <<EOF
[Unit]
Description=Beszel Agent (with NUT monitoring)
After=network-online.target nut-server.service
Wants=network-online.target

[Service]
Type=simple
ExecStart=${BESZEL_AGENT_BIN} -hub ${BESZEL_HUB} -token ${BESZEL_TOKEN}
Restart=on-failure
RestartSec=5
Environment=NUT_SERVER=${NUT_SERVER}
Environment=NUT_AUTHCONF_FILE=${NUT_CONF_DIR}/upscmd.conf
Environment=NUT_INTERVAL=${NUT_INTERVAL}
User=root
Group=root

[Install]
WantedBy=multi-user.target
EOF

		systemctl daemon-reload
		systemctl enable beszel-agent 2>/dev/null || true
		systemctl start beszel-agent 2>/dev/null || true
		info "Beszel agent installed and started."
	else
		run mkdir -p "$BESZEL_AGENT_DIR"
		run curl -sL -o "$BESZEL_AGENT_BIN" "https://github.com/henrygd/beszel/releases/latest/download/beszel-agent_Linux_amd64"
		info "[dry-run] Would create systemd unit at ${BESZEL_SYSTEMD_UNIT}"
	fi
else
	info "Skipping Beszel agent installation (--skip-agent)"
fi

################################################################################
# Summary
################################################################################
echo ""
info "Setup complete!"
echo ""
echo "  NUT Server:    ${NUT_SERVER}"
echo "  NUT Config:    ${UPS_CONF}"
echo "  Auth Config:   ${NUT_CONF_DIR}/upscmd.conf"
echo "  NUT Interval:  ${NUT_INTERVAL}s"
echo ""
if [ "$SKIP_AGENT" = false ]; then
	echo "  Beszel Hub:    ${BESZEL_HUB}"
	echo "  Beszel Agent:  ${BESZEL_AGENT_DIR}/beszel-agent"
	echo ""
fi
echo "  Verify with:"
echo "    upsc -l                    # List configured UPS"
echo "    upsc -j ups1@localhost     # Query UPS data (JSON)"
echo "    systemctl status nut-server"
if [ "$SKIP_AGENT" = false ]; then
	echo "    systemctl status beszel-agent"
fi
echo ""