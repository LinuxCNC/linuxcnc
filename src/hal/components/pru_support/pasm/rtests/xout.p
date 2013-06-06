
	.origin	0

        // Instructions in the form of:
        //     XOUT #Im123, Rsrc, #Im124
        //     XOUT #Im123, Rsrc, bn
.codeword	0x2f000000
.codeword	3
	XOUT	#0, R0, #1
	XOUT	0, R0, 1
	XOUT	0, &R0, 1

.codeword	0x2f003d80
.codeword	1
	XOUT	0, &R0, 124

.codeword	0x2f00019e
.codeword	1
	XOUT	0, &R30, 4

.codeword	0x2f7e807e
.codeword	1
	XOUT	253, &R30.b3, 1

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end