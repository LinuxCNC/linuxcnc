
	.origin	0

        // Instructions in the form of:
        //     ZERO  &Rdst,  #Im124
        //     ZERO  &Rdst,  bn
        //     ZERO  #Im123, #Im124
        //     ZERO  #Im123, bn

.codeword	0x2eff8000
.codeword	4
	ZERO	&R0, #1
	ZERO	#0, #1
	ZERO	0, #1
	ZERO	0, 1

.codeword	0x2effbe00
.codeword	3
	ZERO	&R0, b0
	ZERO	#0, b0
	ZERO	0, b0

.codeword	0x2eff8060
.codeword	2
	ZERO	&R0.b3, 1
	ZERO	3, #1

.codeword	0x2eff803e
.codeword	3
	ZERO	&R30.b1, 1
	ZERO	&R30.w1, 1
	ZERO	121, #1

.codeword	0x2eff805e
.codeword	3
	ZERO	&R30.b2, 1
	ZERO	&R30.w2, 1
	ZERO	122, #1

.codeword	0x2eff807e
.codeword	2
	ZERO	&R30.b3, 1
	ZERO	123, #1

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end