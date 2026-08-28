echo CC-HELLO-START
if cp /tests/hello.c /tmp/hello.c then echo CC-COPY-PASS else exit 1
cd /tmp
if cc -o hello hello.c then echo CC-COMPILE-PASS else exit 1
if ./hello then echo CC-RUN-PASS else exit 1
rm hello
rm hello.c
echo CC-HELLO-TEST-COMPLETE
exit 0
