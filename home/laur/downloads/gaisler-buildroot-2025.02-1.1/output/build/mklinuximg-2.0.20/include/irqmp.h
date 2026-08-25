/* GRLIB IRQMP definitions
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

#ifndef __IRQMP_H__
#define __IRQMP_H__

#include <stdint.h>

struct irqmp_regs {
	uint32_t ilevel;
	uint32_t ipend;
	uint32_t iforce;
	uint32_t iclear;
	uint32_t mpstatus;
	uint32_t mpbroadcast;
	uint32_t notused02;
	uint32_t notused03;
	uint32_t ampctrl;
	uint32_t icsel[2];
	uint32_t notused13;
	uint32_t notused20;
	uint32_t notused21;
	uint32_t notused22;
	uint32_t notused23;
	uint32_t mask[16];
	uint32_t force[16];

	uint32_t intid[16];
	uint32_t unused[(0x1000-0x100)/4];
};

#define IRQMP_STATUS_CPUNR 28

extern struct irqmp_regs *irqmp;

extern int irqmp_numcpus(void);

#endif
