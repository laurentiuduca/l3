/* GRLIB GPTIMER definitions
 *
 * (C) Copyright 2011 Frontgrade Gaisler
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

#ifndef __GPTIMER_H__
#define __GPTIMER_H__

#include <stdint.h>

/* Bit 0-2 mask for reading # timers from the conf register */
#define GPTIMER_CONF_TIMERS_MASK 0x7
/* Bit 8 indicates if the timer unit generates separate interrupts */
#define GPTIMER_CONF_SEP_IRQ (1 << 8)

struct gptimer_timer {
	uint32_t val;
	uint32_t rld;
	uint32_t ctrl;
	uint32_t unused;
};

struct gptimer_regs {
	uint32_t scaler;
	uint32_t scaler_reload;
	uint32_t config;
	uint32_t unused;
	struct gptimer_timer timer[7];
};

extern struct gptimer_regs *gptimer;
extern int gptimer_inited;
#endif
