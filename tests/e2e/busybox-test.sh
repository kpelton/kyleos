#!/usr/bin/env bash
set -euo pipefail

source "$(dirname -- "$0")/testlib.sh"
e2e_init busybox 90
e2e_start_qemu

e2e_wait_for_text "Kyle OS has booted" 30 || e2e_fail "kernel did not boot"
e2e_send "printf 'BUSYBOX-%s\\n' 'INTERACTIVE-ONE'"
e2e_wait_for_text "BUSYBOX-INTERACTIVE-ONE" 15 ||
    e2e_fail "interactive shell did not respond"
for iteration in $(seq 1 20); do
    e2e_send "ls / >/tmp/busybox-interactive-ls-$iteration.txt; printf 'BUSYBOX-LS-$iteration\\n'"
    e2e_wait_for_text "BUSYBOX-LS-$iteration" 5 ||
        e2e_fail "redirected foreground command $iteration did not return"
done
e2e_send "printf 'BUSYBOX-%s\\n' 'INTERACTIVE-TWO'"
e2e_wait_for_text "BUSYBOX-INTERACTIVE-TWO" 15 ||
    e2e_fail "interactive shell stopped responding"
grep -Fq "can't set tty process group" "$E2E_LOG" &&
    e2e_fail "fast foreground command lost its process group before wait"
e2e_send "mkdir /tmp/busybox-interactive-glob; cd /tmp/busybox-interactive-glob; printf 'glob-a\\n' > glob-a.txt; printf 'glob-b\\n' > glob-b.txt; cat glob-*.txt; printf 'BUSYBOX-GLOB-%s\\n' COMPLETE"
e2e_wait_for_text "BUSYBOX-GLOB-COMPLETE" 15 ||
    e2e_fail "glob test did not complete"
grep -Fq "glob-a" "$E2E_LOG" && grep -Fq "glob-b" "$E2E_LOG" ||
    e2e_fail "glob expansion omitted a file"
printf 'vi /tmp/busybox-vi.txt\n' >&3
sleep 1
printf 'iBUSYBOX-VI-SAVED\033:wq\n' >&3
sleep 0.5
e2e_send "cat /tmp/busybox-vi.txt; rm /tmp/busybox-vi.txt; printf 'BUSYBOX-VI-%s\\n' COMPLETE"
e2e_wait_for_text "BUSYBOX-VI-COMPLETE" 15 ||
    e2e_fail "vi test did not complete"
[[ $(grep -Fc "BUSYBOX-VI-SAVED" "$E2E_LOG") -ge 2 ]] ||
    e2e_fail "vi did not save text"
e2e_send 'sh < /tests/busybox.sh'
e2e_wait_for_text "BUSYBOX-TEST-COMPLETE" 45 ||
    e2e_fail "in-image BusyBox suite did not complete"
grep -Fq "BUSYBOX-TEST-FAILED" "$E2E_LOG" &&
    e2e_fail "in-image BusyBox suite reported failure"
echo "BusyBox QEMU test passed; serial log: $E2E_LOG"
