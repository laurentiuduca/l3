/* AMBA definitions resident in PROM
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

char *prop_str_name = "name";
char *prop_str_vendor = "vendor";
char *prop_str_device = "device";
char *prop_str_version = "version";
char *prop_str_devmsk = "devmsk";
char *prop_str_interrupts = "interrupts";
char *prop_str_reg = "reg";
char *prop_str_userdef = "userdef";
char *prop_str_freq = "freq";
char *prop_str_index = "index";
char *prop_str_ampopts = "ampopts";
char *prop_str_description = "description";

/* Linux specific AMBA Property Names */
char *prop_str_compatible = "compatible";
char *prop_str_clockfrequency = "clock-frequency";

#ifdef CONFIG_WATCHDOG
/* Linux specific watchdog property names */
char *prop_str_watchdog = "watchdog";
char *prop_str_timeout_sec = "timeout-sec";
char *prop_str_hw_timeout_ms = "gaisler,max-hw-timeout-ms";
#endif

/* Linux common clock framework properties */

/* Clock producer properties */
char *prop_str_clock_cells = "#clock-cells";
char *prop_str_clock_output_names = "clock-output-names";
char *prop_str_protected_clocks = "protected-clocks";
char *prop_str_clock_indices = "clock-indices";

/* Clock consumer properties: */
char *prop_str_clocks = "clocks";
char *prop_str_clock_names = "clock-names";

#ifdef CONFIG_HRTIMERS
char *prop_str_cs_tmr_idx = "gaisler,cs-timer-idx";
char *prop_str_tmr_offset = "gaisler,timer-offset";
char *prop_str_tmr_len = "gaisler,timer-len";
char *prop_str_cpu_offset = "gaisler,cpu-offset";
char *prop_str_compatible_timer = "gaisler,gptimer-timer";
char *prop_str_compatible_timer_cs = "gaisler,gptimer-timer-cs";
char *prop_str_compatible_timer_ce = "gaisler,gptimer-timer-ce";
#endif

#ifdef CONFIG_CUSTOM_NODES
/* Example for how to create a custom AMBA node tree. This was tested on a
 * GR712 Evaluation Board at 80MHz.
 */

#include <prom_no.h>

/* APBUART */
int apbuart_vendor = 1;
int apbuart_device = 12;
unsigned long apbuart_regs[2] = {0x80000100, 0};
int apbuart_index = 1;
unsigned long apbuart_freq = 80000000;
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

/* irqmp */
int irqmp_vendor = 1;
int irqmp_device = 13;
unsigned long irqmp_regs[2] = {0x80000200, 0};
int irqmp_index = 2;
unsigned long irqmp_freq = 80000000;

struct propa_ptr props_irqmp0[] = {
	PROPA_PTR("name", "GAISLER_IRQMP", 14),
	PROPA_PTR("vendor", &irqmp_vendor, 4),
	PROPA_PTR("device", &irqmp_device, 4),
	PROPA_PTR("reg", &irqmp_regs[0], 8),
	PROPA_PTR("freq", &irqmp_freq, 4),
	PROPA_PTR_END("index", &irqmp_index, 4),
};

/* gptimer */
int gptimer_vendor = 1;
int gptimer_device = 17;
unsigned long gptimer_regs[2] = {0x80000300, 0};
int gptimer_index = 3;
unsigned long gptimer_freq = 80000000;
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

}

#endif /* CONFIG_CUSTOM_NODES */
