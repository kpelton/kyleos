#requires newlib installed 
NEWLIB_INSTALL ?= /tmp/z
LIBS ?= $(NEWLIB_INSTALL)/lib/libc.a
LIBS_M ?=  $(NEWLIB_INSTALL)/lib/libm.a
INCLUDES ?=  $(NEWLIB_INSTALL)/include
CC = gcc-13

TARGETS :=  argv argv-copy shell nushell write writebomb rdwr cp cat ed grep kedit stat ls ps kill wc head tail donut testprint mkdir constest echo rm ued
CFLAGS := -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone -static -g


all: $(TARGETS)

argv : argv.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
argv-copy : argv-copy.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
shell: shell.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
nushell: nushell.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
write: write.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
writebomb: writebomb.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
rdwr: rdwr.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
cp: cp.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
cat: cat.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
ed: ed.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
grep: grep.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
kedit: kedit.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
wc: wc.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
head: head.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
tail: tail.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
stat: stat.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
ls: ls.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
ps: ps.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
kill: kill.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
mkdir: mkdir.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@ 
echo: echo.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
donut: donut.c
	$(CC) $(CFLAGS) $< $(LIBS) $(LIBS_M) -I $(INCLUDES)  -o $@ 
testprint: testprint.c
	$(CC) $(CFLAGS) $< $(LIBS) $(LIBS_M) -I $(INCLUDES)  -o $@ 
constest: constest.c
	$(CC) $(CFLAGS) $< $(LIBS) $(LIBS_M) -I $(INCLUDES)  -o $@ 
ued: ued.c
	$(CC) $(CFLAGS) $< $(LIBS) $(LIBS_M) -I $(INCLUDES)  -o $@ 




install:
	sudo cp $(TARGETS) ~kyle/dev/wheeler/os2/diskimage
clean:
	rm -f $(TARGETS)

rm: rm.c
	$(CC) $(CFLAGS) $< $(LIBS) -I $(INCLUDES)  -o $@
