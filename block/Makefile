all: ata.o

ata.o: ata.c ata.h ../drivers/pci.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rfv *.o
