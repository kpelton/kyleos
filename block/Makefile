all: ata.o fat.o vfs.o

ata.o:  ata.c
	$(CC) $(CFLAGS) -o $@ -c ata.c
fat.o: fat.c
	$(CC) $(CFLAGS) -o $@ -c fat.c
vfs.o: vfs.o
	$(CC) $(CFLAGS) -o $@ -c vfs.c

clean:
	rm -rfv *.o
