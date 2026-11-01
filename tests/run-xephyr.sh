#!/bin/sh
# Usage: run-xephyr.sh <Xephyr> <test binary>
# Runs the test inside a nested Xephyr server and cleans it up afterwards.
set -e

xephyr=$1
test_binary=$2
display=:${WMLIB_TEST_DISPLAY:-97}

"$xephyr" "$display" -screen 1024x768 -noreset >/dev/null 2>&1 &
server=$!
trap 'kill $server 2>/dev/null' EXIT

sleep 1
DISPLAY=$display "$test_binary"