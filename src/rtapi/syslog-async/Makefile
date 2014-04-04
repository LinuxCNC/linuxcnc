all: test libs

test: syslog_test.o syslog_async.o	
	gcc syslog_async.o syslog_test.o -o syslog_test

syslog_test.o: syslog_test.c syslog_async.h
	gcc -c syslog_test.c

syslog_async.o:  syslog_async.c syslog_async.h
	gcc -Fpic -O2 -c -Wall syslog_async.c 

libs: syslog_async.o
	gcc -shared -Wl,-soname,libsyslogasync.so.1 -o \
	      libsyslogasync.so.1.0 syslog_async.o -lc

clean: 
	rm -f *.o *~ *.so.* syslog_test
