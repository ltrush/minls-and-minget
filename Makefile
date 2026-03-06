CFLAGS = -Wall -Wextra -Werror -g

all: minls

minls: minls.o
	gcc $(CFLAGS) minls.o -o minls

minls.o: minls.c
	gcc $(CFLAGS) -c minls.c -o minls.o

clean:
	rm -rf *.o minls minget
