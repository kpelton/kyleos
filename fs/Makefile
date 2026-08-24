all: vfs.o fat.o ramfs.o pipe.o pty.o

fat.o: fat.c fat.h
	$(CC) $(CFLAGS) -o $@ -c $<
vfs.o: vfs.c vfs.h
	$(CC) $(CFLAGS) -o $@ -c $<
ramfs.o: ramfs.c ramfs.h
	$(CC) $(CFLAGS) -o $@ -c $<
pipe.o: pipe.c pipe.h
	$(CC) $(CFLAGS) -o $@ -c $<
pty.o: pty.c pty.h
	$(CC) $(CFLAGS) -o $@ -c $<
clean:
	rm -rfv *.o
