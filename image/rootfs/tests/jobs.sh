echo JOB-CONTROL-START
sleep 1 &
jobs
fg %1
echo JOB-FG-COMPLETE
sleep 20 &
kill -STOP %2
sleep 1
jobs
bg %2
kill %2
sleep 1
jobs
/bin/kill -0 1
echo KILL-COMMAND-STATUS $?
ps
echo JOB-CONTROL-COMPLETE
exit
