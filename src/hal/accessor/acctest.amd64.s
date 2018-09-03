	.file	"acctest.c"
# GNU C (Debian 4.9.2-10) version 4.9.2 (x86_64-linux-gnu)
#	compiled by GNU C version 4.9.2, GMP version 6.0.0, MPFR version 3.1.2-p3, MPC version 1.0.2
# GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
# options passed:  -I . -I machinetalk/generated -I machinetalk/nanopb
# -I machinetalk/include -I machinetalk/msginfo
# -I machinetalk/msgcomponents -I machinetalk/webtalk -I libnml/linklist
# -I libnml/cms -I libnml/rcs -I libnml/inifile -I libnml/os_intf
# -I libnml/nml -I libnml/buffer -I libnml/posemath -I rtapi
# -I rtapi_export -I hal/lib -I hal/vtable-example -I hal/userfunct-example
# -I hal/drivers -I hal/cython -I hal/accessor -imultiarch x86_64-linux-gnu
# -D ULAPI hal/accessor/acctest.c -mtune=generic -march=x86-64
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
# -fpeephole -fprefetch-loop-arrays -freg-struct-return
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
# -fvar-tracking-assignments -fverbose-asm -fzero-initialized-in-bss
# -m128bit-long-double -m64 -m80387 -malign-stringops
# -mavx256-split-unaligned-load -mavx256-split-unaligned-store
# -mfancy-math-387 -mfp-ret-in-387 -mfxsr -mglibc -mieee-fp
# -mlong-double-80 -mmmx -mno-sse4 -mpush-args -mred-zone -msse -msse2
# -mtls-direct-seg-refs

	.text
.Ltext0:
	.globl	foo
	.type	foo, @function
foo:
.LFB463:
	.file 1 "hal/accessor/acctest.c"
	.loc 1 29 0
	.cfi_startproc
.LVL0:
.LBB30:
.LBB31:
.LBB32:
.LBB33:
	.file 2 "hal/lib/hal_accessor.h"
	.loc 2 18 0
	movq	hal_shmem_base(%rip), %rcx	# hal_shmem_base, D.5716
	movq	ip(%rip), %rax	# ip, ip
	movslq	100(%rax), %rax	# MEM[(struct instdata *)_2 + 100B], D.5717
	addq	%rcx, %rax	# D.5716, D.5718
.LVL1:
.LBE33:
.LBE32:
.LBB34:
.LBB35:
	movslq	24(%rax), %rdx	# MEM[(const struct hal_pin *)_14].data_ptr, D.5717
	addq	%rcx, %rdx	# D.5716, D.5718
.LVL2:
.LBE35:
.LBE34:
	.loc 2 79 0
	testb	$64, 17(%rax)	#, MEM[(const struct halhdr *)_14]
	je	.L2	#,
	mfence
.L2:
	movzbl	(%rdx), %eax	#* D.5718, D.5722
.LVL3:
	movb	%al, -1(%rsp)	# D.5722, rvalue
	movzbl	-1(%rsp), %eax	# rvalue, D.5724
.LBE31:
.LBE30:
	.loc 1 31 0
	movb	%al, b(%rip)	# D.5724, b
.LVL4:
.LBB36:
.LBB37:
.LBB38:
.LBB39:
	.loc 2 18 0
	movq	hal_shmem_base(%rip), %rdx	# hal_shmem_base, D.5716
	movq	ip(%rip), %rax	# ip, ip
	movslq	108(%rax), %rax	# MEM[(struct instdata *)_6 + 108B], D.5717
	addq	%rdx, %rax	# D.5716, D.5718
.LVL5:
.LBE39:
.LBE38:
.LBB40:
.LBB41:
	movslq	24(%rax), %rcx	# MEM[(struct hal_pin *)_27].data_ptr, D.5717
.LVL6:
.LBE41:
.LBE40:
	.loc 2 58 0
	movl	$4711, (%rdx,%rcx)	#,,* D.5717
.LVL7:
	mfence
.LVL8:
	cmpb	$0, 17(%rax)	#, MEM[(const struct halhdr *)_27]
	jns	.L3	#,
	mfence
.L3:
.LBE37:
.LBE36:
	.loc 1 37 0
	movq	ip(%rip), %rax	# ip, ip
.LVL9:
	movq	128(%rax), %rax	# _8->floatpin, D.5715
	movsd	.LC0(%rip), %xmm0	#, tmp133
	movsd	%xmm0, (%rax)	# tmp133, *_9
	ret
	.cfi_endproc
.LFE463:
	.size	foo, .-foo
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC0:
	.long	1374389535
	.long	1074339512
	.text
.Letext0:
	.file 3 "/usr/lib/gcc/x86_64-linux-gnu/4.9/include/stddef.h"
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
	.long	0x966
	.value	0x4
	.long	.Ldebug_abbrev0
	.byte	0x8
	.uleb128 0x1
	.long	.LASF124
	.byte	0x1
	.long	.LASF125
	.long	.LASF126
	.quad	.Ltext0
	.quad	.Letext0-.Ltext0
	.long	.Ldebug_line0
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.long	.LASF0
	.uleb128 0x3
	.long	.LASF5
	.byte	0x3
	.byte	0xd4
	.long	0x3f
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.long	.LASF1
	.uleb128 0x4
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.long	.LASF2
	.uleb128 0x2
	.byte	0x10
	.byte	0x4
	.long	.LASF3
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.long	.LASF4
	.uleb128 0x3
	.long	.LASF6
	.byte	0x4
	.byte	0x14
	.long	0x6d
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.long	.LASF7
	.uleb128 0x3
	.long	.LASF8
	.byte	0x4
	.byte	0x16
	.long	0x7f
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.long	.LASF9
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.long	.LASF10
	.uleb128 0x3
	.long	.LASF11
	.byte	0x4
	.byte	0x19
	.long	0x46
	.uleb128 0x3
	.long	.LASF12
	.byte	0x4
	.byte	0x1a
	.long	0xa3
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.long	.LASF13
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.long	.LASF14
	.uleb128 0x3
	.long	.LASF15
	.byte	0x5
	.byte	0x33
	.long	0xa3
	.uleb128 0x3
	.long	.LASF16
	.byte	0x6
	.byte	0x30
	.long	0x3f
	.uleb128 0x5
	.byte	0x8
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.long	.LASF17
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.long	.LASF18
	.uleb128 0x6
	.byte	0x8
	.long	0xd0
	.uleb128 0x7
	.long	.LASF21
	.byte	0x8
	.byte	0x7
	.byte	0x1c
	.long	0xf6
	.uleb128 0x8
	.long	.LASF26
	.byte	0x7
	.byte	0x1d
	.long	0xf6
	.byte	0
	.byte	0
	.uleb128 0x9
	.long	0x106
	.long	0x106
	.uleb128 0xa
	.long	0xc9
	.byte	0
	.byte	0
	.uleb128 0x2
	.byte	0x8
	.byte	0x4
	.long	.LASF19
	.uleb128 0x3
	.long	.LASF20
	.byte	0x7
	.byte	0x1e
	.long	0xdd
	.uleb128 0x7
	.long	.LASF22
	.byte	0x4
	.byte	0x7
	.byte	0x24
	.long	0x143
	.uleb128 0xb
	.long	.LASF23
	.byte	0x7
	.byte	0x25
	.long	0xb1
	.byte	0x4
	.byte	0x18
	.byte	0x8
	.byte	0
	.uleb128 0xb
	.long	.LASF24
	.byte	0x7
	.byte	0x26
	.long	0xb1
	.byte	0x4
	.byte	0x8
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x3
	.long	.LASF25
	.byte	0x7
	.byte	0x27
	.long	0x118
	.uleb128 0xc
	.string	"hdr"
	.byte	0x8
	.byte	0x7
	.byte	0x2a
	.long	0x173
	.uleb128 0x8
	.long	.LASF27
	.byte	0x7
	.byte	0x2b
	.long	0xb1
	.byte	0
	.uleb128 0xd
	.string	"tag"
	.byte	0x7
	.byte	0x2c
	.long	0x143
	.byte	0x4
	.byte	0
	.uleb128 0xe
	.long	.LASF127
	.byte	0x8
	.byte	0x7
	.byte	0x29
	.long	0x194
	.uleb128 0xf
	.string	"s"
	.byte	0x7
	.byte	0x2d
	.long	0x14e
	.uleb128 0x10
	.long	.LASF28
	.byte	0x7
	.byte	0x2e
	.long	0x10d
	.byte	0
	.uleb128 0x3
	.long	.LASF29
	.byte	0x7
	.byte	0x31
	.long	0x173
	.uleb128 0x7
	.long	.LASF30
	.byte	0x50
	.byte	0x7
	.byte	0x33
	.long	0x218
	.uleb128 0x8
	.long	.LASF31
	.byte	0x7
	.byte	0x34
	.long	0x194
	.byte	0
	.uleb128 0x8
	.long	.LASF32
	.byte	0x7
	.byte	0x35
	.long	0x34
	.byte	0x8
	.uleb128 0x8
	.long	.LASF33
	.byte	0x7
	.byte	0x36
	.long	0x34
	.byte	0x10
	.uleb128 0x8
	.long	.LASF34
	.byte	0x7
	.byte	0x37
	.long	0xbc
	.byte	0x18
	.uleb128 0x8
	.long	.LASF35
	.byte	0x7
	.byte	0x38
	.long	0x46
	.byte	0x20
	.uleb128 0x8
	.long	.LASF36
	.byte	0x7
	.byte	0x39
	.long	0x34
	.byte	0x28
	.uleb128 0x8
	.long	.LASF37
	.byte	0x7
	.byte	0x3a
	.long	0x34
	.byte	0x30
	.uleb128 0x8
	.long	.LASF38
	.byte	0x7
	.byte	0x3b
	.long	0x46
	.byte	0x38
	.uleb128 0x8
	.long	.LASF39
	.byte	0x7
	.byte	0x3c
	.long	0x218
	.byte	0x3c
	.byte	0
	.uleb128 0x9
	.long	0xd0
	.long	0x228
	.uleb128 0xa
	.long	0xc9
	.byte	0xf
	.byte	0
	.uleb128 0x3
	.long	.LASF40
	.byte	0x8
	.byte	0x97
	.long	0x8d
	.uleb128 0x6
	.byte	0x8
	.long	0x19f
	.uleb128 0x11
	.long	.LASF41
	.byte	0x38
	.byte	0x9
	.value	0x144
	.long	0x2bc
	.uleb128 0x12
	.string	"hdr"
	.byte	0x9
	.value	0x145
	.long	0x466
	.byte	0
	.uleb128 0x13
	.long	.LASF42
	.byte	0x9
	.value	0x146
	.long	0x46
	.byte	0x14
	.uleb128 0x13
	.long	.LASF43
	.byte	0x9
	.value	0x147
	.long	0x46
	.byte	0x18
	.uleb128 0x13
	.long	.LASF44
	.byte	0x9
	.value	0x148
	.long	0x46
	.byte	0x1c
	.uleb128 0x13
	.long	.LASF45
	.byte	0x9
	.value	0x149
	.long	0x3ab
	.byte	0x20
	.uleb128 0x13
	.long	.LASF46
	.byte	0x9
	.value	0x14a
	.long	0x2e4
	.byte	0x28
	.uleb128 0x12
	.string	"dir"
	.byte	0x9
	.value	0x14b
	.long	0x312
	.byte	0x2c
	.uleb128 0x13
	.long	.LASF35
	.byte	0x9
	.value	0x14c
	.long	0x46
	.byte	0x30
	.uleb128 0x13
	.long	.LASF47
	.byte	0x9
	.value	0x14d
	.long	0x62
	.byte	0x34
	.byte	0
	.uleb128 0x14
	.byte	0x4
	.byte	0xa
	.value	0x1b8
	.long	0x2e4
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
	.long	0x2bc
	.uleb128 0x14
	.byte	0x4
	.byte	0xa
	.value	0x1c8
	.long	0x312
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
	.long	0x2f0
	.uleb128 0x16
	.long	.LASF59
	.byte	0xa
	.value	0x1e1
	.long	0x32a
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.long	.LASF60
	.uleb128 0x16
	.long	.LASF61
	.byte	0xa
	.value	0x1e3
	.long	0x33d
	.uleb128 0x17
	.long	0x31e
	.uleb128 0x16
	.long	.LASF62
	.byte	0xa
	.value	0x1e4
	.long	0x34e
	.uleb128 0x17
	.long	0x98
	.uleb128 0x16
	.long	.LASF63
	.byte	0xa
	.value	0x1e5
	.long	0x35f
	.uleb128 0x17
	.long	0x8d
	.uleb128 0x16
	.long	.LASF64
	.byte	0xa
	.value	0x1e6
	.long	0x106
	.uleb128 0x18
	.byte	0x8
	.byte	0xa
	.value	0x1ee
	.long	0x3a6
	.uleb128 0x19
	.string	"_b"
	.byte	0xa
	.value	0x1ef
	.long	0x331
	.uleb128 0x19
	.string	"_s"
	.byte	0xa
	.value	0x1f0
	.long	0x353
	.uleb128 0x19
	.string	"_u"
	.byte	0xa
	.value	0x1f1
	.long	0x342
	.uleb128 0x19
	.string	"_f"
	.byte	0xa
	.value	0x1f2
	.long	0x3a6
	.byte	0
	.uleb128 0x17
	.long	0x364
	.uleb128 0x16
	.long	.LASF65
	.byte	0xa
	.value	0x1f3
	.long	0x370
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x227
	.long	0x3ce
	.uleb128 0x12
	.string	"_bp"
	.byte	0xa
	.value	0x227
	.long	0x228
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF66
	.byte	0xa
	.value	0x227
	.long	0x3b7
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x228
	.long	0x3f1
	.uleb128 0x12
	.string	"_sp"
	.byte	0xa
	.value	0x228
	.long	0x228
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF67
	.byte	0xa
	.value	0x228
	.long	0x3da
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x22a
	.long	0x414
	.uleb128 0x12
	.string	"_fp"
	.byte	0xa
	.value	0x22a
	.long	0x228
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF68
	.byte	0xa
	.value	0x22a
	.long	0x3fd
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x22e
	.long	0x437
	.uleb128 0x12
	.string	"_ss"
	.byte	0xa
	.value	0x22e
	.long	0x228
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF69
	.byte	0xa
	.value	0x22e
	.long	0x420
	.uleb128 0x1a
	.byte	0x4
	.byte	0xa
	.value	0x230
	.long	0x45a
	.uleb128 0x12
	.string	"_fs"
	.byte	0xa
	.value	0x230
	.long	0x228
	.byte	0
	.byte	0
	.uleb128 0x16
	.long	.LASF70
	.byte	0xa
	.value	0x230
	.long	0x443
	.uleb128 0x7
	.long	.LASF71
	.byte	0x14
	.byte	0xb
	.byte	0x21
	.long	0x4fd
	.uleb128 0x8
	.long	.LASF72
	.byte	0xb
	.byte	0x22
	.long	0x51e
	.byte	0
	.uleb128 0xd
	.string	"_id"
	.byte	0xb
	.byte	0x23
	.long	0x74
	.byte	0x8
	.uleb128 0x8
	.long	.LASF73
	.byte	0xb
	.byte	0x24
	.long	0x74
	.byte	0xa
	.uleb128 0x8
	.long	.LASF74
	.byte	0xb
	.byte	0x25
	.long	0x98
	.byte	0xc
	.uleb128 0xb
	.long	.LASF75
	.byte	0xb
	.byte	0x26
	.long	0x8d
	.byte	0x4
	.byte	0x7
	.byte	0x19
	.byte	0x10
	.uleb128 0xb
	.long	.LASF76
	.byte	0xb
	.byte	0x27
	.long	0x98
	.byte	0x4
	.byte	0x1
	.byte	0x18
	.byte	0x10
	.uleb128 0xb
	.long	.LASF77
	.byte	0xb
	.byte	0x2b
	.long	0x98
	.byte	0x4
	.byte	0x5
	.byte	0x13
	.byte	0x10
	.uleb128 0xb
	.long	.LASF78
	.byte	0xb
	.byte	0x2c
	.long	0x98
	.byte	0x4
	.byte	0x1
	.byte	0x12
	.byte	0x10
	.uleb128 0xb
	.long	.LASF79
	.byte	0xb
	.byte	0x32
	.long	0x98
	.byte	0x4
	.byte	0x1
	.byte	0x11
	.byte	0x10
	.uleb128 0xb
	.long	.LASF80
	.byte	0xb
	.byte	0x34
	.long	0x98
	.byte	0x4
	.byte	0x1
	.byte	0x10
	.byte	0x10
	.byte	0
	.uleb128 0x1b
	.byte	0x8
	.byte	0xc
	.byte	0x13
	.long	0x51e
	.uleb128 0x8
	.long	.LASF27
	.byte	0xc
	.byte	0x14
	.long	0x228
	.byte	0
	.uleb128 0x8
	.long	.LASF81
	.byte	0xc
	.byte	0x15
	.long	0x228
	.byte	0x4
	.byte	0
	.uleb128 0x3
	.long	.LASF82
	.byte	0xc
	.byte	0x16
	.long	0x4fd
	.uleb128 0x1c
	.value	0x140
	.byte	0x9
	.byte	0xdf
	.long	0x633
	.uleb128 0x8
	.long	.LASF83
	.byte	0x9
	.byte	0xe0
	.long	0x46
	.byte	0
	.uleb128 0x8
	.long	.LASF34
	.byte	0x9
	.byte	0xe1
	.long	0x3f
	.byte	0x8
	.uleb128 0x8
	.long	.LASF84
	.byte	0x9
	.byte	0xe2
	.long	0x46
	.byte	0x10
	.uleb128 0x8
	.long	.LASF85
	.byte	0x9
	.byte	0xe3
	.long	0x46
	.byte	0x14
	.uleb128 0x8
	.long	.LASF86
	.byte	0x9
	.byte	0xe5
	.long	0x51e
	.byte	0x18
	.uleb128 0x8
	.long	.LASF87
	.byte	0x9
	.byte	0xe6
	.long	0x51e
	.byte	0x20
	.uleb128 0x8
	.long	.LASF88
	.byte	0x9
	.byte	0xe7
	.long	0x51e
	.byte	0x28
	.uleb128 0x8
	.long	.LASF89
	.byte	0x9
	.byte	0xe9
	.long	0x2d
	.byte	0x30
	.uleb128 0x8
	.long	.LASF90
	.byte	0x9
	.byte	0xea
	.long	0x46
	.byte	0x38
	.uleb128 0x8
	.long	.LASF91
	.byte	0x9
	.byte	0xed
	.long	0x46
	.byte	0x3c
	.uleb128 0x8
	.long	.LASF92
	.byte	0x9
	.byte	0xef
	.long	0x6d
	.byte	0x40
	.uleb128 0x8
	.long	.LASF93
	.byte	0x9
	.byte	0xf1
	.long	0xaa
	.byte	0x48
	.uleb128 0x8
	.long	.LASF94
	.byte	0x9
	.byte	0xf2
	.long	0x34
	.byte	0x50
	.uleb128 0x8
	.long	.LASF95
	.byte	0x9
	.byte	0xf6
	.long	0x633
	.byte	0x58
	.uleb128 0x8
	.long	.LASF96
	.byte	0x9
	.byte	0xf8
	.long	0x643
	.byte	0x78
	.uleb128 0x8
	.long	.LASF97
	.byte	0x9
	.byte	0xfb
	.long	0x34
	.byte	0xa0
	.uleb128 0x8
	.long	.LASF98
	.byte	0x9
	.byte	0xfc
	.long	0x34
	.byte	0xa8
	.uleb128 0x8
	.long	.LASF99
	.byte	0x9
	.byte	0xff
	.long	0x34
	.byte	0xb0
	.uleb128 0x13
	.long	.LASF100
	.byte	0x9
	.value	0x100
	.long	0x34
	.byte	0xb8
	.uleb128 0x13
	.long	.LASF101
	.byte	0x9
	.value	0x104
	.long	0x19f
	.byte	0xc0
	.uleb128 0x1d
	.long	.LASF102
	.byte	0x9
	.value	0x105
	.long	0x653
	.value	0x140
	.byte	0
	.uleb128 0x9
	.long	0xbc
	.long	0x643
	.uleb128 0xa
	.long	0xc9
	.byte	0x3
	.byte	0
	.uleb128 0x9
	.long	0x106
	.long	0x653
	.uleb128 0xa
	.long	0xc9
	.byte	0x4
	.byte	0
	.uleb128 0x9
	.long	0x6d
	.long	0x662
	.uleb128 0x1e
	.long	0xc9
	.byte	0
	.uleb128 0x16
	.long	.LASF103
	.byte	0x9
	.value	0x108
	.long	0x529
	.uleb128 0x6
	.byte	0x8
	.long	0x239
	.uleb128 0x7
	.long	.LASF104
	.byte	0x88
	.byte	0x1
	.byte	0x9
	.long	0x6e1
	.uleb128 0x8
	.long	.LASF105
	.byte	0x1
	.byte	0xa
	.long	0x6e1
	.byte	0
	.uleb128 0xd
	.string	"bin"
	.byte	0x1
	.byte	0xb
	.long	0x3ce
	.byte	0x64
	.uleb128 0x8
	.long	.LASF106
	.byte	0x1
	.byte	0xc
	.long	0x3ce
	.byte	0x68
	.uleb128 0x8
	.long	.LASF107
	.byte	0x1
	.byte	0xd
	.long	0x3f1
	.byte	0x6c
	.uleb128 0x8
	.long	.LASF108
	.byte	0x1
	.byte	0xe
	.long	0x414
	.byte	0x70
	.uleb128 0x8
	.long	.LASF109
	.byte	0x1
	.byte	0x10
	.long	0x45a
	.byte	0x74
	.uleb128 0x8
	.long	.LASF110
	.byte	0x1
	.byte	0x11
	.long	0x437
	.byte	0x78
	.uleb128 0x8
	.long	.LASF111
	.byte	0x1
	.byte	0x13
	.long	0x6f1
	.byte	0x80
	.byte	0
	.uleb128 0x9
	.long	0xd0
	.long	0x6f1
	.uleb128 0xa
	.long	0xc9
	.byte	0x63
	.byte	0
	.uleb128 0x6
	.byte	0x8
	.long	0x3a6
	.uleb128 0x1f
	.long	.LASF112
	.byte	0x2
	.byte	0x11
	.long	0xc7
	.byte	0x3
	.long	0x713
	.uleb128 0x20
	.long	.LASF114
	.byte	0x2
	.byte	0x11
	.long	0x713
	.byte	0
	.uleb128 0x21
	.long	0x228
	.uleb128 0x1f
	.long	.LASF113
	.byte	0xb
	.byte	0x46
	.long	0x46
	.byte	0x3
	.long	0x733
	.uleb128 0x22
	.string	"hh"
	.byte	0xb
	.byte	0x46
	.long	0x733
	.byte	0
	.uleb128 0x6
	.byte	0x8
	.long	0x739
	.uleb128 0x21
	.long	0x466
	.uleb128 0x1f
	.long	.LASF115
	.byte	0xb
	.byte	0x49
	.long	0x46
	.byte	0x3
	.long	0x759
	.uleb128 0x22
	.string	"hh"
	.byte	0xb
	.byte	0x49
	.long	0x733
	.byte	0
	.uleb128 0x1f
	.long	.LASF116
	.byte	0x2
	.byte	0x4f
	.long	0x792
	.byte	0x3
	.long	0x792
	.uleb128 0x22
	.string	"p"
	.byte	0x2
	.byte	0x4f
	.long	0x797
	.uleb128 0x23
	.string	"pin"
	.byte	0x2
	.byte	0x4f
	.long	0x79c
	.uleb128 0x23
	.string	"u"
	.byte	0x2
	.byte	0x4f
	.long	0x7a7
	.uleb128 0x24
	.long	.LASF117
	.byte	0x2
	.byte	0x4f
	.long	0x331
	.byte	0
	.uleb128 0x21
	.long	0x331
	.uleb128 0x21
	.long	0x3ce
	.uleb128 0x6
	.byte	0x8
	.long	0x7a2
	.uleb128 0x21
	.long	0x239
	.uleb128 0x6
	.byte	0x8
	.long	0x7ad
	.uleb128 0x21
	.long	0x3ab
	.uleb128 0x1f
	.long	.LASF118
	.byte	0x2
	.byte	0x3a
	.long	0x7eb
	.byte	0x3
	.long	0x7eb
	.uleb128 0x22
	.string	"p"
	.byte	0x2
	.byte	0x3a
	.long	0x3f1
	.uleb128 0x20
	.long	.LASF119
	.byte	0x2
	.byte	0x3a
	.long	0x7eb
	.uleb128 0x23
	.string	"pin"
	.byte	0x2
	.byte	0x3a
	.long	0x66e
	.uleb128 0x23
	.string	"u"
	.byte	0x2
	.byte	0x3a
	.long	0x7f0
	.byte	0
	.uleb128 0x21
	.long	0x353
	.uleb128 0x6
	.byte	0x8
	.long	0x3ab
	.uleb128 0x25
	.string	"foo"
	.byte	0x1
	.byte	0x1c
	.quad	.LFB463
	.quad	.LFE463-.LFB463
	.uleb128 0x1
	.byte	0x9c
	.long	0x91d
	.uleb128 0x26
	.long	0x759
	.quad	.LBB30
	.quad	.LBE30-.LBB30
	.byte	0x1
	.byte	0x1f
	.long	0x89a
	.uleb128 0x27
	.long	0x769
	.uleb128 0x28
	.quad	.LBB31
	.quad	.LBE31-.LBB31
	.uleb128 0x29
	.long	0x772
	.uleb128 0x29
	.long	0x77d
	.uleb128 0x2a
	.long	0x786
	.uleb128 0x2
	.byte	0x91
	.sleb128 -9
	.uleb128 0x26
	.long	0x6f7
	.quad	.LBB32
	.quad	.LBE32-.LBB32
	.byte	0x2
	.byte	0x4f
	.long	0x877
	.uleb128 0x27
	.long	0x707
	.byte	0
	.uleb128 0x2b
	.long	0x6f7
	.quad	.LBB34
	.quad	.LBE34-.LBB34
	.byte	0x2
	.byte	0x4f
	.uleb128 0x2c
	.long	0x707
	.long	.LLST0
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x2b
	.long	0x7b2
	.quad	.LBB36
	.quad	.LBE36-.LBB36
	.byte	0x1
	.byte	0x22
	.uleb128 0x2d
	.long	0x7cb
	.value	0x1267
	.uleb128 0x27
	.long	0x7c2
	.uleb128 0x28
	.quad	.LBB37
	.quad	.LBE37-.LBB37
	.uleb128 0x29
	.long	0x7d6
	.uleb128 0x29
	.long	0x7e1
	.uleb128 0x26
	.long	0x6f7
	.quad	.LBB38
	.quad	.LBE38-.LBB38
	.byte	0x2
	.byte	0x3a
	.long	0x8f9
	.uleb128 0x27
	.long	0x707
	.byte	0
	.uleb128 0x2b
	.long	0x6f7
	.quad	.LBB40
	.quad	.LBE40-.LBB40
	.byte	0x2
	.byte	0x3a
	.uleb128 0x2c
	.long	0x707
	.long	.LLST1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x2e
	.long	.LASF120
	.byte	0xa
	.byte	0xb3
	.long	0x46
	.uleb128 0x2e
	.long	.LASF121
	.byte	0x1
	.byte	0x1a
	.long	0xd7
	.uleb128 0x2e
	.long	.LASF122
	.byte	0xb
	.byte	0x6a
	.long	0x233
	.uleb128 0x2f
	.long	.LASF123
	.byte	0x9
	.value	0x111
	.long	0x94a
	.uleb128 0x6
	.byte	0x8
	.long	0x662
	.uleb128 0x30
	.string	"ip"
	.byte	0x1
	.byte	0x17
	.long	0x95a
	.uleb128 0x6
	.byte	0x8
	.long	0x674
	.uleb128 0x30
	.string	"b"
	.byte	0x1
	.byte	0x18
	.long	0x331
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
	.uleb128 0x7
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
	.uleb128 0xe
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
	.uleb128 0x8
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
	.uleb128 0x7
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
	.uleb128 0x7
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
	.uleb128 0x7
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
	.uleb128 0x7
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
	.quad	.LVL1-.Ltext0
	.quad	.LVL3-.Ltext0
	.value	0x2
	.byte	0x70
	.sleb128 24
	.quad	0
	.quad	0
.LLST1:
	.quad	.LVL5-.Ltext0
	.quad	.LVL7-.Ltext0
	.value	0x2
	.byte	0x70
	.sleb128 24
	.quad	.LVL7-.Ltext0
	.quad	.LFE463-.Ltext0
	.value	0x1
	.byte	0x52
	.quad	0
	.quad	0
	.section	.debug_aranges,"",@progbits
	.long	0x2c
	.value	0x2
	.long	.Ldebug_info0
	.byte	0x8
	.byte	0
	.value	0
	.value	0
	.quad	.Ltext0
	.quad	.Letext0-.Ltext0
	.quad	0
	.quad	0
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF26:
	.string	"_align"
.LASF44:
	.string	"_signal"
.LASF109:
	.string	"fsig"
.LASF124:
	.string	"GNU C 4.9.2 -mtune=generic -march=x86-64 -g -O -std=c11"
.LASF74:
	.string	"_name_ptr"
.LASF48:
	.string	"HAL_TYPE_UNSPECIFIED"
.LASF35:
	.string	"flags"
.LASF13:
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
.LASF15:
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
.LASF14:
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
.LASF16:
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
.LASF0:
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
.LASF1:
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
