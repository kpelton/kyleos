all: kernel.o loader.o tables.o dshell.o syscall.o

tables.o:  tables.c tables.h
	$(CC) $(CFLAGS) -o $@ -c $<
kernel.o:  kernel.c
	$(CC) $(CFLAGS) -o $@ -c $<
loader.o: loader.s
	$(AS) $(ASFLAGS) -o $@ $<
dshell.o: dshell.c
	$(CC) $(CFLAGS) -o $@ -c $<
syscall.o: syscall.c
	$(CC) $(CFLAGS) -o $@ -c $<
clean:
	rm -rfv *.o
	
