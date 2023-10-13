#include "jacobiandoubletests.hpp"

namespace KDL {


void checkDoubleOps() {
    KDL_CTX;
	checkUnary<OpTan,double>::check();
	checkUnary<OpExp,double>::check();
	checkUnary<OpSin,double>::check();
	checkUnary<OpCos,double>::check();
	checkUnary<OpLog,double>::check(&posrandom);
	checkUnary<OpAtan,double>::check();
	checkUnary<OpAsin,double>::check(&random,1E-8,1E-3);
	checkUnary<OpAcos,double>::check(&random,1E-8,1E-3);
	checkUnary<OpSqrt,double>::check(&posrandom);
	checkBinary<OpMult,double,double>::check();
	checkBinary<OpAtan2,double,double>::check(1E-8,1E-3);
}

/*
void checkDoubleCodeSize() {
	PV<double> a(2);
	PV<double> b(2);
	PV<double> res(2);
	random(a);
	random(b);
	checkDoubleCodeSizeMult(a,b,res);
}
** VISUAL C++ assembler code : 
 * Shows that there is little overhead from the framework
 * There is some overhead because of the isConstant() tests, but this pays itself back
 * if one deals with e.g. Frames or higher number of derivatives.
 *
?checkDoubleCodeSizeMult@@YAXABV?$PV@N@@0AAV1@@Z PROC NEAR ; checkDoubleCodeSizeMult, COMDAT

; 60   : 	res = a*b;

	mov	ecx, DWORD PTR _b$[esp-4]
	mov	edx, DWORD PTR _res$[esp-4]
	push	esi
	mov	esi, DWORD PTR _a$[esp]
	fld	QWORD PTR [esi]
	mov	al, BYTE PTR [esi+16]
	test	al, al
	fmul	QWORD PTR [ecx]
	push	edi
	fstp	QWORD PTR [edx]
	je	SHORT $L13435
	mov	al, BYTE PTR [ecx+16]
	test	al, al
	je	SHORT $L13435
	mov	eax, 1
	jmp	SHORT $L13436
$L13435:
	xor	eax, eax
$L13436:
	test	al, al
	mov	BYTE PTR [edx+16], al
	jne	SHORT $L13419
	mov	edi, DWORD PTR [edx+12]
	xor	eax, eax
	test	edi, edi
	jle	SHORT $L13419
	mov	edx, DWORD PTR [edx+8]
	push	ebx
$L13417:
	mov	bl, BYTE PTR [esi+16]
	test	bl, bl
	je	SHORT $L13446
	mov	ebx, DWORD PTR [ecx+8]
	fld	QWORD PTR [ebx+eax*8]
	fmul	QWORD PTR [esi]
	jmp	SHORT $L13445
$L13446:
	mov	bl, BYTE PTR [ecx+16]
	test	bl, bl
	je	SHORT $L13447
	mov	ebx, DWORD PTR [esi+8]
	fld	QWORD PTR [ebx+eax*8]
	fmul	QWORD PTR [ecx]
	jmp	SHORT $L13445
$L13447:
	mov	ebx, DWORD PTR [ecx+8]
	fld	QWORD PTR [ebx+eax*8]
	mov	ebx, DWORD PTR [esi+8]
	fmul	QWORD PTR [esi]
	fld	QWORD PTR [ebx+eax*8]
	fmul	QWORD PTR [ecx]
	faddp	ST(1), ST(0)
$L13445:
	fstp	QWORD PTR [edx+eax*8]
	inc	eax
	cmp	eax, edi
	jl	SHORT $L13417
	pop	ebx
$L13419:
	pop	edi
	pop	esi

; 61   : }

*
void checkDoubleCodeSizeMult(const PV<double>& a,const PV<double>& b,PV<double>& res) {
	res = a*b;
}*/

} // namespace


