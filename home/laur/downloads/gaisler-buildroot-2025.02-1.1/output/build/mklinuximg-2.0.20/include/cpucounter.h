/* LEON CPU Counter declarations
 *
 * (C) Copyright 2024 Frontgrade Gaisler
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#ifndef __CPUCOUNTER_H__
#define __CPUCOUNTER_H__

#include <config.h>

/* TODO: Confirm what to use as default for the width */
#define LEON_UP_COUNTER_MIN_WIDTH (31)

#ifdef CONFIG_PROBE_LEON_UPCOUNTER

static inline unsigned int leon_read_up_counter_low(void)
{
	unsigned int asr23;

	__asm__ volatile (
		"mov %%asr23, %0"
		: "=&r" (asr23)
	);

	return asr23;
}

static inline unsigned int leon_read_up_counter_high(void)
{
	unsigned int asr22;

	__asm__ volatile (
		"mov %%asr22, %0"
		: "=&r" (asr22)
	);

	return asr22;
}

static inline void leon_enable_up_counter(void)
{
	/* Clear bit 31 in asr22 to enable the upcounter. */
	__asm__ volatile (
		"mov %g0, %asr22"
	);
}

static inline unsigned long long leon_read_up_counter(void)
{
	unsigned int low = leon_read_up_counter_low();
	unsigned int high = leon_read_up_counter_high();
	return low | ((unsigned long long)high << 32);
}

#define LEON_UP_COUNTER_AVAIL() \
	(leon_read_up_counter() != leon_read_up_counter())

#define LEON_UP_COUNTER_INIT() leon_enable_up_counter()
#else
#define LEON_UP_COUNTER_AVAIL() (0)
#define LEON_UP_COUNTER_INIT()
#endif
#endif
