/*******************************************************************************
 * Copyright 2019-2020 Microchip Corporation.
 *
 * SPDX-License-Identifier: MIT
 *
 * MPFS HAL Embedded Software
 *
 */

/*******************************************************************************
 * @file entry.S
 * @author Microchip-FPGA Embedded Systems Solutions
 * @brief entry functions.
 *
 */

  .option norvc




/***********************************************************************************
 *
 * The following config_copy() symbol overrides the weak symbol in the HAL and does
 * a safe copy of HW config data
 */
    // config_copy helper function:
    //  a0 = dest
    //  a1 = src
    //  a2 = length
    .globl  config_copy
    .type   config_copy, @function
config_copy:
    mv  t1,a0
    beqz    a2,2f
1:
    lb  t2,0(a1)
    sb  t2,0(t1)
    addi    a2,a2,-1
    addi    t1,t1,1
    addi    a1,a1,1
    bnez    a2,1b
2:
    ret

/***********************************************************************************
 *
 * config_32_copy () Copies a word at a time, used when copying to contigous registers
 */
    // config_copy helper function:
    //  a0 = dest
    //  a1 = src
    //  a2 = length
    .globl  config_32_copy
    .type   config_32_copy, @function
config_32_copy:
    mv  t1,a0
    beqz    a2,2f
1:
    lw  t2,0(a1)
    sw  t2,0(t1)
    addi    a2,a2,-4
    addi    t1,t1,4
    addi    a1,a1,4
    bnez    a2,1b
2:
    ret

 /***********************************************************************************
 *
 * config_64_copy - copying using 64 bit loads, addresses must be on 64 bit boundary
 */
    // config_copy helper function:
    //  a0 = dest
    //  a1 = src
    //  a2 = length
    .globl  config_64_copy
    .type   config_64_copy, @function
config_64_copy:
    mv  t1,a0
    beqz    a2,2f
1:
    ld  t2,0(a1)
    sd  t2,0(t1)
    addi    a2,a2,-8
    addi    t1,t1,8
    addi    a1,a1,8
    bnez    a2,1b
2:
    ret

/***********************************************************************************
 *
 * The following copy_switch_code() symbol overrides the weak symbol in the HAL and does
 * a safe copy of HW config data
 */
    .globl  copy_switch_code
    .type   copy_switch_code, @function
copy_switch_code:
    la      a5, __sc_start           // a5 = __sc_start
    la      a4, __sc_load            // a4 = __sc_load
    beq a5,a4,.copy_switch_code_done // if a5 == a4, goto copy_switch_code_done
    la      a3, __sc_end             // a3 = __sc_end
    beq a5,a3,.copy_switch_code_done // if a5 == a3, goto copy_switch_code_done
.copy_switch_code_loop:
    lw  a2,0(a4)                     // a2 = *a4
    sw  a2,0(a5)                     // *a5 = a2
    addi    a5,a5,4                  // a5+=4
    addi    a4,a4,4                  // a4+=4

    bltu    a5,a3,.copy_switch_code_loop // if a5 < a3, goto copy_switch_code_loop
.copy_switch_code_done:
    ret

/*******************************************************************************
 *
 */
#define START__OF_LIM 0x08000000
#define END__OF_LIM   0x08200000
#define START__OF_DTM 0x01000000
#define END__OF_DTM   0x01002000


.clear_l2lim:
    // Clear the LIM
    //
    // On reset, the first 15 ways are L2 and the last way is cache
    // We can initialize all, as cache write through to DDR is blocked
    // until DDR in initialized, so will have no effect other than clear ECC
    //
    // NOTE: we need to check if we are debugging from LIM,if so do not
    // initialize.
    //
    la a2, __text_start
    la  a4, 0x08000000          # start of LIM address
    and a2, a2, a4
    bnez a2, .done_clear
    la  a5, 0x08200000          # end of LIM address
    j   1f
.clear_dtim:
    //
    // Clear the E51 DTIM to prevent any ECC memory errors on initial access
    //
    la  a4, 0x01000000          # DTIM start
    la  a5, 0x01002000          # DTIM end
1:
    // common loop used by both .clear_l2lim and .clear_dtim
    sd   x0, 0(a4)
    add a4, a4, __SIZEOF_POINTER__
    blt a4, a5, 1b
.done_clear:
    ret

/*
 * record_ecc_error_counts on reset
 * These are non-zero in the coreplex.
 * Can be checked later on to see if values have changed
 *      a0 = mECCDataFailCount save address
        a1 = mECCDataCorrectionCount save address
        a2 = mECCDirFixCount save address
 */
.record_ecc_error_counts:
    # Store initial ECC errors
    #define mECCDataFailCount               0x02010168U
    la  a5, mECCDataFailCount
    mv  a4, a0// eg. Use stat of DTIM in not used for anything else  0x01000100
    lw  t2,0(a5)
    sw  t2,0(a4)
    #define mECCDataCorrectionCount         0x02010148U
    la  a5, mECCDataCorrectionCount
    mv  a4, a1// eg. Use stat of DTIM in not used for anything else 0x01000110
    lw  t2,0(a5)
    sw  t2,0(a4)
    #define mECCDirFixCount                 0x02010108u
    la  a5, mECCDirFixCount
    mv  a4, a2// eg. Use stat of DTIM in not used for anything else 0x01000120
    lw  t2,0(a5)
    sw  t2,0(a4)
	ret

/*
 * clear_ras , clear_ras_2_deep
 * Two deep function calls. 
 * Used to clear the interal processor Return Address Stack
 * This is belt and braces, may not be required
 */
.clear_ras:
    mv a5, x1
    nop
    call .clear_ras_2_deep
    nop
    nop
    nop
    nop
    nop
    nop
    mv  x1, a5
    ret

.clear_ras_2_deep:
    nop
    nop
    nop
    nop
    nop
    nop
    ret

/***********************************************************************************
 *
 * pdma_transfer
 * Only used by the mpfs hal. App code uses the provided driver.
 *
 *   a0 = dest
 *   a1 = src
 *   a2 = length
 *   a3 = PDMA Base Address - 0x3000000 + (0x01000 * PDMA_CHANNEL)
 */
    .globl  pdma_transfer
    .type   pdma_transfer, @function
pdma_transfer:
    mv  t1,a0
    mv t0, a3           // Base address
    li t1, 1
    sw t1, 0(t0)        // claim
    mv t1, a2           // SIZE
    sd t1, 8(t0)        // bytes
    mv t1, a0           // dest address
    sd t1, 16(t0)       // dest
    mv t1, a1           // source address
    sd t1, 24(t0)       // src
    li t1, 0xff000000
    sw t1, 4(t0)        // full speed copy
    li t1, 3
    sw t1, 0(t0)        // start transfer
    ret

/***********************************************************************************
 *
 * pdma_transfer_complete
 * Loops until transfer complete
 * Only used by the mpfs hal. App code uses the provided driver.
 *
 *    a0 = PDMA Base Address - 0x3000000 + (0x01000 * PDMA_CHANNEL)
 */
    //
    .globl  pdma_transfer_complete
    .type   pdma_transfer_complete, @function
pdma_transfer_complete:
    mv t0, a3           // Base address
1: // wait for completion
    lw t1, 0(t0)
    andi t1, t1, 2
    bnez t1, 1b
    // release DMA
    sw zero, 0(t0)
    ret



