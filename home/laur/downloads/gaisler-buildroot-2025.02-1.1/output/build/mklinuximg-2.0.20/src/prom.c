/* The LEON OpenBoot implementation for Linux
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

#include <config.h>
#include <common.h>
#include <prom.h>
#include <prom_no.h>
#include <ambapp.h>
#include <apbuart.h>
#include <gptimer.h>
#include <irqmp.h>
#include <linkscript.h>
#ifdef CONFIG_WATCHDOG
#include <prom_watchdog.h>
#endif
#ifdef CONFIG_HRTIMERS
#include <cpucounter.h>
#endif

static void leon_reboot(char *bcommand);
static void leon_halt(void);
static void leon_v0_feval(int len, char *str);
static int leon_nbgetchar(void);
static int leon_nbputchar(int c);

/* Common structure accessed by PROM/STARTUP/PROM code
 * Part of the DATA section, so should not be accessed until initialized
 * by startup/prom boot code.
 */
struct common_data __attribute__((section(".prom_common_struct"))) common = {
	0,
	/* ... */
};

/* Hardware Registers */
struct irqmp_regs *irqmp = (struct irqmp_regs *)CONFIG_DEBUG_IRQMP;
struct gptimer_regs *gptimer = (struct gptimer_regs *)CONFIG_DEBUG_GPTIMER;
struct apbuart_regs *apbuart = (struct apbuart_regs *)CONFIG_DEBUG_APBUART;

/* NULL Pointer */
void *null_ptr = NULL;
#define NULL_PTR (null_ptr)
#define NULL_PPTR ((void *)&null_ptr)

/* Total Phyiscal memory (physical addresses) */
struct linux_mlist_v0 totphys = {
	NULL,
	CONFIG_RAM_START,
	0			/* Set by start code */
};
struct linux_mlist_v0 *totphys_p = &totphys;

/* Available memory to Linux (physical addresses) */
struct linux_mlist_v0 avail = {
	NULL,
	CONFIG_RAM_START,
	0			/* Set by start code */
};
struct linux_mlist_v0 *avail_p = &avail;

/*
 * This buffer is maxed out so that there is room for cmdline to be
 * modified after load but before starting the image.
 *
 * Note that the COMMAND_LINE_SIZE constant in an unmodified kernel
 * caps the maximum useable size to 256 for sparc32 regardless of the
 * size of what mklinuximg provides.
 */
#define CMDLINE_SIZE 256
const char bootargs_cmdline[CMDLINE_SIZE] = CONFIG_LINUX_CMDLINE;

/* Linux Arguments */
struct linux_arguments_v0 bootargs = {
	{NULL,
	 (char * const)bootargs_cmdline, /* Kernel Command Line */
	 NULL
	 /*... */
	},
};

struct linux_arguments_v0 *bootargs_p = &bootargs;

/* Node operations */
struct linux_nodeops nodeops = {
	no_nextnode,
	no_child,
	no_proplen,
	no_getprop,
	no_setprop,
	no_nextprop
};

/* Linux Argument - The ROM Vector, every PROM thing is found through here */
struct linux_romvec romvector = {
	0,
	0,			/* sun4c v0 prom */
	0, 0,
	{&totphys_p, NULL_PPTR, &avail_p},
	&nodeops,
	NULL,
	{NULL /* ... */ }, /* pv_v0devops */
	"\001", /* PROMDEV_TTYA */
	"\001", /* PROMDEV_TTYA */
	NULL, NULL,		/* pv_getchar, pv_putchar */
	leon_nbgetchar,
	leon_nbputchar,
	NULL,
	leon_reboot,
	NULL,
	NULL,
	NULL,
	leon_halt,
	NULL_PPTR,
	{leon_v0_feval},
	&bootargs_p,
	/*... */
};

/* SUN SPARC IDPROM */
struct idprom idoprom = {
	0x01,	/* format */
	M_LEON | M_LEON3_SOC,	/* machine type */
#define ETHMACPART(v, s) (((v) >> s) & 0xff)
	{ETHMACPART(CONFIG_ETHMAC, 40),
	 ETHMACPART(CONFIG_ETHMAC, 32),
	 ETHMACPART(CONFIG_ETHMAC, 24),
	 ETHMACPART(CONFIG_ETHMAC, 16),
	 ETHMACPART(CONFIG_ETHMAC, 8),
	 ETHMACPART(CONFIG_ETHMAC, 0)},	/* eth */
	0,	/* date */
	0,	/* sernum */
	0,	/* checksum */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/* reserved */
};

/*
 * Default same as what Linux prints without banner name property,,
 * but with capitalized LEON.
 */
char prom_banner_name[64];

struct propa_ptr props_root[] = {
	PROPA_PTR("device_type", "idprom", 7),
	PROPA_PTR("idprom", &idoprom, sizeof(struct idprom)),
	PROPA_PTR("compatible", "leon", 5),
	PROPA_PTR("name", "idprom", 7),
	PROPA_PTR("banner-name", prom_banner_name, sizeof(prom_banner_name)),
	PROPA_PTR_END("compatability", "leon", 5),
};

unsigned long cpu_nctx = 256;
#ifdef CONFIG_BOOTLOADER_FREQ
unsigned long cpu_freq_khz = CONFIG_BOOTLOADER_FREQ / 1000;
#else
unsigned long cpu_freq_khz; /* autodetected by GPTIMER driver */
#endif
unsigned long uart1_baud = 38400;
unsigned long uart2_baud = 38400;
int mids[8] = {0, 1, 2, 3, 4, 5, 6, 7};

#ifdef HASXML
#include HASXML
#endif

struct propa_ptr props_cpu0[] = {
	PROPA_PTR("device_type", "cpu", 4),
	PROPA_PTR("name", "cpu0", 5),
	PROPA_PTR("mid", &mids[0], 4),
	PROPA_PTR("mmu-nctx", &cpu_nctx, 4),
	PROPA_PTR("clock-frequency", &cpu_freq_khz, 4),
	PROPA_PTR("uart1_baud", &uart1_baud, 4),
	PROPA_PTR_END("uart2_baud", &uart2_baud, 4),
};

struct propa_ptr props_serial[] = {
	PROPA_PTR("device_type", "serial", 7),
	PROPA_PTR_END("name", "a:", 3),
};

unsigned long amba_systemid; /* Set by ambapp_init */
unsigned long amba_ioarea = CONFIG_AMBA_IO_AREA;
unsigned long ipi_num = CONFIG_IPI_NUM;

struct propa_ptr props_ambapp[] = {
	PROPA_PTR("device_type", "ambapp", 7),
	PROPA_PTR("name", "ambapp0", 8),
	PROPA_PTR("systemid", &amba_systemid, 4),
	PROPA_PTR("ioarea", &amba_ioarea, 4),
	PROPA_PTR_END("ipi_num", &ipi_num, 4),
};

struct propa_ptr props_cpu1[] = {
	PROPA_PTR("device_type", "cpu", 4),
	PROPA_PTR("name", "cpu1", 5),
	PROPA_PTR("mid", &mids[1], 4),
	PROPA_PTR("mmu-nctx", &cpu_nctx, 4),
	PROPA_PTR_END("clock-frequency", &cpu_freq_khz, 4),
};

struct propa_ptr props_cpu2[] = {
	PROPA_PTR("device_type", "cpu", 4),
	PROPA_PTR("name", "cpu2", 5),
	PROPA_PTR("mid", &mids[2], 4),
	PROPA_PTR("mmu-nctx", &cpu_nctx, 4),
	PROPA_PTR_END("clock-frequency", &cpu_freq_khz, 4),
};

struct propa_ptr props_cpu3[] = {
	PROPA_PTR("device_type", "cpu", 4),
	PROPA_PTR("name", "cpu3", 5),
	PROPA_PTR("mid", &mids[3], 4),
	PROPA_PTR("mmu-nctx", &cpu_nctx, 4),
	PROPA_PTR_END("clock-frequency", &cpu_freq_khz, 4),
};

struct propa_ptr props_cpu4[] = {
	PROPA_PTR("device_type", "cpu", 4),
	PROPA_PTR("name", "cpu4", 5),
	PROPA_PTR("mid", &mids[4], 4),
	PROPA_PTR("mmu-nctx", &cpu_nctx, 4),
	PROPA_PTR_END("clock-frequency", &cpu_freq_khz, 4),
};

struct propa_ptr props_cpu5[] = {
	PROPA_PTR("device_type", "cpu", 4),
	PROPA_PTR("name", "cpu5", 5),
	PROPA_PTR("mid", &mids[5], 4),
	PROPA_PTR("mmu-nctx", &cpu_nctx, 4),
	PROPA_PTR_END("clock-frequency", &cpu_freq_khz, 4),
};

struct propa_ptr props_cpu6[] = {
	PROPA_PTR("device_type", "cpu", 4),
	PROPA_PTR("name", "cpu6", 5),
	PROPA_PTR("mid", &mids[6], 4),
	PROPA_PTR("mmu-nctx", &cpu_nctx, 4),
	PROPA_PTR_END("clock-frequency", &cpu_freq_khz, 4),
};

struct propa_ptr props_cpu7[] = {
	PROPA_PTR("device_type", "cpu", 4),
	PROPA_PTR("name", "cpu7", 5),
	PROPA_PTR("mid", &mids[7], 4),
	PROPA_PTR("mmu-nctx", &cpu_nctx, 4),
	PROPA_PTR_END("clock-frequency", &cpu_freq_khz, 4),
};

#define PROP struct prop *

/* Nodes at Level 1 in node tree. The initialization code alters this table
 * so the order of the nodes must not be changed.
 *
 * Depending on how many CPUs are available in the system the list is shortened
 * by clearing the node->sibling pointer.
 */
struct node nodes_lvl2[] = {
	{NULL, &nodes_lvl2[1], (PROP)props_cpu0},	/* CPU[0] */
	{NULL, &nodes_lvl2[2], (PROP)props_serial},	/* SERIAL */
	{NULL, &nodes_lvl2[3], (PROP)props_ambapp},	/* AMBA Plug&Play */
	{NULL, &nodes_lvl2[4], (PROP)props_cpu1},	/* CPU[1] (optional) */
	{NULL, &nodes_lvl2[5], (PROP)props_cpu2},	/* CPU[2] (optional) */
	{NULL, &nodes_lvl2[6], (PROP)props_cpu3},	/* CPU[3] (optional) */
	{NULL, &nodes_lvl2[7], (PROP)props_cpu4},	/* CPU[4] (optional) */
	{NULL, &nodes_lvl2[8], (PROP)props_cpu5},	/* CPU[5] (optional) */
	{NULL, &nodes_lvl2[9], (PROP)props_cpu6},	/* CPU[6] (optional) */
	{NULL, NULL, (PROP)props_cpu7},			/* CPU[7] (optional) */
};
#define NODE_AMBAPP_IDX 2
#define NODE_CPU1_IDX 3

struct node nodes_lvl1[] = {
	{&nodes_lvl2[0], NULL, (PROP)props_root},	/* ROOT NODE */
};

/* PROM Root Node */
struct node *node_root = &nodes_lvl1[0];

static unsigned long prom_malloc_end = (unsigned long)&_prom_heap_end;

/* Grow Downwards malloc, no free routine, memory resident after Linux boot,
 * minimum aligned to 4bytes
 */
void *prom_malloc(int size)
{
	size = (size + 3) & ~3;
	if ((prom_malloc_end - size) < (unsigned long)&_prom_heap_start) {
		/*
		 * ERROR: Resize prom heap size with -promheap option that
		 * sets up CONFIG_PROM_HEAP_SIZE.
		 */
		while (1)
			;
	}
	prom_malloc_end -= size;
	return (void *)prom_malloc_end;
}

void *prom_calloc(int size)
{
	void *r = prom_malloc(size);
	_memset(r, 0, size);
	return r;
}

char *prom_strdup(char *str)
{
	int len = _strnlen(str, 127);
	char *dst = prom_malloc(len + 1);
	_memcpy(dst, str, len);
	dst[len] = '\0';
	return dst;
}

static void leon_reboot(char *bcommand)
{
#ifdef CONFIG_WATCHDOG
	outstr("PROM: Reboot\n");
	watchdog_set_timeout(10);
#else
	outstr("PROM: Reboot  (not implemented)\n");

#endif
	while (1)
		;
}

static void leon_halt(void)
{
	outstr("PROM: Halt  (not implemented)\n");

	while (1)
		;
}

static void leon_v0_feval(int len, char *str)
{
	if (_strcmp("reset", str) == 0) {
		leon_reboot(NULL);
	}
}

/* get single char, don't care for blocking */
static int leon_nbgetchar(void)
{
	return -1;
}

/* From Linux kernel 2.6.37 (23bcbf1b63350ed529f7dfb8a5c459e6e0c1a3ca) the
 * return argument interpretation has changed, however fixing this would
 * loose the backward compatability so we still need the blocking write
 * function even though it should be non-blocking.
 */
#if CONFIG_LINUX_VERSION_CODE <= KERNEL_VER(2, 6, 36)
static int leon_nbputchar(int c)
{
	int timeout = 100000, ctrl;
	struct apbuart_regs *b = apbuart;

	if (b == NULL)
		return 0;

	ctrl = BYPASS_LOAD_PA(&b->ctrl);
	BYPASS_STORE_PA(&b->ctrl, ctrl | APBUART_CTRL_TE);
	while (timeout
	       && (BYPASS_LOAD_PA(&b->status) & APBUART_STATUS_THE) == 0)
		timeout--;
	if (timeout)
		BYPASS_STORE_PA(&b->data, c & 0xff);
	BYPASS_STORE_PA(&b->ctrl, ctrl);
	return 0;
}
#else
/* for kernels 2.6.37* and onwards */
static int leon_nbputchar(int c)
{
	int ctrl;
	struct apbuart_regs *b = apbuart;

	if (b == NULL)
		return 1;

	ctrl = BYPASS_LOAD_PA(&b->ctrl);
	BYPASS_STORE_PA(&b->ctrl, ctrl | APBUART_CTRL_TE);

	if ((BYPASS_LOAD_PA(&b->status) & APBUART_STATUS_THE) == 0) {
		BYPASS_STORE_PA(&b->ctrl, ctrl);
		return 0; /* comeback again */
	}

	BYPASS_STORE_PA(&b->data, c & 0xff);
	BYPASS_STORE_PA(&b->ctrl, ctrl);

#ifdef CONFIG_WATCHDOG
	watchdog_ping(PING_ID_UART);
#endif

	return 1; /* One char transmitted */
}
#endif

/* send one character to UART, blocking until done. For debugging */
void outbyte(char c)
{
#if CONFIG_LINUX_VERSION_CODE > KERNEL_VER(2, 6, 36)
	while (leon_nbputchar(c) == 0)
		;
#else
	leon_nbputchar(c);
#endif
}

void outstr(char *str)
{
	while (*str != '\0') {
		outbyte(*str);
		if (*str == '\n')
			outbyte('\r');
		str++;
	}
}

struct node *opprom_get_ambapp0(void) 
{
	return &nodes_lvl2[NODE_AMBAPP_IDX];
}

static struct systemid_banners sysid_banners[] = {
	{0x07120e70, "GR712RC LEON3 System-on-a-Chip"},
	{0x07401038, "GR740 rev0 LEON4 System-on-a-Chip"},
	{0x07401039, "GR740 rev1 LEON4 System-on-a-Chip"},
	{0x0699100e, "UT699E/UT700 LEON3 System-on-a-Chip"}
};
#define NUM_SYSID_BANNERS (sizeof(sysid_banners) / sizeof(sysid_banners[0]))

static void opprom_setup_banner_name(void)
{
	int known_systemid = 0;
	int i;

	/* Check for known systemids */
	for (i = 0; i < NUM_SYSID_BANNERS; i++) {
		if (sysid_banners[i].systemid == amba_systemid) {
			_strcpy(prom_banner_name, sysid_banners[i].banner);
			known_systemid = 1;
		}
	}

	/* Otherwise base banner on PSR version field*/
	if (!known_systemid) {
		int cpuver;
		int psr;

		__asm__ __volatile__ ("mov %%psr, %0"
				      : "=r"(psr));
		cpuver = (psr >> 24) & 0xf;
		if (cpuver == 5) {
			_strcpy(prom_banner_name, "LEON5 System-on-a-Chip");
		} else {
			_strcpy(prom_banner_name, "LEON3 System-on-a-Chip");
		}
	}
}

#ifdef HASXML

static void xml_add_root_subnodes(void)
{
	struct node *root = node_root;
	struct xml_match *m;
	struct xml_node *xn;
	int mi;

	for(mi = 0; mi < xml_matchcount; mi++) {
		m = &xml_matches[mi];
		if (!(m->flags & XMLMATCH_ROOT))
			continue;

		/* Add the node as a child to root */
		xn = &xml_nodes[m->placeholder];
		node_add_child(root, xn->node.child);
	}
}

#else

static void xml_add_root_subnodes(void) {
	;
}

#endif /* HASXML */

#ifdef CONFIG_HRTIMERS

extern int hrtimers_res_ok(void);

/* Common clock related: */
unsigned long clock_cells = 0;

/* System clock related: */
unsigned long node_sysclk = -1;
unsigned long leon_freq_hz = 0;

struct propa_ptr props_sysclk[] = {
       PROPA_PTR("name", "sysclk", 7),
       PROPA_PTR("compatible", "fixed-clock", 12),
       PROPA_PTR("clock-frequency", &leon_freq_hz, 4),
       PROPA_PTR("#clock-cells", &clock_cells, 4),
       PROPA_PTR_END("clock-output-names", "sysclk", 7),
};

/* GPTIMER0 clock properties: */
unsigned long node_gptimer0_clk = -1;
unsigned long gptimer0_clk_div = 1;
unsigned long gptimer0_clk_mult = 1;

struct propa_ptr props_gptimer0_clk[] = {
       PROPA_PTR("name", "gptimer0_clk", 13),
       PROPA_PTR("compatible", "fixed-factor-clock", 19),
       PROPA_PTR("clock-mult", &gptimer0_clk_mult, 4),
       PROPA_PTR("clock-div", &gptimer0_clk_div, 4),
       PROPA_PTR("#clock-cells", &clock_cells, 4),
       PROPA_PTR_END("clocks", &node_sysclk, 4),
};

/* LEON up-counter properties: */
unsigned long upcounter_width = LEON_UP_COUNTER_MIN_WIDTH;

struct propa_ptr props_leon_cs[] = {
       PROPA_PTR("name", "clksrc_leon", 12),
       PROPA_PTR("gaisler,upcounter-width", &upcounter_width, 4),
       PROPA_PTR("clocks", &node_sysclk, 4),
       PROPA_PTR_END("compatible", "gaisler,leon-cs", 16),
};

struct node nodes_lvl2_hrtimer[] = {
	{NULL, &nodes_lvl2_hrtimer[1], (PROP)props_sysclk},
	{NULL, &nodes_lvl2_hrtimer[2], (PROP)props_gptimer0_clk},
	{NULL, NULL, (PROP)props_leon_cs},
};

#define NODE_SYSCLK_IDX 0
#define NODE_GPTIMER0_CLK_IDX 1

static inline int leon_up_counter_node_init(void)
{
	if (!LEON_UP_COUNTER_AVAIL())
		return -1;

	/* Unfortunately its not possible to probe the width of the upcounter */
	if (amba_systemid_is_gr740())
		upcounter_width = 56;

	return 0;
}

static inline void clocks_sysclk_node_init(void)
{
	leon_freq_hz = int_mul(cpu_freq_khz, 1000);
	node_sysclk = (unsigned long)&nodes_lvl2_hrtimer[0];
}

static inline int clocks_gptimer0_clk_node_init(void)
{
	unsigned int scaler;

	if (!gptimer_inited)
		return -1;

	scaler = BYPASS_LOAD_PA(&gptimer->scaler_reload);

	gptimer0_clk_div = scaler + 1;

	node_gptimer0_clk = (unsigned long)&nodes_lvl2_hrtimer[1];

	return 0;
}

void hrtimers_setup_clk_nodes(void)
{
	int ret;

	/* Skip clocks if the timer allocation failed */
	if (!hrtimers_res_ok())
		return;

	/* Setup the system clock node */
	clocks_sysclk_node_init();

	/* Setup the LEON up-counter clocksource */
	ret = leon_up_counter_node_init();
	if (ret) {
		/* up-counter not available, unlink it. */
		nodes_lvl2_hrtimer[NODE_GPTIMER0_CLK_IDX].sibling = NULL;
	}

	/* Setup the GPTIMER0 clock */
	ret = clocks_gptimer0_clk_node_init();
	if (ret) {
		/* No gptimer0 available, update sysclock's sibling. */
		nodes_lvl2_hrtimer[NODE_SYSCLK_IDX].sibling =
			nodes_lvl2_hrtimer[NODE_GPTIMER0_CLK_IDX].sibling;
	}

	/* Add the nodes to root */
	node_add_child(node_root, nodes_lvl2_hrtimer);
}
#endif
/* PROM Initialization routine, called from STARTUP when previous stages has
 * been successfully initialized
 */
void opprom_init(void)
{
	int ncpu;
	unsigned char *ptr, cksum;
	int i;

	/* Auto Detect Memory Size available for Linux */
	avail.num_bytes = common.end_of_mem_linux_pa - CONFIG_RAM_START;
	totphys.num_bytes = avail.num_bytes;

        ncpu = irqmp_numcpus();

#if (CONFIG_WAKE_CPU_MASK != 0)
	/* Wake other (disabled) CPUs if user has requested it */
	BYPASS_STORE_PA(&irqmp->mpstatus, CONFIG_WAKE_CPU_MASK);
#endif
	nodes_lvl2[NODE_CPU1_IDX - 1 + (ncpu - 1)].sibling = NULL;

#ifdef CONFIG_CUSTOM_NODES
	/* Link in AMBA nodes into node tree,
	 * startup_ambapp.c insert childs using opprom_get_ambapp0() */
	nodes_lvl2[NODE_AMBAPP_IDX].child = pnodes_amba;
#endif

	xml_add_root_subnodes();

#ifdef CONFIG_HRTIMERS
	hrtimers_setup_clk_nodes();
#endif
	opprom_setup_banner_name();

	/* Calculate IDPROM Checksum */
	ptr = (unsigned char *)&idoprom;
	for (i = cksum = 0; i <= 0x0E; i++)
		cksum ^= *ptr++;
	idoprom.id_cksum = cksum;
}
