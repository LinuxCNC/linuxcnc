
LINUXCNCDIR = ../../../..

#CFLAGS += 
CPPFLAGS = -O2 -I./include -I./ -I$(LINUXCNCDIR)/include -I$(LINUXCNCDIR)/src/emc/usr_intf -s -D__DIR__="\"$(dir $(abspath $<))"\" 
LDFLAGS += -L$(LINUXCNCDIR)/lib -lboost_serialization-mt -lboost_thread-mt -lpthread -llinuxcnc -lnml -llinuxcncini -ldl
EXEC = emcweb
OBJS =	emccontroller.o main.o mongoose_wrapper.o mongoose.o shcom.o

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) $(CPPFLAGS) -o $@ $(OBJS)  $(LDFLAGS) 

shcom.o: $(LINUXCNCDIR)/src/emc/usr_intf/shcom.cc
	$(CC) $(CPPFLAGS) -c  $<

clean:
	-rm -f $(EXEC) *.o

