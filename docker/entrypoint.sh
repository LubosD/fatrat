#!/bin/bash
# Entry point of the FatRat container.
#
# With a terminal attached (docker run -it) it shows the terminal UI, otherwise it runs FatRat as a plain
# headless server. The web interface is served in both cases, and in headless mode you can still get the
# terminal UI later with "docker exec -it CONTAINER fatrat-tui".
#
# Environment:
#   FATRAT_MODE      auto (default: TUI if stdin and stdout are terminals), tui or headless
#   FATRAT_PORT      port of the web interface (default 2233)
#   FATRAT_PASSWORD  password of the web interface (any user name). If neither this nor an earlier run
#                    set one, a random password is generated, stored and printed.
#
# Persistent data: /config (settings, queues, .torrent files) and /downloads.
# Arguments are passed to the server (fatrat-nogui), e.g. "--no-java"; "--" is not needed.
set -eu

CONF=/config/fatrat.conf
RUNTIME=${XDG_RUNTIME_DIR:-/tmp/runtime}
SOCK=$RUNTIME/fatrat/rest.sock
export XDG_RUNTIME_DIR=$RUNTIME
mkdir -p -m 0700 "$RUNTIME"

# Set KEY in [SECTION] of an ini file, creating the file, section or key as needed
ini_set() {
	local file=$1 section=$2 key=$3 value=$4 tmp
	tmp=$(mktemp)
	[ -f "$file" ] || : > "$file"
	awk -v sec="$section" -v key="$key" -v val="$value" '
		function flush() { if (insec && !done) { print key "=" val; done = 1 } }
		/^\[.*\]$/ { flush(); insec = ($0 == "[" sec "]"); if (insec) seen = 1 }
		insec && index($0, key "=") == 1 { if (!done) print key "=" val; done = 1; next }
		{ print }
		END { flush(); if (!seen) { print "\n[" sec "]"; print key "=" val } }
	' "$file" > "$tmp"
	cat "$tmp" > "$file"; rm -f "$tmp"
}
ini_get() { sed -n "/^\[$2\]\$/,/^\[/ s/^$3=//p" "$1" 2>/dev/null | head -n1; }

mkdir -p /config
password=${FATRAT_PASSWORD:-$(ini_get "$CONF" remote password)}
generated=
if [ -z "$password" ] || { [ -z "${FATRAT_PASSWORD:-}" ] && [ "$password" = fatrat ]; }; then
	# fatrat's built-in default password is public, do not keep it
	password=$(head -c 18 /dev/urandom | base64 | tr -d '/+=' )
	generated=1
fi
ini_set "$CONF" remote enable true
ini_set "$CONF" remote port "${FATRAT_PORT:-2233}"
ini_set "$CONF" remote password "$password"
chmod 0600 "$CONF"
[ -n "$generated" ] && echo "fatrat: generated a web interface password: $password (set FATRAT_PASSWORD to choose one)" >&2

server=(fatrat-nogui --config "$CONF" --rest-socket "$SOCK" "$@")

mode=${FATRAT_MODE:-auto}
if [ "$mode" = auto ]; then
	if [ -t 0 ] && [ -t 1 ]; then mode=tui; else mode=headless; fi
fi

case $mode in
headless)
	# exec: the server becomes PID 1 and gets SIGTERM from docker stop (it saves its state on SIGTERM)
	exec "${server[@]}"
	;;
tui)
	"${server[@]}" > /config/server.log 2>&1 &
	pid=$!
	stop() { kill -TERM "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; }
	trap 'stop; exit 143' TERM HUP
	trap stop EXIT

	for _ in $(seq 1 100); do
		[ -S "$SOCK" ] && break
		kill -0 "$pid" 2>/dev/null || { echo "fatrat: the server failed to start:" >&2; cat /config/server.log >&2; exit 1; }
		sleep 0.2
	done
	[ -S "$SOCK" ] || { echo "fatrat: the server did not open $SOCK" >&2; cat /config/server.log >&2; exit 1; }

	# The TUI is a client; leaving it stops the container. Web interface: http://HOST:${FATRAT_PORT:-2233}/
	fatrat-tui --mode socket --socket "$SOCK" --no-start
	;;
*)
	echo "fatrat: FATRAT_MODE must be auto, tui or headless" >&2
	exit 2
	;;
esac
