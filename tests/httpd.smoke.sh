#!/bin/sh
# Smoke test for examples/httpd: boot the server on an ephemeral port, hit it
# with concurrent curl requests, assert 200s and the body shape, reject a
# body-bearing request with 501, require a draining SIGINT shutdown, then
# prove a second instance survives accept() pressure under fd exhaustion.
#
# usage: httpd.smoke.sh <path-to-starter_httpd> [request-count]
set -u

bin="$1"
requests="${2:-64}"

tmp="$(mktemp -d)" || exit 1
server=""
pressure_server=""
cleanup() {
	if [ -n "$server" ] && kill -0 "$server" 2>/dev/null; then
		kill -9 "$server" 2>/dev/null
	fi
	if [ -n "$pressure_server" ] && kill -0 "$pressure_server" 2>/dev/null; then
		kill -9 "$pressure_server" 2>/dev/null
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

# The template never reads message bodies: a request declaring one must be
# refused with a complete 501, and refusal must not take down the reactor.
body_code="$(curl -sS --max-time 10 -o /dev/null -w '%{http_code}' -d 'x=1' "http://127.0.0.1:$port/post")"
if [ "$body_code" != "501" ]; then
	echo "FAIL: body-bearing request: status $body_code (expected 501)"
	failed=$((failed + 1))
fi
if ! kill -0 "$server" 2>/dev/null; then
	echo "FAIL: server exited after rejecting a body-bearing request"
	failed=$((failed + 1))
fi

# Draining shutdown: park a request mid-read, SIGINT the server, and require
# the in-flight request to complete while new connections are refused. Then
# expect exit 0 and the "stopped" marker.
(
	{
		printf 'GET /drain HTTP/1.1\r\nHost: x\r\n'
		sleep 1
		printf '\r\n'
	} | nc 127.0.0.1 "$port" >"$tmp/drain"
) &
drain=$!
sleep 0.3
kill -INT "$server"
if ! wait "$drain"; then
	echo "FAIL: drain probe client failed"
	failed=$((failed + 1))
fi
if ! grep -q 'HTTP/1\.[01] 200' "$tmp/drain"; then
	echo "FAIL: in-flight request did not complete with 200 across SIGINT"
	failed=$((failed + 1))
fi
if ! grep -q 'path /drain' "$tmp/drain"; then
	echo "FAIL: in-flight request body missing across SIGINT: $(cat "$tmp/drain")"
	failed=$((failed + 1))
fi

# A new connection after the signal must not be served. Timing-tolerant: any
# curl failure or non-200 status counts as refusal.
sleep 0.2
late="$(curl -sS --max-time 3 -o /dev/null -w '%{http_code}' "http://127.0.0.1:$port/late" 2>/dev/null)" || late="000"
if [ "$late" = "200" ]; then
	echo "FAIL: server accepted and served a new request after SIGINT"
	failed=$((failed + 1))
fi

wait "$server"
status=$?
server=""

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

# Accept pressure: a second instance under a tight fd limit. Held-open
# partial requests force accept() into EMFILE; the reactor must back off,
# expire the stalled slots at the absolute deadline, and serve again.
sh -c 'ulimit -n 24; exec "$1"' _ "$bin" >"$tmp/pressure.log" 2>&1 &
pressure_server=$!

pressure_port=""
tries=0
while [ "$tries" -lt 100 ]; do
	pressure_port="$(sed -n 's/^listening \([0-9][0-9]*\)$/\1/p' "$tmp/pressure.log")"
	[ -n "$pressure_port" ] && break
	if ! kill -0 "$pressure_server" 2>/dev/null; then
		echo "FAIL: pressure server exited before listening"
		cat "$tmp/pressure.log"
		exit 1
	fi
	sleep 0.1
	tries=$((tries + 1))
done
if [ -z "$pressure_port" ]; then
	echo "FAIL: pressure server announced no listening port"
	cat "$tmp/pressure.log"
	exit 1
fi

i=0
while [ "$i" -lt 30 ]; do
	(
		{
			printf 'GET /x HTTP/1.1\r\n'
			sleep 4
		} | nc 127.0.0.1 "$pressure_port" >/dev/null 2>&1
	) &
	i=$((i + 1))
done

# Ride out the storm: past the absolute deadlines the stalled slots expire,
# descriptors free up, and the backed-off accept must re-arm.
sleep 5
if ! kill -0 "$pressure_server" 2>/dev/null; then
	echo "FAIL: server died under accept() pressure"
	cat "$tmp/pressure.log"
	failed=$((failed + 1))
else
	recovered=""
	tries=0
	while [ "$tries" -lt 10 ]; do
		code="$(curl -sS --max-time 5 -o "$tmp/recover" -w '%{http_code}' "http://127.0.0.1:$pressure_port/recover" 2>/dev/null)" \
			&& [ "$code" = "200" ] && recovered=1 && break
		sleep 1
		tries=$((tries + 1))
	done
	if [ -z "$recovered" ]; then
		echo "FAIL: server did not recover after accept() pressure"
		cat "$tmp/pressure.log"
		failed=$((failed + 1))
	fi
	kill -INT "$pressure_server"
	wait "$pressure_server"
	pressure_status=$?
	pressure_server=""
	if [ "$pressure_status" -ne 0 ]; then
		echo "FAIL: pressure server exited with status $pressure_status after SIGINT"
		cat "$tmp/pressure.log"
		failed=$((failed + 1))
	fi
	if ! grep -q '^stopped$' "$tmp/pressure.log"; then
		echo "FAIL: no clean-shutdown marker in pressure server output"
		cat "$tmp/pressure.log"
		failed=$((failed + 1))
	fi
fi

if [ "$failed" -ne 0 ]; then
	echo "FAIL: $failed smoke checks failed"
	cat "$tmp/log"
	exit 1
fi

echo "pass: $requests concurrent requests on port $port, 501 body rejection, draining SIGINT shutdown, accept-pressure recovery"
