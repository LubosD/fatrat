#!/bin/bash
# End-to-end test of fatrat-tui against a real headless FatRat, using the TUI's --script mode (a virtual screen).
#
#   TUI=/path/to/fatrat-tui SERVER=/path/to/fatrat-nogui tests/tui-smoke.sh
#
# The server is started by the TUI itself on a unix socket and uses a throw-away HOME, so nothing of the
# real profile is touched. A second part checks the password prompt against a server on a TCP port.
set -u

TUI=${TUI:?set TUI to the fatrat-tui binary}
SERVER=${SERVER:?set SERVER to the fatrat-nogui (or fatrat) binary}
PORT=${PORT:-19192}

WORK=$(mktemp -d)
SOCKDIR=$(mktemp -d /tmp/ftui-smoke.XXXXXX)   # short path: unix socket paths are limited to ~100 bytes
SOCK=$SOCKDIR/rest.sock
DAEMON_PID=
cleanup() {
	[ -n "$DAEMON_PID" ] && kill "$DAEMON_PID" 2>/dev/null
	rm -rf "$WORK" "$SOCKDIR"
}
trap cleanup EXIT

fail=0
run() {   # run NAME SCRIPT [extra tui args]
	local name=$1 script=$2; shift 2
	echo "$script" > "$WORK/$name.txt"
	if env HOME="$WORK/home" XDG_CONFIG_HOME="$WORK/cfg" LC_ALL=C.UTF-8 timeout 90 "$TUI" --script "$WORK/$name.txt" --size 110x34 "$@" > "$WORK/$name.out" 2> "$WORK/$name.err"; then
		echo "ok    $name"
	else
		echo "FAIL  $name"; cat "$WORK/$name.err"; fail=1
	fi
}

fresh_profile() {
	rm -rf "$WORK/home" "$SOCKDIR"/*; mkdir -p "$WORK/home"
	printf '[remote]\nenable=false\n' > "$WORK/home/fatrat.conf"
}
local_args=(--server-bin "$SERVER" --socket "$SOCK" --config "$WORK/home/fatrat.conf")

# a torrent with three files
python3 - "$WORK/sample.torrent" <<'PY'
import hashlib, sys
def b(x):
    if isinstance(x, int): return b'i%de' % x
    if isinstance(x, bytes): return b'%d:%s' % (len(x), x)
    if isinstance(x, str): return b(x.encode())
    if isinstance(x, list): return b'l' + b''.join(b(i) for i in x) + b'e'
    return b'd' + b''.join(b(k) + b(v) for k, v in sorted(x.items())) + b'e'
files = [{'length': 40000, 'path': ['docs', 'readme.txt']}, {'length': 100000, 'path': ['video.mkv']}, {'length': 5000, 'path': ['subs.srt']}]
n = (sum(f['length'] for f in files) + 16383) // 16384
info = {'name': 'sample-pack', 'piece length': 16384, 'pieces': b''.join(hashlib.sha1(bytes([i])).digest() for i in range(n)), 'files': files}
open(sys.argv[1], 'wb').write(b({'announce': 'http://127.0.0.1:1/announce', 'info': info}))
PY

# ------------------------------------------------------------------------------------------------
fresh_profile
run queues-and-transfers "
waitfor connected
waitfor Main queue
# a second queue
key Left
key n
waitfor New queue
type Movies
key Enter
waitfor Movies
# three transfers in the first queue (paused, so nothing is downloaded)
key Up
key Right
key n
waitfor New transfer
type http://127.0.0.1:1/a.iso http://127.0.0.1:1/b.iso http://127.0.0.1:1/c.iso
key Tab
key Tab
key Tab
key Tab
key Tab
key Tab
key Space
key Enter
waitfor c.iso
expect Paused
# send b.iso to Movies
key Down
key s
waitfor Send to queue
key Enter
wait 1500
reject b.iso
# drag and drop: a.iso onto the Movies queue (the right half of the screen starts at column 24)
mouse press 30 4
mouse drag 31 4
mouse drag 6 4
mouse release 6 4
wait 1500
reject a.iso
expect c.iso
# the queue pane shows the counts; deleting Movies moves its two transfers back
key Left
key Down
wait 1200
expect a.iso
key d
waitfor Delete queue
key m
key Enter
wait 1500
expect a.iso
expect b.iso
expect c.iso
reject Movies
" "${local_args[@]}"

fresh_profile
run torrent-files "
waitfor connected
waitfor Main queue
key n
waitfor New transfer
type $WORK/sample.torrent
key Tab
key Tab
key Tab
key Tab
key Tab
key Tab
key Space
key Enter
waitfor sample-pack
wait 1500
key Enter
waitfor Files
key Tab
waitfor video.mkv
key Down
key s
expect Skip*
key Enter
waitfor Applied 1 priority
reject Skip*
expect Skip
" "${local_args[@]}"

fresh_profile
run rss "
waitfor connected
waitfor Main queue
key 2
key n
waitfor New feed
type Releases
key Tab
type http://127.0.0.1:1/feed.xml
key Enter
waitfor Releases
key Tab
key n
waitfor New download rule
type ubuntu
key Enter
waitfor ubuntu
key t
waitfor Test a regular
type Ubuntu 24.04
key Enter
waitfor The expression matches
" "${local_args[@]}"

# ------------------------------------------------------------------------------------------------
# the password prompt, against a server on a TCP port
mkdir -p "$WORK/home2"
printf '[remote]\nenable=true\nport=%s\npassword=secret\n' "$PORT" > "$WORK/home2/fatrat.conf"
HOME="$WORK/home2" "$SERVER" -c "$WORK/home2/fatrat.conf" --force > "$WORK/daemon.log" 2>&1 &
DAEMON_PID=$!
for i in $(seq 50); do curl -s -o /dev/null "http://127.0.0.1:$PORT/api/v1/stats" && break; sleep 0.2; done

run password-prompt "
waitfor Password required
type wrong
key Enter
waitfor Wrong password
type secret
key Enter
waitfor Main queue
expect connected
" --url "http://127.0.0.1:$PORT"

FATRAT_PASSWORD=secret run password-env "
waitfor Main queue
expect connected
" --url "http://127.0.0.1:$PORT"

[ $fail -eq 0 ] && echo "ALL OK"
exit $fail
