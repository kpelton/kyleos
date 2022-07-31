
all: test

test: test.c
	gcc -m64 -Wall -Wextra -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 $^ -o test

install: test
	cp test /media/kyle/1143-B4D8
clean: test
	rm test
