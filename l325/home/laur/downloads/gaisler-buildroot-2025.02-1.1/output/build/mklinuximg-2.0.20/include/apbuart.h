/* GRLIB APBUART definitions
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

#ifndef __APBUART_H__
#define __APBUART_H__

#include <stdint.h>

struct apbuart_regs {
	uint32_t data;
	uint32_t status;
	uint32_t ctrl;
	uint32_t scaler;
};

#define APBUART_CTRL_TE		0x00000002
#define APBUART_STATUS_THE	0x00000004

extern struct apbuart_regs *apbuart;

#endif
