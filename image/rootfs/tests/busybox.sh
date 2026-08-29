#!/bin/ash

fail()
{
    echo "BUSYBOX-TEST-FAILED: $1"
    exit 1
}

echo BUSYBOX-TEST-START

PATH=/bin:/usr/bin
export PATH

# Native KyleOS files have no Unix execute bits.  Ash must still allow the
# kernel to execute them when PATH resolves an ordinary (non-BusyBox) command.
stat /bin/cat >/tmp/busybox-stat.txt || fail "PATH native executable"
ls / >/tmp/busybox-root-ls.txt || fail "root directory stat"
grep '^bin$' /tmp/busybox-root-ls.txt >/dev/null || fail "root directory listing"

value="ash-works"
test "$value" = "ash-works" || fail "shell variables"
case "$value" in
    ash-*) ;;
    *) fail "case pattern" ;;
esac

mkdir /tmp/busybox-path-test || fail "standalone mkdir"
cd /tmp/busybox-path-test || fail "relative cd"
printf 'relative-path\n' > relative.txt || fail "relative redirect"
text=$(cat relative.txt) || fail "standalone relative cat"
test "$text" = "relative-path" || fail "relative file contents"
printf 'glob-a\n' > glob-a.txt || fail "glob file a"
printf 'glob-b\n' > glob-b.txt || fail "glob file b"
text=$(cat glob-*.txt) || fail "cat glob"
test "$text" = "glob-a
glob-b" || fail "glob contents"
ls | grep '^relative.txt$' >/tmp/busybox-ls.txt || fail "standalone ls"

count=$(printf abc | wc -c) || fail "pipeline"
test "$count" -eq 3 || fail "pipeline count"

printf 'alpha\nbeta\n' | grep '^beta$' >/tmp/busybox-grep.txt ||
    fail "grep regex"
test "$(cat /tmp/busybox-grep.txt)" = beta || fail "grep output"

if definitely-not-a-command 2>/tmp/busybox-missing.txt; then
    fail "missing command status"
fi
grep 'not found' /tmp/busybox-missing.txt >/dev/null || fail "missing command errno"

rm -r /tmp/busybox-path-test /tmp/busybox-grep.txt /tmp/busybox-ls.txt \
    /tmp/busybox-missing.txt /tmp/busybox-stat.txt \
    /tmp/busybox-root-ls.txt || fail "cleanup"
echo BUSYBOX-TEST-COMPLETE
