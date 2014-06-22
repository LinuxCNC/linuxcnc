# get at various _CFLAGS
include ../../../Makefile.inc

all:	demowtplugin.o
	comp --install micromot.comp

clean:
	rm -f demowtplugin.o webtalk_demoplugin.o

CXXFLAGS := -std=c++0x -g -fPIC -Wall		\
	-I../protobuf 		\
	-I../../../../include \
	$(PROTOBUF_CFLAGS) 	\
	$(CZMQ_CFLAGS) 		\
	$(JANSSON_CFLAGS)	\
	$(LWS_CFLAGS)		\
	$(SSL_CFLAGS)		\
	$(UUID_CFLAGS) 		\
	$(AVAHI_CFLAGS) 	\
	$(URIPARSER_CFLAGS)


demowtplugin.o: webtalk_demoplugin.o
	@echo Linking $(notdir $@)
	$(CC) -g -shared -o $@ $^
