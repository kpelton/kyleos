echo KMSG-TEST-START
echo kmsg-write-test > /dev/kmsg
cat /dev/kmsg | grep "userspace: kmsg-write-test"
echo $?
cat /dev/kmsg | grep "Kyle OS has booted"
echo $?
echo KMSG-TEST-COMPLETE
