
TARGET=pruexample.bin 

all:	$(TARGET)


%.bin:	%.p
	echo  Assembling PRU code $@
	pasm -b -d -L $(basename $@).p

clean:
	rm -f *.bin *.dbg *.lst
