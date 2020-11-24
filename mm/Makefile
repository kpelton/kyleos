all: mm.o paging.o

mm.o:  mm.c
	$(CC) $(CFLAGS) -o $@ -c mm.c

paging.o:  paging.c paging.h
	$(CC) $(CFLAGS) -o $@ -c paging.c


clean:
	rm -rfv *.o
