PROMPT="Please enter your password: "

all: emc_module_helper
	
emc_module_helper: module_helper.c
	$(CC) -Wall -o $@ $<

clean:
	rm -f emc_module_helper
