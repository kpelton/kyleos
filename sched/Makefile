all: sched.o

sched.o :  sched.c sched.h
	$(CC) $(CFLAGS) -o $@ -c sched.c

exec.o :  exec.c exec.h
	$(CC) $(CFLAGS) -o $@ -c exec.h

#asm_calls.o: asm_calls.s
#	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -rfv *.o
	
