include ../../../Makefile.inc

CXXFLAGS += -Wall -g -I. -I../../include $(PROTOBUF_CFLAGS) $(JANSSON_CFLAGS)

PROTOC_INCLUDE := --proto_path=. \
	--proto_path=/usr/include \
	--proto_path=../../nanopb/generator/proto

NPB_PROTOC_FLAGS :=  --plugin=protoc-gen-nanopb=../../../../lib/python/protoc-gen-nanopb
NPB_CFLAGS := -Wall -g -I../../nanopb -I.
# see note on PBDEP_OPT below
vpath %.proto  .:../../nanopb/generator/proto

all:	demo_pb2.py c++parse npbparse

demo:	 c++parse npbparse pydemo decode-raw decode-symbolic pydemo cppdemo npbdemo

clean:
	rm -f demo_pb2.py demo.pb.* demo.npb.* nanopb.* c++parse npbparse *.o *.pyc \
	 c++parse npbparse

%_pb2.py:	%.proto
	protoc   $(PROTOC_INCLUDE) --python_out=. $<


%.pb.cc:	%.proto
	protoc   $(PROTOC_INCLUDE) --cpp_out=. $<


%.npb.c:	%.proto
	protoc $(PROTOC_INCLUDE) $(NPB_PROTOC_FLAGS) \
		--nanopb_out="--extension=.npb:." $<

nanopb.pb.cc: ../../nanopb/generator/proto/nanopb.proto
	protoc   $(PROTOC_INCLUDE) --cpp_out=. $<

# show various output formats of serialisation
pydemo:	demo_pb2.py
	python pydemo.py text	# stock protobuf text representation
	python pydemo.py hex   # wire format hex dump
	python pydemo.py json  # autoconvert into JSON
	python pydemo.py binary | od -cx  # wire format

# show how to parse those in c++
cppdemo:	demo_pb2.py c++parse
	python pydemo.py binary | ./c++parse binary
	python pydemo.py text   | ./c++parse text
	python pydemo.py json   | ./c++parse json

# show how to parse a binary wireformat message in C (realtime compatible)
# using nanopb
npbdemo:	demo_pb2.py npbparse
	python pydemo.py binary | ./npbparse binary


# decode wireformat message into tag/value using protoc
decode-raw: demo_pb2.py
	python pydemo.py binary | protoc --decode_raw

# if the message type is known and the descriptor available,
# protoc can parse the message into its symbolic format:
decode-symbolic: demo_pb2.py
	python pydemo.py binary | protoc $(PROTOC_INCLUDE)  --decode=pb.DemoContainer demo.proto

json2pb.o: ../../lib/json2pb.cc

c++parse: demo.pb.cc nanopb.pb.cc cppmain.cc ../../lib/json2pb.cc
	g++ $^ $(CXXFLAGS) -o c++parse  $(PROTOBUF_LIBS) $(JANSSON_LIBS) -lstdc++

npbparse: npbmain.c ../../nanopb/pb_decode.c ../../nanopb/pb_common.c demo.npb.c
	gcc $^ $(NPB_CFLAGS) -o npbparse
