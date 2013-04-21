
	.origin	0

        // Instruction in the form of:
        //     LOOP  #Im65535, Rcnt
        //     LOOP  #Im65535, #Im255
.codeword	0x30000000
.codeword	3
L1:
	LOOP	L1, R0.b0
L1A:
	LOOP	L1A, R0.b0
L1B:
	LOOP	L1B, R0.b0

.codeword	0x30200000
.codeword	1
L11:
	LOOP	L11, R0.b1

.codeword	0x30800000
.codeword	1
L2:
	LOOP	L2, R0.w0

.codeword	0x30E00000
.codeword	1
L3:
	LOOP	L3, R0

.codeword	0x31000000
.codeword	1
L4:
	LOOP	L4, #1

.codeword	0x31000001
.codeword	2
	LOOP	L5, #1
L5:
	LOOP	L6, #1
L6:

.codeword	0x31010001
.codeword	1
	LOOP	L7, #2
L7:

.codeword	0x311F0001
.codeword	1
	LOOP	L8, #32
L8:

// Add one instruction to work around assembler bug
the_end:
	QBA	the_end

	.end