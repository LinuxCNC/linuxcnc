
	.origin	0

        // Instruction in the form of:
        //     ILOOP  #Im65535, Rcnt
        //     ILOOP  #Im65535, #Im255
.codeword	0x30008000
.codeword	3
L1:
	ILOOP	L1, R0.b0
L1A:
	ILOOP	L1A, R0.b0
L1B:
	ILOOP	L1B, R0.b0

.codeword	0x30208000
.codeword	1
L11:
	ILOOP	L11, R0.b1

.codeword	0x30808000
.codeword	1
L2:
	ILOOP	L2, R0.w0

.codeword	0x30E08000
.codeword	1
L3:
	ILOOP	L3, R0

.codeword	0x31008000
.codeword	1
L4:
	ILOOP	L4, #1

.codeword	0x31008001
.codeword	2
	ILOOP	L5, #1
L5:
	ILOOP	L6, #1
L6:

.codeword	0x31018001
.codeword	1
	ILOOP	L7, #2
L7:

.codeword	0x311F8001
.codeword	1
	ILOOP	L8, #32
L8:

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end