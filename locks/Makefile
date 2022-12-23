all: spinlock.o mutex.o

spinlock.o:  spinlock.c spinlock.h
	$(CC) $(CFLAGS) -o $@ -c $<

mutex.o:  mutex.c mutex.h
	$(CC) $(CFLAGS) -o $@ -c $<
	rm -rfv *.o
