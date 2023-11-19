all: vfs.o fat.o ramfs.o

fat.o: fat.c fat.h
	$(CC) $(CFLAGS) -o $@ -c $<
vfs.o: vfs.c vfs.h
	$(CC) $(CFLAGS) -o $@ -c $<
ramfs.o: ramfs.c ramfs.h
	$(CC) $(CFLAGS) -o $@ -c $<
clean:
	rm -rfv *.o
