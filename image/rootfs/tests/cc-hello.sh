echo CC-HELLO-START
cc /tests/hello.c -o /tmp/hello
echo CC-COMPILE-STATUS
echo $?
/tmp/hello
echo CC-RUN-STATUS
echo $?
echo CC-HELLO-TEST-COMPLETE
