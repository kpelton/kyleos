CC ?= gcc
NEWLIB_INSTALL ?= /tmp/z
CFLAGS = -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone  -static  -I $(NEWLIB_INSTALL)/include -fno-stack-protector
#CFLAGS = -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -static  -I /tmp/z/include
LDFLAGS = $(NEWLIB_INSTALL)/lib/libc.a $(NEWLIB_INSTALL)/lib/libm.a
all: forktest names pyramid fullpyramid quicksort fputest donut fpuerrortest filetest write kedit cp mv ued

forktest: forktest.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

donut: donut.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

fputest: fputest.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

fpuerrortest: fpuerrortest.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

filetest: filetest.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

filetestprint: filetestprint
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@


exectest: exectest.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

pyramid: pyramid.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

fullpyramid: fullpyramid.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

quicksort: quicksort.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

names: names.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

write: write.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

kedit: kedit.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

cp: cp.c path_destination.h
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

mv: mv.c path_destination.h
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@

ued: ued.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o progs/$@


clean:
	rm progs/*

install:
	sudo mount /dev/mapper/loop21p1 /mnt
	sudo mkdir -p /mnt/progs 
	sudo cp progs/* /mnt/progs/ 
	sudo umount /mnt
