all: irq.o

irq.o:  irq.c irq.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rfv *.o
