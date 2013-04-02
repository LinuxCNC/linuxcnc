CPPFLAGS := $(shell python-config --cflags) -fPIC
LDFLAGS := -L.

all: ring.so

ring.so: pyring.o
	$(CC) $(LDFLAGS) -shared -o $@ $^ -lboost_python -lring

pyring.o: ring.h
