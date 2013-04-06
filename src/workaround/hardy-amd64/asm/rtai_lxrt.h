#warning "manually modified header /usr/realtime-2.6.24-16-rtai/include/asm-x86_64/rtai_lxrt.h"

#ifdef BROKEN_RTAI_LXRT

/* Copyright (C) 1999-2003 Paolo Mantegazza <mantegazza@aero.polimi.it>
 * extensions for user space modules are jointly copyrighted (2000) with:
 *		Pierre Cloutier <pcloutier@poseidoncontrols.com>,
 *		Steve Papacharalambous <stevep@zentropix.com>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#ifndef _RTAI_ASM_X8664_LXRT_H
#define _RTAI_ASM_X8664_LXRT_H

#include <linux/version.h>

// the following include is in error, see https://gna.org/bugs/?19185
// #include <asm/asm-offsets.h>

#include <asm/rtai_vectors.h>

#ifdef CONFIG_RTAI_LXRT_USE_LINUX_SYSCALL
#define USE_LINUX_SYSCALL
#else
#undef USE_LINUX_SYSCALL
#endif

#define RTAI_SYSCALL_NR      0x70000000
#define RTAI_SYSCALL_CODE    rdi
#define RTAI_SYSCALL_ARGS    rsi
#define RTAI_SYSCALL_RETPNT  rdx

#define RTAI_FAKE_LINUX_SYSCALL  39

#ifndef NR_syscalls
#define NR_syscalls __NR_syscall_max
#endif

#define LINUX_SYSCALL_NR      orig_rax
#define LINUX_SYSCALL_REG1    rdi
#define LINUX_SYSCALL_REG2    rsi
#define LINUX_SYSCALL_REG3    rdx
#define LINUX_SYSCALL_REG4    r10
#define LINUX_SYSCALL_REG5    r8
#define LINUX_SYSCALL_REG6    r9
#define LINUX_SYSCALL_RETREG  rax
#define LINUX_SYSCALL_FLAGS   eflags

#define LXRT_DO_IMMEDIATE_LINUX_SYSCALL(regs) \
	do { \
		regs->rcx = regs->r10; \
		regs->LINUX_SYSCALL_RETREG = ((asmlinkage int (*)(long, ...))sys_call_table[regs->rax])(regs->rdi, regs->rsi, regs->rdx, regs->rcx, regs->rax, regs->r8, regs->r9, regs->r10, regs->r11, regs->rbx, regs->rbp, regs->r12, regs->r13, regs->r14, regs->r15); \
	} while (0)

#define SET_LXRT_RETVAL_IN_SYSCALL(regs, retval) \
	do { \
                if (regs->RTAI_SYSCALL_RETPNT) { \
			rt_copy_to_user((void *)regs->RTAI_SYSCALL_RETPNT, &retval, sizeof(retval)); \
		} \
	} while (0)

#define LOW  0
#define HIGH 1

#if defined(CONFIG_RTAI_RTC_FREQ) && CONFIG_RTAI_RTC_FREQ >= 2

#define TIMER_NAME        "RTC"
#define TIMER_FREQ        CONFIG_RTAI_RTC_FREQ
#define TIMER_LATENCY     0
#define TIMER_SETUP_TIME  0
#define ONESHOT_SPAN      0

#else /* CONFIG_RTAI_RTC_FREQ == 0 */

#ifdef CONFIG_X86_LOCAL_APIC

#define TIMER_NAME        "APIC"
#define TIMER_TYPE  1
#define HRT_LINUX_TIMER_NAME  "lapic"
#define FAST_TO_READ_TSC
#define TIMER_FREQ        RTAI_FREQ_APIC
#define TIMER_LATENCY     RTAI_LATENCY_APIC
#define TIMER_SETUP_TIME  RTAI_SETUP_TIME_APIC
#define ONESHOT_SPAN      (CPU_FREQ/(CONFIG_RTAI_CAL_FREQS_FACT + 2)) //(0x7FFFFFFFLL*(CPU_FREQ/TIMER_FREQ))
#ifdef CONFIG_GENERIC_CLOCKEVENTS
#define USE_LINUX_TIMER
#define update_linux_timer(cpuid) \
        do { hal_pend_uncond(LOCAL_TIMER_IPI, cpuid); } while (0)
#else /* !CONFIG_GENERIC_CLOCKEVENTS */
#define update_linux_timer(cpuid)
#endif /* CONFIG_GENERIC_CLOCKEVENTS */

#else /* !CONFIG_X86_LOCAL_APIC */

#define USE_LINUX_TIMER
#define TIMER_NAME        "8254-PIT"
#define TIMER_TYPE  0
#define HRT_LINUX_TIMER_NAME  "pit"
#define TIMER_FREQ        RTAI_FREQ_8254
#define TIMER_LATENCY     RTAI_LATENCY_8254
#define TIMER_SETUP_TIME  RTAI_SETUP_TIME_8254
#define ONESHOT_SPAN      ((0x7FFF*(CPU_FREQ/TIMER_FREQ))/(CONFIG_RTAI_CAL_FREQS_FACT + 1)) //(0x7FFF*(CPU_FREQ/TIMER_FREQ))
#define update_linux_timer(cpuid)  hal_pend_uncond(TIMER_8254_IRQ, cpuid)

#endif /* CONFIG_X86_LOCAL_APIC */

#endif /* CONFIG_RTAI_RTC_FREQ != 0 */

union rtai_lxrt_t {
    RTIME rt;
    long i[1];
    void *v[1];
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __KERNEL__

#include <asm/segment.h>
#include <asm/mmu_context.h>

static inline void _lxrt_context_switch (struct task_struct *prev, struct task_struct *next, int cpuid)
{
	extern void *context_switch(void *, void *, void *);
/* REMARK: the line below is not needed in i386, why should it be so if both
   math_restore do a "clts" before orring TS_USEDFPU in status ?????          */
	if (task_thread_info(prev)->status & TS_USEDFPU) clts();
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
	prev->fpu_counter = 0;
#endif
	context_switch(NULL, prev, next);
}

#if 0
#define IN_INTERCEPT_IRQ_ENABLE()   do { rtai_hw_sti(); } while (0)
#define IN_INTERCEPT_IRQ_DISABLE()  do { rtai_hw_cli(); } while (0)
#else
#define IN_INTERCEPT_IRQ_ENABLE()   do { } while (0)
#define IN_INTERCEPT_IRQ_DISABLE()  do { } while (0)
#endif

#if 0 // optimised (?)
static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) = kmalloc(sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/ + (lnxtsk->thread.rsp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.rsp, GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
//	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), (void *)(lnxtsk->thread.rsp & ~(THREAD_SIZE - 1)), sizeof(struct thread_info));
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/, (void *)(lnxtsk->thread.rsp), (lnxtsk->thread.rsp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.rsp);
}

static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
//	memcpy((void *)(lnxtsk->thread.rsp & ~(THREAD_SIZE - 1)), lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), sizeof(struct thread_info));
	memcpy((void *)lnxtsk->thread.rsp, lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/, (lnxtsk->thread.rsp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.rsp);
}
#else  // brute force
static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) = kmalloc(sizeof(struct thread_struct) + THREAD_SIZE, GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), (void *)(lnxtsk->thread.rsp & ~(THREAD_SIZE - 1)), THREAD_SIZE);
}

static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
	memcpy((void *)(lnxtsk->thread.rsp & ~(THREAD_SIZE - 1)), lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), THREAD_SIZE);
}
#endif

#define rt_copy_from_user(a, b, c)  \
        ( { int ret = __copy_from_user_inatomic(a, b, c); ret; } )

#define rt_copy_to_user(a, b, c)  \
        ( { int ret = __copy_to_user_inatomic(a, b, c); ret; } )

#define rt_put_user  __put_user
#define rt_get_user  __get_user

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)

#define rt_strncpy_from_user(a, b, c)  \
        ( { int ret = strncpy_from_user(a, b, c); ret; } )

#else

/*
 * From Linux lib/usercopy.c.
 */

#define __do_strncpy_from_user(dst,src,count,res)			   \
do {									   \
	long __d0, __d1, __d2;						   \
	__asm__ __volatile__(						   \
		"	testq %1,%1\n"					   \
		"	jz 2f\n"					   \
		"0:	lodsb\n"					   \
		"	stosb\n"					   \
		"	testb %%al,%%al\n"				   \
		"	jz 1f\n"					   \
		"	decq %1\n"					   \
		"	jnz 0b\n"					   \
		"1:	subq %1,%0\n"					   \
		"2:\n"							   \
		".section .fixup,\"ax\"\n"				   \
		"3:	movq %5,%0\n"					   \
		"	jmp 2b\n"					   \
		".previous\n"						   \
		".section __ex_table,\"a\"\n"				   \
		"	.align 8\n"					   \
		"	.quad 0b,3b\n"					   \
		".previous"						   \
		: "=r"(res), "=c"(count), "=&a" (__d0), "=&S" (__d1),	   \
		  "=&D" (__d2)						   \
		: "i"(-EFAULT), "0"(count), "1"(count), "3"(src), "4"(dst) \
		: "memory");						   \
} while (0)

static inline long rt_strncpy_from_user(char *dst, const char __user *src, long count)
{
	long res;
	__do_strncpy_from_user(dst, src, count, res);
	return res;
}

#endif

//#define RTAI_DO_LINUX_SIGNAL
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
extern int FASTCALL(do_signal(struct pt_regs *regs, sigset_t *oldset));
#define RT_DO_SIGNAL(regs)  do_signal(regs, NULL)
#else
__attribute__((regparm(3))) void do_notify_resume(struct pt_regs *regs, void *_unused, __u32 thread_info_flags);
#define RT_DO_SIGNAL(regs)  do_notify_resume(regs, NULL, (_TIF_SIGPENDING | _TIF_RESTORE_SIGMASK));
#endif

#else /* !__KERNEL__ */

/* NOTE: Keep the following routines unfold: this is a compiler
   compatibility issue. */

#include <sys/syscall.h>
#include <unistd.h>

static union rtai_lxrt_t _rtai_lxrt(long srq, void *arg)
{
	union rtai_lxrt_t retval;
#if 1 //def USE_LINUX_SYSCALL
	syscall(RTAI_SYSCALL_NR, srq, arg, &retval);
#else
	RTAI_DO_TRAP(RTAI_SYS_VECTOR, retval, srq, arg);
#endif
	return retval;
}

static inline union rtai_lxrt_t rtai_lxrt(long dynx, long lsize, long srq, void *arg)
{
	return _rtai_lxrt(ENCODE_LXRT_REQ(dynx, srq, lsize), arg);
}

#define rtai_iopl()  do { extern int iopl(int); iopl(3); } while (0)

#endif /* __KERNEL__ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_RTAI_ASM_X8664_LXRT_H */
#endif // BROKEN_RTAI_LXRT

