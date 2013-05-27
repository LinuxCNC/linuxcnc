#
#
#
MAKEFILE ?= Makefile
ASSEMBLER ?= pasm

CSTD := c99

SRCS :=	pasm.c \
	pasmpp.c \
	pasmexp.c \
	pasmop.c \
	pasmdot.c \
	pasmstruct.c \
	pasmmacro.c

OBJS := $(patsubst %.c,%.o, ${SRCS})

TESTS := rtests

CFLAGS += -D_UNIX_ -Wall

CC = gcc

SUBDIRS := $(TESTS)

%.o : %.c	$(MAKEFILE)
	@echo "  CC	$@"
	@$(CC) -c $(CFLAGS) -o $@ $(subst .o,.c, $@)

pasm :	$(OBJS)
	@echo "  LINK	$@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

all :	$(ASSEMBLER) test

.PHONY : test

test :	$(ASSEMBLER) $(MAKEFILE)
	@for dir in $(TESTS); do \
		$(MAKE) -C $$dir; \
	done

clean : clean-subdirs
	-rm -rf $(OBJS) $(ASSEMBLER)

clean-subdirs :
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

install :	$(ASSEMBLER)
	cp $(ASSEMBLER) ../$(ASSEMBLER)

depend :
	@echo "  Appending dependency information to '$(MAKEFILE)'"
	@if grep '^# DO NOT DELETE' $(MAKEFILE) >/dev/null; then \
		sed -e '/^# DO NOT DELETE/,$$d' $(MAKEFILE) > $(MAKEFILE).$$$$ && \
			mv -f $(MAKEFILE).$$$$ $(MAKEFILE); \
	fi ; \
	echo '# DO NOT DELETE THIS LINE -- make depend depends on it.' >> $(MAKEFILE) ; \
	$(CC) -MM $(SRCS) >> $(MAKEFILE)
# DO NOT DELETE THIS LINE -- make depend depends on it.
pasm.o: pasm.c pasm.h pru_ins.h pasmdbg.h
pasmpp.o: pasmpp.c pasm.h pru_ins.h
pasmexp.o: pasmexp.c pasm.h pru_ins.h
pasmop.o: pasmop.c pasm.h pru_ins.h
pasmdot.o: pasmdot.c pasm.h pru_ins.h
pasmstruct.o: pasmstruct.c pasm.h pru_ins.h
pasmmacro.o: pasmmacro.c pasm.h pru_ins.h
