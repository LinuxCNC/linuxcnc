

CFLAGS=-O3 -g -Wall -fPIC
LDFLAGS = -O3 -g -fPIC

all: libring.so build.ring build.main

libring.so: ring.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

ring.o:	ring.c ring.h

build.%:
	CGO_CFLAGS=-I`pwd` CGO_LDFLAGS=-L`pwd` GOPATH=. go build -v $*.go

clean:
	rm -f test.o ring.o
