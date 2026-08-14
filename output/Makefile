all: output.o vga.o uart.o keyboard.o input.o mouse.o

output.o:  output.c output.h
	$(CC) $(CFLAGS) -o $@ -c output.c

uart.o:  uart.c uart.h
	$(CC) $(CFLAGS) -o $@  -c uart.c

font.h: font.psf.gz ../scripts/psf-to-header.sh
	sh ../scripts/psf-to-header.sh $< $@

vga.o:  vga.c vga.h font.h
	$(CC) $(CFLAGS) -o $@ -c vga.c

keyboard.o:  keyboard.c keyboard.h
	$(CC) $(CFLAGS) -o $@ -c keyboard.c

input.o:  input.c input.h
	$(CC) $(CFLAGS) -o $@ -c input.c

mouse.o: mouse.c mouse.h
	$(CC) $(CFLAGS) -o $@ -c mouse.c


clean:
	rm -rfv *.o
	
