#
#
#
MAKEFILE ?= Makefile
ASSEMBLER ?= ../pasm
EVALUATOR ?= ../reval.py

SRCS :=	xchg.p xin.p xout.p \
	zero.p fill.p \
	sxchg.p sxin.p sxout.p \
	loop.p iloop.p

OBJS := $(patsubst %.p,%.bin, ${SRCS})
LSTS := $(patsubst %.p,%.txt, ${SRCS})

.PHONY : all

all : $(OBJS)

%.bin : %.p $(MAKEFILE) $(ASSEMBLER) $(EVALUATOR)
	@echo -n "  TEST	$<"
	@$(ASSEMBLER) -b -L $< $(subst .p,, $<) 2>&1 >/dev/null ; \
		if test $$? -ne 0 ; \
		then \
			$(ASSEMBLER) -b -L $< $(subst .p,, $<) ; \
		else \
			$(EVALUATOR) $@ 2>&1 >/dev/null ; \
			if test $$? -ne 0 ; \
			then echo "	- FAIL" ; \
			else echo "	- OK" ; \
			fi \
		fi

clean:
	-rm $(OBJS) $(LSTS)
