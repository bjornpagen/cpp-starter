#!/bin/sh
# Smoke test for examples/httpd: boot the server on an ephemeral port, hit it
# with concurrent curl requests, assert 200s and the body shape
# ("worker <id> path <target>"), then require a clean SIGINT shutdown.
#
# usage: httpd.smoke.sh <path-to-starter_httpd> [request-count]
set -u

bin="$1"
requests="${2:-16}"

tmp="$(mktemp -d)" || exit 1
server=""
cleanup() {
	if [ -n "$server" ] && kill -0 "$server" 2>/dev/null; then
		kill -9 "$server" 2>/dev/null
	fi
	rm -rf "$tmp"
}
trap cleanup EXIT

"$bin" >"$tmp/log" 2>&1 &
server=$!

# The server prints "listening <port>" (flushed) once the shared listener
# is bound and every worker is started.
port=""
tries=0
while [ "$tries" -lt 100 ]; do
	port="$(sed -n 's/^listening \([0-9][0-9]*\)$/\1/p' "$tmp/log")"
	[ -n "$port" ] && break
	if ! kill -0 "$server" 2>/dev/null; then
		echo "FAIL: server exited before listening"
		cat "$tmp/log"
		exit 1
	fi
	sleep 0.1
	tries=$((tries + 1))
done
if [ -z "$port" ]; then
	echo "FAIL: no listening port announced"
	cat "$tmp/log"
	exit 1
fi

# Concurrent requests, each with a distinct path; every response must be a
# 200 whose body names some worker and echoes exactly that path.
pids=""
i=0
while [ "$i" -lt "$requests" ]; do
	(
		code="$(curl -sS --max-time 10 -o "$tmp/body.$i" -w '%{http_code}' "http://127.0.0.1:$port/hello/$i")" || exit 1
		[ "$code" = "200" ] || {
			echo "FAIL: request $i: status $code"
			exit 1
		}
		grep -Eq "^worker [0-9]+ path /hello/$i\$" "$tmp/body.$i" || {
			echo "FAIL: request $i: unexpected body: $(cat "$tmp/body.$i")"
			exit 1
		}
	) &
	pids="$pids $!"
	i=$((i + 1))
done

failed=0
for pid in $pids; do
	wait "$pid" || failed=$((failed + 1))
done

# A malformed request (non-token method) must come back 400, not hang or
# kill a worker.
bad="$(curl -sS --max-time 10 -o /dev/null -w '%{http_code}' -X 'B@GUS' "http://127.0.0.1:$port/")"
if [ "$bad" != "400" ]; then
	echo "FAIL: malformed request: status $bad (expected 400)"
	failed=$((failed + 1))
fi

# Clean shutdown: SIGINT, expect exit 0 and the "stopped" marker.
kill -INT "$server"
wait "$server"
status=$?
server=""

if [ "$failed" -ne 0 ]; then
	echo "FAIL: $failed of $requests requests failed"
	cat "$tmp/log"
	exit 1
fi
if [ "$status" -ne 0 ]; then
	echo "FAIL: server exited with status $status after SIGINT"
	cat "$tmp/log"
	exit 1
fi
if ! grep -q '^stopped$' "$tmp/log"; then
	echo "FAIL: no clean-shutdown marker in server output"
	cat "$tmp/log"
	exit 1
fi

echo "pass: $requests concurrent requests on port $port, clean SIGINT shutdown"
