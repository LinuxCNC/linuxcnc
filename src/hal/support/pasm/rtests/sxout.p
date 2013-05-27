
	.origin	0

        // Instructions in the form of:
        //     SXOUT #Im123, Rsrc, #Im124
        //     SXOUT #Im123, Rsrc, bn
.codeword	0x2f004000
.codeword	3
	SXOUT	#0, R0, #1
	SXOUT	0, R0, 1
	SXOUT	0, &R0, 1

.codeword	0x2f007d80
.codeword	1
	SXOUT	0, &R0, 124

.codeword	0x2f00419e
.codeword	1
	SXOUT	0, &R30, 4

.codeword	0x2f7ec07e
.codeword	1
	SXOUT	253, &R30.b3, 1

.codeword	0x2f4e6f00
.codeword	1
	SXOUT	#156, R0.b0, 95

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end