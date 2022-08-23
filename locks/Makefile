all: spinlock.o mutex.o

spinlock.o:  spinlock.c
	$(CC) $(CFLAGS) -o $@ -c spinlock.c

mutex.o:  mutex.c
	$(CC) $(CFLAGS) -o $@ -c mutex.c
	rm -rfv *.o
