/*
 * Copyright (c) 2020 Stephanos Ioannidis <root@stephanos.io>
 * Copyright (c) 2018 Lexmark International, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <kernel_internal.h>
#include <exc_handle.h>
#include <logging/log.h>
LOG_MODULE_DECLARE(os, CONFIG_KERNEL_LOG_LEVEL);

#define FAULT_DUMP_VERBOSE	(CONFIG_FAULT_DUMP == 2)

#if FAULT_DUMP_VERBOSE
static const char *get_dbgdscr_moe_string(uint32_t moe)
{
	switch (moe) {
	case DBGDSCR_MOE_HALT_REQUEST:
		return "Halt Request";
	case DBGDSCR_MOE_BREAKPOINT:
		return "Breakpoint";
	case DBGDSCR_MOE_ASYNC_WATCHPOINT:
		return "Asynchronous Watchpoint";
	case DBGDSCR_MOE_BKPT_INSTRUCTION:
		return "BKPT Instruction";
	case DBGDSCR_MOE_EXT_DEBUG_REQUEST:
		return "External Debug Request";
	case DBGDSCR_MOE_VECTOR_CATCH:
		return "Vector Catch";
	case DBGDSCR_MOE_OS_UNLOCK_CATCH:
		return "OS Unlock Catch";
	case DBGDSCR_MOE_SYNC_WATCHPOINT:
		return "Synchronous Watchpoint";
	default:
		return "Unknown";
	}
}

static void dump_debug_event(void)
{
	/* Read and parse debug mode of entry */
	uint32_t dbgdscr = __get_DBGDSCR();
	uint32_t moe = (dbgdscr & DBGDSCR_MOE_Msk) >> DBGDSCR_MOE_Pos;

	/* Print debug event information */
	LOG_ERR("Debug Event (%s)", get_dbgdscr_moe_string(moe));
}

static void dump_fault(uint32_t status, uint32_t addr)
{
	/*
	 * Dump fault status and, if applicable, tatus-specific information.
	 * Note that the fault address is only displayed for the synchronous
	 * faults because it is unpredictable for asynchronous faults.
	 */
	switch (status) {
	case FSR_FS_ALIGNMENT_FAULT:
		LOG_ERR("Alignment Fault @ 0x%08x", addr);
		break;
	case FSR_FS_BACKGROUND_FAULT:
		LOG_ERR("Background Fault @ 0x%08x", addr);
		break;
	case FSR_FS_PERMISSION_FAULT:
		LOG_ERR("Permission Fault @ 0x%08x", addr);
		break;
	case FSR_FS_SYNC_EXTERNAL_ABORT:
		LOG_ERR("Synchronous External Abort @ 0x%08x", addr);
		break;
	case FSR_FS_ASYNC_EXTERNAL_ABORT:
		LOG_ERR("Asynchronous External Abort");
		break;
	case FSR_FS_SYNC_PARITY_ERROR:
		LOG_ERR("Synchronous Parity/ECC Error @ 0x%08x", addr);
		break;
	case FSR_FS_ASYNC_PARITY_ERROR:
		LOG_ERR("Asynchronous Parity/ECC Error");
		break;
	case FSR_FS_DEBUG_EVENT:
		dump_debug_event();
		break;
	default:
		LOG_ERR("Unknown (%u)", status);
	}
}
#endif

/**
 * @brief Undefined instruction fault handler
 *
 * @return Returns true if the fault is fatal
 */
bool z_arm_fault_undef_instruction(z_arch_esf_t *esf)
{
	/* Print fault information */
	LOG_ERR("***** UNDEFINED INSTRUCTION ABORT *****");

	/* Invoke kernel fatal exception handler */
	z_arm_fatal_error(K_ERR_CPU_EXCEPTION, esf);

	/* All undefined instructions are treated as fatal for now */
	return true;
}

/**
 * @brief Prefetch abort fault handler
 *
 * @return Returns true if the fault is fatal
 */
bool z_arm_fault_prefetch(z_arch_esf_t *esf)
{
	/* Read and parse Instruction Fault Status Register (IFSR) */
	uint32_t ifsr = __get_IFSR();
	uint32_t fs = ((ifsr & IFSR_FS1_Msk) >> 6) | (ifsr & IFSR_FS0_Msk);

	/* Read Instruction Fault Address Register (IFAR) */
	uint32_t ifar = __get_IFAR();

	/* Print fault information*/
	LOG_ERR("***** PREFETCH ABORT *****");
	if (FAULT_DUMP_VERBOSE) {
		dump_fault(fs, ifar);
	}

	/* Invoke kernel fatal exception handler */
	z_arm_fatal_error(K_ERR_CPU_EXCEPTION, esf);

	/* All prefetch aborts are treated as fatal for now */
	return true;
}

#ifdef CONFIG_USERSPACE
Z_EXC_DECLARE(z_arm_user_string_nlen);

static const struct z_exc_handle exceptions[] = {
	Z_EXC_HANDLE(z_arm_user_string_nlen)
};

/* Perform an assessment whether an MPU fault shall be
 * treated as recoverable.
 *
 * @return true if error is recoverable, otherwise return false.
 */
static bool memory_fault_recoverable(z_arch_esf_t *esf)
{
	for (int i = 0; i < ARRAY_SIZE(exceptions); i++) {
		/* Mask out instruction mode */
		uint32_t start = (uint32_t)exceptions[i].start & ~0x1U;
		uint32_t end = (uint32_t)exceptions[i].end & ~0x1U;

		if (esf->basic.pc >= start && esf->basic.pc < end) {
			esf->basic.pc = (uint32_t)(exceptions[i].fixup);
			return true;
		}
	}

	return false;
}
#endif

/**
 * @brief Data abort fault handler
 *
 * @return Returns true if the fault is fatal
 */
bool z_arm_fault_data(z_arch_esf_t *esf)
{
	/* Read and parse Data Fault Status Register (DFSR) */
	uint32_t dfsr = __get_DFSR();
	uint32_t fs = ((dfsr & DFSR_FS1_Msk) >> 6) | (dfsr & DFSR_FS0_Msk);

	/* Read Data Fault Address Register (DFAR) */
	uint32_t dfar = __get_DFAR();

#if defined(CONFIG_USERSPACE)
	if ((fs == FSR_FS_BACKGROUND_FAULT)
			|| (fs == FSR_FS_PERMISSION_FAULT)) {
		if (memory_fault_recoverable(esf)) {
			return false;
		}
	}
#endif

	/* Print fault information*/
	LOG_ERR("***** DATA ABORT *****");
	if (FAULT_DUMP_VERBOSE) {
		dump_fault(fs, dfar);
	}

	/* Invoke kernel fatal exception handler */
	z_arm_fatal_error(K_ERR_CPU_EXCEPTION, esf);

	/* All data aborts are treated as fatal for now */
	return true;
}

/**
 * @brief Initialisation of fault handling
 */
void z_arm_fault_init(void)
{
	/* Nothing to do for now */
}
