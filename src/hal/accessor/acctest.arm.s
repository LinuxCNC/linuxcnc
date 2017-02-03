	.syntax unified
	.arch armv7-a
	.eabi_attribute 27, 3	@ Tag_ABI_HardFP_use
	.eabi_attribute 28, 1	@ Tag_ABI_VFP_args
	.fpu vfpv3-d16
	.eabi_attribute 20, 1	@ Tag_ABI_FP_denormal
	.eabi_attribute 21, 1	@ Tag_ABI_FP_exceptions
	.eabi_attribute 23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute 24, 1	@ Tag_ABI_align8_needed
	.eabi_attribute 25, 1	@ Tag_ABI_align8_preserved
	.eabi_attribute 26, 2	@ Tag_ABI_enum_size
	.eabi_attribute 30, 1	@ Tag_ABI_optimization_goals
	.eabi_attribute 34, 1	@ Tag_CPU_unaligned_access
	.eabi_attribute 18, 4	@ Tag_ABI_PCS_wchar_t
	.file	"acctest.c"
@ GNU C (Debian 4.9.2-10) version 4.9.2 (arm-linux-gnueabihf)
@	compiled by GNU C version 4.9.2, GMP version 6.0.0, MPFR version 3.1.2-p3, MPC version 1.0.2
@ GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
@ options passed:  -I . -I machinetalk/generated -I machinetalk/nanopb
@ -I machinetalk/include -I machinetalk/msginfo
@ -I machinetalk/msgcomponents -I machinetalk/webtalk -I libnml/linklist
@ -I libnml/cms -I libnml/rcs -I libnml/inifile -I libnml/os_intf
@ -I libnml/nml -I libnml/buffer -I libnml/posemath -I rtapi
@ -I rtapi_export -I hal/lib -I hal/vtable-example -I hal/userfunct-example
@ -I hal/drivers -I hal/cython -I hal/accessor -imultilib .
@ -imultiarch arm-linux-gnueabihf -D ULAPI hal/accessor/acctest.c
@ -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16 -mthumb -mtls-dialect=gnu
@ -auxbase-strip /tmp/acctest.s -g -O -std=c11 -fverbose-asm
@ options enabled:  -faggressive-loop-optimizations -fauto-inc-dec
@ -fbranch-count-reg -fcombine-stack-adjustments -fcommon -fcompare-elim
@ -fcprop-registers -fdefer-pop -fdelete-null-pointer-checks
@ -fdwarf2-cfi-asm -fearly-inlining -feliminate-unused-debug-types
@ -fforward-propagate -ffunction-cse -fgcse-lm -fgnu-runtime -fgnu-unique
@ -fguess-branch-probability -fident -fif-conversion -fif-conversion2
@ -finline -finline-atomics -finline-functions-called-once -fipa-profile
@ -fipa-pure-const -fipa-reference -fira-hoist-pressure
@ -fira-share-save-slots -fira-share-spill-slots -fivopts
@ -fkeep-static-consts -fleading-underscore -fmath-errno -fmerge-constants
@ -fmerge-debug-strings -fmove-loop-invariants -fomit-frame-pointer
@ -fpeephole -fprefetch-loop-arrays -freg-struct-return
@ -fsched-critical-path-heuristic -fsched-dep-count-heuristic
@ -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
@ -fsched-pressure -fsched-rank-heuristic -fsched-spec
@ -fsched-spec-insn-heuristic -fsched-stalled-insns-dep -fsection-anchors
@ -fshow-column -fshrink-wrap -fsigned-zeros -fsplit-ivs-in-unroller
@ -fsplit-wide-types -fstrict-volatile-bitfields -fsync-libcalls
@ -ftoplevel-reorder -ftrapping-math -ftree-bit-ccp -ftree-ccp -ftree-ch
@ -ftree-coalesce-vars -ftree-copy-prop -ftree-copyrename -ftree-cselim
@ -ftree-dce -ftree-dominator-opts -ftree-dse -ftree-forwprop -ftree-fre
@ -ftree-loop-if-convert -ftree-loop-im -ftree-loop-ivcanon
@ -ftree-loop-optimize -ftree-parallelize-loops= -ftree-phiprop -ftree-pta
@ -ftree-reassoc -ftree-scev-cprop -ftree-sink -ftree-slsr -ftree-sra
@ -ftree-ter -funit-at-a-time -fvar-tracking -fvar-tracking-assignments
@ -fverbose-asm -fzero-initialized-in-bss -mglibc -mlittle-endian -mlra
@ -mpic-data-is-text-relative -msched-prolog -mthumb -munaligned-access
@ -mvectorize-with-neon-quad

	.text
.Ltext0:
	.cfi_sections	.debug_frame
	.align	2
	.global	foo
	.thumb
	.thumb_func
	.type	foo, %function
foo:
.LFB423:
	.file 1 "hal/accessor/acctest.c"
	.loc 1 29 0
	.cfi_startproc
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
	sub	sp, sp, #8	@,,
	.cfi_def_cfa_offset 8
.LVL0:
.LBB30:
.LBB31:
.LBB32:
.LBB33:
	.file 2 "hal/lib/hal_accessor.h"
	.loc 2 18 0
	movw	r3, #:lower16:hal_shmem_base	@ tmp135,
	movt	r3, #:upper16:hal_shmem_base	@ tmp135,
	ldr	r2, [r3]	@ D.8378, hal_shmem_base
	movw	r3, #:lower16:ip	@ tmp137,
	movt	r3, #:upper16:ip	@ tmp137,
	ldr	r3, [r3]	@ ip, ip
	ldr	r3, [r3, #100]	@ MEM[(struct instdata *)_2 + 100B], MEM[(struct instdata *)_2 + 100B]
	add	r3, r3, r2	@ D.8380, D.8378
.LVL1:
.LBE33:
.LBE32:
.LBB34:
.LBB35:
	ldr	r1, [r3, #24]	@ MEM[(const struct hal_pin *)_14].data_ptr, MEM[(const struct hal_pin *)_14].data_ptr
.LVL2:
.LBE35:
.LBE34:
.LBB36:
.LBB37:
	.file 3 "hal/lib/hal_object.h"
	.loc 3 70 0
	ldrb	r3, [r3, #17]	@ zero_extendqisi2	@ MEM[(const struct halhdr *)_14], MEM[(const struct halhdr *)_14]
.LVL3:
.LBE37:
.LBE36:
	.loc 2 79 0
	tst	r3, #64	@ MEM[(const struct halhdr *)_14],
	beq	.L2	@,
	dmb	sy
.L2:
	dmb	sy
	ldrb	r3, [r2, r1]	@ zero_extendqisi2	@ tmp146,* MEM[(const struct hal_pin *)_14].data_ptr
	dmb	sy
	uxtb	r3, r3	@ D.8384, tmp146
	strb	r3, [sp, #7]	@ D.8384, rvalue
	ldrb	r2, [sp, #7]	@ zero_extendqisi2	@ rvalue, rvalue
.LVL4:
	uxtb	r2, r2	@ D.8386, rvalue
.LBE31:
.LBE30:
	.loc 1 31 0
	movw	r3, #:lower16:b	@ tmp151,
	movt	r3, #:upper16:b	@ tmp151,
	strb	r2, [r3]	@ D.8386, b
.LVL5:
.LBB38:
.LBB39:
.LBB40:
.LBB41:
	.loc 2 18 0
	movw	r3, #:lower16:hal_shmem_base	@ tmp153,
.LVL6:
	movt	r3, #:upper16:hal_shmem_base	@ tmp153,
.LVL7:
	ldr	r2, [r3]	@ D.8378, hal_shmem_base
	movw	r3, #:lower16:ip	@ tmp155,
.LVL8:
	movt	r3, #:upper16:ip	@ tmp155,
.LVL9:
	ldr	r3, [r3]	@ ip, ip
	ldr	r3, [r3, #108]	@ MEM[(struct instdata *)_6 + 108B], MEM[(struct instdata *)_6 + 108B]
	add	r3, r3, r2	@ D.8380, D.8378
.LVL10:
.LBE41:
.LBE40:
	.loc 2 58 0
	ldr	r1, [r3, #24]	@ MEM[(struct hal_pin *)_27].data_ptr, MEM[(struct hal_pin *)_27].data_ptr
.LVL11:
	dmb	sy
	movw	r0, #4711	@ tmp158,
	str	r0, [r2, r1]	@ tmp158,* MEM[(struct hal_pin *)_27].data_ptr
.LVL12:
	dmb	sy
.LVL13:
.LBB42:
.LBB43:
	.loc 3 73 0
	ldrb	r3, [r3, #17]	@ zero_extendqisi2	@ MEM[(const struct halhdr *)_27], MEM[(const struct halhdr *)_27]
.LVL14:
.LBE43:
.LBE42:
	.loc 2 58 0
	lsrs	r3, r3, #7	@ tmp167, MEM[(const struct halhdr *)_27],
	beq	.L3	@,
	dmb	sy
.L3:
.LBE39:
.LBE38:
	.loc 1 37 0
	movw	r3, #:lower16:ip	@ tmp165,
	movt	r3, #:upper16:ip	@ tmp165,
	ldr	r3, [r3]	@ ip, ip
	ldr	r3, [r3, #124]	@ D.8377, _8->floatpin
	adr	r1, .L5	@,
	ldrd	r0, [r1]	@ tmp166,
	strd	r0, [r3]	@ tmp166, *_9
	.loc 1 40 0
	add	sp, sp, #8	@,,
	.cfi_def_cfa_offset 0
	@ sp needed	@
	bx	lr	@
.L6:
	.align	3
.L5:
	.word	1374389535
	.word	1074339512
	.cfi_endproc
.LFE423:
	.size	foo, .-foo
.Letext0:
	.file 4 "/usr/lib/gcc/arm-linux-gnueabihf/4.9/include/stddef.h"
	.file 5 "/usr/include/asm-generic/int-ll64.h"
	.file 6 "/usr/include/stdint.h"
	.file 7 "rtapi/rtapi_bitops.h"
	.file 8 "rtapi/rtapi_heap_private.h"
	.file 9 "rtapi/rtapi.h"
	.file 10 "hal/lib/hal_priv.h"
	.file 11 "hal/lib/hal.h"
	.file 12 "hal/lib/hal_list.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x933
	.2byte	0x4
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF124
	.byte	0x1
	.4byte	.LASF125
	.4byte	.LASF126
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	.Ldebug_line0
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.ascii	"int\000"
	.uleb128 0x3
	.4byte	.LASF4
	.byte	0x4
	.byte	0xd4
	.4byte	0x37
	.uleb128 0x4
	.byte	0x4
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x4
	.byte	0x8
	.byte	0x5
	.4byte	.LASF1
	.uleb128 0x4
	.byte	0x8
	.byte	0x4
	.4byte	.LASF2
	.uleb128 0x4
	.byte	0x1
	.byte	0x6
	.4byte	.LASF3
	.uleb128 0x3
	.4byte	.LASF5
	.byte	0x5
	.byte	0x14
	.4byte	0x5e
	.uleb128 0x4
	.byte	0x1
	.byte	0x8
	.4byte	.LASF6
	.uleb128 0x3
	.4byte	.LASF7
	.byte	0x5
	.byte	0x16
	.4byte	0x70
	.uleb128 0x4
	.byte	0x2
	.byte	0x5
	.4byte	.LASF8
	.uleb128 0x4
	.byte	0x2
	.byte	0x7
	.4byte	.LASF9
	.uleb128 0x3
	.4byte	.LASF10
	.byte	0x5
	.byte	0x19
	.4byte	0x25
	.uleb128 0x3
	.4byte	.LASF11
	.byte	0x5
	.byte	0x1a
	.4byte	0x37
	.uleb128 0x4
	.byte	0x8
	.byte	0x7
	.4byte	.LASF12
	.uleb128 0x3
	.4byte	.LASF13
	.byte	0x6
	.byte	0x33
	.4byte	0x37
	.uleb128 0x3
	.4byte	.LASF14
	.byte	0x7
	.byte	0x30
	.4byte	0xb1
	.uleb128 0x4
	.byte	0x4
	.byte	0x7
	.4byte	.LASF15
	.uleb128 0x5
	.byte	0x4
	.uleb128 0x4
	.byte	0x4
	.byte	0x5
	.4byte	.LASF16
	.uleb128 0x4
	.byte	0x4
	.byte	0x7
	.4byte	.LASF17
	.uleb128 0x4
	.byte	0x1
	.byte	0x8
	.4byte	.LASF18
	.uleb128 0x6
	.4byte	.LASF21
	.byte	0x8
	.byte	0x8
	.byte	0x1c
	.4byte	0xe8
	.uleb128 0x7
	.4byte	.LASF26
	.byte	0x8
	.byte	0x1d
	.4byte	0xe8
	.byte	0
	.byte	0
	.uleb128 0x8
	.4byte	0xf8
	.4byte	0xf8
	.uleb128 0x9
	.4byte	0xc1
	.byte	0
	.byte	0
	.uleb128 0x4
	.byte	0x8
	.byte	0x4
	.4byte	.LASF19
	.uleb128 0x3
	.4byte	.LASF20
	.byte	0x8
	.byte	0x1e
	.4byte	0xcf
	.uleb128 0x6
	.4byte	.LASF22
	.byte	0x4
	.byte	0x8
	.byte	0x24
	.4byte	0x135
	.uleb128 0xa
	.4byte	.LASF23
	.byte	0x8
	.byte	0x25
	.4byte	0x9b
	.byte	0x4
	.byte	0x18
	.byte	0x8
	.byte	0
	.uleb128 0xa
	.4byte	.LASF24
	.byte	0x8
	.byte	0x26
	.4byte	0x9b
	.byte	0x4
	.byte	0x8
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x3
	.4byte	.LASF25
	.byte	0x8
	.byte	0x27
	.4byte	0x10a
	.uleb128 0xb
	.ascii	"hdr\000"
	.byte	0x8
	.byte	0x8
	.byte	0x2a
	.4byte	0x165
	.uleb128 0x7
	.4byte	.LASF27
	.byte	0x8
	.byte	0x2b
	.4byte	0x9b
	.byte	0
	.uleb128 0xc
	.ascii	"tag\000"
	.byte	0x8
	.byte	0x2c
	.4byte	0x135
	.byte	0x4
	.byte	0
	.uleb128 0xd
	.4byte	.LASF127
	.byte	0x8
	.byte	0x8
	.byte	0x29
	.4byte	0x186
	.uleb128 0xe
	.ascii	"s\000"
	.byte	0x8
	.byte	0x2d
	.4byte	0x140
	.uleb128 0xf
	.4byte	.LASF28
	.byte	0x8
	.byte	0x2e
	.4byte	0xff
	.byte	0
	.uleb128 0x3
	.4byte	.LASF29
	.byte	0x8
	.byte	0x31
	.4byte	0x165
	.uleb128 0x6
	.4byte	.LASF30
	.byte	0x38
	.byte	0x8
	.byte	0x33
	.4byte	0x20a
	.uleb128 0x7
	.4byte	.LASF31
	.byte	0x8
	.byte	0x34
	.4byte	0x186
	.byte	0
	.uleb128 0x7
	.4byte	.LASF32
	.byte	0x8
	.byte	0x35
	.4byte	0x2c
	.byte	0x8
	.uleb128 0x7
	.4byte	.LASF33
	.byte	0x8
	.byte	0x36
	.4byte	0x2c
	.byte	0xc
	.uleb128 0x7
	.4byte	.LASF34
	.byte	0x8
	.byte	0x37
	.4byte	0xa6
	.byte	0x10
	.uleb128 0x7
	.4byte	.LASF35
	.byte	0x8
	.byte	0x38
	.4byte	0x25
	.byte	0x14
	.uleb128 0x7
	.4byte	.LASF36
	.byte	0x8
	.byte	0x39
	.4byte	0x2c
	.byte	0x18
	.uleb128 0x7
	.4byte	.LASF37
	.byte	0x8
	.byte	0x3a
	.4byte	0x2c
	.byte	0x1c
	.uleb128 0x7
	.4byte	.LASF38
	.byte	0x8
	.byte	0x3b
	.4byte	0x25
	.byte	0x20
	.uleb128 0x7
	.4byte	.LASF39
	.byte	0x8
	.byte	0x3c
	.4byte	0x20a
	.byte	0x24
	.byte	0
	.uleb128 0x8
	.4byte	0xc8
	.4byte	0x21a
	.uleb128 0x9
	.4byte	0xc1
	.byte	0xf
	.byte	0
	.uleb128 0x3
	.4byte	.LASF40
	.byte	0x9
	.byte	0x97
	.4byte	0x7e
	.uleb128 0x10
	.byte	0x4
	.4byte	0x191
	.uleb128 0x10
	.byte	0x4
	.4byte	0xc8
	.uleb128 0x11
	.4byte	.LASF41
	.byte	0x38
	.byte	0xa
	.2byte	0x144
	.4byte	0x2b4
	.uleb128 0x12
	.ascii	"hdr\000"
	.byte	0xa
	.2byte	0x145
	.4byte	0x45e
	.byte	0
	.uleb128 0x13
	.4byte	.LASF42
	.byte	0xa
	.2byte	0x146
	.4byte	0x25
	.byte	0x14
	.uleb128 0x13
	.4byte	.LASF43
	.byte	0xa
	.2byte	0x147
	.4byte	0x25
	.byte	0x18
	.uleb128 0x13
	.4byte	.LASF44
	.byte	0xa
	.2byte	0x148
	.4byte	0x25
	.byte	0x1c
	.uleb128 0x13
	.4byte	.LASF45
	.byte	0xa
	.2byte	0x149
	.4byte	0x3a3
	.byte	0x20
	.uleb128 0x13
	.4byte	.LASF46
	.byte	0xa
	.2byte	0x14a
	.4byte	0x2dc
	.byte	0x28
	.uleb128 0x12
	.ascii	"dir\000"
	.byte	0xa
	.2byte	0x14b
	.4byte	0x30a
	.byte	0x2c
	.uleb128 0x13
	.4byte	.LASF35
	.byte	0xa
	.2byte	0x14c
	.4byte	0x25
	.byte	0x30
	.uleb128 0x13
	.4byte	.LASF47
	.byte	0xa
	.2byte	0x14d
	.4byte	0x53
	.byte	0x34
	.byte	0
	.uleb128 0x14
	.byte	0x4
	.byte	0xb
	.2byte	0x1b8
	.4byte	0x2dc
	.uleb128 0x15
	.4byte	.LASF48
	.sleb128 -1
	.uleb128 0x15
	.4byte	.LASF49
	.sleb128 1
	.uleb128 0x15
	.4byte	.LASF50
	.sleb128 2
	.uleb128 0x15
	.4byte	.LASF51
	.sleb128 3
	.uleb128 0x15
	.4byte	.LASF52
	.sleb128 4
	.byte	0
	.uleb128 0x16
	.4byte	.LASF53
	.byte	0xb
	.2byte	0x1be
	.4byte	0x2b4
	.uleb128 0x14
	.byte	0x4
	.byte	0xb
	.2byte	0x1c8
	.4byte	0x30a
	.uleb128 0x15
	.4byte	.LASF54
	.sleb128 -1
	.uleb128 0x15
	.4byte	.LASF55
	.sleb128 16
	.uleb128 0x15
	.4byte	.LASF56
	.sleb128 32
	.uleb128 0x15
	.4byte	.LASF57
	.sleb128 48
	.byte	0
	.uleb128 0x16
	.4byte	.LASF58
	.byte	0xb
	.2byte	0x1cd
	.4byte	0x2e8
	.uleb128 0x16
	.4byte	.LASF59
	.byte	0xb
	.2byte	0x1e1
	.4byte	0x322
	.uleb128 0x4
	.byte	0x1
	.byte	0x2
	.4byte	.LASF60
	.uleb128 0x16
	.4byte	.LASF61
	.byte	0xb
	.2byte	0x1e3
	.4byte	0x335
	.uleb128 0x17
	.4byte	0x316
	.uleb128 0x16
	.4byte	.LASF62
	.byte	0xb
	.2byte	0x1e4
	.4byte	0x346
	.uleb128 0x17
	.4byte	0x89
	.uleb128 0x16
	.4byte	.LASF63
	.byte	0xb
	.2byte	0x1e5
	.4byte	0x357
	.uleb128 0x17
	.4byte	0x7e
	.uleb128 0x16
	.4byte	.LASF64
	.byte	0xb
	.2byte	0x1e6
	.4byte	0xf8
	.uleb128 0x18
	.byte	0x8
	.byte	0xb
	.2byte	0x1ee
	.4byte	0x39e
	.uleb128 0x19
	.ascii	"_b\000"
	.byte	0xb
	.2byte	0x1ef
	.4byte	0x329
	.uleb128 0x19
	.ascii	"_s\000"
	.byte	0xb
	.2byte	0x1f0
	.4byte	0x34b
	.uleb128 0x19
	.ascii	"_u\000"
	.byte	0xb
	.2byte	0x1f1
	.4byte	0x33a
	.uleb128 0x19
	.ascii	"_f\000"
	.byte	0xb
	.2byte	0x1f2
	.4byte	0x39e
	.byte	0
	.uleb128 0x17
	.4byte	0x35c
	.uleb128 0x16
	.4byte	.LASF65
	.byte	0xb
	.2byte	0x1f3
	.4byte	0x368
	.uleb128 0x1a
	.byte	0x4
	.byte	0xb
	.2byte	0x227
	.4byte	0x3c6
	.uleb128 0x12
	.ascii	"_bp\000"
	.byte	0xb
	.2byte	0x227
	.4byte	0x21a
	.byte	0
	.byte	0
	.uleb128 0x16
	.4byte	.LASF66
	.byte	0xb
	.2byte	0x227
	.4byte	0x3af
	.uleb128 0x1a
	.byte	0x4
	.byte	0xb
	.2byte	0x228
	.4byte	0x3e9
	.uleb128 0x12
	.ascii	"_sp\000"
	.byte	0xb
	.2byte	0x228
	.4byte	0x21a
	.byte	0
	.byte	0
	.uleb128 0x16
	.4byte	.LASF67
	.byte	0xb
	.2byte	0x228
	.4byte	0x3d2
	.uleb128 0x1a
	.byte	0x4
	.byte	0xb
	.2byte	0x22a
	.4byte	0x40c
	.uleb128 0x12
	.ascii	"_fp\000"
	.byte	0xb
	.2byte	0x22a
	.4byte	0x21a
	.byte	0
	.byte	0
	.uleb128 0x16
	.4byte	.LASF68
	.byte	0xb
	.2byte	0x22a
	.4byte	0x3f5
	.uleb128 0x1a
	.byte	0x4
	.byte	0xb
	.2byte	0x22e
	.4byte	0x42f
	.uleb128 0x12
	.ascii	"_ss\000"
	.byte	0xb
	.2byte	0x22e
	.4byte	0x21a
	.byte	0
	.byte	0
	.uleb128 0x16
	.4byte	.LASF69
	.byte	0xb
	.2byte	0x22e
	.4byte	0x418
	.uleb128 0x1a
	.byte	0x4
	.byte	0xb
	.2byte	0x230
	.4byte	0x452
	.uleb128 0x12
	.ascii	"_fs\000"
	.byte	0xb
	.2byte	0x230
	.4byte	0x21a
	.byte	0
	.byte	0
	.uleb128 0x16
	.4byte	.LASF70
	.byte	0xb
	.2byte	0x230
	.4byte	0x43b
	.uleb128 0x6
	.4byte	.LASF71
	.byte	0x14
	.byte	0x3
	.byte	0x21
	.4byte	0x4f5
	.uleb128 0x7
	.4byte	.LASF72
	.byte	0x3
	.byte	0x22
	.4byte	0x516
	.byte	0
	.uleb128 0xc
	.ascii	"_id\000"
	.byte	0x3
	.byte	0x23
	.4byte	0x65
	.byte	0x8
	.uleb128 0x7
	.4byte	.LASF73
	.byte	0x3
	.byte	0x24
	.4byte	0x65
	.byte	0xa
	.uleb128 0x7
	.4byte	.LASF74
	.byte	0x3
	.byte	0x25
	.4byte	0x89
	.byte	0xc
	.uleb128 0xa
	.4byte	.LASF75
	.byte	0x3
	.byte	0x26
	.4byte	0x7e
	.byte	0x4
	.byte	0x7
	.byte	0x19
	.byte	0x10
	.uleb128 0xa
	.4byte	.LASF76
	.byte	0x3
	.byte	0x27
	.4byte	0x89
	.byte	0x4
	.byte	0x1
	.byte	0x18
	.byte	0x10
	.uleb128 0xa
	.4byte	.LASF77
	.byte	0x3
	.byte	0x2b
	.4byte	0x89
	.byte	0x4
	.byte	0x5
	.byte	0x13
	.byte	0x10
	.uleb128 0xa
	.4byte	.LASF78
	.byte	0x3
	.byte	0x2c
	.4byte	0x89
	.byte	0x4
	.byte	0x1
	.byte	0x12
	.byte	0x10
	.uleb128 0xa
	.4byte	.LASF79
	.byte	0x3
	.byte	0x32
	.4byte	0x89
	.byte	0x4
	.byte	0x1
	.byte	0x11
	.byte	0x10
	.uleb128 0xa
	.4byte	.LASF80
	.byte	0x3
	.byte	0x34
	.4byte	0x89
	.byte	0x4
	.byte	0x1
	.byte	0x10
	.byte	0x10
	.byte	0
	.uleb128 0x1b
	.byte	0x8
	.byte	0xc
	.byte	0x13
	.4byte	0x516
	.uleb128 0x7
	.4byte	.LASF27
	.byte	0xc
	.byte	0x14
	.4byte	0x21a
	.byte	0
	.uleb128 0x7
	.4byte	.LASF81
	.byte	0xc
	.byte	0x15
	.4byte	0x21a
	.byte	0x4
	.byte	0
	.uleb128 0x3
	.4byte	.LASF82
	.byte	0xc
	.byte	0x16
	.4byte	0x4f5
	.uleb128 0x1c
	.2byte	0x100
	.byte	0xa
	.byte	0xdf
	.4byte	0x62b
	.uleb128 0x7
	.4byte	.LASF83
	.byte	0xa
	.byte	0xe0
	.4byte	0x25
	.byte	0
	.uleb128 0x7
	.4byte	.LASF34
	.byte	0xa
	.byte	0xe1
	.4byte	0xb1
	.byte	0x4
	.uleb128 0x7
	.4byte	.LASF84
	.byte	0xa
	.byte	0xe2
	.4byte	0x25
	.byte	0x8
	.uleb128 0x7
	.4byte	.LASF85
	.byte	0xa
	.byte	0xe3
	.4byte	0x25
	.byte	0xc
	.uleb128 0x7
	.4byte	.LASF86
	.byte	0xa
	.byte	0xe5
	.4byte	0x516
	.byte	0x10
	.uleb128 0x7
	.4byte	.LASF87
	.byte	0xa
	.byte	0xe6
	.4byte	0x516
	.byte	0x18
	.uleb128 0x7
	.4byte	.LASF88
	.byte	0xa
	.byte	0xe7
	.4byte	0x516
	.byte	0x20
	.uleb128 0x7
	.4byte	.LASF89
	.byte	0xa
	.byte	0xe9
	.4byte	0xba
	.byte	0x28
	.uleb128 0x7
	.4byte	.LASF90
	.byte	0xa
	.byte	0xea
	.4byte	0x25
	.byte	0x2c
	.uleb128 0x7
	.4byte	.LASF91
	.byte	0xa
	.byte	0xed
	.4byte	0x25
	.byte	0x30
	.uleb128 0x7
	.4byte	.LASF92
	.byte	0xa
	.byte	0xef
	.4byte	0x5e
	.byte	0x34
	.uleb128 0x7
	.4byte	.LASF93
	.byte	0xa
	.byte	0xf1
	.4byte	0x94
	.byte	0x38
	.uleb128 0x7
	.4byte	.LASF94
	.byte	0xa
	.byte	0xf2
	.4byte	0x2c
	.byte	0x40
	.uleb128 0x7
	.4byte	.LASF95
	.byte	0xa
	.byte	0xf6
	.4byte	0x62b
	.byte	0x44
	.uleb128 0x7
	.4byte	.LASF96
	.byte	0xa
	.byte	0xf8
	.4byte	0x63b
	.byte	0x68
	.uleb128 0x7
	.4byte	.LASF97
	.byte	0xa
	.byte	0xfb
	.4byte	0x2c
	.byte	0x90
	.uleb128 0x7
	.4byte	.LASF98
	.byte	0xa
	.byte	0xfc
	.4byte	0x2c
	.byte	0x94
	.uleb128 0x7
	.4byte	.LASF99
	.byte	0xa
	.byte	0xff
	.4byte	0x2c
	.byte	0x98
	.uleb128 0x13
	.4byte	.LASF100
	.byte	0xa
	.2byte	0x100
	.4byte	0x2c
	.byte	0x9c
	.uleb128 0x13
	.4byte	.LASF101
	.byte	0xa
	.2byte	0x104
	.4byte	0x191
	.byte	0xa0
	.uleb128 0x1d
	.4byte	.LASF102
	.byte	0xa
	.2byte	0x105
	.4byte	0x64b
	.2byte	0x100
	.byte	0
	.uleb128 0x8
	.4byte	0xa6
	.4byte	0x63b
	.uleb128 0x9
	.4byte	0xc1
	.byte	0x7
	.byte	0
	.uleb128 0x8
	.4byte	0xf8
	.4byte	0x64b
	.uleb128 0x9
	.4byte	0xc1
	.byte	0x4
	.byte	0
	.uleb128 0x8
	.4byte	0x5e
	.4byte	0x65a
	.uleb128 0x1e
	.4byte	0xc1
	.byte	0
	.uleb128 0x16
	.4byte	.LASF103
	.byte	0xa
	.2byte	0x108
	.4byte	0x521
	.uleb128 0x10
	.byte	0x4
	.4byte	0x231
	.uleb128 0x6
	.4byte	.LASF104
	.byte	0x80
	.byte	0x1
	.byte	0x9
	.4byte	0x6d9
	.uleb128 0x7
	.4byte	.LASF105
	.byte	0x1
	.byte	0xa
	.4byte	0x6d9
	.byte	0
	.uleb128 0xc
	.ascii	"bin\000"
	.byte	0x1
	.byte	0xb
	.4byte	0x3c6
	.byte	0x64
	.uleb128 0x7
	.4byte	.LASF106
	.byte	0x1
	.byte	0xc
	.4byte	0x3c6
	.byte	0x68
	.uleb128 0x7
	.4byte	.LASF107
	.byte	0x1
	.byte	0xd
	.4byte	0x3e9
	.byte	0x6c
	.uleb128 0x7
	.4byte	.LASF108
	.byte	0x1
	.byte	0xe
	.4byte	0x40c
	.byte	0x70
	.uleb128 0x7
	.4byte	.LASF109
	.byte	0x1
	.byte	0x10
	.4byte	0x452
	.byte	0x74
	.uleb128 0x7
	.4byte	.LASF110
	.byte	0x1
	.byte	0x11
	.4byte	0x42f
	.byte	0x78
	.uleb128 0x7
	.4byte	.LASF111
	.byte	0x1
	.byte	0x13
	.4byte	0x6e9
	.byte	0x7c
	.byte	0
	.uleb128 0x8
	.4byte	0xc8
	.4byte	0x6e9
	.uleb128 0x9
	.4byte	0xc1
	.byte	0x63
	.byte	0
	.uleb128 0x10
	.byte	0x4
	.4byte	0x39e
	.uleb128 0x1f
	.4byte	.LASF112
	.byte	0x2
	.byte	0x11
	.4byte	0xb8
	.byte	0x3
	.4byte	0x70b
	.uleb128 0x20
	.4byte	.LASF114
	.byte	0x2
	.byte	0x11
	.4byte	0x70b
	.byte	0
	.uleb128 0x21
	.4byte	0x21a
	.uleb128 0x1f
	.4byte	.LASF113
	.byte	0x3
	.byte	0x46
	.4byte	0x25
	.byte	0x3
	.4byte	0x72b
	.uleb128 0x22
	.ascii	"hh\000"
	.byte	0x3
	.byte	0x46
	.4byte	0x72b
	.byte	0
	.uleb128 0x10
	.byte	0x4
	.4byte	0x731
	.uleb128 0x21
	.4byte	0x45e
	.uleb128 0x1f
	.4byte	.LASF115
	.byte	0x3
	.byte	0x49
	.4byte	0x25
	.byte	0x3
	.4byte	0x751
	.uleb128 0x22
	.ascii	"hh\000"
	.byte	0x3
	.byte	0x49
	.4byte	0x72b
	.byte	0
	.uleb128 0x1f
	.4byte	.LASF116
	.byte	0x2
	.byte	0x4f
	.4byte	0x78a
	.byte	0x3
	.4byte	0x78a
	.uleb128 0x22
	.ascii	"p\000"
	.byte	0x2
	.byte	0x4f
	.4byte	0x78f
	.uleb128 0x23
	.ascii	"pin\000"
	.byte	0x2
	.byte	0x4f
	.4byte	0x794
	.uleb128 0x23
	.ascii	"u\000"
	.byte	0x2
	.byte	0x4f
	.4byte	0x79f
	.uleb128 0x24
	.4byte	.LASF117
	.byte	0x2
	.byte	0x4f
	.4byte	0x329
	.byte	0
	.uleb128 0x21
	.4byte	0x329
	.uleb128 0x21
	.4byte	0x3c6
	.uleb128 0x10
	.byte	0x4
	.4byte	0x79a
	.uleb128 0x21
	.4byte	0x231
	.uleb128 0x10
	.byte	0x4
	.4byte	0x7a5
	.uleb128 0x21
	.4byte	0x3a3
	.uleb128 0x1f
	.4byte	.LASF118
	.byte	0x2
	.byte	0x3a
	.4byte	0x7e3
	.byte	0x3
	.4byte	0x7e3
	.uleb128 0x22
	.ascii	"p\000"
	.byte	0x2
	.byte	0x3a
	.4byte	0x3e9
	.uleb128 0x20
	.4byte	.LASF119
	.byte	0x2
	.byte	0x3a
	.4byte	0x7e3
	.uleb128 0x23
	.ascii	"pin\000"
	.byte	0x2
	.byte	0x3a
	.4byte	0x666
	.uleb128 0x23
	.ascii	"u\000"
	.byte	0x2
	.byte	0x3a
	.4byte	0x7e8
	.byte	0
	.uleb128 0x21
	.4byte	0x34b
	.uleb128 0x10
	.byte	0x4
	.4byte	0x3a3
	.uleb128 0x25
	.ascii	"foo\000"
	.byte	0x1
	.byte	0x1c
	.4byte	.LFB423
	.4byte	.LFE423-.LFB423
	.uleb128 0x1
	.byte	0x9c
	.4byte	0x8ea
	.uleb128 0x26
	.4byte	0x751
	.4byte	.LBB30
	.4byte	.LBE30-.LBB30
	.byte	0x1
	.byte	0x1f
	.4byte	0x887
	.uleb128 0x27
	.4byte	0x761
	.uleb128 0x28
	.4byte	.LBB31
	.4byte	.LBE31-.LBB31
	.uleb128 0x29
	.4byte	0x76a
	.uleb128 0x29
	.4byte	0x775
	.uleb128 0x2a
	.4byte	0x77e
	.uleb128 0x2
	.byte	0x91
	.sleb128 -1
	.uleb128 0x26
	.4byte	0x6ef
	.4byte	.LBB32
	.4byte	.LBE32-.LBB32
	.byte	0x2
	.byte	0x4f
	.4byte	0x84f
	.uleb128 0x27
	.4byte	0x6ff
	.byte	0
	.uleb128 0x26
	.4byte	0x6ef
	.4byte	.LBB34
	.4byte	.LBE34-.LBB34
	.byte	0x2
	.byte	0x4f
	.4byte	0x86c
	.uleb128 0x2b
	.4byte	0x6ff
	.4byte	.LLST0
	.byte	0
	.uleb128 0x2c
	.4byte	0x710
	.4byte	.LBB36
	.4byte	.LBE36-.LBB36
	.byte	0x2
	.byte	0x4f
	.uleb128 0x2b
	.4byte	0x720
	.4byte	.LLST1
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x2c
	.4byte	0x7aa
	.4byte	.LBB38
	.4byte	.LBE38-.LBB38
	.byte	0x1
	.byte	0x22
	.uleb128 0x2d
	.4byte	0x7c3
	.2byte	0x1267
	.uleb128 0x27
	.4byte	0x7ba
	.uleb128 0x28
	.4byte	.LBB39
	.4byte	.LBE39-.LBB39
	.uleb128 0x29
	.4byte	0x7ce
	.uleb128 0x29
	.4byte	0x7d9
	.uleb128 0x26
	.4byte	0x6ef
	.4byte	.LBB40
	.4byte	.LBE40-.LBB40
	.byte	0x2
	.byte	0x3a
	.4byte	0x8ce
	.uleb128 0x27
	.4byte	0x6ff
	.byte	0
	.uleb128 0x2c
	.4byte	0x736
	.4byte	.LBB42
	.4byte	.LBE42-.LBB42
	.byte	0x2
	.byte	0x3a
	.uleb128 0x2b
	.4byte	0x746
	.4byte	.LLST2
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x2e
	.4byte	.LASF120
	.byte	0xb
	.byte	0xb3
	.4byte	0x25
	.uleb128 0x2e
	.4byte	.LASF121
	.byte	0x1
	.byte	0x1a
	.4byte	0x22b
	.uleb128 0x2e
	.4byte	.LASF122
	.byte	0x3
	.byte	0x6a
	.4byte	0x225
	.uleb128 0x2f
	.4byte	.LASF123
	.byte	0xa
	.2byte	0x111
	.4byte	0x917
	.uleb128 0x10
	.byte	0x4
	.4byte	0x65a
	.uleb128 0x30
	.ascii	"ip\000"
	.byte	0x1
	.byte	0x17
	.4byte	0x927
	.uleb128 0x10
	.byte	0x4
	.4byte	0x66c
	.uleb128 0x30
	.ascii	"b\000"
	.byte	0x1
	.byte	0x18
	.4byte	0x329
	.byte	0
	.section	.debug_abbrev,"",%progbits
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
	.uleb128 0x7
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
	.uleb128 0x8
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xa
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
	.uleb128 0xb
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
	.uleb128 0xc
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
	.uleb128 0xd
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
	.uleb128 0xe
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
	.uleb128 0xf
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
	.uleb128 0x10
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
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
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x2c
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
	.section	.debug_loc,"",%progbits
.Ldebug_loc0:
.LLST0:
	.4byte	.LVL1-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x2
	.byte	0x73
	.sleb128 24
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL11-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	0
	.4byte	0
.LLST1:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	0
	.4byte	0
.LLST2:
	.4byte	.LVL13-.Ltext0
	.4byte	.LVL14-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	0
	.4byte	0
	.section	.debug_aranges,"",%progbits
	.4byte	0x1c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x4
	.byte	0
	.2byte	0
	.2byte	0
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	0
	.4byte	0
	.section	.debug_line,"",%progbits
.Ldebug_line0:
	.section	.debug_str,"MS",%progbits,1
.LASF26:
	.ascii	"_align\000"
.LASF44:
	.ascii	"_signal\000"
.LASF109:
	.ascii	"fsig\000"
.LASF74:
	.ascii	"_name_ptr\000"
.LASF48:
	.ascii	"HAL_TYPE_UNSPECIFIED\000"
.LASF35:
	.ascii	"flags\000"
.LASF0:
	.ascii	"unsigned int\000"
.LASF27:
	.ascii	"next\000"
.LASF83:
	.ascii	"version\000"
.LASF101:
	.ascii	"heap\000"
.LASF76:
	.ascii	"_legacy\000"
.LASF40:
	.ascii	"shmoff_t\000"
.LASF81:
	.ascii	"prev\000"
.LASF100:
	.ascii	"hal_malloced\000"
.LASF66:
	.ascii	"bit_pin_ptr\000"
.LASF105:
	.ascii	"blabla\000"
.LASF36:
	.ascii	"requested\000"
.LASF72:
	.ascii	"list\000"
.LASF32:
	.ascii	"free_p\000"
.LASF95:
	.ascii	"rings\000"
.LASF13:
	.ascii	"uint32_t\000"
.LASF30:
	.ascii	"rtapi_heap\000"
.LASF34:
	.ascii	"mutex\000"
.LASF43:
	.ascii	"data_ptr\000"
.LASF31:
	.ascii	"base\000"
.LASF29:
	.ascii	"rtapi_malloc_hdr_t\000"
.LASF108:
	.ascii	"fout\000"
.LASF21:
	.ascii	"rtapi_malloc_align\000"
.LASF12:
	.ascii	"long long unsigned int\000"
.LASF93:
	.ascii	"dead_beef\000"
.LASF98:
	.ascii	"str_freed\000"
.LASF24:
	.ascii	"attr\000"
.LASF54:
	.ascii	"HAL_DIR_UNSPECIFIED\000"
.LASF56:
	.ascii	"HAL_OUT\000"
.LASF102:
	.ascii	"arena\000"
.LASF119:
	.ascii	"value\000"
.LASF63:
	.ascii	"hal_s32_t\000"
.LASF4:
	.ascii	"size_t\000"
.LASF50:
	.ascii	"HAL_FLOAT\000"
.LASF114:
	.ascii	"offset\000"
.LASF60:
	.ascii	"_Bool\000"
.LASF49:
	.ascii	"HAL_BIT\000"
.LASF77:
	.ascii	"_object_type\000"
.LASF14:
	.ascii	"rtapi_atomic_type\000"
.LASF96:
	.ascii	"epsilon\000"
.LASF99:
	.ascii	"rt_alignment_loss\000"
.LASF52:
	.ascii	"HAL_U32\000"
.LASF18:
	.ascii	"char\000"
.LASF45:
	.ascii	"dummysig\000"
.LASF82:
	.ascii	"hal_list_t\000"
.LASF64:
	.ascii	"real_t\000"
.LASF123:
	.ascii	"hal_data\000"
.LASF103:
	.ascii	"hal_data_t\000"
.LASF65:
	.ascii	"hal_data_u\000"
.LASF11:
	.ascii	"__u32\000"
.LASF121:
	.ascii	"hal_shmem_base\000"
.LASF86:
	.ascii	"halobjects\000"
.LASF111:
	.ascii	"floatpin\000"
.LASF67:
	.ascii	"s32_pin_ptr\000"
.LASF22:
	.ascii	"rtapi_malloc_tag\000"
.LASF1:
	.ascii	"long long int\000"
.LASF112:
	.ascii	"hal_ptr\000"
.LASF92:
	.ascii	"lock\000"
.LASF118:
	.ascii	"set_s32_pin\000"
.LASF59:
	.ascii	"hal_bool\000"
.LASF7:
	.ascii	"__s16\000"
.LASF69:
	.ascii	"s32_sig_ptr\000"
.LASF23:
	.ascii	"size\000"
.LASF47:
	.ascii	"eps_index\000"
.LASF61:
	.ascii	"hal_bit_t\000"
.LASF124:
	.ascii	"GNU C 4.9.2 -march=armv7-a -mfloat-abi=hard -mfpu=v"
	.ascii	"fpv3-d16 -mthumb -mtls-dialect=gnu -g -O -std=c11\000"
.LASF51:
	.ascii	"HAL_S32\000"
.LASF62:
	.ascii	"hal_u32_t\000"
.LASF120:
	.ascii	"_halerrno\000"
.LASF115:
	.ascii	"hh_get_wmb\000"
.LASF2:
	.ascii	"long double\000"
.LASF107:
	.ascii	"sout\000"
.LASF113:
	.ascii	"hh_get_rmb\000"
.LASF78:
	.ascii	"_valid\000"
.LASF10:
	.ascii	"__s32\000"
.LASF8:
	.ascii	"short int\000"
.LASF75:
	.ascii	"_refcnt\000"
.LASF16:
	.ascii	"long int\000"
.LASF37:
	.ascii	"allocated\000"
.LASF33:
	.ascii	"arena_size\000"
.LASF116:
	.ascii	"get_bit_pin\000"
.LASF73:
	.ascii	"_owner_id\000"
.LASF94:
	.ascii	"default_ringsize\000"
.LASF39:
	.ascii	"name\000"
.LASF41:
	.ascii	"hal_pin\000"
.LASF17:
	.ascii	"sizetype\000"
.LASF15:
	.ascii	"long unsigned int\000"
.LASF5:
	.ascii	"__u8\000"
.LASF127:
	.ascii	"rtapi_malloc_header\000"
.LASF87:
	.ascii	"threads\000"
.LASF68:
	.ascii	"float_pin_ptr\000"
.LASF80:
	.ascii	"_wmb\000"
.LASF46:
	.ascii	"type\000"
.LASF6:
	.ascii	"unsigned char\000"
.LASF79:
	.ascii	"_rmb\000"
.LASF71:
	.ascii	"halhdr\000"
.LASF84:
	.ascii	"shmem_bot\000"
.LASF42:
	.ascii	"_data_ptr_addr\000"
.LASF88:
	.ascii	"funct_entry_free\000"
.LASF90:
	.ascii	"exact_base_period\000"
.LASF125:
	.ascii	"hal/accessor/acctest.c\000"
.LASF55:
	.ascii	"HAL_IN\000"
.LASF57:
	.ascii	"HAL_IO\000"
.LASF3:
	.ascii	"signed char\000"
.LASF126:
	.ascii	"/next/home/mah/machinekit-arm/src\000"
.LASF9:
	.ascii	"short unsigned int\000"
.LASF106:
	.ascii	"bout\000"
.LASF117:
	.ascii	"rvalue\000"
.LASF19:
	.ascii	"double\000"
.LASF89:
	.ascii	"base_period\000"
.LASF53:
	.ascii	"hal_type_t\000"
.LASF70:
	.ascii	"float_sig_ptr\000"
.LASF91:
	.ascii	"threads_running\000"
.LASF25:
	.ascii	"rtapi_malloc_tag_t\000"
.LASF38:
	.ascii	"freed\000"
.LASF20:
	.ascii	"rtapi_malloc_align_t\000"
.LASF28:
	.ascii	"align\000"
.LASF97:
	.ascii	"str_alloc\000"
.LASF122:
	.ascii	"global_heap\000"
.LASF104:
	.ascii	"instdata\000"
.LASF85:
	.ascii	"shmem_top\000"
.LASF58:
	.ascii	"hal_pin_dir_t\000"
.LASF110:
	.ascii	"ssig\000"
	.ident	"GCC: (Debian 4.9.2-10) 4.9.2"
	.section	.note.GNU-stack,"",%progbits
