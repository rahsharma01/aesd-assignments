CC=gcc
CFLAGS=-g -Wall
all: aesdsocket aesdsocket.o aesdsocket.c
aeldsocket: aesdsocket.o
	$(CROSS_COMPILE)$(CC) $< -o $@
aeldsocket.o: aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $< -o $@ 
clean:
	rm -f aesdsocket.o aesdsocket
