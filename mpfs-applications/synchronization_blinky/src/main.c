/* main.c - Hello World demo */

/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include "mss_gpio.h"

/*
 * The hello world demo has two threads that utilize semaphores and sleeping
 * to take turns printing a greeting message at a controlled rate. The demo
 * shows both the static and dynamic approaches for spawning a thread; a real
 * world application would likely use the static approach for both threads.
 */


/* size of stack area used by each thread */
#define STACKSIZE 2048

/* scheduling priority used by each thread */
#define PRIORITY 7

/* delay between greetings (in ms) */
#define SLEEPTIME 2000


/*
 * @param my_name      thread identification string
 * @param my_sem       thread's own semaphore
 * @param other_sem    other thread's semaphore
 */
void helloLoop(const char *my_name,
	       struct k_sem *my_sem, struct k_sem *other_sem, uint32_t led_val) 
{
	const char *tname;
	uint8_t cpu;
	struct k_thread *current_thread;

	while (1) {
		/* take my semaphore */
		k_sem_take(my_sem, K_FOREVER);

		current_thread = k_current_get();
		tname = k_thread_name_get(current_thread);
#if CONFIG_SMP
		cpu = arch_curr_cpu()->id;
#else
		cpu = 0;
#endif

		/*set leds*/
		MSS_GPIO_set_outputs(GPIO2_LO, led_val);
		/* say "hello" */
		if (tname == NULL) {
			printk("%s: Hello World from cpu %d on %s!\n",
				my_name, cpu, CONFIG_BOARD);
		} else {
			printk("%s: Hello World from cpu %d on %s!\n",
				tname, cpu, CONFIG_BOARD);
		}

		/* wait a while, then let other thread have a turn */
		k_busy_wait(100000);
		k_msleep(SLEEPTIME);
		k_sem_give(other_sem);
	}
}

/* define semaphores */

K_SEM_DEFINE(threadA_sem, 1, 1);	/* starts off "available" */
K_SEM_DEFINE(threadB_sem, 0, 1);	/* starts off "not available" */


/* threadB is a dynamic thread that is spawned by threadA */

void threadB(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	/* invoke routine to ping-pong hello messages with threadA */
	helloLoop(__func__, &threadB_sem, &threadA_sem,0xffffffff);
}

K_THREAD_STACK_DEFINE(threadB_stack_area, STACKSIZE);
static struct k_thread threadB_data;

/* threadA is a static thread that is spawned automatically */

void threadA(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	MSS_GPIO_init(GPIO2_LO);

	for (int cnt = 16u; cnt< 20u; cnt++)
	{
		MSS_GPIO_config(GPIO2_LO,
						cnt,
						MSS_GPIO_OUTPUT_MODE);
	}

	MSS_GPIO_config(GPIO2_LO, MSS_GPIO_26, MSS_GPIO_OUTPUT_MODE);
	MSS_GPIO_config(GPIO2_LO, MSS_GPIO_27, MSS_GPIO_OUTPUT_MODE);
	MSS_GPIO_config(GPIO2_LO, MSS_GPIO_28, MSS_GPIO_OUTPUT_MODE);

	/* spawn threadB */
	k_tid_t tid = k_thread_create(&threadB_data, threadB_stack_area,
			STACKSIZE, threadB, NULL, NULL, NULL,
			PRIORITY, 0, K_FOREVER);

	k_thread_name_set(tid, "thread_b");
#if CONFIG_SCHED_CPU_MASK
	k_thread_cpu_mask_disable(&threadB_data, 1);
	k_thread_cpu_mask_enable(&threadB_data, 0);
#endif
	k_thread_start(&threadB_data);

	/* invoke routine to ping-pong hello messages with threadB */
	helloLoop(__func__, &threadA_sem, &threadB_sem,0u);
}

K_THREAD_DEFINE(thread_a, STACKSIZE, threadA, NULL, NULL, NULL,
		PRIORITY, 0, 0);
