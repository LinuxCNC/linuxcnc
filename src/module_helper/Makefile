include ../Makefile.inc

all: $(BIN_DIR)/emc_module_helper
	
$(BIN_DIR)/emc_module_helper: module_helper.c
	$(CC) -Wall -o $@ $<

clean:
	rm -f emc_module_helper
