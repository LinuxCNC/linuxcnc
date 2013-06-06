
	.origin	0

        // Instructions in the form of:
        //     SXIN  #Im123, Rdst, #Im124
        //     SXIN  #Im123, Rdst, bn
.codeword	0x2e804000
.codeword	3
	SXIN	#0, R0, #1
	SXIN	0, R0, 1
	SXIN	0, &R0, 1

.codeword	0x2e807d80
.codeword	1
	SXIN	0, &R0, 124

.codeword	0x2e80419e
.codeword	1
	SXIN	0, &R30, 4

.codeword	0x2efec07e
.codeword	1
	SXIN	253, &R30.b3, 1

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end
