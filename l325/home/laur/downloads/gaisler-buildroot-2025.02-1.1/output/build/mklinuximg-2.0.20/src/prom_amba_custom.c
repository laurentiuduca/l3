/* Example for how to create a custom AMBA node tree. This was tested on a
 * GR712 Evaluation Board at 80MHz.
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

#include <common.h>

#ifdef CONFIG_CUSTOM_NODES

#define SYSTEM_FREQ_HZ 80000000

#include <prom_no.h>
#include <apbuart.h>
#include <irqmp.h>
#include <gptimer.h>

/* APBUART */
int apbuart_vendor = 1;
int apbuart_device = 12;
unsigned long apbuart_regs[2] = {0x80000100, 0};
int apbuart_index = 1;
unsigned long apbuart_freq = SYSTEM_FREQ_HZ;
int apbuart_interrupts = 2;

struct propa_ptr props_apbuart0[] = {
	PROPA_PTR("name", "GAISLER_APBUART", 16),
	PROPA_PTR("vendor", &apbuart_vendor, 4),
	PROPA_PTR("device", &apbuart_device, 4),
	PROPA_PTR("interrupts", &apbuart_interrupts, 4),
	PROPA_PTR("reg", &apbuart_regs[0], 8),
	PROPA_PTR("freq", &apbuart_freq, 4),
	PROPA_PTR_END("index", &apbuart_index, 4),
};

/* IRQMP */
int irqmp_vendor = 1;
int irqmp_device = 13;
unsigned long irqmp_regs[2] = {0x80000200, 0};
int irqmp_index = 2;
unsigned long irqmp_freq = SYSTEM_FREQ_HZ;

struct propa_ptr props_irqmp0[] = {
	PROPA_PTR("name", "GAISLER_IRQMP", 14),
	PROPA_PTR("vendor", &irqmp_vendor, 4),
	PROPA_PTR("device", &irqmp_device, 4),
	PROPA_PTR("reg", &irqmp_regs[0], 8),
	PROPA_PTR("freq", &irqmp_freq, 4),
	PROPA_PTR_END("index", &irqmp_index, 4),
};

/* GPTIMER */
int gptimer_vendor = 1;
int gptimer_device = 17;
unsigned long gptimer_regs[2] = {0x80000300, 0};
int gptimer_index = 3;
unsigned long gptimer_freq = SYSTEM_FREQ_HZ;
int gptimer_interrupts = 8;

struct propa_ptr props_gptimer0[] = {
	PROPA_PTR("name", "GAISLER_GPTIMER", 16),
	PROPA_PTR("vendor", &gptimer_vendor, 4),
	PROPA_PTR("device", &gptimer_device, 4),
	PROPA_PTR("interrupts", &gptimer_interrupts, 4),
	PROPA_PTR("reg", &gptimer_regs[0], 8),
	PROPA_PTR("freq", &gptimer_freq, 4),
	PROPA_PTR_END("index", &gptimer_index, 4),
};

#define PROP struct prop *

struct node nodes_amba[] = {
	{NULL, &nodes_amba[1], (PROP)props_gptimer0},	/* GPTIMER */
	{NULL, &nodes_amba[2], (PROP)props_apbuart0},	/* APBUART */
	{NULL, NULL, (PROP)props_irqmp0},	/* IRQMP */
};

struct node *pnodes_amba = &nodes_amba[0];

void ambapp_nodes_init(struct node *n)
{
	cpu_freq_khz = SYSTEM_FREQ_HZ / 1000;

	apbuart = (struct apbuart_regs *)0x80000100;
	irqmp = (struct irqmp_regs *)0x80000200;
	gptimer = (struct gptimer_regs *)0x80000300;
}

#endif /* CONFIG_CUSTOM_NODES */
