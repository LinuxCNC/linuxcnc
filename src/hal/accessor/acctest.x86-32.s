	.file	"acctest.c"
# GNU C (Debian 4.9.2-10) version 4.9.2 (i586-linux-gnu)
#	compiled by GNU C version 4.9.2, GMP version 6.0.0, MPFR version 3.1.2-p3, MPC version 1.0.2
# GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
# options passed:  -I . -I machinetalk/generated -I machinetalk/nanopb
# -I machinetalk/include -I machinetalk/msginfo
# -I machinetalk/msgcomponents -I machinetalk/webtalk -I libnml/linklist
# -I libnml/cms -I libnml/rcs -I libnml/inifile -I libnml/os_intf
# -I libnml/nml -I libnml/buffer -I libnml/posemath -I rtapi
# -I rtapi_export -I hal/lib -I hal/vtable-example -I hal/userfunct-example
# -I hal/drivers -I hal/cython -I hal/accessor -imultiarch i386-linux-gnu
# -D ULAPI hal/accessor/acctest.c -mtune=generic -march=i586
# -auxbase-strip /tmp/acctest.s -g -O -std=c11 -fverbose-asm
# options enabled:  -faggressive-loop-optimizations
# -fasynchronous-unwind-tables -fauto-inc-dec -fbranch-count-reg
# -fcombine-stack-adjustments -fcommon -fcompare-elim -fcprop-registers
# -fdefer-pop -fdelete-null-pointer-checks -fdwarf2-cfi-asm
# -fearly-inlining -feliminate-unused-debug-types -fforward-propagate
# -ffunction-cse -fgcse-lm -fgnu-runtime -fgnu-unique
# -fguess-branch-probability -fident -fif-conversion -fif-conversion2
# -finline -finline-atomics -finline-functions-called-once -fipa-profile
# -fipa-pure-const -fipa-reference -fira-hoist-pressure
# -fira-share-save-slots -fira-share-spill-slots -fivopts
# -fkeep-static-consts -fleading-underscore -fmath-errno -fmerge-constants
# -fmerge-debug-strings -fmove-loop-invariants -fomit-frame-pointer
# -fpcc-struct-return -fpeephole -fprefetch-loop-arrays
# -fsched-critical-path-heuristic -fsched-dep-count-heuristic
# -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
# -fsched-rank-heuristic -fsched-spec -fsched-spec-insn-heuristic
# -fsched-stalled-insns-dep -fshow-column -fshrink-wrap -fsigned-zeros
# -fsplit-ivs-in-unroller -fsplit-wide-types -fstrict-volatile-bitfields
# -fsync-libcalls -ftoplevel-reorder -ftrapping-math -ftree-bit-ccp
# -ftree-ccp -ftree-ch -ftree-coalesce-vars -ftree-copy-prop
# -ftree-copyrename -ftree-cselim -ftree-dce -ftree-dominator-opts
# -ftree-dse -ftree-forwprop -ftree-fre -ftree-loop-if-convert
# -ftree-loop-im -ftree-loop-ivcanon -ftree-loop-optimize
# -ftree-parallelize-loops= -ftree-phiprop -ftree-pta -ftree-reassoc
# -ftree-scev-cprop -ftree-sink -ftree-slsr -ftree-sra -ftree-ter
# -funit-at-a-time -funwind-tables -fvar-tracking
# -fvar-tracking-assignments -fverbose-asm -fzero-initialized-in-bss -m32
# -m80387 -m96bit-long-double -malign-stringops
# -mavx256-split-unaligned-load -mavx256-split-unaligned-store
# -mfancy-math-387 -mfp-ret-in-387 -mglibc -mieee-fp -mlong-double-80
# -mno-red-zone -mno-sse4 -mpush-args -msahf -mtls-direct-seg-refs

	.text
.Ltext0:
	.globl	foo
	.type	foo, @function
foo:
.LFB431:
	.file 1 "hal/accessor/acctest.c"
	.loc 1 29 0
	.cfi_startproc
	pushl	%ebx	#
	.cfi_def_cfa_offset 8
	.cfi_offset 3, -8
	subl	$16, %esp	#,
	.cfi_def_cfa_offset 24
.LVL0:
.LBB30:
.LBB31:
.LBB32:
.LBB33:
	.file 2 "hal/lib/hal_accessor.h"
	.loc 2 18 0
	movl	hal_shmem_base, %eax	# hal_shmem_base, D.5477
	movl	ip, %edx	# ip, ip
	movl	%eax, %ecx	# D.5477, D.5479
	addl	100(%edx), %ecx	# MEM[(struct instdata *)_2 + 100B], D.5479
.LVL1:
.LBE33:
.LBE32:
.LBB34:
.LBB35:
	addl	24(%ecx), %eax	# MEM[(const struct hal_pin *)_14].data_ptr, D.5479
.LVL2:
.LBE35:
.LBE34:
	.loc 2 79 0
	testb	$64, 17(%ecx)	#, MEM[(const struct halhdr *)_14]
	je	.L2	#,
	lock orl	$0, (%esp)
.L2:
	movzbl	(%eax), %eax	#* D.5479, D.5483
	movb	%al, 15(%esp)	# D.5483, rvalue
	movzbl	15(%esp), %eax	# rvalue, D.5485
.LBE31:
.LBE30:
	.loc 1 31 0
	movb	%al, b	# D.5485, b
.LVL3:
.LBB36:
.LBB37:
.LBB38:
.LBB39:
	.loc 2 18 0
	movl	hal_shmem_base, %eax	# hal_shmem_base, D.5477
	movl	ip, %edx	# ip, ip
	movl	%eax, %ecx	# D.5477, D.5479
.LVL4:
	addl	108(%edx), %ecx	# MEM[(struct instdata *)_6 + 108B], D.5479
	movl	%ecx, %edx	# D.5479, D.5479
.LVL5:
.LBE39:
.LBE38:
	.loc 2 58 0
	movl	$4711, %ecx	#, tmp121
	movl	24(%edx), %ebx	# MEM[(struct hal_pin *)_27].data_ptr, tmp133
	xchgl	(%eax,%ebx), %ecx	#,, tmp122
.LVL6:
	cmpb	$0, 17(%edx)	#, MEM[(const struct halhdr *)_27]
	jns	.L3	#,
	lock orl	$0, (%esp)
.L3:
.LBE37:
.LBE36:
	.loc 1 37 0
	movl	ip, %eax	# ip, ip
	movl	124(%eax), %eax	# _8->floatpin, D.5476
	fldl	.LC0	#
	fstpl	(%eax)	# *_9
	.loc 1 40 0
	addl	$16, %esp	#,
	.cfi_def_cfa_offset 8
	popl	%ebx	#
	.cfi_restore 3
	.cfi_def_cfa_offset 4
.LVL7:
	ret
	.cfi_endproc
.LFE431:
	.size	foo, .-foo
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC0:
	.long	1374389535
	.long	1074339512
	.text
.Letext0:
	.file 3 "/usr/lib/gcc/i586-linux-gnu/4.9/include/stddef.h"
	.file 4 "/usr/include/asm-generic/int-ll64.h"
	.file 5 "/usr/include/stdint.h"
	.file 6 "rtapi/rtapi_bitops.h"
	.file 7 "rtapi/rtapi_heap_private.h"
	.file 8 "rtapi/rtapi.h"
	.file 9 "hal/lib/hal_priv.h"
	.file 10 "hal/lib/hal.h"
	.file 11 "hal/lib/hal_object.h"
	.file 12 "hal/lib/hal_list.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x8f9
	.value	0x4
	.long	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.long	.LASF124
	.byte	0x1
	.long	.LASF125
	.long	.LASF126
	.long	.Ltext0
	.long	.Letext0-.Ltext0
	.long	.Ldebug_line0
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x3
	.long	.LASF5
	.byte	0x3
	.byte	0xd4
	.long	0x37
	.uleb128 0x4
	.byte	0x4
	.byte	0x7
	.long	.LASF0
	.uleb128 0x4
	.byte	0x4
	.byte	0x5
	.long	.LASF1
	.uleb128 0x4
	.byte	0x8
	.byte	0x5
	.long	.LASF2
	.uleb128 0x4
	.byte	0xc
	.byte	0x4
	.long	.LASF3
	.uleb128 0x4
	.byte	0x1
	.byte	0x6
	.long	.LASF4
	.uleb128 0x3
	.long	.LASF6
	.byte	0x4
	.byte	0x14
	.long	0x65
	.uleb128 0x4
	.byte	0x1
	.byte	0x8
	.long	.LASF7
	.uleb128 0x3
	.long	.LASF8
	.byte	0x4
	.byte	0x16
	.long	0x77
	.uleb128 0x4
	.byte	0x2
	.byte	0x5
	.long	.LASF9
	.uleb128 0x4
	.byte	0x2
	.byte	0x7
	.long	.LASF10
	.uleb128 0x3
	.long	.LASF11
	.byte	0x4
	.byte	0x19
	.long	0x25
	.uleb128 0x3
	.long	.LASF12
	.byte	0x4
	.byte	0x1a
	.long	0x37
	.uleb128 0x4
	.byte	0x8
	.byte	0x7
	.long	.LASF13
	.uleb128 0x3
	.long	.LASF14
	.byte	0x5
	.byte	0x33
	.long	0x37
	.uleb128 0x3
	.long	.LASF15
	.byte	0x6
	.byte	0x30
	.long	0xb8
	.uleb128 0x4
	.byte	0x4
	.byte	0x7
	.long	.LASF16
	.uleb128 0x5
	.byte	0x4
	.uleb128 0x4
	.byte	0x4
	.byte	0x7
	.long	.LASF17
	.uleb128 0x4
	.byte	0x1
	.byte	0x6
	.long	.LASF18
	.uleb128 0x6
	.byte	0x4
	.long	0xc8
	.uleb128 0x7
	.long	.LASF21
	.byte	0x8
	.byte	0x7
	.byte	0x1c
	.long	0xee
	.uleb128 0x8
	.long	.LASF26
	.byte	0x7
	.byte	0x1d
	.long	0xee
	.byte	0
	.byte	0
	.uleb128 0x9
	.long	0xfe
	.long	0xfe
	.uleb128 0xa
	.long	0xc1
	.byte	0
	.byte	0
	.uleb128 0x4
	.byte	0x8
	.byte	0x4
	.long	.LASF19
	.uleb128 0x3
	.long	.LASF20
	.byte	0x7
	.byte	0x1e
	.long	0xd5
	.uleb128 0x7
	.long	.LASF22
	.byte	0x4
	.byte	0x7
	.byte	0x24
	.long	0x13b
	.uleb128 0xb
	.long	.LASF23
	.byte	0x7
	.byte	0x25
	.long	0xa2
	.byte	0x4
	.byte	0x18
	.byte	0x8
	.byte	0
	.uleb128 0xb
	.long	.LASF24
	.byte	0x7
	.byte	0x26
	.long	0xa2
	.byte	0x4
	.byte	0x8
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x3
	.long	.LASF25
	.byte	0x7
	.byte	0x27
	.long	0x110
	.uleb128 0xc
	.string	"hdr"
	.byte	0x8
	.byte	0x7
	.byte	0x2a
	.long	0x16b
	.uleb128 0x8
	.long	.LASF27
	.byte	0x7
	.byte	0x2b
	.long	0xa2
	.byte	0
	.uleb128 0xd
	.string	"tag"
	.byte	0x7
	.byte	0x2c
	.long	0x13b
	.byte	0x4
	.byte	0
	.uleb128 0xe
	.long	.LASF127
	.byte	0x8
	.byte	0x7
	.byte	0x29
	.long	0x18c
	.uleb128 0xf
	.string	"s"
	.byte	0x7
	.byte	0x2d
	.long	0x146
	.uleb128 0x10
	.long	.LASF28
	.byte	0x7
	.byte	0x2e
	.long	0x105
	.byte	0
	.uleb128 0x3
	.long	.LASF29
	.byte	0x7
	.byte	0x31
	.long	0x16b
	.uleb128 0x7
	.long	.LASF30
	.byte	0x34
	.byte	0x7
	.byte	0x33
	.long	0x210
	.uleb128 0x8
	.long	.LASF31
	.byte	0x7
	.byte	0x34
	.long	0x18c
	.byte	0
	.uleb128 0x8
	.long	.LASF32
	.byte	0x7
	.byte	0x35
	.long	0x2c
	.byte	0x8
	.uleb128 0x8
	.long	.LASF33
	.byte	0x7
	.byte	0x36
	.long	0x2c
	.byte	0xc
	.uleb128 0x8
	.long	.LASF34
	.byte	0x7
	.byte	0x37
	.long	0xad
	.byte	0x10
	.uleb128 0x8
	.long	.LASF35
	.byte	0x7
	.byte	0x38
	.long	0x25
	.byte	0x14
	.uleb128 0x8
	.long	.LASF36
	.byte	0x7
	.byte	0x39
	.long	0x2c
	.byte	0x18
	.uleb128 0x8
	.long	.LASF37
	.byte	0x7
	.byte	0x3a
	.long	0x2c
	.byte	0x1c
	.uleb128 0x8
	.long	.LASF38
	.byte	0x7
	.byte	0x3b
	.long	0x25
	.byte	0x20
	.uleb128 0x8
	.long	.LASF39
	.byte	0x7
	.byte	0x3c
	.long	0x210
	.byte	0x24
	.byte	0
	.uleb128 0x9
	.long	0xc8
	.long	0x220
	.uleb128 0xa
	.long	0xc1
	.byte	0xf
	.byte	0
	.uleb128 0x3
	.long	.LASF40
	.byte	0x8
	.byte	0x97
	.long	0x85
	.uleb128 0x6
	.byte	0x4
	.long	0x197
	.uleb128 0x11
	.long	.LASF41
	.byte	0x38
	.byte	0x9
	.value	0x144
	.long	0x2b4
	.uleb128 0x12
	.string	"hdr"
	.byte	0x9
	.value	0x145
	.long	0x45e
	.byte	0
	.uleb128 0x13
	.long	.LASF42
	.byte	0x9
	.value	0x146
	.long	0x25
	.byte	0x14
	.uleb128 0x13
	.long	.LASF43
	.byte	0x9
	.value	0x147
	.long	0x25
	.byte	0x18
	.uleb128 0x13
	.long	.LASF44
	.byte	0x9
	.value	0x148
	.long	0x25
	.byte	0x1c
	.uleb128 0x13
	.long	.LASF45
	.byte	0x9
	.value	0x149
	.long	0x3a3
	.byte	0x20
	.uleb128 0x13
	.long	.LASF46
	.byte	0x9
	.value	0x14a
	.long	0x2dc
	.byte	0x28
	.uleb128 0x12
	.string	"dir"
	.byte	0x9
	.value	0x14b
	.long	0x30a
	.byte	0x2c
	.uleb128 0x13
	.long	.LASF35
	.byte	0x9
	.value	0x14c
	.long	0x25
	.byte	0x30
	.uleb128 0x13
	.long	.LASF47
	.byte	0x9
	.value	0x14d
	.long	0x5a
	.byte	0x34
	.byte	0
	.uleb128 0x14
	.byte	0x4
	.byte	0xa
	.value	0x1b8
	.long	0x2dc
	.uleb128 0x15
	.long	.LASF48
	.sleb128 -1
	.uleb128 0x15
	.long	.LASF49
	.sleb128 1
	.uleb128 0x15
	.long	.LASF50
	.sleb128 2
	.uleb128 0x15
	.long	.LASF51
	.sleb128 3
	.uleb128 0x15
	.long	.LASF52
	.sleb128 4
	.byte	0
	.uleb128 0x16
	.long	.LASF53
	.byte	0xa
	.value	0x1be
	.long	0x2b4
	.uleb128 0x14
	.byte	0x4
	.byte	0xa
	.value	0x1c8
	.long	0x30a
	.uleb128 0x15
	.long	.LASF54
	.sleb128 -1
	.uleb128 0x15
	.long	.LASF55
	.sleb128 16
	.uleb128 0x15
	.long	.LASF56
	.sleb128 32
	.uleb128 0x15
	.long	.LASF57
	.sleb128 48
	.byte	0
	.uleb128 0x16
	.long	.LASF58
	.byte	0xa
	.value	0x1cd
	.long	0x2e8
	.uleb128 0x16
	.long	.LASF59
	.byte	0xa
	.value	0x1e1
	.long	0x322
	.uleb128 0x4
	.byte	0x1
	.byte	0x2
	.long	.LASF60
	.uleb128 0x16
	.long	.LASF61
	.byte	0xa
	.value	0x1e3
	.long	0x335
	.uleb128 0x17
	.long	0x316
	.uleb128 0x16
	.long	.LASF62
	.byte	0xa
	.value	0x1e4
	.long	0x346
	.uleb128 0x17
	.long	0x90
	.uleb128 0x16
	.long	.LASF63
	.byte	0xa
	.value	0x1e5
	.long	0x357
	.uleb128 0x17
	.long	0x85
	.uleb128 0x16
	.long	.LASF64
	.byte	0xa
	.value	0x1e6
	.long	0xfe
	.uleb128 0x18
	.byte	0x8
	.byte	0xa
	.value	0x1ee
	.long	0x39e
	.uleb128 0x19
	.string	"_b"
	.byte	0xa
	.value	0x1ef
	.long	0x329
	.uleb128 0x19
	.string	"_s"
	.byte	0xa
	.value	0x1f0
	.long	0x34b
	.uleb128 0x19
	.string	"_u"
	.byte	0xa
	.value	0x1f1
	.long	0x33a
	.uleb128 0x19
	.string	"_f"
	.byte	0xa
	.value	0x1f2
	.long	0x39e
	.byte	0
	.uleb128 0x17
	.long	0x35c
	.uleb128 0x16
	.long	.LASF65
	.byte	0xa
	.value	0x1f3
	.long	0x368
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x227
	.long	0x3c6
	.uleb128 0x12
	.string	"_bp"
	.byte	0xa
	.value	0x227
	.long	0x220
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF66
	.byte	0xa
	.value	0x227
	.long	0x3af
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x228
	.long	0x3e9
	.uleb128 0x12
	.string	"_sp"
	.byte	0xa
	.value	0x228
	.long	0x220
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF67
	.byte	0xa
	.value	0x228
	.long	0x3d2
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x22a
	.long	0x40c
	.uleb128 0x12
	.string	"_fp"
	.byte	0xa
	.value	0x22a
	.long	0x220
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF68
	.byte	0xa
	.value	0x22a
	.long	0x3f5
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x22e
	.long	0x42f
	.uleb128 0x12
	.string	"_ss"
	.byte	0xa
	.value	0x22e
	.long	0x220
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF69
	.byte	0xa
	.value	0x22e
	.long	0x418
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x230
	.long	0x452
	.uleb128 0x12
	.string	"_fs"
	.byte	0xa
	.value	0x230
	.long	0x220
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF70
	.byte	0xa
	.value	0x230
	.long	0x43b
	.uleb128 0x7
	.long	.LASF71
	.byte	0x14
	.byte	0xb
	.byte	0x21
	.long	0x4f5
	.uleb128 0x8
	.long	.LASF72
	.byte	0xb
	.byte	0x22
	.long	0x516
	.byte	0
	.uleb128 0xd
	.string	"_id"
	.byte	0xb
	.byte	0x23
	.long	0x6c
	.byte	0x8
	.uleb128 0x8
	.long	.LASF73
	.byte	0xb
	.byte	0x24
	.long	0x6c
	.byte	0xa
	.uleb128 0x8
	.long	.LASF74
	.byte	0xb
	.byte	0x25
	.long	0x90
	.byte	0xc
	.uleb128 0xb
	.long	.LASF75
	.byte	0xb
	.byte	0x26
	.long	0x85
	.byte	0x4
	.byte	0x7
	.byte	0x19
	.byte	0x10
	.uleb128 0xb
	.long	.LASF76
	.byte	0xb
	.byte	0x27
	.long	0x90
	.byte	0x4
	.byte	0x1
	.byte	0x18
	.byte	0x10
	.uleb128 0xb
	.long	.LASF77
	.byte	0xb
	.byte	0x2b
	.long	0x90
	.byte	0x4
	.byte	0x5
	.byte	0x13
	.byte	0x10
	.uleb128 0xb
	.long	.LASF78
	.byte	0xb
	.byte	0x2c
	.long	0x90
	.byte	0x4
	.byte	0x1
	.byte	0x12
	.byte	0x10
	.uleb128 0xb
	.long	.LASF79
	.byte	0xb
	.byte	0x32
	.long	0x90
	.byte	0x4
	.byte	0x1
	.byte	0x11
	.byte	0x10
	.uleb128 0xb
	.long	.LASF80
	.byte	0xb
	.byte	0x34
	.long	0x90
	.byte	0x4
	.byte	0x1
	.byte	0x10
	.byte	0x10
	.byte	0
	.uleb128 0x1b
	.byte	0x8
	.byte	0xc
	.byte	0x13
	.long	0x516
	.uleb128 0x8
	.long	.LASF27
	.byte	0xc
	.byte	0x14
	.long	0x220
	.byte	0
	.uleb128 0x8
	.long	.LASF81
	.byte	0xc
	.byte	0x15
	.long	0x220
	.byte	0x4
	.byte	0
	.uleb128 0x3
	.long	.LASF82
	.byte	0xc
	.byte	0x16
	.long	0x4f5
	.uleb128 0x1c
	.value	0x100
	.byte	0x9
	.byte	0xdf
	.long	0x62b
	.uleb128 0x8
	.long	.LASF83
	.byte	0x9
	.byte	0xe0
	.long	0x25
	.byte	0
	.uleb128 0x8
	.long	.LASF34
	.byte	0x9
	.byte	0xe1
	.long	0xb8
	.byte	0x4
	.uleb128 0x8
	.long	.LASF84
	.byte	0x9
	.byte	0xe2
	.long	0x25
	.byte	0x8
	.uleb128 0x8
	.long	.LASF85
	.byte	0x9
	.byte	0xe3
	.long	0x25
	.byte	0xc
	.uleb128 0x8
	.long	.LASF86
	.byte	0x9
	.byte	0xe5
	.long	0x516
	.byte	0x10
	.uleb128 0x8
	.long	.LASF87
	.byte	0x9
	.byte	0xe6
	.long	0x516
	.byte	0x18
	.uleb128 0x8
	.long	.LASF88
	.byte	0x9
	.byte	0xe7
	.long	0x516
	.byte	0x20
	.uleb128 0x8
	.long	.LASF89
	.byte	0x9
	.byte	0xe9
	.long	0x3e
	.byte	0x28
	.uleb128 0x8
	.long	.LASF90
	.byte	0x9
	.byte	0xea
	.long	0x25
	.byte	0x2c
	.uleb128 0x8
	.long	.LASF91
	.byte	0x9
	.byte	0xed
	.long	0x25
	.byte	0x30
	.uleb128 0x8
	.long	.LASF92
	.byte	0x9
	.byte	0xef
	.long	0x65
	.byte	0x34
	.uleb128 0x8
	.long	.LASF93
	.byte	0x9
	.byte	0xf1
	.long	0x9b
	.byte	0x38
	.uleb128 0x8
	.long	.LASF94
	.byte	0x9
	.byte	0xf2
	.long	0x2c
	.byte	0x40
	.uleb128 0x8
	.long	.LASF95
	.byte	0x9
	.byte	0xf6
	.long	0x62b
	.byte	0x44
	.uleb128 0x8
	.long	.LASF96
	.byte	0x9
	.byte	0xf8
	.long	0x63b
	.byte	0x64
	.uleb128 0x8
	.long	.LASF97
	.byte	0x9
	.byte	0xfb
	.long	0x2c
	.byte	0x8c
	.uleb128 0x8
	.long	.LASF98
	.byte	0x9
	.byte	0xfc
	.long	0x2c
	.byte	0x90
	.uleb128 0x8
	.long	.LASF99
	.byte	0x9
	.byte	0xff
	.long	0x2c
	.byte	0x94
	.uleb128 0x13
	.long	.LASF100
	.byte	0x9
	.value	0x100
	.long	0x2c
	.byte	0x98
	.uleb128 0x13
	.long	.LASF101
	.byte	0x9
	.value	0x104
	.long	0x197
	.byte	0x9c
	.uleb128 0x1d
	.long	.LASF102
	.byte	0x9
	.value	0x105
	.long	0x64b
	.value	0x100
	.byte	0
	.uleb128 0x9
	.long	0xad
	.long	0x63b
	.uleb128 0xa
	.long	0xc1
	.byte	0x7
	.byte	0
	.uleb128 0x9
	.long	0xfe
	.long	0x64b
	.uleb128 0xa
	.long	0xc1
	.byte	0x4
	.byte	0
	.uleb128 0x9
	.long	0x65
	.long	0x65a
	.uleb128 0x1e
	.long	0xc1
	.byte	0
	.uleb128 0x16
	.long	.LASF103
	.byte	0x9
	.value	0x108
	.long	0x521
	.uleb128 0x6
	.byte	0x4
	.long	0x231
	.uleb128 0x7
	.long	.LASF104
	.byte	0x80
	.byte	0x1
	.byte	0x9
	.long	0x6d9
	.uleb128 0x8
	.long	.LASF105
	.byte	0x1
	.byte	0xa
	.long	0x6d9
	.byte	0
	.uleb128 0xd
	.string	"bin"
	.byte	0x1
	.byte	0xb
	.long	0x3c6
	.byte	0x64
	.uleb128 0x8
	.long	.LASF106
	.byte	0x1
	.byte	0xc
	.long	0x3c6
	.byte	0x68
	.uleb128 0x8
	.long	.LASF107
	.byte	0x1
	.byte	0xd
	.long	0x3e9
	.byte	0x6c
	.uleb128 0x8
	.long	.LASF108
	.byte	0x1
	.byte	0xe
	.long	0x40c
	.byte	0x70
	.uleb128 0x8
	.long	.LASF109
	.byte	0x1
	.byte	0x10
	.long	0x452
	.byte	0x74
	.uleb128 0x8
	.long	.LASF110
	.byte	0x1
	.byte	0x11
	.long	0x42f
	.byte	0x78
	.uleb128 0x8
	.long	.LASF111
	.byte	0x1
	.byte	0x13
	.long	0x6e9
	.byte	0x7c
	.byte	0
	.uleb128 0x9
	.long	0xc8
	.long	0x6e9
	.uleb128 0xa
	.long	0xc1
	.byte	0x63
	.byte	0
	.uleb128 0x6
	.byte	0x4
	.long	0x39e
	.uleb128 0x1f
	.long	.LASF112
	.byte	0x2
	.byte	0x11
	.long	0xbf
	.byte	0x3
	.long	0x70b
	.uleb128 0x20
	.long	.LASF114
	.byte	0x2
	.byte	0x11
	.long	0x70b
	.byte	0
	.uleb128 0x21
	.long	0x220
	.uleb128 0x1f
	.long	.LASF113
	.byte	0xb
	.byte	0x46
	.long	0x25
	.byte	0x3
	.long	0x72b
	.uleb128 0x22
	.string	"hh"
	.byte	0xb
	.byte	0x46
	.long	0x72b
	.byte	0
	.uleb128 0x6
	.byte	0x4
	.long	0x731
	.uleb128 0x21
	.long	0x45e
	.uleb128 0x1f
	.long	.LASF115
	.byte	0xb
	.byte	0x49
	.long	0x25
	.byte	0x3
	.long	0x751
	.uleb128 0x22
	.string	"hh"
	.byte	0xb
	.byte	0x49
	.long	0x72b
	.byte	0
	.uleb128 0x1f
	.long	.LASF116
	.byte	0x2
	.byte	0x4f
	.long	0x78a
	.byte	0x3
	.long	0x78a
	.uleb128 0x22
	.string	"p"
	.byte	0x2
	.byte	0x4f
	.long	0x78f
	.uleb128 0x23
	.string	"pin"
	.byte	0x2
	.byte	0x4f
	.long	0x794
	.uleb128 0x23
	.string	"u"
	.byte	0x2
	.byte	0x4f
	.long	0x79f
	.uleb128 0x24
	.long	.LASF117
	.byte	0x2
	.byte	0x4f
	.long	0x329
	.byte	0
	.uleb128 0x21
	.long	0x329
	.uleb128 0x21
	.long	0x3c6
	.uleb128 0x6
	.byte	0x4
	.long	0x79a
	.uleb128 0x21
	.long	0x231
	.uleb128 0x6
	.byte	0x4
	.long	0x7a5
	.uleb128 0x21
	.long	0x3a3
	.uleb128 0x1f
	.long	.LASF118
	.byte	0x2
	.byte	0x3a
	.long	0x7e3
	.byte	0x3
	.long	0x7e3
	.uleb128 0x22
	.string	"p"
	.byte	0x2
	.byte	0x3a
	.long	0x3e9
	.uleb128 0x20
	.long	.LASF119
	.byte	0x2
	.byte	0x3a
	.long	0x7e3
	.uleb128 0x23
	.string	"pin"
	.byte	0x2
	.byte	0x3a
	.long	0x666
	.uleb128 0x23
	.string	"u"
	.byte	0x2
	.byte	0x3a
	.long	0x7e8
	.byte	0
	.uleb128 0x21
	.long	0x34b
	.uleb128 0x6
	.byte	0x4
	.long	0x3a3
	.uleb128 0x25
	.string	"foo"
	.byte	0x1
	.byte	0x1c
	.long	.LFB431
	.long	.LFE431-.LFB431
	.uleb128 0x1
	.byte	0x9c
	.long	0x8b0
	.uleb128 0x26
	.long	0x751
	.long	.LBB30
	.long	.LBE30-.LBB30
	.byte	0x1
	.byte	0x1f
	.long	0x86a
	.uleb128 0x27
	.long	0x761
	.uleb128 0x28
	.long	.LBB31
	.long	.LBE31-.LBB31
	.uleb128 0x29
	.long	0x76a
	.uleb128 0x29
	.long	0x775
	.uleb128 0x2a
	.long	0x77e
	.uleb128 0x2
	.byte	0x91
	.sleb128 -9
	.uleb128 0x26
	.long	0x6ef
	.long	.LBB32
	.long	.LBE32-.LBB32
	.byte	0x2
	.byte	0x4f
	.long	0x84f
	.uleb128 0x27
	.long	0x6ff
	.byte	0
	.uleb128 0x2b
	.long	0x6ef
	.long	.LBB34
	.long	.LBE34-.LBB34
	.byte	0x2
	.byte	0x4f
	.uleb128 0x2c
	.long	0x6ff
	.long	.LLST0
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x2b
	.long	0x7aa
	.long	.LBB36
	.long	.LBE36-.LBB36
	.byte	0x1
	.byte	0x22
	.uleb128 0x2d
	.long	0x7c3
	.value	0x1267
	.uleb128 0x27
	.long	0x7ba
	.uleb128 0x28
	.long	.LBB37
	.long	.LBE37-.LBB37
	.uleb128 0x29
	.long	0x7ce
	.uleb128 0x29
	.long	0x7d9
	.uleb128 0x2b
	.long	0x6ef
	.long	.LBB38
	.long	.LBE38-.LBB38
	.byte	0x2
	.byte	0x3a
	.uleb128 0x27
	.long	0x6ff
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x2e
	.long	.LASF120
	.byte	0xa
	.byte	0xb3
	.long	0x25
	.uleb128 0x2e
	.long	.LASF121
	.byte	0x1
	.byte	0x1a
	.long	0xcf
	.uleb128 0x2e
	.long	.LASF122
	.byte	0xb
	.byte	0x6a
	.long	0x22b
	.uleb128 0x2f
	.long	.LASF123
	.byte	0x9
	.value	0x111
	.long	0x8dd
	.uleb128 0x6
	.byte	0x4
	.long	0x65a
	.uleb128 0x30
	.string	"ip"
	.byte	0x1
	.byte	0x17
	.long	0x8ed
	.uleb128 0x6
	.byte	0x4
	.long	0x66c
	.uleb128 0x30
	.string	"b"
	.byte	0x1
	.byte	0x18
	.long	0x329
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x10
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0xd
	.uleb128 0xb
	.uleb128 0xc
	.uleb128 0xb
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x17
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x4
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xd
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x35
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x17
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1b
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1c
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0x5
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1d
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x1e
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1f
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x20
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x20
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x21
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x22
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x23
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x24
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x25
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2117
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x26
	.uleb128 0x1d
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x58
	.uleb128 0xb
	.uleb128 0x59
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x27
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x28
	.uleb128 0xb
	.byte	0x1
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x29
	.uleb128 0x34
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x2a
	.uleb128 0x34
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x2b
	.uleb128 0x1d
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x58
	.uleb128 0xb
	.uleb128 0x59
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x2c
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x2d
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x1c
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x2e
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x2f
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x30
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_loc,"",@progbits
.Ldebug_loc0:
.LLST0:
	.long	.LVL1-.Ltext0
	.long	.LVL4-.Ltext0
	.value	0x2
	.byte	0x71
	.sleb128 24
	.long	0
	.long	0
	.section	.debug_aranges,"",@progbits
	.long	0x1c
	.value	0x2
	.long	.Ldebug_info0
	.byte	0x4
	.byte	0
	.value	0
	.value	0
	.long	.Ltext0
	.long	.Letext0-.Ltext0
	.long	0
	.long	0
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF26:
	.string	"_align"
.LASF44:
	.string	"_signal"
.LASF109:
	.string	"fsig"
.LASF74:
	.string	"_name_ptr"
.LASF48:
	.string	"HAL_TYPE_UNSPECIFIED"
.LASF35:
	.string	"flags"
.LASF0:
	.string	"unsigned int"
.LASF27:
	.string	"next"
.LASF83:
	.string	"version"
.LASF101:
	.string	"heap"
.LASF76:
	.string	"_legacy"
.LASF40:
	.string	"shmoff_t"
.LASF81:
	.string	"prev"
.LASF100:
	.string	"hal_malloced"
.LASF66:
	.string	"bit_pin_ptr"
.LASF105:
	.string	"blabla"
.LASF36:
	.string	"requested"
.LASF72:
	.string	"list"
.LASF32:
	.string	"free_p"
.LASF95:
	.string	"rings"
.LASF14:
	.string	"uint32_t"
.LASF30:
	.string	"rtapi_heap"
.LASF34:
	.string	"mutex"
.LASF43:
	.string	"data_ptr"
.LASF31:
	.string	"base"
.LASF29:
	.string	"rtapi_malloc_hdr_t"
.LASF108:
	.string	"fout"
.LASF21:
	.string	"rtapi_malloc_align"
.LASF13:
	.string	"long long unsigned int"
.LASF93:
	.string	"dead_beef"
.LASF98:
	.string	"str_freed"
.LASF24:
	.string	"attr"
.LASF54:
	.string	"HAL_DIR_UNSPECIFIED"
.LASF56:
	.string	"HAL_OUT"
.LASF102:
	.string	"arena"
.LASF119:
	.string	"value"
.LASF63:
	.string	"hal_s32_t"
.LASF5:
	.string	"size_t"
.LASF50:
	.string	"HAL_FLOAT"
.LASF114:
	.string	"offset"
.LASF60:
	.string	"_Bool"
.LASF49:
	.string	"HAL_BIT"
.LASF77:
	.string	"_object_type"
.LASF15:
	.string	"rtapi_atomic_type"
.LASF96:
	.string	"epsilon"
.LASF99:
	.string	"rt_alignment_loss"
.LASF52:
	.string	"HAL_U32"
.LASF18:
	.string	"char"
.LASF45:
	.string	"dummysig"
.LASF82:
	.string	"hal_list_t"
.LASF126:
	.string	"/home/mah/machinekit/src"
.LASF64:
	.string	"real_t"
.LASF123:
	.string	"hal_data"
.LASF103:
	.string	"hal_data_t"
.LASF65:
	.string	"hal_data_u"
.LASF12:
	.string	"__u32"
.LASF121:
	.string	"hal_shmem_base"
.LASF86:
	.string	"halobjects"
.LASF111:
	.string	"floatpin"
.LASF67:
	.string	"s32_pin_ptr"
.LASF22:
	.string	"rtapi_malloc_tag"
.LASF2:
	.string	"long long int"
.LASF112:
	.string	"hal_ptr"
.LASF92:
	.string	"lock"
.LASF118:
	.string	"set_s32_pin"
.LASF59:
	.string	"hal_bool"
.LASF8:
	.string	"__s16"
.LASF69:
	.string	"s32_sig_ptr"
.LASF23:
	.string	"size"
.LASF47:
	.string	"eps_index"
.LASF61:
	.string	"hal_bit_t"
.LASF51:
	.string	"HAL_S32"
.LASF62:
	.string	"hal_u32_t"
.LASF120:
	.string	"_halerrno"
.LASF115:
	.string	"hh_get_wmb"
.LASF3:
	.string	"long double"
.LASF107:
	.string	"sout"
.LASF113:
	.string	"hh_get_rmb"
.LASF78:
	.string	"_valid"
.LASF11:
	.string	"__s32"
.LASF9:
	.string	"short int"
.LASF75:
	.string	"_refcnt"
.LASF1:
	.string	"long int"
.LASF37:
	.string	"allocated"
.LASF33:
	.string	"arena_size"
.LASF116:
	.string	"get_bit_pin"
.LASF73:
	.string	"_owner_id"
.LASF94:
	.string	"default_ringsize"
.LASF39:
	.string	"name"
.LASF41:
	.string	"hal_pin"
.LASF17:
	.string	"sizetype"
.LASF16:
	.string	"long unsigned int"
.LASF6:
	.string	"__u8"
.LASF127:
	.string	"rtapi_malloc_header"
.LASF87:
	.string	"threads"
.LASF68:
	.string	"float_pin_ptr"
.LASF80:
	.string	"_wmb"
.LASF46:
	.string	"type"
.LASF7:
	.string	"unsigned char"
.LASF79:
	.string	"_rmb"
.LASF71:
	.string	"halhdr"
.LASF84:
	.string	"shmem_bot"
.LASF42:
	.string	"_data_ptr_addr"
.LASF88:
	.string	"funct_entry_free"
.LASF90:
	.string	"exact_base_period"
.LASF125:
	.string	"hal/accessor/acctest.c"
.LASF55:
	.string	"HAL_IN"
.LASF57:
	.string	"HAL_IO"
.LASF124:
	.string	"GNU C 4.9.2 -mtune=generic -march=i586 -g -O -std=c11"
.LASF4:
	.string	"signed char"
.LASF10:
	.string	"short unsigned int"
.LASF106:
	.string	"bout"
.LASF117:
	.string	"rvalue"
.LASF19:
	.string	"double"
.LASF89:
	.string	"base_period"
.LASF53:
	.string	"hal_type_t"
.LASF70:
	.string	"float_sig_ptr"
.LASF91:
	.string	"threads_running"
.LASF25:
	.string	"rtapi_malloc_tag_t"
.LASF38:
	.string	"freed"
.LASF20:
	.string	"rtapi_malloc_align_t"
.LASF28:
	.string	"align"
.LASF97:
	.string	"str_alloc"
.LASF122:
	.string	"global_heap"
.LASF104:
	.string	"instdata"
.LASF85:
	.string	"shmem_top"
.LASF58:
	.string	"hal_pin_dir_t"
.LASF110:
	.string	"ssig"
	.ident	"GCC: (Debian 4.9.2-10) 4.9.2"
	.section	.note.GNU-stack,"",@progbits
