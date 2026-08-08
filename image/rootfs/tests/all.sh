echo ALL-TESTS-START
fstest
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
echo PASS-GP-FAULT-ISOLATION
echo GP-FAULT-SUITE-COMPLETE
echo EXEC-STRESS-START
repeat 200 ls > /dev/null
echo EXEC-STRESS-PASS
echo ALL-TESTS-COMPLETE
