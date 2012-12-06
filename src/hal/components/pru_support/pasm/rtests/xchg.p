
	.origin	0

        // Instructions in the form of:
        //     XCHG #Im123, Rsrc, #Im124
        //     XCHG #Im123, Rsrc, bn
.codeword	0x2f800000
.codeword	3
	XCHG	#0, R0, #1
	XCHG	0, R0, 1
	XCHG	0, &R0, 1

.codeword	0x2f803d80
.codeword	1
	XCHG	0, &R0, 124

.codeword	0x2f80019e
.codeword	1
	XCHG	0, &R30, 4

.codeword	0x2ffe807e
.codeword	1
	XCHG	253, &R30.b3, 1

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end