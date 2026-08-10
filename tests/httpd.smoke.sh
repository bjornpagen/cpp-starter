#!/bin/sh
# Smoke test for examples/httpd: boot the server on an ephemeral port, hit it
# with concurrent curl requests, assert 200s and the body shape, then require
# a clean SIGINT shutdown.
#
# usage: httpd.smoke.sh <path-to-starter_httpd> [request-count]
set -u

bin="$1"
requests="${2:-64}"

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

# The server prints "listening <port>" once its bounded reactor is ready.
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

# Concurrent requests, each with a distinct path; every response must echo it.
pids=""
i=0
while [ "$i" -lt "$requests" ]; do
	(
		code="$(curl -sS --max-time 10 -o "$tmp/body.$i" -w '%{http_code}' "http://127.0.0.1:$port/hello/$i")" || exit 1
		[ "$code" = "200" ] || {
			echo "FAIL: request $i: status $code"
			exit 1
		}
		grep -Eq "^path /hello/$i\$" "$tmp/body.$i" || {
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

# Hold a torn request open beyond the configured absolute deadline. The
# reactor must close it without writing a response, then remain usable.
(
	{
		printf 'GET /slow HTTP/1.1\r\nHost: localhost\r\n'
		sleep 3
	} | nc 127.0.0.1 "$port" >"$tmp/slow"
) &
slow=$!
if ! wait "$slow"; then
	echo "FAIL: slow-client probe failed"
	failed=$((failed + 1))
fi
if [ -s "$tmp/slow" ]; then
	echo "FAIL: timed-out partial request received a response"
	failed=$((failed + 1))
fi
if ! kill -0 "$server" 2>/dev/null; then
	echo "FAIL: server exited while expiring a partial request"
	failed=$((failed + 1))
fi

# A malformed request (non-token method) must come back 400, not hang or
# kill the reactor.
bad="$(curl -sS --max-time 10 -o /dev/null -w '%{http_code}' -X 'B@GUS' "http://127.0.0.1:$port/")"
if [ "$bad" != "400" ]; then
	echo "FAIL: malformed request: status $bad (expected 400)"
	failed=$((failed + 1))
fi

# The example handler's typed failure is translated to a complete 500 by the
# module trampoline; application code never serializes into reactor storage.
long_path="$(awk 'BEGIN { for (i = 0; i < 300; ++i) printf "x" }')"
handler_error="$(curl -sS --max-time 10 -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/$long_path")"
if [ "$handler_error" != "500" ]; then
	echo "FAIL: handler error: status $handler_error (expected 500)"
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
