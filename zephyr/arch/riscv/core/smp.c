/*
 * Copyright (c) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <ksched.h>
#include <zephyr/irq.h>
#include <rv_smp_defs.h>

uintptr_t *get_hart_msip(int hart_id);
volatile struct {
	arch_cpustart_t fn;
	void *arg;
} riscv_cpu_init[CONFIG_MP_MAX_NUM_CPUS];

/* Do we need to allocate stacks for CPUs that Zephyr is not running on? */
#if CONFIG_HAS_MONITOR_HART && !defined(CONFIG_RV_MP_TOTAL_NUM_CPUS)
/* Just 1 needed */
K_KERNEL_STACK_ARRAY_DEFINE(_non_z_stacks,
				1,
			    CONFIG_ISR_STACK_SIZE);
#elif defined(CONFIG_RV_MP_TOTAL_NUM_CPUS)
#if CONFIG_MP_MAX_NUM_CPUS < CONFIG_RV_MP_TOTAL_NUM_CPUS
/* calculate how many needed */
K_KERNEL_STACK_ARRAY_DEFINE(_non_z_stacks,
				(CONFIG_RV_MP_TOTAL_NUM_CPUS - CONFIG_MP_MAX_NUM_CPUS),
			    CONFIG_ISR_STACK_SIZE);
#endif
#endif
/*
 * Collection of flags to control wake up of harts. This is trickier than
 * expected due to the fact that the wfi can be triggered when in the
 * debugger so we have to stage things carefully to ensure we only wake
 * up at the correct time.
 *
 * initial implementation which assumes any monitor hart is hart id 0 and
 * SMP harts have contiguous hart IDs. CONFIG_RV_BASE_CPU will have minimum
 * value of 1 for systems with monitor hart and zero otherwise.
 *
 */

#if (CONFIG_RV_MP_TOTAL_NUM_CPUS > CONFIG_MP_MAX_NUM_CPUS)
#define WAKE_FLAG_COUNT (CONFIG_RV_MP_TOTAL_NUM_CPUS)
#else
#define WAKE_FLAG_COUNT (CONFIG_MP_MAX_NUM_CPUS)
#endif

volatile void *riscv_cpu_sp;

/* we will index directly off of mhartid so need to be careful... */
volatile __noinit unsigned long hart_wake_flags[WAKE_FLAG_COUNT];
/*
 * _curr_cpu is used to record the struct of _cpu_t of each cpu.
 * for efficient usage in assembly
 */
volatile _cpu_t *_curr_cpu[CONFIG_MP_MAX_NUM_CPUS];

void arch_start_cpu(int cpu_num, k_thread_stack_t *stack, int sz,
		    arch_cpustart_t fn, void *arg)
{
	int hart_num = cpu_num + CONFIG_RV_BASE_CPU;
	volatile uint32_t *r = (uint32_t *)get_hart_msip(cpu_num + CONFIG_RV_BASE_CPU);


	/* Used to avoid empty loops which can cause debugger issues
	 * and also for retry count on interrupt to keep sending every now and again...
	 */
	volatile int counter = 0;

	_curr_cpu[cpu_num] = &(_kernel.cpus[cpu_num]);
	riscv_cpu_init[cpu_num].fn = fn;
	riscv_cpu_init[cpu_num].arg = arg;

	/* set the initial sp of target sp through riscv_cpu_sp
	 * Controlled sequencing of start up will ensure
	 * only one secondary cpu can read it per time
	 */
	riscv_cpu_sp = Z_KERNEL_STACK_BUFFER(stack) + sz;

	/* wait slave cpu to start */
	while (hart_wake_flags[hart_num] != RV_WAKE_WAIT) {
		counter++;
	}

	hart_wake_flags[hart_num] = RV_WAKE_GO;
	*r = 0x01U;   /*raise soft interrupt for hart(x) where x== hart ID*/

	while (hart_wake_flags[hart_num] != RV_WAKE_DONE) {
		counter++;
		if (0 == (counter % 64)) {
			*r = 0x01U;   /* Another nudge... */
	}
	}

	*r = 0x00U;   /* Clear int now we are done */
}

void z_riscv_secondary_cpu_init(int cpu_num)
{
#ifdef CONFIG_THREAD_LOCAL_STORAGE
	__asm__("mv tp, %0" : : "r" (z_idle_threads[cpu_num].tls));
#endif
#if defined(CONFIG_RISCV_SOC_INTERRUPT_INIT)
	soc_interrupt_init();
#endif
#ifdef CONFIG_RISCV_PMP
	z_riscv_pmp_init();
#endif
#ifdef CONFIG_SMP
	irq_enable(RISCV_MACHINE_SOFT_IRQ);
#endif
	riscv_cpu_init[cpu_num].fn(riscv_cpu_init[cpu_num].arg);
}

#ifdef CONFIG_SMP
uintptr_t *get_hart_msip(int hart_id)
{
#ifdef CONFIG_64BIT
	return (uintptr_t *)(uint64_t)(RISCV_MSIP_BASE + (hart_id * 4));
#else
	return (uintptr_t *)(RISCV_MSIP_BASE + (hart_id * 4));
#endif
}

void arch_sched_ipi(void)
{
	unsigned int key;
	uint32_t i;
	uint8_t id;

	key = arch_irq_lock();

	id = _current_cpu->id;
	unsigned int num_cpus = arch_num_cpus();

	for (i = 0U; i < num_cpus; i++) {
		if (i != id) {
			volatile uint32_t *r = (uint32_t *)get_hart_msip(i + CONFIG_RV_BASE_CPU);
			*r = 1U;
		}
	}

	arch_irq_unlock(key);
}

static void sched_ipi_handler(const void *unused)
{
	ARG_UNUSED(unused);

	volatile uint32_t *r = (uint32_t *)(get_hart_msip(_current_cpu->id + CONFIG_RV_BASE_CPU));
	*r = 0U;

	z_sched_ipi();
}

static int riscv_smp_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	IRQ_CONNECT(RISCV_MACHINE_SOFT_IRQ, 0, sched_ipi_handler, NULL, 0);
	irq_enable(RISCV_MACHINE_SOFT_IRQ);

	return 0;
}

SYS_INIT(riscv_smp_init, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif /* CONFIG_SMP */
