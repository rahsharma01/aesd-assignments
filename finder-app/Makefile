CC=gcc
#CROSS_COMPILE=aarch64-none-linux-gnu-
CFLAGS=-g -Wall

.PHONY: all clean

all: writer writer.o writer.c
writer: writer.o
	$(CROSS_COMPILE)$(CC) $< -o $@
writer.o: writer.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $< -o $@ 
clean:
	rm -f writer.o writer
