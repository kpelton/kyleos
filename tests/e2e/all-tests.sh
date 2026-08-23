#!/usr/bin/env bash
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
kernel=${TEST_KERNEL:-"$repo_root/kernel32.bin"}
image=${TEST_IMAGE:-"$repo_root/build/image/test-suite.img"}
qemu=${QEMU:-qemu-system-x86_64}
qemu_opts=()
if [[ -n ${QEMU_OPTS:-} ]]; then
    read -r -a qemu_opts <<< "$QEMU_OPTS"
fi
timeout_seconds=${TEST_TIMEOUT:-180}
log=${TEST_LOG:-"$repo_root/build/test/all-tests.log"}
runtime=$(mktemp -d /tmp/os3-qemu-test.XXXXXX)
serial_input="$runtime/serial.in"
qemu_pid=""

cleanup() {
    exec 3>&- || true
    if [[ -n $qemu_pid ]] && kill -0 "$qemu_pid" 2>/dev/null; then
        kill "$qemu_pid" 2>/dev/null || true
        wait "$qemu_pid" 2>/dev/null || true
    fi
    rm -rf -- "$runtime"
}
trap cleanup EXIT

fail() {
    echo "QEMU test suite failed: $1" >&2
    if [[ -f $log ]]; then
        echo "Last serial output ($log):" >&2
        tail -80 "$log" >&2
    fi
    exit 1
}

wait_for_text() {
    local text=$1
    local seconds=$2
    local deadline=$((SECONDS + seconds))

    while (( SECONDS < deadline )); do
        grep -Fq "$text" "$log" && return 0
        if ! kill -0 "$qemu_pid" 2>/dev/null; then
            return 1
        fi
        sleep 0.1
    done
    return 1
}

[[ -f $kernel ]] || fail "missing kernel: $kernel"
[[ -f $image ]] || fail "missing test image: $image"
[[ $timeout_seconds =~ ^[1-9][0-9]*$ ]] || fail "invalid TEST_TIMEOUT: $timeout_seconds"
command -v qemu-system-x86_64 >/dev/null || fail "qemu-system-x86_64 not found"

mkdir -p "$(dirname -- "$log")"
: > "$log"
mkfifo "$serial_input"
exec 3<> "$serial_input"

"$qemu" "${qemu_opts[@]}" \
    -snapshot \
    -no-reboot \
    -display none \
    -monitor none \
    -m 64M \
    -kernel "$kernel" \
    -hda "$image" \
    -serial stdio \
    < "$serial_input" > "$log" 2>&1 &
qemu_pid=$!

wait_for_text "Kyle OS has booted" 30 || fail "kernel did not boot within 30 seconds"
printf 'nushell < /tests/all.sh\n' >&3

deadline=$((SECONDS + timeout_seconds))
while (( SECONDS < deadline )); do
    if grep -Fq "ALL-TESTS-FAILED" "$log"; then
        fail "the in-image runner reported a failure"
    fi
    if grep -Fq "ALL-TESTS-COMPLETE count=10" "$log"; then
        marker_count=$(grep -Fc "ALL-TESTS-COMPLETE count=10" "$log")
        [[ $marker_count -eq 1 ]] || fail "completion marker appeared $marker_count times"
        echo "QEMU test suite passed; serial log: $log"
        exit 0
    fi
    if ! kill -0 "$qemu_pid" 2>/dev/null; then
        fail "QEMU exited before the suite completed"
    fi
    sleep 0.1
done

fail "no completion marker within $timeout_seconds seconds"
