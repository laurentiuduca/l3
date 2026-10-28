/************************************************************************/
/*   This file is a part of the mkprom3 boot-prom utility               */
/*   Copyright (C) 2004 Cobham Gaisler AB                             */
/*                                                                      */
/*   This library is free software; you can redistribute it and/or      */
/*   modify it under the terms of the GNU General Public                */
/*   License as published by the Free Software Foundation; either       */
/*   version 2 of the License, or (at your option) any later version.   */
/*                                                                      */
/*   See the file COPYING.GPL for the full details of the license.      */
/************************************************************************/

/*
 * FTMCTRL SDRAM Initialization fix. When enabling the SDRAM on some
 * implementations of the FTMCTRL IP, the LOAD_MODE_COMMAND does not correctly
 * drive the Bank addres pin BA[1].
 *
 * The fix_ftmctrl_sdram_init function solves the issue by:
 *  1. Enable the SDRAM via the MCFG2 register
 *  2. Perform write operation to first SDRAM address to set the FTMCTRL pins
 *     in the correct state
 *  3. Disable the SDRAM
 *
 * Note: Function is a leaf routine. RAM and C runtime have not been
 *      initialized at this stage.
 *
 * Input register: o0. o0 contains the mcfg2 value
 *
 * Used registers: o0, o2, o3. o1 is expected to hold the register base
 *      address.
 */

    .section    ".text"
    .align 4
    .global     fix_mctrl_sdram_init

fix_mctrl_sdram_init:

    /* Execute SDRAM fix iff SDRAM will be enabled */
    set (0x3 << 13), %o2
    andcc %o0, %o2, %g0
    be .Lfix_mctrl_sdram_init_end
     nop
    st %o0, [%o1 + 4]
    mov %o0, %o3

    /* Delay until SDRAM init COMMANDS are complete */
    set 0x100, %o0
.Lfix_mctrl_sdram_init_delay:
    deccc %o0
    bne .Lfix_mctrl_sdram_init_delay
     nop

    set _clean_ram0, %o0
    ld [%o0], %o0
    st %g0, [%o0]

    /* Disable SDRAM */
    andn %o3, %o2, %o3
    st %o3, [%o1 + 4]

.Lfix_mctrl_sdram_init_end:
    retl
     nop
