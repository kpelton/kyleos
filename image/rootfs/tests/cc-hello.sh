echo CC-HELLO-START
if cc /tests/hello.c -o /tmp/hello then echo CC-COMPILE-PASS else exit 1
if /tmp/hello then echo CC-RUN-PASS else exit 1
echo CC-HELLO-TEST-COMPLETE
exit 0
