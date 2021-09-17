/*******************************************************************************
 * Copyright 2019-2020 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * MPFS HAL Embedded Software
 *
 */

/*******************************************************************************
 * @file mss_sgmii.c
 * @author Microchip-FPGA Embedded Systems Solutions
 * @brief sgmii related functions
 *
 */

#include <string.h>
#include <stdio.h>
//#include "mpfs_hal/mss_hal.h"
//#include "encoding.h"
//#include "mss_plic.h"
#include "hw_sgmii_tip.h"
#include "mss_nwc_init.h"
#include "mss_sgmii.h"
#include "mss_pll.h"
#include "simulation.h"
#include "mss_ddr_sgmii_phy_defs.h"
#include "mss_ddr_SGMII_regs.h"

//static PART_TYPE silicon_variant = PART_NOT_DETERMINED;

/*
 * local functions
 */
//static void setup_sgmii_rpc_per_config(void);
#ifdef SGMII_SUPPORT
//static uint32_t sgmii_channel_setup(void);
#endif

extern CFG_DDR_SGMII_PHY_TypeDef *CFG_DDR_SGMII_PHY;
extern DDR_CSR_APB_TypeDef *DDRCFG;
extern IOSCBCFG_TypeDef                *SCBCFG_REGS;
extern g5_mss_top_scb_regs_TypeDef     *SCB_REGS;
        
/*
 * local variable
 */
//static uint32_t sro_dll_90_code;

/*
 * local functions
 */
//static void set_early_late_thresholds(uint8_t n_late_threshold, uint8_t p_early_threshold);

/*
 * extern functions
 */
extern void sgmii_mux_config(uint8_t option);

/**
 * setup_sgmii_rpc_per_config
 * @param sgmii_instruction
 */
static void setup_sgmii_rpc_per_config(void)
{
    CFG_DDR_SGMII_PHY->SGMII_MODE.SGMII_MODE    = (LIBERO_SETTING_SGMII_MODE & ~REG_CDR_MOVE_STEP);
    CFG_DDR_SGMII_PHY->CH0_CNTL.CH0_CNTL        = LIBERO_SETTING_CH0_CNTL;
    CFG_DDR_SGMII_PHY->CH1_CNTL.CH1_CNTL        = LIBERO_SETTING_CH1_CNTL;
    CFG_DDR_SGMII_PHY->RECAL_CNTL.RECAL_CNTL    = LIBERO_SETTING_RECAL_CNTL;
    CFG_DDR_SGMII_PHY->CLK_CNTL.CLK_CNTL        = LIBERO_SETTING_CLK_CNTL;
    /* ibuffmx_p and _n rx1, bit 22 and 23 , rx0, bit 20 and 21 */
    CFG_DDR_SGMII_PHY->SPARE_CNTL.SPARE_CNTL    = LIBERO_SETTING_SPARE_CNTL;
    CFG_DDR_SGMII_PHY->PLL_CNTL.PLL_CNTL        = LIBERO_SETTING_PLL_CNTL;
}

/**
 * SGMII Off mode
 */
void sgmii_off_mode(void)
{
    /*
     * do soft reset of SGMII TIP
     */
    CFG_DDR_SGMII_PHY->SOFT_RESET_SGMII.SOFT_RESET_SGMII = (0x01 << 8U) | 1U;
    CFG_DDR_SGMII_PHY->SOFT_RESET_SGMII.SOFT_RESET_SGMII = 1U;

    /*
     *
     */
    setup_sgmii_rpc_per_config();

    /*
     * Resetting the SCB register only required in already in dynamic mode. If
     * not reset, IO will not be configured.
     */
    IOSCB_DLL_SGMII->soft_reset = (0x01U << 0x00U);         /*  reset sgmii */

}

uint32_t sgmii_setup(void)
{
    {
        sgmii_off_mode();
        sgmii_mux_config(RPC_REG_UPDATE);
    }
    return(0UL);
}





#if 0
/**
 * Set eye thresholds
 * @param n_late_threshold
 * @param p_early_threshold
 */
static void set_early_late_thresholds(uint8_t n_late_threshold, uint8_t p_early_threshold)
{
    uint32_t n_eye_values;
    uint32_t p_eye_value;

    /*
     * Set the N eye width value
     * bits 31:29 for CH1, bits 28:26  for CH0 in spare control (N eye width value)
     */
    n_eye_values = (n_late_threshold << SHIFT_TO_CH0_N_EYE_VALUE);
    n_eye_values |= (n_late_threshold << SHIFT_TO_CH1_N_EYE_VALUE);

    CFG_DDR_SGMII_PHY->SPARE_CNTL.SPARE_CNTL    = (LIBERO_SETTING_SPARE_CNTL & N_EYE_MASK) | n_eye_values;

    /*
     * Set P values
     */
    p_eye_value = (p_early_threshold << SHIFT_TO_REG_RX0_EYEWIDTH);
    CFG_DDR_SGMII_PHY->CH0_CNTL.CH0_CNTL        = ((LIBERO_SETTING_CH0_CNTL & REG_RX0_EYEWIDTH_P_MASK) | p_eye_value) | REG_RX0_EN_FLAG_N;
    CFG_DDR_SGMII_PHY->CH1_CNTL.CH1_CNTL        = ((LIBERO_SETTING_CH1_CNTL & REG_RX0_EYEWIDTH_P_MASK) | p_eye_value) | REG_RX1_EN_FLAG_N;

}
#endif

