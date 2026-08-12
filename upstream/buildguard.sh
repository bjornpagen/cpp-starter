#!/bin/sh
# buildguard.sh MAX_SWAP_MB MAX_RSS_MB TIMEOUT_S command...
# Runs command; every 5s checks system swap-used, the largest cc1plus/lto1
# RSS, and elapsed time. On any breach: kills the command and every
# cc1plus/lto1, prints a BUILDGUARD KILL line, exits 99.
set -u
max_swap_mb="$1"; max_rss_mb="$2"; timeout_s="$3"; shift 3

"$@" &
pid=$!
start=$(date +%s)

while kill -0 "$pid" 2>/dev/null; do
	sleep 5
	now=$(date +%s)
	swap_mb=$(sysctl -n vm.swapusage | sed 's/.*used = \([0-9]*\)\..*/\1/')
	rss_kb=$(ps -Ao rss=,comm= | awk '$2 ~ /cc1plus|lto1/ {if ($1 > m) m = $1} END {print m + 0}')
	rss_mb=$((rss_kb / 1024))
	elapsed=$((now - start))
	if [ "${swap_mb:-0}" -gt "$max_swap_mb" ] || [ "$rss_mb" -gt "$max_rss_mb" ] || [ "$elapsed" -gt "$timeout_s" ]; then
		echo "BUILDGUARD KILL: swap=${swap_mb}MB max_rss=${rss_mb}MB elapsed=${elapsed}s (limits: ${max_swap_mb}/${max_rss_mb}/${timeout_s})" >&2
		kill -TERM "$pid" 2>/dev/null
		pkill -TERM -x cc1plus 2>/dev/null
		pkill -TERM -x lto1 2>/dev/null
		sleep 3
		kill -KILL "$pid" 2>/dev/null
		pkill -KILL -x cc1plus 2>/dev/null
		pkill -KILL -x lto1 2>/dev/null
		exit 99
	fi
done
wait "$pid"
