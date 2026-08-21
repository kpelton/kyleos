echo CC-BOOTSTRAP-START
if cc -DONE_SOURCE -DCONFIG_KYLEOS -I/usr/src/tinycc /usr/src/tinycc/tcc.c /usr/src/tinycc-kyleos/compat.c -o /tmp/cc-stage2 then echo CC-STAGE2-BUILD-PASS else exit 1
if /tmp/cc-stage2 /tests/hello.c -o /tmp/hello-stage2 then echo CC-STAGE2-HELLO-BUILD-PASS else exit 1
if /tmp/hello-stage2 then echo CC-STAGE2-RUN-PASS else exit 1
if /tmp/cc-stage2 -DONE_SOURCE -DCONFIG_KYLEOS -I/usr/src/tinycc /usr/src/tinycc/tcc.c /usr/src/tinycc-kyleos/compat.c -o /tmp/cc-stage3 then echo CC-STAGE3-BUILD-PASS else exit 1
if /tmp/cc-stage3 /tests/hello.c -o /tmp/hello-stage3 then echo CC-STAGE3-HELLO-BUILD-PASS else exit 1
if /tmp/hello-stage3 then echo CC-STAGE3-RUN-PASS else exit 1
echo CC-BOOTSTRAP-COMPLETE
exit 0
