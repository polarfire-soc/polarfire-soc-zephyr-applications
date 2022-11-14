/*
 * Copyright (c) 2016 Jean-Paul Etienne <fractalclone@gmail.com>
 * Contributors: 2018 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/toolchain.h>
#include <zephyr/linker/sections.h>
#include <zephyr/arch/cpu.h>
#include <offsets.h>
#include <rv_smp_defs.h>
#include "asm_macros.inc"

/* exports */
GTEXT(__initialize)
GTEXT(__reset)

/* imports */
GTEXT(_PrepC)
GTEXT(riscv_cpu_wake_flag)
GTEXT(riscv_cpu_sp)
GTEXT(z_riscv_secondary_cpu_init)

GDATA(_non_z_stacks)

#if CONFIG_INCLUDE_RESET_VECTOR
SECTION_FUNC(reset, __reset)
	/*
	 * jump to __initialize
	 * use call opcode in case __initialize is far away.
	 * This will be dependent on linker.ld configuration.
	 */
	call __initialize
#endif /* CONFIG_INCLUDE_RESET_VECTOR */

/* use ABI name of registers for the sake of simplicity */

/*
 * Remainder of asm-land initialization code before we can jump into
 * the C domain
 */
SECTION_FUNC(TEXT, __initialize)

    csrr a0, mhartid /* Who are we? */
	/* First stage of wake up - we assume for MPFS and other systems with
	 * monitor hart that array has CONFIG_RV_MP_TOTAL_NUM_CPUS */
	slli a1, a0, RV_REGSHIFT
	la a2, hart_wake_flags
	add a2, a2, a1
	li a1, RV_WAKE_INIT
	sr a1, 0(a2)
#if CONFIG_HAS_MONITOR_HART
/*
 * For SMP operation the monitor hart cannot be part of the SMP group
 * of harts as it is generally not the same as the application harts...
 * Just loop for ever for now if we get here.
 */
	li a1, CONFIG_MONITOR_HART_ID
	bne a0, a1, not_monitor_hart
	/*
	 * Disable interrupts etc to try and keep things quiet.
	 * But first assign the first non zephyr isr stack to us for safety,
	 * If monitor hart is not hart id 0, this will need changing...
	 */
	la sp, _non_z_stacks
	li t0, __z_interrupt_stack_SIZEOF
	add sp, sp, t0
no_monitor_loop:
	csrw mstatus, zero
    csrw mie, zero
    csrw mip, zero
	wfi
	j no_monitor_loop

not_monitor_hart:
#endif	/* CONFIG_HAS_MONITOR_HART */
    /*
     * This will boot primary core which is usually thelowest numbered, just halt
	 * other cores.
     *
     * Note. For systems with monitor hart we assume no supervisor supported on monitor
	 * hart so if this is selected as the hart to use we don't touch mxdeleg registers.
	 * Also, if not in master mode this is a bad thing to do...
     */
#if CONFIG_NO_SUPERVISOR_DELEGATION
#if CONFIG_HAS_MONITOR_HART && !CONFIG_MONITOR_HART_SUPERVISOR
	beq a0, a1, not_super_capable /* regs set up earlier... */
#endif /* CONFIG_HAS_MONITOR_HART && !CONFIG_MONITOR_HART_SUPERVISOR */
    csrw mideleg,0
    csrw medeleg,0
	/*
	 * Enable all counters for user mode otherwise some user mode samples
	 * will fail on real hardware...
	 */
	li t0,0xFFFFFFFF
	csrw mcounteren,t0
#if CONFIG_SUPERVISOR_MODE_CAPABLE
	csrw scounteren,t0
#endif

not_super_capable:
#endif /* CONFIG_NO_SUPERVISOR_DELEGATION */

#ifdef CONFIG_FPU
	/*
	 * Enable floating-point.
	 */
	li t0, MSTATUS_FS_INIT
	csrs mstatus, t0

	/*
	 * Floating-point rounding mode set to IEEE-754 default, and clear
	 * all exception flags.
	 */
	fscsr zero
#endif

#ifdef CONFIG_INIT_STACKS
	/* Pre-populate all bytes in z_interrupt_stacks with 0xAA */
	la t0, z_interrupt_stacks
#if defined(CONFIG_MP_MAX_NUM_CPUS) && (CONFIG_MP_MAX_NUM_CPUS > 0)
	li t1, __z_interrupt_stack_SIZEOF * CONFIG_MP_MAX_NUM_CPUS
#else
	li t1, __z_interrupt_stack_SIZEOF
#endif
	add t1, t1, t0
	/* Only do this once on the startup hart - slightly risky as we are relying
	 * on other harts not using the stack until we allow them to wake...*/
    li a1, CONFIG_RV_BASE_CPU
    csrr a0, mhartid
    bne a0, a1, skip_stack_fill

	/* Populate z_interrupt_stacks with 0xaaaaaaaa */
	li t2, 0xaaaaaaaa
aa_loop:
	sw t2, 0x00(t0)
	addi t0, t0, 4
	blt t0, t1, aa_loop
skip_stack_fill:
#endif /* CONFIG_INIT_STACKS */

	/*
	 * Initially, setup stack pointer to
	 * z_interrupt_stacks[cpu] + __z_interrupt_stack_SIZEOF
	 * for Zephyr involved harts.
	 *
	 * For other harts we allocate from the _non_z_stacks pool
	 */

	csrr t0, mhartid
#if CONFIG_RV_MP_TOTAL_NUM_CPUS > 1
	li t1, CONFIG_RV_BASE_CPU
	bltu t0, t1, is_not_z_hart2
	addi t0, t0, -CONFIG_RV_BASE_CPU // Convert to 0 based cpu id
	li t1, CONFIG_MP_MAX_NUM_CPUS
	bgeu t0, t1, is_not_z_hart1
#else
	addi t0, t0, -CONFIG_RV_BASE_CPU // Convert to 0 based cpu id
#endif /* CONFIG_RV_MP_TOTAL_NUM_CPUS > 1 */

	li t1, __z_interrupt_stack_SIZEOF
	la a1, z_interrupt_stacks
	shiftmul_add a1, t0, __z_interrupt_stack_SIZEOF
	add sp, a1, t1
    csrw mscratch, zero /* mscratch needs to be 0 for SMP at this stage */
#if CONFIG_RV_MP_TOTAL_NUM_CPUS > 1
	j irq_stack_set
/*
 * Now need to allocate stack for non zephyr harts for safety.
 */
is_not_z_hart1: /* hart is above last zephyr cpu - allocate based on hart id - CONFIG_MP_MAX_NUM_CPUS */
	csrr t0, mhartid
	addi t0, t0, -CONFIG_MP_MAX_NUM_CPUS
is_not_z_hart2: /* hart is below base cpu - allocate based on hart id which is in t0 */
	la t1, _non_z_stacks
	shiftmul_add t1, t0, __z_interrupt_stack_SIZEOF
	add sp, t1, 0

irq_stack_set:
#endif /* CONFIG_RV_MP_TOTAL_NUM_CPUS > 1 */

#ifdef CONFIG_WDOG_INIT
	call _WdogInit
#endif
	li a1, CONFIG_RV_BASE_CPU
	csrr a0, mhartid
	beq a0, a1, boot_primary_core

	/* Second stage of wake up - a2 already set up */
	li a1, RV_WAKE_WAIT
	sr a1, 0(a2)
	li a0, RV_WAKE_GO
	/*
	 * Enable software interrupt
	 */
	li  t0, MIP_MSIP
	csrrs x0, mie, t0
	/*
	 * Loop here with wfi to try and keep the bus quiet if multiple
	 * harts are spinning on same region.
	 */
loop_secondary_core:
	wfi

	lr a1, 0(a2)
	bne a0, a1, loop_secondary_core
	/*
	 * Jump into C domain assuming _PrepC has already been called.
	 */
	li a1, RV_WAKE_DONE
	sr a1, 0(a2)
	la t0, riscv_cpu_sp
	lr t0, 0(t0)
	addi sp, t0, 0
	csrr a0, mhartid

#if CONFIG_RV_BASE_CPU != 0
	addi a0, a0, -CONFIG_RV_BASE_CPU /* Convert to 0 - n cpu number */
#endif /* CONFIG_RV_BASE_CPU != 0 */

	csrw mstatus, zero
    csrw mie, zero
    csrw mip, zero
	call z_riscv_secondary_cpu_init

boot_primary_core:
	/*
	 * Jump into C domain. _PrepC zeroes BSS, copies rw data into RAM,
	 * and then enters kernel z_cstart
	 */
	call _PrepC
