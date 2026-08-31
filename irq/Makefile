all: irq.o apic.o

irq.o:  irq.c irq.h
	$(CC) $(CFLAGS) -o $@ -c $<

apic.o: apic.c apic.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rfv *.o
