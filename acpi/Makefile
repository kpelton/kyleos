all: acpi.o

acpi.o: acpi.c acpi.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rfv *.o
