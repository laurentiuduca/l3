/* GRLIB IOMMU definitions
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

#ifndef __IOMMU_H__
#define __IOMMU_H__

#include <stdint.h>

struct iommu_regs {
	uint32_t cap0;
	uint32_t cap1;
	uint32_t cap2;
	uint32_t res0;
	uint32_t ctrl;/*0x10*/
	uint32_t flush;
	uint32_t status;
	uint32_t irqmask;
	uint32_t ahbfail; /*0x20*/
	uint32_t res1[3+4];
	uint32_t mstconf[16]; /*0x40*/
	uint32_t grpctrl[16]; /*0x80*/
	uint32_t diagcache; /*0xc0*/
};

#endif
