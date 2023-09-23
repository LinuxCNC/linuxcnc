# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Linker flags, e.g., for linking with SQLite
LDFLAGS = -lsqlite3

# Source and object files
SRC = backend_interface.c sql_interface.c
OBJS = $(SRC:.c=.o)

# Target executable
TARGET = tool_loader

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c tooldb.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

