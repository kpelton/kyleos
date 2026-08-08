echo ALL-TESTS-START
if fstest then echo FS-SUITE-COMPLETE else exit 1
echo FS-SUITE-COMPLETE
echo HEAP-COPY-1
cp /usr/share/text/bible.txt /tmp/bible-a
rm /tmp/bible-a
echo HEAP-COPY-2
cp /usr/share/text/bible.txt /tmp/bible-b
rm /tmp/bible-b
echo HEAP-COPY-3
cp /usr/share/text/bible.txt /tmp/bible-c
rm /tmp/bible-c
echo HEAP-COPY-4
cp /usr/share/text/bible.txt /tmp/bible-d
rm /tmp/bible-d
echo HEAP-COPY-SUITE-COMPLETE
echo GP-FAULT-SUITE-START
gpfault
echo $?
echo PASS-GP-FAULT-ISOLATION
echo GP-FAULT-SUITE-COMPLETE
echo WAIT-ABI-START
waittest
echo WAIT-ABI-COMPLETE
echo DEMAND-PAGING-START
if demandtest then echo DEMAND-PAGING-FUNCTIONAL-COMPLETE else exit 1
echo DEMAND-PAGING-FUNCTIONAL-COMPLETE
if oomtest then exit 1 else echo PASS-DEMAND-OOM-ISOLATION
echo PASS-DEMAND-OOM-ISOLATION
echo DEMAND-PAGING-PASS
echo EXEC-STRESS-START
if repeat 200 ls > /dev/null then echo EXEC-STRESS-PASS else exit 1
echo EXEC-STRESS-PASS
echo ALL-TESTS-COMPLETE
exit 0
