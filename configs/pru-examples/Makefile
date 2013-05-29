
TARGET=pruexample.bin pru_main.bin pru_generic.bin

all:	$(TARGET)


%.bin:	%.p
	echo  Assembling PRU code $@
	pasm -b -d -L $(basename $@).p

clean:
	rm -f *.bin *.dbg *.lst
