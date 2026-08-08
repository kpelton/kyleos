all: mm.o paging.o pmem.o vmm.o mtrr.o

mm.o:  mm.c
	$(CC) $(CFLAGS) -o $@ -c $<

paging.o:  paging.c
	$(CC) $(CFLAGS) -o $@ -c $<

pmem.o:  pmem.c pmem.h
	$(CC) $(CFLAGS) -o $@ -c $<

vmm.o:  vmm.c vmm.h
	$(CC) $(CFLAGS) -o $@ -c $<

mtrr.o:  mtrr.c mtrr.h
	$(CC) $(CFLAGS) -o $@ -c $<
	
clean:
	rm -rfv *.o
