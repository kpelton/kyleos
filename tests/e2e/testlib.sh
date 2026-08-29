#!/usr/bin/env bash

# Shared QEMU serial-test lifecycle. Call e2e_init before using the helpers.
e2e_init()
{
    local suite=$1
    local default_timeout=${2:-180}

    E2E_ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[1]}")/../.." && pwd)
    E2E_KERNEL=${TEST_KERNEL:-"$E2E_ROOT/kernel32.bin"}
    E2E_IMAGE=${TEST_IMAGE:-"$E2E_ROOT/build/image/test-suite.img"}
    E2E_QEMU=${QEMU:-qemu-system-x86_64}
    E2E_TIMEOUT=${TEST_TIMEOUT:-$default_timeout}
    E2E_LOG=${TEST_LOG:-"$E2E_ROOT/build/test/$suite.log"}
    E2E_RUNTIME=$(mktemp -d "/tmp/os3-$suite.XXXXXX")
    E2E_INPUT="$E2E_RUNTIME/serial.in"
    E2E_QEMU_PID=""
    E2E_QEMU_OPTS=()

    [[ -z ${QEMU_OPTS:-} ]] || read -r -a E2E_QEMU_OPTS <<< "$QEMU_OPTS"
    [[ -f $E2E_KERNEL ]] || e2e_fail "missing kernel: $E2E_KERNEL"
    [[ -f $E2E_IMAGE ]] || e2e_fail "missing test image: $E2E_IMAGE"
    [[ $E2E_TIMEOUT =~ ^[1-9][0-9]*$ ]] ||
        e2e_fail "invalid TEST_TIMEOUT: $E2E_TIMEOUT"
    command -v "$E2E_QEMU" >/dev/null || e2e_fail "$E2E_QEMU not found"

    mkdir -p "$(dirname -- "$E2E_LOG")"
    : > "$E2E_LOG"
    mkfifo "$E2E_INPUT"
    exec 3<> "$E2E_INPUT"
    trap e2e_cleanup EXIT
}

e2e_cleanup()
{
    exec 3>&- || true
    if [[ -n ${E2E_QEMU_PID:-} ]] &&
       kill -0 "$E2E_QEMU_PID" 2>/dev/null; then
        kill "$E2E_QEMU_PID" 2>/dev/null || true
        wait "$E2E_QEMU_PID" 2>/dev/null || true
    fi
    [[ -z ${E2E_RUNTIME:-} ]] || rm -rf -- "$E2E_RUNTIME"
}

e2e_fail()
{
    echo "QEMU test failed: $1" >&2
    if [[ -f ${E2E_LOG:-} ]]; then
        echo "Last serial output ($E2E_LOG):" >&2
        tail -120 "$E2E_LOG" >&2
    fi
    exit 1
}

e2e_start_qemu()
{
    "$E2E_QEMU" -cpu max "${E2E_QEMU_OPTS[@]}" \
        -snapshot -no-reboot -display none -monitor none -m 64M \
        -kernel "$E2E_KERNEL" -hda "$E2E_IMAGE" -serial stdio \
        < "$E2E_INPUT" > "$E2E_LOG" 2>&1 &
    E2E_QEMU_PID=$!
}

e2e_wait_for_text()
{
    local text=$1
    local seconds=${2:-30}
    local deadline=$((SECONDS + seconds))

    while (( SECONDS < deadline )); do
        grep -Fq "$text" "$E2E_LOG" && return 0
        kill -0 "$E2E_QEMU_PID" 2>/dev/null || return 1
        sleep 0.1
    done
    return 1
}

e2e_send()
{
    printf '%s\n' "$1" >&3
}
