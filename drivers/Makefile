all: pci.o

pci.o: pci.c pci.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rfv *.o
