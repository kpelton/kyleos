echo CC-USERLAND-VERBOSE-START
if cc -vvv -DONE_SOURCE -DCONFIG_KYLEOS -I/usr/src/tinycc /usr/src/tinycc/tcc.c /usr/src/tinycc-kyleos/compat.c -o /tmp/cc-stage2 then echo TINYCC-STAGE2-PASS else exit 1
if /tmp/cc-stage2 -vvv /tests/hello.c -o /tmp/hello-verbose then echo HELLO-COMPILE-PASS else exit 1
if /tmp/hello-verbose then echo HELLO-RUN-PASS else exit 1
echo CC-USERLAND-VERBOSE-COMPLETE
exit 0
