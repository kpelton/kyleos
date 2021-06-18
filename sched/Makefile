all: sched.o

sched.o :  sched.c sched.h
	$(CC) $(CFLAGS) -o $@ -c *.c

#asm_calls.o: asm_calls.s
#	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -rfv *.o
	
