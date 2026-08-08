echo USERLAND-BUILD-START
echo BUILD argv
cc -vvv /usr/src/kyleos-userland/core/argv.c -o /tmp/argv
echo BUILD cat
cc -vvv /usr/src/kyleos-userland/core/cat.c -o /tmp/cat
echo BUILD cp
cc -vvv /usr/src/kyleos-userland/core/cp.c -o /tmp/cp
echo BUILD ed
cc -vvv /usr/src/kyleos-userland/core/ed.c -o /tmp/ed
echo BUILD echo
cc -vvv /usr/src/kyleos-userland/core/echo.c -o /tmp/echo
echo BUILD grep
cc -vvv /usr/src/kyleos-userland/core/grep.c -o /tmp/grep
echo BUILD head
cc -vvv /usr/src/kyleos-userland/core/head.c -o /tmp/head
echo BUILD ls
cc -vvv /usr/src/kyleos-userland/core/ls.c -o /tmp/ls
echo BUILD mkdir
cc -vvv /usr/src/kyleos-userland/core/mkdir.c -o /tmp/mkdir
echo BUILD mv
cc -vvv /usr/src/kyleos-userland/progs/mv.c -o /tmp/mv
echo BUILD nushell
cc -vvv /usr/src/kyleos-userland/core/nushell.c -o /tmp/nushell
echo BUILD rm
cc -vvv /usr/src/kyleos-userland/core/rm.c -o /tmp/rm
echo BUILD stat
cc -vvv /usr/src/kyleos-userland/core/stat.c -o /tmp/stat
echo BUILD tail
cc -vvv /usr/src/kyleos-userland/core/tail.c -o /tmp/tail
echo BUILD wc
cc -vvv /usr/src/kyleos-userland/core/wc.c -o /tmp/wc
echo BUILD write
cc -vvv /usr/src/kyleos-userland/core/write.c -o /tmp/write
echo USERLAND-BUILD-COMPLETE
