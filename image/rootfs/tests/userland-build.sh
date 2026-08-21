echo USERLAND-BUILD-START
if cc -vvv /usr/src/kyleos-userland/core/argv.c -o /tmp/argv then echo BUILD-ARGV-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/cat.c -o /tmp/cat then echo BUILD-CAT-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/cp.c -o /tmp/cp then echo BUILD-CP-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/ed.c -o /tmp/ed then echo BUILD-ED-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/echo.c -o /tmp/echo then echo BUILD-ECHO-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/grep.c -o /tmp/grep then echo BUILD-GREP-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/head.c -o /tmp/head then echo BUILD-HEAD-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/ls.c -o /tmp/ls then echo BUILD-LS-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/mkdir.c -o /tmp/mkdir then echo BUILD-MKDIR-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/progs/mv.c -o /tmp/mv then echo BUILD-MV-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/nushell.c -o /tmp/nushell then echo BUILD-NUSHELL-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/rm.c -o /tmp/rm then echo BUILD-RM-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/stat.c -o /tmp/stat then echo BUILD-STAT-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/tail.c -o /tmp/tail then echo BUILD-TAIL-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/wc.c -o /tmp/wc then echo BUILD-WC-PASS else exit 1
if cc -vvv /usr/src/kyleos-userland/core/write.c -o /tmp/write then echo BUILD-WRITE-PASS else exit 1
echo USERLAND-BUILD-COMPLETE
exit 0
