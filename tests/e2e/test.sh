
VAL=$(date "+%N")
echo "Unique val $VAL"

function start_qemu {
    qemu-system-x86_64  -snapshot  -display none -m 8G -kernel kernel32.bin  -hda test-hd.img -serial pty -rtc base=localtime 2>/dev/null >qemu.log.$VAL & #-display none &
    pid=$!
    sleep 3
    pts=$(grep pts qemu.log.$VAL | awk '{print $5}')
    cat $pts>> qemu_out.log.$VAL &
    cat_pid=$!
    exec 3>$pts
}

function check_status() {
    cat qemu_out.log.$VAL | tail -5 | grep -q "$2" 
    retcode=$?
    kill $cat_pid &>/dev/null
    kill $pid &>/dev/null
    if [ $retcode -ne 0 ]; then
        echo "test:$1 FAILED"
        return 1;
    fi

    echo "test:$1 PASSED"
    return 0
}

function kill_test() {
    start_qemu
    echo "sched" >$pts
    sleep 3
    echo "cd progs" >$pts
    sleep 3
    echo "exec names" >$pts
    sleep 3
    echo "exec names" >$pts
    m
    m
    m
    sleep 3
    echo "kill 2" >$pts
    sleep 3
    echo "kill 3" >$pts
    sleep 3
    check_status "kill test" "Kyle OS |/progs>"
}

function fs_bible_test() {
    start_qemu
    echo "sched" >$pts
    sleep 3
    echo "cat bible.txt" >$pts
    sleep 45
    check_status "fs bible test" "Kyle OS |/>" 
}

function donut_test() {
    start_qemu
    echo "cd progs" >$pts
    sleep 3
    echo "exec donut" >$pts
    sleep 5
    echo "kill 2" >$pts
    sleep 3
    check_status "donut_test" "Kyle OS |/progs>"
}

function stress_test() {
    start_qemu
    echo "sched" >$pts
    sleep 3
    echo "cd progs" >$pts
    sleep 3
    echo "exec names" >$pts
    sleep 3
    echo "exec names" >$pts
    sleep 3
    echo "exec names" >$pts
    sleep 3
    echo "exec donut" >$pts
    sleep 3
    echo "exec donut" >$pts
    sleep 3
 
    echo "kill 2" >$pts
    sleep 3
    echo "kill 3" >$pts
    sleep 3
    echo "kill 4" >$pts
    sleep 3
    echo "kill 5" >$pts
    sleep 3
    echo "kill 6" >$pts
    sleep 3
    
   check_status "stress test" "Kyle OS |/progs>"
}

function ramfs_test() {
    start_qemu
    echo "cd /2" >$pts
    sleep 3
    echo "mkdir a" >$pts
    sleep 3
    echo "cd a" >$pts
    sleep 3
    echo "mkdir b" >$pts
    sleep 3
    echo "cd b" >$pts
    sleep 3
    echo "cd .." >$pts
    sleep 3
    echo "cd .." >$pts
    sleep 3
    echo "cd .." >$pts
    sleep 3
   check_status "ramfs_mkdir" "Kyle OS |/>" 
}

function time_test() {
    start_qemu
    echo "time" >$pts
    sleep 3
   check_status "time" "Kyle OS |/>" 
}

function sleep_test() {
    start_qemu
    echo "sleep 5" >$pts
    sleep 7
   check_status "sleep" "Kyle OS |/>" 
}

function cat_test() {
    start_qemu
    echo "cd cs" >$pts
    sleep 3
    echo "cd cs300" >$pts
    sleep 3
    echo "cd assignment-4" >$pts
    sleep 3
    echo "cat tictactoe.cc" >$pts
    sleep 3
    echo "cd .." >$pts
    sleep 3
    echo "cd .." >$pts
    sleep 3
    echo "cd .." >$pts
    sleep 3

   check_status "vfs/cat file" "Kyle OS |/>" 
}


function shell_lua_test() {
    start_qemu
    echo "cd progs" >$pts
    sleep 3
    echo "exew nushell" >$pts
    sleep 3
    echo "/progs/lua" >$pts
    sleep  5
   check_status "shell_lua" "> "
}

function shell_lua_test_compute() {
    start_qemu
    echo "exec fputest_compute" >$pts
    sleep 3
    echo "exec fputest_compute" >$pts
    sleep 3
    echo "exec fputest_compute" >$pts
    sleep 3
    echo "exec fputest_compute" >$pts
    sleep 3
    echo "exec fputest_compute" >$pts
    sleep 3
    echo "exec fputest_compute" >$pts
    sleep 3
    echo "cd progs" >$pts
    sleep 3
    echo "exew nushell" >$pts
    sleep 3
    echo "/progs/lua" >$pts
    sleep  30
   check_status "shell_lua" "> "
}

function test_fs() {
    start_qemu
    echo ls > $pts 
    sleep 3
    echo "cd 2" >$pts
    sleep 3
    echo "mkdir z" >$pts
    sleep 3
    echo "cp /donut /2/donut" >$pts
    sleep 3
    check_status "basic file/mkdir" "successfully"
}

function test_fs2() {
    start_qemu
    echo ls > $pts 
    sleep 3
    echo "cd 2" >$pts
    sleep 3
    echo "mkdir z" >$pts
    sleep 3
    echo "cp /donut /2/z/donut" >$pts
    sleep 3
    echo "cp /2/z/donut /2/z/c.lua" >$pts
    sleep 3
    check_status "basic file/mkdir 2" "successfully"
}

function test_fs3() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cd /'> $pts  && sleep 3
    echo 'cp donut /2/kyle/kyle2/donut'> $pts  && sleep 3
    check_status "complex file/mkdir 2" "successfully"
}

function test_fs4() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cd /'> $pts  && sleep 3
    echo 'cd /2/kyle'> $pts  && sleep 3
    echo 'cp /donut /2/kyle/kyle2/donut'> $pts  && sleep 3
    check_status "test absoulte copy insde dir" "successfully"
}

function test_fs5() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cd /'> $pts  && sleep 3
    echo 'cp /donut /2/kyle/kyle2/donut'> $pts  && sleep 3
    check_status "test absoulte copy outside dir" "successfully"
}


function test_fs_relative() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cp /donut donut'> $pts  && sleep 3

    check_status "relative copy" "successfully"
}

function test_fs_relative_local() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cd ../..' > $pts  && sleep 3
    echo 'cp /donut ./kyle/kyle2/donut'> $pts  && sleep 3

    check_status "relative copy local" "successfully"
}


function test_fs_relative_back() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cp /donut ../donut'> $pts  && sleep 3
    check_status "relative copy back" "successfully"
}

function test_fs_abs_back() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cp /donut /2/kyle/../donut'> $pts  && sleep 3
    check_status "abs copy back" "successfully"
}

function test_fs_abs_back2() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cp /donut /2/kyle/../kyle/donut'> $pts  && sleep 3
    check_status "abs copy back 2" "successfully"
}

function test_fs_abs_back3() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cp /donut /2/kyle/../kyle/.././donut'> $pts  && sleep 3
    check_status "abs copy back 3" "successfully"
}

function test_fs_abs_back4() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cp /donut /2/kyle/../kyle/.././kyle/kyle2/../donut'> $pts  && sleep 3
    check_status "abs copy back 4" "successfully"
}

function test_fs_abs_back4() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cp /donut /2/kyle/../kyle/.././kyle/kyle2/../donut'> $pts  && sleep 3
    check_status "abs copy back 4" "successfully"
}

function test_root_dot() {
    start_qemu
    echo ls > $pts  && sleep 3
    echo 'cd 2' > $pts  && sleep 3
    echo 'mkdir kyle' > $pts  && sleep 3
    echo 'cd kyle' > $pts  && sleep 3
    echo 'mkdir kyle2'  > $pts  && sleep 3
    echo 'cd kyle2' > $pts  && sleep 3
    echo 'cp /././donut /2/kyle/../kyle/.././kyle/kyle2/../donut'> $pts  && sleep 3
    check_status "test_root_dot" "successfully"
}


declare -i r=0
test_fs_relative;r+=$?
test_fs_relative_back;r+=$?
test_fs_relative_local;r+=$?
test_fs_abs_back;r+=$?

test_fs_abs_back2;r+=$?
test_fs_abs_back3;r+=$?
test_fs_abs_back4;r+=$?
test_fs;r+=$?
test_fs2;r+=$?
test_fs3;r+=$?
test_fs4;r+=$?
test_fs5;r+=$?
test_root_dot;r+=$?



#shell_lua_test_compute
#r+=$?
#cat_test
#r+=$?
#shell_lua_test
#r+=$?
#sleep_test
#r+=$?
#time_test
#r+=$?
#ramfs_test
#r+=$?
#kill_test
#r+=$?
#donut_test
#r+=$?
#stress_test
#r+=$?
#fs_bible_test
#r+=$?
if [ "$1" == "-d" ];
then
    cat qemu_out.log.$VAL
fi
exit $r
