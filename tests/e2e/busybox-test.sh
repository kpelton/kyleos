#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
kernel=${TEST_KERNEL:-"$root/kernel32.bin"}
image=${TEST_IMAGE:-"$root/build/image/test-suite.img"}
qemu=${QEMU:-qemu-system-x86_64}
log=${TEST_LOG:-"$root/build/test/busybox.log"}
runtime=$(mktemp -d /tmp/os3-busybox-test.XXXXXX)
input="$runtime/serial.in"
qemu_pid=""

cleanup()
{
    exec 3>&- || true
    if [[ -n $qemu_pid ]] && kill -0 "$qemu_pid" 2>/dev/null; then
        kill "$qemu_pid" 2>/dev/null || true
        wait "$qemu_pid" 2>/dev/null || true
    fi
    rm -rf -- "$runtime"
}
trap cleanup EXIT

wait_for_text()
{
    local text=$1
    local deadline=$((SECONDS + ${2:-30}))
    while (( SECONDS < deadline )); do
        grep -Fq "$text" "$log" && return 0
        kill -0 "$qemu_pid" 2>/dev/null || return 1
        sleep 0.1
    done
    return 1
}

mkdir -p "$(dirname -- "$log")"
: > "$log"
mkfifo "$input"
exec 3<> "$input"

qemu_options=()
[[ -z ${QEMU_OPTS:-} ]] || read -r -a qemu_options <<< "$QEMU_OPTS"
"$qemu" "${qemu_options[@]}" -snapshot -no-reboot -display none -monitor none \
    -m 64M -kernel "$kernel" -hda "$image" -serial stdio \
    < "$input" > "$log" 2>&1 &
qemu_pid=$!

wait_for_text "Kyle OS has booted" 30 || {
    tail -80 "$log" >&2
    exit 1
}
printf 'echo BUSYBOX-INTERACTIVE-ONE\n' >&3
wait_for_text "BUSYBOX-INTERACTIVE-ONE" 15 || {
    tail -120 "$log" >&2
    exit 1
}
printf 'ls / >/tmp/busybox-interactive-ls.txt\n' >&3
printf 'echo BUSYBOX-INTERACTIVE-TWO\n' >&3
wait_for_text "BUSYBOX-INTERACTIVE-TWO" 15 || {
    tail -120 "$log" >&2
    exit 1
}
printf "mkdir /tmp/busybox-interactive-glob; cd /tmp/busybox-interactive-glob; printf 'glob-a\\n' > glob-a.txt; printf 'glob-b\\n' > glob-b.txt; cat glob-*.txt; echo BUSYBOX-GLOB-COMPLETE\n" >&3
wait_for_text "BUSYBOX-GLOB-COMPLETE" 15 || {
    tail -120 "$log" >&2
    exit 1
}
grep -Fq "glob-a" "$log" && grep -Fq "glob-b" "$log" || {
    tail -120 "$log" >&2
    exit 1
}
printf 'vi /tmp/busybox-vi.txt\n' >&3
sleep 1
printf 'iBUSYBOX-VI-SAVED\033:wq\n' >&3
sleep 0.5
printf 'cat /tmp/busybox-vi.txt; rm /tmp/busybox-vi.txt; echo BUSYBOX-VI-COMPLETE\n' >&3
wait_for_text "BUSYBOX-VI-COMPLETE" 15 || {
    tail -160 "$log" >&2
    exit 1
}
grep -Fq "BUSYBOX-VI-SAVED" "$log" || {
    tail -160 "$log" >&2
    exit 1
}
printf 'sh < /tests/busybox.sh\n' >&3
wait_for_text "BUSYBOX-TEST-COMPLETE" 45 || {
    tail -120 "$log" >&2
    exit 1
}
if grep -Fq "BUSYBOX-TEST-FAILED" "$log"; then
    tail -120 "$log" >&2
    exit 1
fi
echo "BusyBox QEMU test passed; serial log: $log"
