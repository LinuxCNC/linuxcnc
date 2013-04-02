CPPFLAGS := $(shell python-config --cflags) -fPIC
LDFLAGS := -L.

#CFLAGS=-O3 -g -Wall
CFLAGS=-O0 -g -Wall

all: ringtest mttest python

python: libring.so ring.so

ringtest:  test.o ring.o
	gcc -g test.o ring.o -o ringtest

libring.so: ring.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

ring.so: pyring.o
	$(CC) $(LDFLAGS) -shared -o $@ $^ -lboost_python -lring

pyring.o: ring.h

ring.o:	ring.c ring.h


mttest:	 ring.o mttest.o
	gcc -g mttest.o ring.o -lpthread -o mttest


clean:
	rm -f test.o ring.o
