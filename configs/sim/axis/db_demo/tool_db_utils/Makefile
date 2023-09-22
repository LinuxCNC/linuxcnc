# Makefile

CC=gcc
CFLAGS=-Wall 
LIBS=-lsqlite3

all: tool_loader

tool_loader: tool_loader.o
	$(CC) $(CFLAGS) -o tool_loader tool_loader.o $(LIBS)

tool_loader.o: tool_loader.c
	$(CC) $(CFLAGS) -c tool_loader.c

clean:
	rm -f tool_loader.o tool_loader

