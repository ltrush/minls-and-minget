CFLAGS = -Wall -Wextra -Werror -g

all: minls minget

minls: minls.o utils.o
	gcc $(CFLAGS) minls.o utils.o -o minls

minls.o: minls.c utils.h
	gcc $(CFLAGS) -c minls.c -o minls.o

minget: minget.o utils.o
	gcc $(CFLAGS) minget.o utils.o -o minget

minget.o: minget.c utils.h
	gcc $(CFLAGS) -c minget.c -o minget.o

utils.o: utils.c utils.h
	gcc $(CFLAGS) -c utils.c -o utils.o
clean:
	rm -rf *.o minls minget logminfs.* diffminfs.* errminfs.* testminfs.*
