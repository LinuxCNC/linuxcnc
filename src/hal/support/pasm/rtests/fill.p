
	.origin	0

        // Instructions in the form of:
        //     FILL  &Rdst,  #Im124
        //     FILL  &Rdst,  bn
        //     FILL  #Im123, #Im124
        //     FILL  #Im123, bn
.codeword	0x2eff0000
.codeword	4
	FILL	&R0, #1
	FILL	#0, #1
	FILL	0, #1
	FILL	0, 1

.codeword	0x2eff3e00
.codeword	3
	FILL	&R0, b0
	FILL	#0, b0
	FILL	0, b0

.codeword	0x2eff0060
.codeword	2
	FILL	&R0.b3, 1
	FILL	3, #1

.codeword	0x2eff003e
.codeword	3
	FILL	&R30.b1, 1
	FILL	&R30.w1, 1
	FILL	121, #1

.codeword	0x2eff005e
.codeword	3
	FILL	&R30.b2, 1
	FILL	&R30.w2, 1
	FILL	122, #1

.codeword	0x2eff007e
.codeword	2
	FILL	&R30.b3, 1
	FILL	123, #1

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end