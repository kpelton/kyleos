#!/usr/bin/env bash
set -euo pipefail

source "$(dirname -- "$0")/testlib.sh"
e2e_init all-tests 180
e2e_start_qemu

e2e_wait_for_text "Kyle OS has booted" 30 ||
    e2e_fail "kernel did not boot within 30 seconds"
e2e_send 'nushell < /tests/jobs.sh'
e2e_wait_for_text "JOB-CONTROL-COMPLETE" 30 ||
    e2e_fail "shell job-control test did not complete"
grep -Eq '^\[1\] Running +[0-9]+ sleep 1$' "$E2E_LOG" ||
    e2e_fail "background job was not listed"
grep -Eq '^\[2\] Stopped +[0-9]+ sleep 20$' "$E2E_LOG" ||
    e2e_fail "stopped job was not reported"
grep -Eq '^\[2\] Running +sleep 20$' "$E2E_LOG" ||
    e2e_fail "bg did not resume the stopped job"
grep -Fq "KILL-COMMAND-STATUS 0" "$E2E_LOG" ||
    e2e_fail "standalone kill command failed"
grep -Fq "PID  STATE     TYPE    NAME" "$E2E_LOG" ||
    e2e_fail "ps did not print the process table"
e2e_send 'nushell < /tests/all.sh'

deadline=$((SECONDS + E2E_TIMEOUT))
while (( SECONDS < deadline )); do
    if grep -Fq "ALL-TESTS-FAILED" "$E2E_LOG"; then
        e2e_fail "the in-image runner reported a failure"
    fi
    if grep -Fq "ALL-TESTS-COMPLETE count=13" "$E2E_LOG"; then
        marker_count=$(grep -Fc "ALL-TESTS-COMPLETE count=13" "$E2E_LOG")
        [[ $marker_count -eq 1 ]] ||
            e2e_fail "completion marker appeared $marker_count times"
        echo "QEMU test suite passed; serial log: $E2E_LOG"
        exit 0
    fi
    kill -0 "$E2E_QEMU_PID" 2>/dev/null ||
        e2e_fail "QEMU exited before the suite completed"
    sleep 0.1
done

e2e_fail "no completion marker within $E2E_TIMEOUT seconds"
