
	.origin	0

        // Instructions in the form of:
        //     SXCHG #Im123, Rsrc, #Im124
        //     SXCHG #Im123, Rsrc, bn
.codeword	0x2f804000
.codeword	3
	SXCHG	#0, R0, #1
	SXCHG	0, R0, 1
	SXCHG	0, &R0, 1

.codeword	0x2f807d80
.codeword	1
	SXCHG	0, &R0, 124

.codeword	0x2f80419e
.codeword	1
	SXCHG	0, &R30, 4

.codeword	0x2ffec07e
.codeword	1
	SXCHG	253, &R30.b3, 1

.codeword	0x2f8ec5f5
.codeword	1
	SXCHG	#29, R21.b3, 12

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end