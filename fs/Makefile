all: vfs.o fat.o

fat.o: fat.c
	$(CC) $(CFLAGS) -o $@ -c fat.c
vfs.o: vfs.c
	$(CC) $(CFLAGS) -o $@ -c vfs.c

clean:
	rm -rfv *.o
