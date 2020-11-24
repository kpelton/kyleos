all: output.o vga.o uart.o

output.o:  output.c output.h
	$(CC) $(CFLAGS) -o $@ -c output.c

uart.o:  uart.c uart.h
	$(CC) $(CFLAGS) -o $@  -c uart.c

vga.o:  vga.c vga.h
	$(CC) $(CFLAGS) -o $@ -c vga.c



clean:
	rm -rfv *.o
	
