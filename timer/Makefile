all: pit.o timer.o

pit.o:  pit.c pit.h
	$(CC) $(CFLAGS) -o $@ -c pit.c

timer.h:  timer.c timer.h
	$(CC) $(CFLAGS) -o $@ -c timer.c

clean:
	rm -rfv *.o
