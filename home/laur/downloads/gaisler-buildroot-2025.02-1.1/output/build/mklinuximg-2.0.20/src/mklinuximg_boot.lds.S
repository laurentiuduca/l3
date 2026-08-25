/* Final mklinuximg linker script, linkes together:
 *   * boot code (physical addresses)
 *   * Linux kernel image (virtual addresses)
 *   * STARTUP/PROM virtual image (virtual addresses)
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

#include <config.h>

__bss_start = CONFIG_LINUX_BSS_START;
__bss_stop = CONFIG_LINUX_BSS_START + CONFIG_LINUX_BSS_SIZE;

OUTPUT_FORMAT("elf32-sparc", "elf32-sparc", "elf32-sparc")
OUTPUT_ARCH(sparc)
ENTRY(_start)
SECTIONS
{
        /DISCARD/ : {
        *(.exit.text)
        *(.exit.data)
        *(.exitcall.exit)
	*(.eh_frame)
#ifdef CONFIG_NO_LINUX
	*image_linux
#endif
        }

        . = CONFIG_RAM_START;
        .text : {
		/* BOOT TEXT */
                _start = .;
		_boot_text_start = .;
		*head.o (.text)  /* Must be located first */
                *boot.o (.text)
		. = ALIGN(16);
		_boot_text_end = .;
	}

	.data : {
		/* BOOT DATA/BSS */
		_boot_data_start = .;
		*head.o(.data*)
		*boot.o(.data*)
		. = ALIGN(16);
		_boot_data_end = .;
	}

	.bss : {
		_boot_bss_start = .;
		*head.o(.bss*)
		*boot.o(.bss*)
		. = ALIGN(16);
		_boot_bss_end = .;
	}

	. = ALIGN(0x2000);
	.mmu_tables : {
		/* BOOT/STARTUP/PROM/LINUX MMU Table
		 *
		 * SRMMU has alignment requirements on tables that must be
		 * fullfilled:
		 *   - CTX Table:        align 0x2000, size 0x400
		 *   - Level 1 Table:    align 0x0400, size 0x400
		 *   - Level 2 Table:    align 0x0100, size 0x100
		 *   - Level 3 Table:    align 0x0100, size 0x100
		 */

		_mmu_ctx_table = .;	/* Context Table */
		. += 0x400;
		. = ALIGN(0x400);
		_mmu_ctx0_level1 = .;		/* Level1 Table 16M/Entry */
		. += 0x400;
		. = ALIGN(0x100);
		_mmu_ctx0_fc_level2 = .;	/* Level2 Table 256K/Entry for
						 * 0xFC000000 - 0xFFFFFFFF
						 */
		. += 0x100;
		. = ALIGN(0x100);
		_mmu_ctx0_ffd_level3 = .;	/* Level3 Table 4K/Entry for
						 * 0xFFD00000 - 0xFFD3FFFF
						 */
		. += 0x100;
        }

	/* Linux Kernel */
	. = CONFIG_RAM_START + 0x4000;
        .vmlinux : {
#ifndef CONFIG_NO_LINUX
                *image_linux
#else
		. += 0x6000;
#endif
        }

	. = ALIGN(16);
	_boot_lma_end = ABSOLUTE(.);
}

#include "mklinuximg_virt.lds.S"
