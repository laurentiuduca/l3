/* Exported functionality from PROM sections
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

#ifndef __PROM_H__
#define __PROM_H__

#include <common.h>
#include <linux.h>

extern struct common_data common;
extern struct linux_romvec romvector;
extern unsigned long cpu_freq_khz;
extern unsigned long amba_systemid;

/* AMBA Property Names */
extern char *prop_str_name;
extern char *prop_str_vendor;
extern char *prop_str_device;
extern char *prop_str_version;
extern char *prop_str_devmsk;
extern char *prop_str_interrupts;
extern char *prop_str_reg;
extern char *prop_str_userdef;
extern char *prop_str_freq;
extern char *prop_str_index;
extern char *prop_str_ampopts;
extern char *prop_str_description;

/* Linux specific AMBA Property Names */
extern char *prop_str_compatible;
extern char *prop_str_clockfrequency;

#ifdef CONFIG_WATCHDOG
/* Linux specific watchdog property names */
extern char *prop_str_watchdog;
extern char *prop_str_timeout_sec;
extern char *prop_str_hw_timeout_ms;
#endif

/* Linux common clock framework properties */

/* Clock producer properties: */
extern char *prop_str_clock_cells;
extern char *prop_str_clock_output_names;
extern char *prop_str_protected_clocks;
extern char *prop_str_clock_indices;

/* Clock consumer properties */
extern char *prop_str_clocks;
extern char *prop_str_clock_names;

#ifdef CONFIG_HRTIMERS
extern char *prop_str_cs_tmr_idx;
extern char *prop_str_tmr_offset;
extern char *prop_str_tmr_len;
extern char *prop_str_cpu_offset;
extern char *prop_str_compatible_timer;
extern char *prop_str_compatible_timer_cs;
extern char *prop_str_compatible_timer_ce;

extern unsigned long node_gptimer0_clk;
#endif

extern void opprom_init(void);
extern void *prom_malloc(int size);
extern void *prom_calloc(int size);
extern char *prom_strdup(char *str);
struct node *opprom_get_ambapp0(void);

#define PROM_STRDUP_ONCE(var, string)			\
	do {						\
		if (!var)				\
			var = prom_strdup(string);	\
	} while(0)					\

#define promstdup

/* send one character to UART, blocking until done. For debugging.
 * Works only after AMBA has been scanned or if CONFIG_DEBUG_UART has been
 * set correcty.
 */
extern void outbyte(char c);
extern void outstr(char *str);

/* avoid UMUL/DIV reference (all hardware does not have those instructions) */
extern unsigned int int_div(unsigned int v, unsigned int d);
extern unsigned int int_mul(unsigned int v, unsigned int m);

/* String operations */
extern void _memcpy(char *d, char *s, int len);
extern void _strcpy(char *d, char *s);
extern void _memset(void *d, int v, int len);
extern int _strcmp(char *d, char *s);
extern int _strnlen(const char *s, int count);

#ifdef CONFIG_DEBUG

#define DBG_PRINTF0(fmt) \
	_printf(fmt, 0, 0, 0, 0, 0, 0, 0)

#define DBG_PRINTF1(fmt, a0) \
	_printf(fmt, (int)(a0), 0, 0, 0, 0, 0, 0)

#define DBG_PRINTF2(fmt, a0, a1) \
	_printf(fmt, (int)(a0), (int)(a1), 0, 0, 0, 0, 0)

#define DBG_PRINTF3(fmt, a0, a1, a2) \
	_printf(fmt, (int)(a0), (int)(a1), (int)(a2), 0, 0, 0, 0)

#define DBG_PRINTF4(fmt, a0, a1, a2, a3) \
	_printf(fmt, (int)(a0), (int)(a1), (int)(a2), (int)(a3), 0, 0, 0)

#define DBG_PRINTF5(fmt, a0, a1, a2, a3, a4) \
	_printf(fmt, (int)(a0), (int)(a1), (int)(a2), (int)(a3), (int)(a4), \
		0, 0)

#define DBG_PRINTF6(fmt, a0, a1, a2, a3, a4, a5) \
	_printf(fmt, (int)(a0), (int)(a1), (int)(a2), (int)(a3), (int)(a4), \
		(int)(a5), 0)

#define DBG_PRINTF7(fmt, a0, a1, a2, a3, a4, a5, a6) \
	_printf(fmt, (int)(a0), (int)(a1), (int)(a2), (int)(a3), (int)(a4), \
		(int)(a5), (int)(a6))


extern int _printf(const char *fmt, int a0, int a1, int a2, int a3, int a4,
		   int a5, int a6);

#else /* !CONFIG_DEBUG */

#define DBG_PRINTF0(fmt)
#define DBG_PRINTF1(fmt, a0)
#define DBG_PRINTF2(fmt, a0, a1)
#define DBG_PRINTF3(fmt, a0, a1, a2)
#define DBG_PRINTF4(fmt, a0, a1, a2, a3)
#define DBG_PRINTF5(fmt, a0, a1, a2, a3, a4)
#define DBG_PRINTF6(fmt, a0, a1, a2, a3, a4, a5)
#define DBG_PRINTF7(fmt, a0, a1, a2, a3, a4, a5, a6)

#endif /* CONFIG_DEBUG */

struct systemid_banners {
	unsigned long systemid;
	char *banner;
};

#endif
