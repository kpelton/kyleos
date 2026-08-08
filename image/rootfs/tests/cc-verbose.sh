echo CC-USERLAND-VERBOSE-START
echo BUILDING-TINYCC-STAGE2
cc -vvv -DONE_SOURCE -DCONFIG_KYLEOS -I/usr/src/tinycc /usr/src/tinycc/tcc.c /usr/src/tinycc-kyleos/compat.c -o /tmp/cc-stage2
echo TINYCC-STAGE2-STATUS
echo $?
echo COMPILING-HELLO
/tmp/cc-stage2 -vvv /tests/hello.c -o /tmp/hello-verbose
echo HELLO-COMPILE-STATUS
echo $?
/tmp/hello-verbose
echo HELLO-RUN-STATUS
echo $?
echo CC-USERLAND-VERBOSE-COMPLETE
