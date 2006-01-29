PROMPT="Please enter your password: "

all: emc_module_helper
	
emc_module_helper: module_helper.c
	$(CC) -Wall -o $@ $<
	@echo "Root permissions are needed to correctly set up the EMC module helper."
	@sudo -p $(PROMPT) chown root $@
	@sudo -p $(PROMPT) chmod u+s $@

clean:
	rm -f emc_module_helper
