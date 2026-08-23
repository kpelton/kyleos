#!/usr/bin/env bash
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_root=$(mktemp -d /tmp/os3-alltest-host.XXXXXX)
runner="$test_root/alltest-host"
pass_log="$test_root/pass.log"
fail_log="$test_root/fail.log"

cleanup() {
    rm -rf -- "$test_root"
}
trap cleanup EXIT

mkdir -p "$test_root/bin" "$test_root/tmp"

cc -std=c11 -Wall -Wextra -Werror \
    -DALLTEST_ROOT="\"$test_root\"" \
    "$repo_root/extras/alltest/alltest.c" \
    -o "$runner"

for program in fstest kmsgtest waittest pipetest polltest demandtest; do
    ln -s /bin/true "$test_root/bin/$program"
done
ln -s /bin/cp "$test_root/bin/cp"
ln -s /bin/rm "$test_root/bin/rm"
ln -s /bin/true "$test_root/bin/ls"
ln -s /bin/true "$test_root/bin/nushell"

printf '#!/bin/sh\nexit 139\n' > "$test_root/bin/gpfault"
printf '#!/bin/sh\nexit 139\n' > "$test_root/bin/oomtest"
chmod +x "$test_root/bin/gpfault" "$test_root/bin/oomtest"

"$runner" > "$pass_log"
[[ $(grep -c '^ALL-TESTS-COMPLETE count=10$' "$pass_log") -eq 1 ]]
! grep -q '^ALL-TESTS-FAILED' "$pass_log"

ln -sfn /bin/false "$test_root/bin/waittest"
set +e
"$runner" > "$fail_log"
failure_status=$?
set -e

[[ $failure_status -eq 1 ]]
grep -q '^ALL-TESTS-FAILED wait$' "$fail_log"
! grep -q '^ALL-TESTS-COMPLETE' "$fail_log"

echo "alltest runner pass/fail control flow verified"
