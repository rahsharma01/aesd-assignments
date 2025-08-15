CROSS_CC=$(CROSS_COMPILE)
CC=$(CROSS_CC)gcc
CFLAGS=-g -Wall
all: aesdsocket aesdsocket.o aesdsocket.c
aeldsocket: aesdsocket.o
	$(CC) $< -o $@
aeldsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f aesdsocket.o aesdsocket
