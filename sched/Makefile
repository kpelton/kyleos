all: sched.o

sched.o :  sched.c sched.h
	$(CC) $(CFLAGS) -o $@ -c sched.c

exec.o :  exec.c exec.h
	$(CC) $(CFLAGS) -o $@ -c exec.c

ps.o : ps.c ps.h
	$(CC) $(CFLAGS) -o $@ -c ps.c

#asm_calls.o: asm_calls.s
#	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -rfv *.o
	
