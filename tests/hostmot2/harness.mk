# Used with src/Makefile so mocks inherit the configured realtime build flags.
ifneq ($(BUILD_SYS),uspace)
$(error HostMot2 mock tests require a configured uspace build)
endif

ifneq ($(SANITIZERS),)
HM2_TEST_SANITIZERS = -fsanitize=$(SANITIZERS) -fno-sanitize-recover=all
endif

.PHONY: hm2-test
hm2-test:
	$(CC) $(OPT) $(DEBUG) $(EXTRA_DEBUG) -DRTAPI \
	    $(EXTRA_CFLAGS) $(XENOMAI_EVL_CFLAGS) $(HM2_TEST_SANITIZERS) \
	    -I$(BASEPWD)/hal/drivers/mesa-hostmot2 -ffunction-sections -fdata-sections \
	    $(TEST_CFLAGS) $(TEST_SOURCES) -Wl,--gc-sections $(TEST_LIBS) \
	    $(XENOMAI_EVL_LDFLAGS) $(LDFLAGS) -o "$(TEST_OUTPUT)"
