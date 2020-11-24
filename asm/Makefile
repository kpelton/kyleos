all: asm.o asm_calls.o

asm.o:  asm.c asm.h
	$(CC) $(CFLAGS) -o $@ -c *.c

asm_calls.o: asm_calls.s
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -rfv *.o
	
