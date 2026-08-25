/* mklinuximg STARTUP/PROM sections linker script, all in virtual address space
 *
 * Copyright (C) 2022 Frontgrade Gaisler AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

/*
 * This is not an independent linker script. It is #included by
 * mklinuximg_boot.lds.S
 */

SECTIONS
{
	/* MKLINUX Startup sections (overwritten by Linux) [64KBytes]
	 *
	 * 0xFFD00000-0xFFD3FFFF: Mapped to STARTUP/PROM code (End of RAM)
	 *
	 *  ----/--------------------------------------------\ 0xFFD00000
	 *  A   |   STARTUP HEAP/MALLOC                      |
	 *  R   |                                            |
	 *  E   |                                            |
	 *  A   |                                            |
	 *      |    (CONFIG_STARTUP_HEAP_SIZE)              |
	 *  C   |                                            |
	 *  L   |--------------------------------------------|
	 *  R   |   STARTUP .BSS                             |
	 *  ----|--------------------------------------------|
	 *  A   |   STARTUP .TEXT                            |
	 *  R   |                                            |
	 *  E   |                                            |
	 *  A   |--------------------------------------------|
	 *      |   STARTUP .DATA                            |
	 *  M   |--------------------------------------------|<- Linux EOM,
	 *  O   |   PROM .TEXT                               |   All above over-
	 *  V   |                                            |   written after
	 *  E   |                                            |   boot by Linux
	 *      |--------------------------------------------|
	 *      |   PROM .DATA                               |
	 *  ----|--------------------------------------------|
	 *  A   |   PROM .BSS                                |
	 *  R   |--------------------------------------------|
	 *  E   |   PROM HEAP/MALLOC                         |
	 *  A   |    (CONFIG_PROM_HEAP_SIZE)                 |
	 *      |--------------------------------------------|
	 *  C   |   STARTUP/PROM STACK                       |
	 *  L   |    (CONFIG_PROMSTART_STACK_SIZE)           |
	 *  R   |                                            |
	 *  --- \--------------------------------------------/<- EOM
	 *
	 */
	. = CONFIG_LINUX_OPPROM_BEGVM;

	_startup_start = ABSOLUTE(.);

	. = ALIGN(16);
	_startup_heap_start = ABSOLUTE(.);
	. += CONFIG_STARTUP_HEAP_SIZE;
	. = ALIGN(16);
	_startup_heap_end = ABSOLUTE(.);

	. = ALIGN(16);
	.startup_bss : {
		_startup_bss_start = .;
		*startup*.o(.bss)
		*startup*.o(COMMON)
		. = ALIGN(16);
		_startup_bss_end = .;
	}

	/*
	 * Section which is stored in mklinuximg after Linux kernel, it is
	 * moved to the end of memory (location determined by boot code) and
	 * the MMU is setup so that the addresses matches.
	 *
	 * The startup code is overwritten by Linux once Linux boots, the
	 * PROM lives on and respond to Linux requests.
	 *
	 */
	. = ALIGN(16);
	_promstartup_move_start_va = ABSOLUTE(.);
	.ps_move_startup :
	AT (_boot_lma_end) /* From mklinuximg_boot.lds.S */
	{
		/*** STARTUP TEXT ***/
		. = ALIGN(16);
		_startup_text_start = .;
		*startup.o(.startup.main.text) /* Must be first */
		*startup*.o(.text)
		. = ALIGN(16);
		_startup_text_end = .;

		/*** STARTUP DATA ***/
		_startup_data_start = .;
		*startup*.o(.*data*)
		. = ALIGN(16);
		_startup_data_end = .;

		_startup_end = .;
		_linux_end_of_memory_va = .;
	}
	. = .;
	_prom_start_va = ABSOLUTE(.);
	.ps_move_prom :
	{

		/*** PROM TEXT ***/
		. = ALIGN(16);
		_prom_text_start_va = .;
		*prom*.o(.text)
		. = ALIGN(16);
		_prom_text_end_va = .;

		/*** PROM DATA ***/
		. = ALIGN(16);
		_prom_data_start_va = .;
		*prom*.o(.prom_common_struct)
		*prom*.o(.*data*)
		. = ALIGN(16);
		_prom_data_end_va = .;
		. = ALIGN(16);
	}
	_prom_start = LOADADDR(.ps_move_prom);
	_prom_text_start = LOADADDR(.ps_move_prom) + _prom_text_start_va - ADDR(.ps_move_prom);
	_prom_text_end = LOADADDR(.ps_move_prom) + _prom_text_end_va - ADDR(.ps_move_prom);
	_prom_data_start = LOADADDR(.ps_move_prom) + _prom_data_start_va - ADDR(.ps_move_prom);
	_prom_data_end = LOADADDR(.ps_move_prom) + _prom_data_end_va - ADDR(.ps_move_prom);
	_linux_end_of_memory = LOADADDR(.ps_move_startup) + _linux_end_of_memory_va - ADDR(.ps_move_startup);

	_promstartup_move_end_va = ABSOLUTE(.);
	_startup_prom_start = LOADADDR(.ps_move_startup);
	_startup_prom_end = LOADADDR(.ps_move_prom) + _prom_data_end_va - ADDR(.ps_move_prom);
	_promstartup_move_start = _startup_prom_start;
	_promstartup_move_end = _startup_prom_end;

	/*
	 * Where the kernel command line resides (in physical memory) before
	 * execution of mklinuximg starts.
	 */
	_bootargs_cmdline_lma = LOADADDR(.ps_move_prom) + bootargs_cmdline - ADDR(.ps_move_prom);

	. = ALIGN(16);
	.prom_bss : {
		_prom_bss_start = .;
		*prom*.o(.bss)
		*prom*.o(COMMON)
		. = ALIGN(16);
		_prom_bss_end = .;
	}

	. = ALIGN(16);
	_prom_heap_start = ABSOLUTE(.);
	. += CONFIG_PROM_HEAP_SIZE;
	. = ALIGN(16);
	_prom_heap_end = ABSOLUTE(.);

	. = ALIGN(16);
	.ps_stack : {
		_promstartup_stack_start = .;
		. += CONFIG_PROMSTART_STACK_SIZE;
		. = ALIGN(0x1000); /* MMU will go on page boundary */
		_promstartup_stack_end = .;
	}

	_prom_end = ABSOLUTE(.);
}
