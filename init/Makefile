all: kernel.o loader.o tables.o

tables.o:  tables.c tables.h
	$(CC) $(CFLAGS) -o $@ -c tables.c

kernel.o:  kernel.c
	$(CC) $(CFLAGS) -o $@ -c kernel.c

loader.o: loader.s
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -rfv *.o
	
