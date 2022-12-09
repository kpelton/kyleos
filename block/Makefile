all: ata.o

ata.o:  ata.c ata.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rfv *.o
