all: vfs.o fat.o

fat.o: fat.c fat.h
	$(CC) $(CFLAGS) -o $@ -c $<
vfs.o: vfs.c vfs.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rfv *.o
