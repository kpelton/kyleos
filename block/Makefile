all: ata.o fat.o

ata.o:  ata.c
	$(CC) $(CFLAGS) -o $@ -c ata.c
fat.o: fat.c
	$(CC) $(CFLAGS) -o $@ -c fat.c
clean:
	rm -rfv *.o
