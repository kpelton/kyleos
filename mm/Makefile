all: mm.o paging.o

mm.o:  mm.c
	$(CC) $(CFLAGS) -o $@ -c $<

paging.o:  paging.c paging.h
	$(CC) $(CFLAGS) -o $@ -c $<

pmem.o:  pmem.c pmem.h
	$(CC) $(CFLAGS) -o $@ -c $<

vmm.o:  vmm.c vmm.h
	$(CC) $(CFLAGS) -o $@ -c $<
	
clean:
	rm -rfv *.o
