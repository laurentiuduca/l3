/* Trap Table, Entry point and low level setup stuff
 *
 * Copyright (C) 2011 Frontgrade Gaisler AB
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

	.extern boot_clr_region, boot_main, boot_kernel
	.extern end_of_mem
	.extern first_cpu_booted
	.text
	.align	4

#define BAD_TRAP(i) ba head_bad_trap; mov i, %o0; nop; nop;
#define TRAP(H)  mov %psr, %l0; sethi %hi(H), %l4; jmp %l4+%lo(H); nop;

#ifdef __FIX_LEON3FT_B2BST
#define B2B_SINGLE_NOP \
	nop
#define B2B_DOUBLE_NOP \
	nop \
	nop
#else
#define B2B_SINGLE_NOP
#define B2B_DOUBLE_NOP
#endif
/*** SPARC BOOT/STARTUP/PROM TRAP TABLE ***/

_start:
trapbase:
reset_trap:		/* 00 reset trap (ENTRY POINT) */
	ba	head_init
	 nop
	nop
	nop
trap01:			/* 01 instruction_access_exception */
	BAD_TRAP(0x01)
trap02:			/* 02 illegal_instruction */
	BAD_TRAP(0x02)
trap03:			/* 03 priveleged_instruction */
	BAD_TRAP(0x03)
trap04:			/* 04 fp_disabled */
	BAD_TRAP(0x04)
trap05:			/* 05 window_overflow */
	mov	%psr, %l0
	sethi	%hi(_window_overflow), %l4
	jmp	%l4+%lo(_window_overflow)
	 nop
trap06:			/* 06 window_underflow */
	mov	%psr, %l0
	sethi	%hi(_window_underflow), %l4
	jmp	%l4+%lo(_window_underflow)
	 nop
trap07:			/* 07 memory_address_not_aligned */
	BAD_TRAP(0x07)
trap08:			/* 08 fp_exception */
	BAD_TRAP(0x08)
trap09:			/* 09 data_access_exception */
	BAD_TRAP(0x09)
trap0A:			/* 0A Tag Overflow */
	BAD_TRAP(0x0a)
trap0B:			/* 0B Watchpoint detected */
	BAD_TRAP(0x0b)
trap0C_0F:		/* 0C-0F unknown traps */
	BAD_TRAP(0x0c)
	BAD_TRAP(0x0d)
	BAD_TRAP(0x0e)
	BAD_TRAP(0x0f)
trap10_1F:		/* 0x10-0x1F Interrupt Traps */
	BAD_TRAP(0x10)
	BAD_TRAP(0x11)
	BAD_TRAP(0x12)
	BAD_TRAP(0x13)
	BAD_TRAP(0x14)
	BAD_TRAP(0x15)
	BAD_TRAP(0x16)
	BAD_TRAP(0x17)
	BAD_TRAP(0x18)
	BAD_TRAP(0x19)
	BAD_TRAP(0x1a)
	BAD_TRAP(0x1b)
	BAD_TRAP(0x1c)
	BAD_TRAP(0x1d)
	BAD_TRAP(0x1e)
	BAD_TRAP(0x1f)
trap20_2C:		/* 0x10-0x1F Interrupt Traps */
	BAD_TRAP(0x20)
	BAD_TRAP(0x21)
	BAD_TRAP(0x22)
	BAD_TRAP(0x23)
	BAD_TRAP(0x24)
	BAD_TRAP(0x25)
	BAD_TRAP(0x26)
	BAD_TRAP(0x27)
	BAD_TRAP(0x28)
	BAD_TRAP(0x29)
	BAD_TRAP(0x2a)
	BAD_TRAP(0x2b)
	BAD_TRAP(0x2c)
	BAD_TRAP(0x2d)
	BAD_TRAP(0x2e)
	BAD_TRAP(0x2f)

	BAD_TRAP(0x30)	BAD_TRAP(0x31)	BAD_TRAP(0x32)	BAD_TRAP(0x33)
	BAD_TRAP(0x34)	BAD_TRAP(0x35)	BAD_TRAP(0x36)	BAD_TRAP(0x37)
	BAD_TRAP(0x38)	BAD_TRAP(0x39)	BAD_TRAP(0x3a)	BAD_TRAP(0x3b)
	BAD_TRAP(0x3c)	BAD_TRAP(0x3d)	BAD_TRAP(0x3e)	BAD_TRAP(0x3f)

	BAD_TRAP(0x40)	BAD_TRAP(0x41)	BAD_TRAP(0x42)	BAD_TRAP(0x43)
	BAD_TRAP(0x44)	BAD_TRAP(0x45)	BAD_TRAP(0x46)	BAD_TRAP(0x47)
	BAD_TRAP(0x48)	BAD_TRAP(0x49)	BAD_TRAP(0x4a)	BAD_TRAP(0x4b)
	BAD_TRAP(0x4c)	BAD_TRAP(0x4d)	BAD_TRAP(0x4e)	BAD_TRAP(0x4f)

	BAD_TRAP(0x50)	BAD_TRAP(0x51)	BAD_TRAP(0x52)	BAD_TRAP(0x53)
	BAD_TRAP(0x54)	BAD_TRAP(0x55)	BAD_TRAP(0x56)	BAD_TRAP(0x57)
	BAD_TRAP(0x58)	BAD_TRAP(0x59)	BAD_TRAP(0x5a)	BAD_TRAP(0x5b)
	BAD_TRAP(0x5c)	BAD_TRAP(0x5d)	BAD_TRAP(0x5e)	BAD_TRAP(0x5f)

	BAD_TRAP(0x60)	BAD_TRAP(0x61)	BAD_TRAP(0x62)	BAD_TRAP(0x63)
	BAD_TRAP(0x64)	BAD_TRAP(0x65)	BAD_TRAP(0x66)	BAD_TRAP(0x67)
	BAD_TRAP(0x68)	BAD_TRAP(0x69)	BAD_TRAP(0x6a)	BAD_TRAP(0x6b)
	BAD_TRAP(0x6c)	BAD_TRAP(0x6d)	BAD_TRAP(0x6e)	BAD_TRAP(0x6f)

	BAD_TRAP(0x70)	BAD_TRAP(0x71)	BAD_TRAP(0x72)	BAD_TRAP(0x73)
	BAD_TRAP(0x74)	BAD_TRAP(0x75)	BAD_TRAP(0x76)	BAD_TRAP(0x77)
	BAD_TRAP(0x78)	BAD_TRAP(0x79)	BAD_TRAP(0x7a)	BAD_TRAP(0x7b)
	BAD_TRAP(0x7c)	BAD_TRAP(0x7d)	BAD_TRAP(0x7e)	BAD_TRAP(0x7f)

soft_trap:		/* 80 Soft-trap, happens on error */
head_bad_trap:
	/* not recoverable... stop */
	ta	0	/* halt CPU */
	nop

/* trap table should continue, but the traps below are undefined and should
 * not happen
 */

_window_overflow:
	mov	%wim, %l3		! Calculate next WIM
	mov	%g1, %l7
	srl	%l3, 1, %g1

	mov	%asr17, %l4		! LEON3/4 has nwindows in %asr17
	and	%l4, 0x1f, %l4

	sll	%l3, %l4, %l4
	or	%l4, %g1, %g1

	save				! Get into window to be saved.
	mov	%g1, %wim
	nop; nop; nop
	std	%l0, [%sp + 0]
	B2B_SINGLE_NOP
	std	%l2, [%sp + 8]
	B2B_SINGLE_NOP
	std	%l4, [%sp + 16]
	B2B_SINGLE_NOP
	std	%l6, [%sp + 24]
	B2B_SINGLE_NOP
	std	%i0, [%sp + 32]
	B2B_SINGLE_NOP
	std	%i2, [%sp + 40]
	B2B_SINGLE_NOP
	std	%i4, [%sp + 48]
	B2B_SINGLE_NOP
	std	%i6, [%sp + 56]
	restore				! Go back to trap window.
	mov	%l7, %g1
	jmp	%l1			! Re-execute save.
	rett	%l2

_window_underflow:
	mov	%wim, %l3		! Calculate next WIM
	sll	%l3, 1, %l4

	mov	%asr17, %l5		! LEON3/4 has nwindows in %asr17
	and	%l5, 0x1f, %l5

	srl	%l3, %l5, %l5
	or	%l5, %l4, %l5
	mov	%l5, %wim
	nop; nop; nop
	restore				! Two restores to get into the
	restore				! window to restore
	ldd 	[%sp + 0], %l0		! Restore window from the stack
	ldd	[%sp + 8], %l2
	ldd	[%sp + 16], %l4
	ldd	[%sp + 24], %l6
	ldd	[%sp + 32], %i0
	ldd	[%sp + 40], %i2
	ldd	[%sp + 48], %i4
	ldd	[%sp + 56], %i6
	save				! Get back to the trap window.
	save
	jmp	%l1			! Re-execute restore.
	rett	%l2

/* Assume Boot loader has initlaized CPU and hardware where neccessary for us,
 * but MMU is still not turned on.
 *
 * 1. Setup CPU:
 *     - Turn off interrupts (PIL=0xf)
 *     - Turn off Floating-Point unit
 *     - Enable Traps
 *     - Set S, PS bits
 *     - Set trap Table Base address Regsiter (TBR)
 *     - SPARC register window management (WIM/CWP)
 *
 * 2. set up a C environment
 * 3. call boot initialization C code
 * 4. Switch into Virtual Address Space Stack
 */
laur:
    // laur
    sethi  %hi(0x80000000), %l3
    or  %l3, 0x104, %l2
1: ld  [ %l2 ], %l1
    btst  4, %l1
    be  1b
    or  %l3, 0x100, %l3
    st  %l4, [ %l3 ]
    b laureti
    nop
laureti:
head_init:

	mov	0xfe0, %l0
	wr	%l0, %g0, %psr
	nop; nop; nop

	set	trapbase, %l0
	mov	%l0, %tbr

	mov	0x2, %l0	/*CWP is decremented on save, mark last window*/
	mov	%l0, %wim

	flush

	/* Setup end_of_mem only on boot CPU */
	sethi	%hi(first_cpu_booted), %l0
	ld	[%l0 + %lo(first_cpu_booted)], %l0
	cmp	%l0, 0
	bne	1f
	 nop

	/* Stack pointer is already set by bootloader, it tells us where
	 * end of memory is.
	 */
	sethi	%hi(end_of_mem), %l0
	st	%sp, [%l0 + %lo(end_of_mem)]

1:
	/* Setup Stack to end of memory, on a 16-byte boundary */
	sub	%sp, 4, %fp
	andn	%fp, 0x0f, %fp
	sub	%fp, 0x60, %sp

	/* Clear boot BSS */
	set	_boot_bss_start, %o0
	set	_boot_bss_end, %o1
	call	boot_clr_region
	 nop

   // laur
   // mov 0x46, %l4
   // b laur
   //laureti:

	/* Call boot code (o0,o1 initialized by bootloader)*/
	call	boot_main
	 nop

	/* Change to stack in virtual address space, on a 16-byte boundary */
	set	_prom_end, %fp
	sub	%fp, 4, %fp
	andn	%fp, 0x0f, %fp
	sub	%fp, 0x60, %sp

	/* Return code determine if we go straight to kernel (0) or to run
	 * STARTUP/PROM code (1)
	 *
	 * The startup_main function is located in virtual address
	 * space (0xFFD.....)
	 */

	cmp	%o0, 0
	beq	kernel_start
	 nop

	set	_promstartup_move_start_va, %g1
	call	%g1
	 nop

kernel_start:

	call	boot_kernel
	 nop

	/* This should not happend... we stop */
head_init_fail:
	ba	head_init_fail
	 nop
/*
 * We need to provide note.GNU-stack section, saying that we want
 * NOT executable stack. Otherwise the final linking will assume that
 * the ELF stack should not be restricted at all and set it RWX.
 */
.section .note.GNU-stack,"",@progbits
