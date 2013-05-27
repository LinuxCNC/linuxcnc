
	.origin	0

        // Instructions in the form of:
        //     XIN  #Im123, Rdst, #Im124
        //     XIN  #Im123, Rdst, bn
.codeword	0x2e800000
.codeword	3
	XIN	#0, R0, #1
	XIN	0, R0, 1
	XIN	0, &R0, 1

.codeword	0x2e803d80
.codeword	1
	XIN	0, &R0, 124

.codeword	0x2e80019e
.codeword	1
	XIN	0, &R30, 4

.codeword	0x2efe807e
.codeword	1
	XIN	253, &R30.b3, 1

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end