all: kernel.o loader.o tables.o dshell.o syscall.o

tables.o:  tables.c tables.h
	$(CC) $(CFLAGS) -o $@ -c tables.c
kernel.o:  kernel.c
	$(CC) $(CFLAGS) -o $@ -c kernel.c
loader.o: loader.s
	$(AS) $(ASFLAGS) -o $@ $<
dshell.o: dshell.c
	$(CC) $(CFLAGS) -o $@ -c dshell.c
syscall.o: syscall.c
	$(CC) $(CFLAGS) -o $@ -c syscall.c
	
clean:
	rm -rfv *.o
	
