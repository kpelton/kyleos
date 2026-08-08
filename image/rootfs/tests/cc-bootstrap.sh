echo CC-BOOTSTRAP-START
cc -DONE_SOURCE -DCONFIG_KYLEOS -I/usr/src/tinycc /usr/src/tinycc/tcc.c /usr/src/tinycc-kyleos/compat.c -o /tmp/cc-stage2
echo CC-STAGE2-BUILD-STATUS
echo $?
/tmp/cc-stage2 /tests/hello.c -o /tmp/hello-stage2
/tmp/hello-stage2
echo CC-STAGE2-RUN-STATUS
echo $?
/tmp/cc-stage2 -DONE_SOURCE -DCONFIG_KYLEOS -I/usr/src/tinycc /usr/src/tinycc/tcc.c /usr/src/tinycc-kyleos/compat.c -o /tmp/cc-stage3
echo CC-STAGE3-BUILD-STATUS
echo $?
/tmp/cc-stage3 /tests/hello.c -o /tmp/hello-stage3
/tmp/hello-stage3
echo CC-STAGE3-RUN-STATUS
echo $?
echo CC-BOOTSTRAP-COMPLETE
