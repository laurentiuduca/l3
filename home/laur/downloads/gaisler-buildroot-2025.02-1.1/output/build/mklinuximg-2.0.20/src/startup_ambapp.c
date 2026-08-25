/* AMBA Plug&Play scanning and initialization
 */

#include <common.h>
#include <startup.h>
#include <prom.h>
#include <ambapp.h>
#include <ambapp_names.h>
#include <gptimer.h>
#include <irqmp.h>
#include <apbuart.h>
#include <iommu.h>
#include <grcg.h>
#include <gpreg.h>
#include <grwatchdog.h>

#ifdef CONFIG_WATCHDOG
#include <prom_watchdog.h>
#endif
#ifdef CONFIG_HRTIMERS
#include <cpucounter.h>
#endif

#define REMOVE_NODE 1

#define GR712RC_SYSTEMID 0x7120e70
#define GR740_REV0_SYSTEMID 0x07401038
#define GR740_REV1_SYSTEMID 0x07401039

#define GR740_DEBUG_UART 0xff900000

int amba_systemid_is_gr740(void)
{
	return (amba_systemid == GR740_REV0_SYSTEMID) ||
	       (amba_systemid == GR740_REV1_SYSTEMID);
}

#ifdef CONFIG_AMPOPTS
struct amp_opt {
	short idx, val;
} ampopts[] = {
#include "config_ampopts.h"
	{-1, -1} /* END */
};
#endif

void _memcpy4_pa(void *dst, void *src, int len);

ambapp_dev *ambapp_get_first_dev(ambapp_core *core)
{
	if (core->ahbmst)
		return (ambapp_dev *)core->ahbmst;
	else if (core->ahbslv)
		return (ambapp_dev *)core->ahbslv;
	else if (core->apbslv)
		return (ambapp_dev *)core->apbslv;
	else {
		/* this should not happen */
		while (1)
			;
	}
}

void leon_init(ambapp_core *core, int level)
{
	unsigned long leon_freq_hz;

	if (level != 1)
		return;

	/* Get frequency for the CPU */
	leon_freq_hz = ambapp_dev_freq((ambapp_dev *)core->ahbmst);
	cpu_freq_khz = int_div(leon_freq_hz, 1000);
}

static int irqmp_initied = 0;
static int eirqs[16];
static int irqmp_ncpus = 0;

static void irqmp_add_eirq(struct irqmp_regs *cregs)
{
	int eirq;
	int i;

	eirq = (BYPASS_LOAD_PA(&cregs->mpstatus) >> 16) & 0xf;
	for (i=0; eirq && i < 16; i++) {
		if (!eirqs[i]) {
			eirqs[i] = eirq;
			break;
		} else if (eirqs[i] == eirq) {
			break;
		}
	}
}

int irqmp_dev(ambapp_dev *dev)
{
	int c, icsel, nctrl, ncpu;
	ambapp_apbdev *apbslv;
	struct irqmp_regs *regs, *cregs;

	apbslv = (ambapp_apbdev *)dev;
	regs = (struct irqmp_regs *)apbslv->address;
	if (!irqmp_initied) {
		/* Only process the first IRQMP core */
		irqmp_initied = 1;
		irqmp = regs;
		/* Get Number of CPUs */
		#ifndef CONFIG_SMP
			ncpu = 1;
		#else
			ncpu = BYPASS_LOAD_PA(&irqmp->mpstatus);
			ncpu = ((ncpu >> IRQMP_STATUS_CPUNR) & 0xf) + 1;
			if (ncpu > CONFIG_MAX_CPUS)
				ncpu = CONFIG_MAX_CPUS;
		#endif
		irqmp_ncpus= ncpu;
	}

	/* Record eirqs, so that grgpio can avoid collisions */
	nctrl = BYPASS_LOAD_PA(&regs->ampctrl) >> 28;
	if (nctrl) {
		for (c = 0; c < 16; c++) {
			/* Logic lifted from leon_kernel.c in linux kernel */
			icsel = BYPASS_LOAD_PA(&regs->icsel[c/8]);
			icsel = (icsel >> ((7 - (c & 0x7)) * 4)) & 0xf;
			cregs = regs + icsel;

			irqmp_add_eirq(cregs);
		}
	} else {
		irqmp_add_eirq(regs);
	}
	return 0;
}

int irqmp_numcpus(void)
{
	return irqmp_ncpus;
}

int node_has_child(struct node *node, const char *child_name, int name_len, struct node **child);

void ambapp_prop_init(struct prop_std *buf, const char *prop_str);
int ambapp_prop_int(struct prop_std *buf, const char *prop_str, int value);
int ambapp_prop_str(struct prop_std *buf, const char *prop_str, const char *val_str);
int ambapp_prop_uintarray(struct prop_std *buf, const char *prop_str,
			  unsigned int *array, int len);
int ambapp_prop_str_array(struct prop_std *buf, const char *prop_str,
			  const char *val_array[]);
int ambapp_prop_empty(struct prop_std *buf, const char *prop_str);
int ambapp_prop_intptr(struct prop_std *buf, const char *prop_str, int *value,
		       int len);

int gptimer_inited = 0;

#ifdef CONFIG_HRTIMERS
/* Incremented by gptimer_init for each core */
int gptimer_core_cnt = 0;
#endif

/* Fixup IRQ of GPTIMER APBSLV interface */
int gptimer_apb_fixup(ambapp_dev *dev)
{
	struct gptimer_regs *regs;
	ambapp_apbdev *apbslv;
	uint32_t config;
	int ntimers, i;

	if (dev->dev_type != DEV_APB_SLV)
		return 0;
	apbslv = (ambapp_apbdev *)dev;
	regs = (struct gptimer_regs *)apbslv->address;
	config = BYPASS_LOAD_PA(&regs->config);
	if ((config & (1<<8)) == 0)
		return 0; /* All timers share the first IRQ */

	ntimers = config & 0x7;
	for (i = 1; i < ntimers; i++) {
		apbslv->irq[i] = apbslv->irq[0] + i;
	}
	return 0;
}


static struct gpreg_regs *gpreg = NULL;

void gpreg_init(ambapp_core *core, int level)
{
	if (amba_systemid_is_gr740() && !gpreg && core->apbslv)
		gpreg = (struct gpreg_regs *)core->apbslv->address;
}

#ifdef CONFIG_GPTIMER_WDT

void node_add_watchdog(struct node *parent, unsigned int ambapp_device_id,
		       unsigned int corenum)
{
	struct node *n;
	int size;
	unsigned int prop_buf[64];
	struct prop_std *buf = (struct prop_std *)prop_buf;
	struct watchdog_prop_conf *prop_conf;

#ifdef HASXML
	/* Check if there already exist a watchdog node. In that case skip
	 * adding the node as it most likely has been added from xml
	 */
	if (node_has_child(parent, prop_str_watchdog, 16, NULL))
		return;
#endif

	prop_conf = watchdog_get_prop_conf(ambapp_device_id, corenum);
	if (!prop_conf)
		return;

	/* Create a watchdog node and add it:
	 * watchdog {
	 *    compatible = "<compatible-string>";
	 *    gaisler,max-hw-timeout-ms = <milliseconds>;
	 *    timeout-sec = <seconds>;
	 *    name = "watchdog";
	 * };
	 */

	n = (struct node *)prom_malloc(sizeof(struct node));
	n->child = NULL;
	n->sibling = NULL;
	n->props = NULL;

	/* Name of the node */
	size = ambapp_prop_str(buf, prop_str_name, prop_str_watchdog);
	prop_add_dup(n, buf, size, NULL);

	/* Set the compatible string so the driver can match */
	size = ambapp_prop_str(buf, prop_str_compatible, prop_conf->compatible);
	prop_add_dup(n, buf, size, NULL);

	/* Write the user-timeout (optional) */
	if (prop_conf->user_timeout_sec != WATCHDOG_TIMEOUT_NOT_SET) {
		size = ambapp_prop_int(buf, prop_str_timeout_sec,
				prop_conf->user_timeout_sec);
		prop_add_dup(n, buf, size, NULL);
	}

	/* Set the hw timeout of the watchdog */
	size = ambapp_prop_int(buf, prop_str_hw_timeout_ms,
			prop_conf->hw_timeout_ms);
	buf->options |= PO_END;
	prop_add_dup(n, buf, size, NULL);

	/* Finally, add the watchdog node to the parent node */
	node_add_child(parent, n);
}
#endif

#ifdef CONFIG_HRTIMERS

/* Holds the property values for the driver's bindings */
struct hrtimers_prop_conf {
	int timer_offset;
	int timer_len;
	int cpu_offset;
	int cs_timer_index;
};

/* Initialized by the hrtimers_init */
static struct hrtimers_prop_conf hrtimers_prop_conf;

/*
 * Here follows the different timer allocation strategies that are supported:
 *  TMR_ALLOC_SETUP_CE:
 *    Setup clock event devices on subtimers for each GPTIMER core for as
 *    many CPUs in the system. Care have to be taken for GPTIMER0 where the
 *    last timer is reserved for GPTIMER Watchdog and the for GPTIMERS without
 *    separate interrupts only one timer can be setup.
 *  TMR_ALLOC_SETUP_CS:
 *    Setup a clock source either on the same core as the clock events or on a
 *    separate GPTIMER core. Care have to be taken for GPTIMER0 where the
 *    last timer is reserved for GPTIMER Watchdog.
 */

#define TMR_ALLOC_SETUP_CE (1 << 0)
#define TMR_ALLOC_SETUP_CS (1 << 1)

/*
 * Setup by hrtimers_init and is then assigned:
 * - TMR_ALLOC_SETUP_CE
 * Optionally masked with:
 * - TMR_ALLOC_SETUP_CS
 */
static int tmr_alloc_alt;
/* Number of configured clock event devices */
static int clkevt_dev_cnt;

/* The result values */
enum hrtimers_errno {
	HRTIMER_RES_OK,
	HRTIMER_RES_CS_SETUP_FAILED,
	HRTIMER_RES_SEP_GPTIMERS_FAILED,
	HRTIMER_RES_INVALID_CONFIG,
};

/* The result of the hrtimer operations */
static enum hrtimers_errno hrtimers_res = HRTIMER_RES_OK;

/* Returns true (1) if the hrtimer operations were successful. */
int hrtimers_res_ok(void)
{
	return hrtimers_res == HRTIMER_RES_OK;
}

/* Set the pre-scaler and scaler reload to the same as GPTIMER0 */
static inline void hrtimers_set_prescaler(ambapp_core *core, int numtmr)
{
	struct gptimer_regs *regs;
	unsigned int scaler;

	if (numtmr == 0 || !core->apbslv || !gptimer_inited)
		return;

	/* Setup the prescaler (use same as for gptimer0) */
	regs = (struct gptimer_regs *)core->apbslv->address;
	scaler = BYPASS_LOAD_PA(&gptimer->scaler_reload);
	BYPASS_STORE_PA(&regs->scaler_reload, scaler);
	BYPASS_STORE_PA(&regs->scaler, scaler);
}

static int hrtimers_gptimer_probe(ambapp_core *core, int numtmr)
{
	struct gptimer_regs *regs;
	unsigned int config;
	unsigned int subtimers;
	int ncpu;
	int timers_left;
	int timer_len;

	/* Nothing to do */
	if (!tmr_alloc_alt)
		return -1;

	if (!core || !core->apbslv)
		goto err_invalid_configuration;

	regs = (struct gptimer_regs *)core->apbslv->address;
	config = BYPASS_LOAD_PA(&regs->config);

	ncpu = irqmp_numcpus();
	if (!ncpu)
		goto err_invalid_configuration;

	/* How many timers are left ? */
	timers_left = ncpu - clkevt_dev_cnt;
	if (timers_left < 0)
		goto err_invalid_configuration;

	/* The number of timers we would like to setup on this core */
	timer_len = timers_left;

	/* Now check that there is enough sub-timers */
	subtimers = config & GPTIMER_CONF_TIMERS_MASK;
	if (!subtimers)
		goto err_invalid_configuration;

	/* The last timer of GPTIMER0 is reserved to the GPTIMER watchdog. */
	if (numtmr == 0)
		subtimers--;

	if (subtimers <= 0)
		goto err_invalid_configuration;
	/*
	 * Setup all clockevent devices on the GPTIMER.
	 * If the GPTIMER supports separate interrupts then we use as many
	 * subtimers as we can.
	 */
	if (tmr_alloc_alt & TMR_ALLOC_SETUP_CE) {
		if (config & GPTIMER_CONF_SEP_IRQ) {
			if (timer_len > subtimers)
				timer_len = subtimers;
		} else {
			timer_len = 1;
		}

		/* Initialize property values */
		/* FIXME: ampopts not considered as for now */
		hrtimers_prop_conf.timer_offset = 0;
		hrtimers_prop_conf.timer_len = timer_len;
		hrtimers_prop_conf.cpu_offset = clkevt_dev_cnt;

	} else {
		hrtimers_prop_conf.timer_offset = 0;
		hrtimers_prop_conf.timer_len = 0;
		hrtimers_prop_conf.cpu_offset = 0;
		timer_len = 0;
	}

	/*
	 * Setup clock event source as long as there is one available subtimer
	 */
	hrtimers_prop_conf.cs_timer_index = -1;
	if ((tmr_alloc_alt & TMR_ALLOC_SETUP_CS) && timer_len < subtimers)
		hrtimers_prop_conf.cs_timer_index = timer_len;

	DBG_PRINTF1("HRTIMER conf (gptimer%d):\n", numtmr);
	DBG_PRINTF2("   subtimers: %d, clkevt_dev_cnt: %d\n",
		    subtimers, clkevt_dev_cnt);
	DBG_PRINTF2("   cpu_offset: %d, timer_len: %d\n",
		    hrtimers_prop_conf.cpu_offset,
		    hrtimers_prop_conf.timer_len);
	DBG_PRINTF3("   timer_offset: %d, cs_timer_index: %d, ncpu: %d\n",
		    hrtimers_prop_conf.timer_offset,
		    hrtimers_prop_conf.cs_timer_index, ncpu);

	return 0;

err_invalid_configuration:
	hrtimers_res = HRTIMER_RES_INVALID_CONFIG;
	return -1;
}

/* Adds the hrtimer related properties */
static int hrtimers_add_timer_prop(ambapp_core *core, struct prop_std *buf,
				   int index, int numtmr)
{
	const char *compatible = prop_str_compatible_timer_ce;

	switch (index) {
	case 0:
		if (hrtimers_gptimer_probe(core, numtmr) < 0)
			return 0;

		hrtimers_set_prescaler(core, numtmr);
		return ambapp_prop_intptr(buf, prop_str_clocks,
					  (int*)&node_gptimer0_clk,
					  sizeof(node_gptimer0_clk));
	case 1:
		return ambapp_prop_int(buf, prop_str_tmr_offset,
				       hrtimers_prop_conf.timer_offset);
	case 2:
		return ambapp_prop_int(buf, prop_str_tmr_len,
				       hrtimers_prop_conf.timer_len);
	case 3:
		return ambapp_prop_int(buf, prop_str_cpu_offset,
				       hrtimers_prop_conf.cpu_offset);
	case 4:
		/*
		 * Update compatible in case we setup both clocksource and
		 * clockevents for this GPTIMER core
		 */
		if (hrtimers_prop_conf.cs_timer_index >= 0)
			compatible = prop_str_compatible_timer;

		return ambapp_prop_str(buf, prop_str_compatible, compatible);
	case 5:
		if (hrtimers_prop_conf.cs_timer_index >= 0) {
			/* Clock source has been setup, clear action */
			tmr_alloc_alt &= ~TMR_ALLOC_SETUP_CS;
			return ambapp_prop_int(buf, prop_str_cs_tmr_idx,
					       hrtimers_prop_conf.cs_timer_index);
		}
		/* Fallthrough */
	case 6:
		/* The GPTIMER core has been configured. */
		clkevt_dev_cnt += hrtimers_prop_conf.timer_len;
		break;
	default:
		break;
	}

	return 0;
}

/* Setup all clock event devices */
static int hrtimers_setup_ce(ambapp_core *core, struct prop_std *buf, int index,
			     int numtmr)
{
	int ret = 0;

	ret = hrtimers_add_timer_prop(core, buf, index, numtmr);
	if (ret == 0 && (irqmp_numcpus() == clkevt_dev_cnt))
		tmr_alloc_alt &= ~TMR_ALLOC_SETUP_CE;

	return ret;
}

/* Setup the timer allocation for the GPTIMER clocksource */
static int hrtimers_setup_cs(ambapp_core *core, struct prop_std *buf,
				     int index, int numtmr)
{
	switch (index) {
	case 0:
		if (hrtimers_gptimer_probe(core, numtmr) < 0)
			return 0;

		if (hrtimers_prop_conf.cs_timer_index == -1)
			return 0;

		hrtimers_set_prescaler(core, numtmr);
		return ambapp_prop_intptr(buf, prop_str_clocks,
					  (int*)&node_gptimer0_clk,
					  sizeof(node_gptimer0_clk));
	case 1:
		return ambapp_prop_str(buf, prop_str_compatible,
				       prop_str_compatible_timer_cs);
	case 2:
		tmr_alloc_alt &= ~TMR_ALLOC_SETUP_CS;
		return ambapp_prop_int(buf, prop_str_cs_tmr_idx, 0);

	default:
		break;
	}
	return 0;
}

/* Find out how to allocate the GPTIMERS in this system */
void hrtimers_init()
{
	tmr_alloc_alt = TMR_ALLOC_SETUP_CE;

	/*
	 * Check if a GPTIMER based clocksource should be setup:
	 * - If its possible to use the CPU up-counter in LEON then prefer it.
	 * - Otherwise setup a GPTIMER clocksource on the first found core with
	 *   enough timers.
	 */
	if (LEON_UP_COUNTER_AVAIL())
		goto done;

	/* Setup a GPTIMER clocksource */
	tmr_alloc_alt |= TMR_ALLOC_SETUP_CS;

done:
	return;
}

int gptimer_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numtmr = -1;
	int ret = 0;

	if (numtmr == -1)
		hrtimers_init();

	if (index == 0)
		numtmr++;

	if (hrtimers_res_ok()) {
		if (tmr_alloc_alt & TMR_ALLOC_SETUP_CE)
			ret = hrtimers_setup_ce(core, buf, index, numtmr);
		else if (tmr_alloc_alt & TMR_ALLOC_SETUP_CS)
			ret = hrtimers_setup_cs(core, buf, index, numtmr);
	}

	/* Sanity check when all gptimer cores have been setup */
	if (ret == 0 && numtmr == (gptimer_core_cnt - 1)) {
		if (tmr_alloc_alt & TMR_ALLOC_SETUP_CE) {
			DBG_PRINTF0("HRTIMER: Failed to setup clock event bindings!\n");
			hrtimers_res = HRTIMER_RES_INVALID_CONFIG;
		}

		if (tmr_alloc_alt & TMR_ALLOC_SETUP_CS) {
			DBG_PRINTF0("HRTIMER: Failed to setup clock source bindings!\n");
			hrtimers_res = HRTIMER_RES_INVALID_CONFIG;
		}
	}

	return ret;
}

#else
#define gptimer_fixup_prop NULL
#endif

#if defined(CONFIG_WATCHDOG) || defined(CONFIG_HRTIMERS)

#ifdef CONFIG_HRTIMERS

#define HRTIMER_MAX_ROLLBACK_NODES 32
static struct node* hrtimer_rollback_nodes[HRTIMER_MAX_ROLLBACK_NODES];
static int hrtimer_rollback_cnt = 0;

static void hrtimer_rollback_node(struct node *node)
{
	DBG_PRINTF1("Rollback timer properties from node: 0x%p\n", node);
	/* Remove all hrtimer related properties from the node. */
	prop_del(node, prop_str_compatible);
	prop_del(node, prop_str_clocks);
	prop_del(node, prop_str_tmr_offset);
	prop_del(node, prop_str_tmr_len);
	prop_del(node, prop_str_cpu_offset);
	prop_del(node, prop_str_cs_tmr_idx);
}
#endif

int gptimer_fixup_node(ambapp_core *core, struct node *node)
{
	/* Use static as core subindex is not generally available. */
	static int numnode = 0;

#ifdef CONFIG_WATCHDOG
	ambapp_dev *dev;

	dev = ambapp_get_first_dev(core);
	if (numnode == 0)
		node_add_watchdog(node, dev->device, numnode);
#endif

#ifdef CONFIG_HRTIMERS
	/* Rollback hrtimer related properties in case an error occurred. */
	if (!hrtimers_res_ok()) {
		/* Start with any previously setup nodes */
		int i;
		for (i = 0; i < hrtimer_rollback_cnt; i++)
			hrtimer_rollback_node(hrtimer_rollback_nodes[i]);

		hrtimer_rollback_cnt = 0;
		/* Rollback the current node */
		hrtimer_rollback_node(node);
	} else if (hrtimer_rollback_cnt < HRTIMER_MAX_ROLLBACK_NODES) {
		/* Store the node in case a future error occurs */
		hrtimer_rollback_nodes[hrtimer_rollback_cnt++] = node;
	}
#endif
	numnode++;

	return 0;
}
#else
#define gptimer_fixup_node NULL
#endif

struct mutable_ambapp_pnp_ahb {
	unsigned int	id;		/* VENDOR, DEVICE, VER, IRQ, */
	unsigned int	custom[3];
	unsigned int	mbar[4];	/* MASK, ADDRESS, TYPE,
					 * CACHABLE/PREFETCHABLE */
};

unsigned int canoc_ahb_fixup_dev_count(struct ambapp_pnp_axb *axb, void *bus, int type)
{
	int cores = ambapp_pnp_ver(axb->id) + 1;
	return cores - 1 ;
}

int canoc_ahb_fixup_virtual(ambapp_dev *dev, void *axb, void *bus, void **pdevp_)
{
	ambapp_ahbdev **pdevp = (ambapp_ahbdev **)pdevp_;
	int i;
	struct ambapp_pnp_ahb *orig_ahb = (struct ambapp_pnp_ahb *)axb;
	struct mutable_ambapp_pnp_ahb ahb_buf;
	int cores = dev->ver+1;
	dev = NULL; /* We will overwrite it in a moment */

	_memcpy((char *)&ahb_buf, (char *)orig_ahb, sizeof(struct ambapp_pnp_ahb));
	ahb_buf.id &= ~0x3e0;		/* Set version to 0 */
	ahb_buf.mbar[0] |= 0xfff0;	/* Set MASK to 0xfff */

	for (i = 0; i < cores; i++) {
		if(i > 0) {
			/* Overwrites actual device entry at i == 0 */
			(*pdevp)++;
		}
		(*pdevp)->dev_type = DEV_AHB_SLV;
		ambapp_dev_init_ahb((struct ahb_bus*)bus,
				    (struct ambapp_pnp_ahb *)&ahb_buf,
				    *pdevp,
				    1);
		ahb_buf.id++;			/* Increase irq */
		ahb_buf.mbar[0] += 0x100000;	/* Increase address */
	}

	return 1;
}

void ambapp_prop_init(struct prop_std *buf, const char *prop_str);
int ambapp_prop_int(struct prop_std *buf, const char *prop_str, int value);
int ambapp_prop_str(struct prop_std *buf, const char *prop_str, const char *val_str);
int ambapp_prop_uintarray(struct prop_std *buf, const char *prop_str,
			  unsigned int *array, int len);
int ambapp_prop_str_array(struct prop_std *buf, const char *prop_str,
			  const char *val_array[]);
int ambapp_prop_empty(struct prop_std *buf, const char *prop_str);

/* =========================================================================*/
/* Clock gating */

#ifdef CONFIG_CLKGATE
/**
 * grcg_add_consumer_props - Common helper function for writing the
 * clocks and clock-names properties to the provided property buf.
 *
 * buf      - The property buffer, where the properties are written.
 * consumer - The consumer, where the properties will be added to.
 *
 * Return values:
 *  0 = Done. No more properties to be added
 * >0 = The size of the property struct to be added
 */
static int grcg_add_consumer_props(struct prop_std *buf,
				   struct grcg_consumer *consumer)
{
	struct grcg_clkgate *prop_clocks;

	switch (consumer->props_written) {
	case 0: /* clocks */
		if (consumer->clk_id >= consumer->producer->num_clks)
			return 0;
		prop_clocks = &consumer->producer->clks[consumer->clk_id];
		return ambapp_prop_intptr(buf, prop_str_clocks,
					  (int *)prop_clocks,
					  sizeof(*prop_clocks));
	case 1: /* clock-names */
		return ambapp_prop_str(buf, prop_str_clock_names, "gate");
	default:
		return 0;
	}
}

/**
 * grcg_add_producer_props - Common function for writing all the supported
 * clock producer properties that the Linux driver use.
 *
 * The following properties are required:
 * compatible, #clock-cells and clock_output_names
 *
 * Optional properties:
 * protected-clocks and clock-indices
 *
 * buf      - The property buffer, where the properties are written.
 * producer - The producer, where the properties will be added to.
 *
 * Return values:
 *  0 = Done. No more properties to be added
 * >0 = The size of the property struct to be added
 */
static int grcg_add_producer_props(struct prop_std *buf,
				   struct grcg_producer *producer)
{
	switch (producer->props_written) {
	case 0: /* compatible */
		return ambapp_prop_str(buf, prop_str_compatible,
				       "gaisler,grcg");
	case 1: /* #clock-cells */
		return ambapp_prop_int(buf, prop_str_clock_cells, 1);
	case 2: /* clock-output-names */
		return ambapp_prop_str_array(buf, prop_str_clock_output_names,
					     producer->clock_output_names);
	case 3: /* Optional: protected-clocks or clock-indices */
		if (producer->protected_clks)
			return ambapp_prop_uintarray(
				buf, prop_str_protected_clocks,
				producer->protected_clks,
				producer->num_protected_clks);
		else if (producer->clk_indices)
			return ambapp_prop_uintarray(
				buf, prop_str_clock_indices,
				producer->clk_indices,
				producer->num_clk_indices);
		break;
	case 4: /* clock-indices (if protected-clocks was written before) */
		if (producer->protected_clks && producer->clk_indices)
			return ambapp_prop_uintarray(
				buf, prop_str_clock_indices,
				producer->clk_indices,
				producer->num_clk_indices);
		break;

	default:
		break;
	}

	return 0;
}

/* Uses the default property writer for the consumer */
#define GRCG_CONSUMER_DEFAULT(name, clk_id, device, corenum, producer)         \
	GRCG_CONSUMER(name, clk_id, device, corenum, producer,                 \
		      grcg_add_consumer_props)

/* Uses the default property writer for the producer */
#define GRCG_PRODUCER_DEFAULT(name, corenum)                                   \
	GRCG_PRODUCER(name, corenum, grcg_add_producer_props)

/* =========================================================================*/
/* GR740 Clock gating */

/* The prefix used for all GR740 related functions and variables */
#define GRCG_GR740_PREFIX grcg_gr740

/* GR740 clock output names */
static const char *grcg_gr740_producer_clk_output_names[] = {
	"greth0", "greth1",   "spwrtr",	  "pci",     "gr1553b", "grcan",
	"l4stat", "apbuart0", "apbuart1", "spictrl", "mctrl",	NULL,
};

/* GR740 - Clock id of the clock gated cores */
enum grcg_gr740_clkgate_id {
	GRCG_GR740_GRETH0,
	GRCG_GR740_GRETH1,
	GRCG_GR740_SPWRTR,
	GRCG_GR740_GRPCI2,
	GRCG_GR740_GR1553B,
	GRCG_GR740_GRCAN0,
	GRCG_GR740_GRCAN1,
	GRCG_GR740_L4STAT,
	GRCG_GR740_APBUART0,
	GRCG_GR740_APBUART1,
	GRCG_GR740_SPICTRL,
	GRCG_GR740_FTMCTRL,
	GRCG_GR740_MAX,
};

/* GR740 - Array of hardware clock gates, one per clock id */
static struct grcg_clkgate grcg_gr740_producer_clks[GRCG_GR740_MAX] = {
	GRCG_CLKGATE_ENTRY(GRCG_GR740_GRETH0, 0),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_GRETH1, 1),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_SPWRTR, 2),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_GRPCI2, 3),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_GR1553B, 4),
	/* Both GRCAN's are controlled by the same gate (5) */
	GRCG_CLKGATE_ENTRY(GRCG_GR740_GRCAN0, 5),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_GRCAN1, 5),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_L4STAT, 6),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_APBUART0, 7),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_APBUART1, 8),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_SPICTRL, 9),
	GRCG_CLKGATE_ENTRY(GRCG_GR740_FTMCTRL, 10),
};

/* No protected clocks for GR740 */
#define grcg_gr740_producer_protected_clks NULL
#define grcg_gr740_producer_num_protected_clks 0

/* No clock-indices for GR740 */
#define grcg_gr740_producer_clk_indices NULL
#define grcg_gr740_producer_num_clk_indices 0

/* Special property writer for greth1:
 * greth1 will reference both greth0 and greth1 to work around a known issue.
 * If greth0 is disabled when greth1 is enabled then that
 * would also disable greth1. Keeping two references by greth1 will ensure that
 * the clock never gets disabled as long as greth1 is enabled.
 */
static const char *grcg_gr740_greth1_clk_names[] = { "gate", "gate1", NULL };
static int grcg_gr740_greth1_fixup(struct prop_std *buf,
				   struct grcg_consumer *consumer)
{
	struct grcg_clkgate *prop_clocks;

	switch (consumer->props_written) {
	case 0:
		/* Write two phandle entries in the same go so both
		 * GRCG_GR740_GRETH0 and GRCG_GR740_GRETH1 are referenced by
		 * greth1
		 */
		prop_clocks = &consumer->producer->clks[GRCG_GR740_GRETH0];
		return ambapp_prop_intptr(buf, prop_str_clocks,
					  (int *)prop_clocks,
					  2 * sizeof(*prop_clocks));
	case 1:
		return ambapp_prop_str_array(buf, prop_str_clock_names,
					     grcg_gr740_greth1_clk_names);
	default:
		break;
	}

	return 0;
}

static struct grcg_producer grcg_gr740_producer0 =
	GRCG_PRODUCER_DEFAULT(GRCG_GR740_PREFIX, 0);

/* This array contains the clock producer for GR740 */
static struct grcg_producer *grcg_gr740_producers[] = {
	&grcg_gr740_producer0,
};

/**
 * Helper macro that creates a struct grcg_consumer linked to the producer with
 * index 0 in the grcg_gr740_producers array
 */
#define GRCG_GR740_CONSUMER(clk_id, device, corenum)                           \
	GRCG_CONSUMER_DEFAULT(GRCG_GR740_PREFIX, clk_id, device, corenum,      \
			      &grcg_gr740_producer0)

/* This array contains all clock consumers of GR740 */
static struct grcg_consumer grcg_gr740_consumers[] = {
	GRCG_GR740_CONSUMER(GRCG_GR740_GRETH0, GAISLER_ETHMAC, 0),
	GRCG_CONSUMER(GRCG_GR740_PREFIX, GRCG_GR740_GRETH1, GAISLER_ETHMAC,    \
		      1, &grcg_gr740_producer0, grcg_gr740_greth1_fixup),
	GRCG_GR740_CONSUMER(GRCG_GR740_SPWRTR, GAISLER_SPWROUTER, 0),
	GRCG_GR740_CONSUMER(GRCG_GR740_GRPCI2, GAISLER_GRPCI2, 0),
	GRCG_GR740_CONSUMER(GRCG_GR740_GR1553B, GAISLER_GR1553B, 0),
	GRCG_GR740_CONSUMER(GRCG_GR740_GRCAN0, GAISLER_GRCAN, 0),
	GRCG_GR740_CONSUMER(GRCG_GR740_GRCAN1, GAISLER_GRCAN, 1),
	GRCG_GR740_CONSUMER(GRCG_GR740_L4STAT, GAISLER_L4STAT, 0),
	GRCG_GR740_CONSUMER(GRCG_GR740_APBUART0, GAISLER_APBUART, 0),
	GRCG_GR740_CONSUMER(GRCG_GR740_APBUART1, GAISLER_APBUART, 1),
	GRCG_GR740_CONSUMER(GRCG_GR740_SPICTRL, GAISLER_SPICTRL, 0),
	GRCG_GR740_CONSUMER(GRCG_GR740_FTMCTRL, GAISLER_FTMCTRL, 0),
};

/* End GR740 Clock gating */
/* =========================================================================*/

#endif

/*
 * Contains grcg clkgating entries
 */
static struct grcg grcg_list[] = {
#ifdef CONFIG_CLKGATE
	GRCG_ENTRY(GRCG_GR740_PREFIX, amba_systemid_is_gr740),
#else
	{ NULL, NULL, 0, NULL, 0 },
#endif
};

#define GRCG_NUM GRCG_ARRAY_SIZE(grcg_list)

/**
 * grcg_is_clkgating_enabled
 *
 * Returns 1 if found otherwise 0
 */
static int grcg_is_clkgating_enabled(void)
{
	int i;

	for (i = 0; i < GRCG_NUM; i++)
		if (grcg_list[i].is_enabled && grcg_list[i].is_enabled())
			return 1;

	return 0;
}

/**
 * grcg_get_enabled_producer
 *
 * Returns the first found (enabled) producer of the specified corenum.
 * If not found then NULL is returned.
 */
static struct grcg_producer *grcg_get_enabled_producer(int corenum)
{
	int i, j;
	struct grcg *grcg;

	for (i = 0; i < GRCG_NUM; i++) {
		grcg = &grcg_list[i];
		if (!grcg->is_enabled || !grcg->is_enabled())
			continue;

		for (j = 0; j < grcg->num_producers; j++)
			if (grcg->producers[j]->corenum == corenum)
				return grcg->producers[j];
	}

	return NULL;
}

/**
 * grcg_add_consumer_props_for_core - Adds clocks and clock-names properties
 *
 * buf     - The property buffer, where the properties are written.
 * core    - The core that might support clock gating
 * corenum - The core subindex or equivalent
 *           Used when identifing if a core is enabled for clock gating.
 *
 * This function is expected to be called from a prop_fixup callback as it
 * follows the same return value semantics and hence should be called until it
 * return 0.
 *
 * Return values:
 *  0 = Done. No more properties to be added
 * >0 = The size of the property struct to be added
 */
int grcg_add_consumer_props_for_core(struct prop_std *buf, ambapp_core *core,
				     int corenum)
{
	struct grcg *grcg = NULL;
	struct grcg_consumer *consumer = NULL;
	ambapp_dev *dev;
	int i, j, ret;

	for (i = 0; i < GRCG_NUM; i++) {
		grcg = &grcg_list[i];
		if (!grcg->is_enabled || !grcg->is_enabled())
			continue;

		dev = ambapp_get_first_dev(core);

		/* Check if the core is a clk consumer */
		for (j = 0; j < grcg->num_consumers; j++) {
			if (grcg->consumers[j].device == dev->device &&
			    grcg->consumers[j].corenum == corenum) {
				consumer = &grcg->consumers[j];
				break;
			}
		}

		/* No match? Then continue with the next enabled GRCG */
		if (!consumer)
			continue;

		/* Found a clock consumer, now write the consumer properties */
		ret = consumer->write_prop(buf, consumer);
		if (ret)
			consumer->props_written++;

		return ret;
	}

	return 0;
}

int grcg_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numgrcgs = -1;
	struct grcg_producer *producer = NULL;
	int ret = 0;

	switch (index) {
	case 0:
		numgrcgs++;
		break;
	default:
		break;
	}

	/* Check if this clock gating core is a clk producer */
	producer = grcg_get_enabled_producer(numgrcgs);
	if (!producer)
		return 0;

	/* Found a clock producer, now write the producer properties */
	ret = producer->write_prop(buf, producer);
	if (ret)
		producer->props_written++;

	return ret;
}

int grcg_fixup_node(ambapp_core *core, struct node *node)
{
	/* Use static as core subindex is not generally available. */
	static int numnode = 0;
	struct grcg_producer *producer = NULL;
	int i;

	/* Update phandle's of the clock gates now that the node is available */
	producer = grcg_get_enabled_producer(numnode);
	if (producer) {
		for (i = 0; i < producer->num_clks; i++)
			producer->clks[i].phandle = (int)node;
	}

	numnode++;

	return 0;
}

const char *canoc_compatibles[] = {
	"gaisler,can_oc",
	"aeroflexgaisler,can_oc",
	"nxp,sja1000",
	NULL
};

int canoc_ahb_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	int val;
	static char *prop_str_nxpfreq = NULL;

	switch (index) {
	case 0:
		return ambapp_prop_str_array(buf, prop_str_compatible,
					     canoc_compatibles);
	case 1:
		PROM_STRDUP_ONCE(prop_str_nxpfreq, "nxp,external-clock-frequency");
		val = ambapp_dev_freq((ambapp_dev *)core->ahbslv);
		return ambapp_prop_int(buf, prop_str_nxpfreq, val);

	default:
		return 0;
	}
}

const char *i2c_compatibles[] = {
	"gaisler,i2cmst",
	"aeroflexgaisler,i2cmst",
	NULL
};

int i2c_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	int val;
	static char *prop_str_regshift = NULL;
	static char *prop_str_ocores_clock_frequency = NULL;

	switch (index) {
	case 0:
		return ambapp_prop_str_array(buf, prop_str_compatible,
					     i2c_compatibles);
	case 1:
 /* Since Linux 4.0 (3a33a85401ecdb0e2c01ea86d9e36a5711ce01d4) the meaning of
  * "clock-frequency" has changed.
  *
  * For kernel versions >= 4.0, Mklinuximg will generate:
  * "clock-frequency" = I2C bus frequency, default 100 KHz.
  * "opencores,ip-clock-frequency" = Input clock frequency of the I2C controller
  *
  * For kernel versions < 4.0 Mklinuximg will generate:
  * "clock-frequency" = Input clock frequency of the I2C controller
  * Note: The Linux driver (< 4.0) use a hardcoded I2C bus frequency = 100 KHz.
  */
#if CONFIG_LINUX_VERSION_CODE >= KERNEL_VER(4, 0, 0)
		/* I2C bus frequency, default 100 KHz. */
		val = 100000;
#else
		/* Input clock frequency of the I2C controller */
		val = ambapp_dev_freq((ambapp_dev *)core->apbslv);
#endif
		return ambapp_prop_int(buf, prop_str_clockfrequency, val);
	case 2:
		/* Set "reg-shift" */
		PROM_STRDUP_ONCE(prop_str_regshift, "reg-shift");
		return ambapp_prop_int(buf, prop_str_regshift, 2);
#if CONFIG_LINUX_VERSION_CODE >= KERNEL_VER(4, 0, 0)
	case 3:
		/* Input clock frequency of the I2C controller */
		PROM_STRDUP_ONCE(prop_str_ocores_clock_frequency,
				 "opencores,ip-clock-frequency");
		val = ambapp_dev_freq((ambapp_dev *)core->apbslv);
		return ambapp_prop_int(buf, prop_str_ocores_clock_frequency,
				       val);
#endif
	default:
		return 0;
	}
}

const char *spi_compatibles[] = {
	"gaisler,spictrl",
	"aeroflexgaisler,spictrl",
	NULL
};

int spi_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	int val;
	/* Use static as core subindex is not generally available. */
	static int numspi = -1;

	switch (index) {
	case 0:
		numspi++;
		return ambapp_prop_str_array(buf, prop_str_compatible,
					     spi_compatibles);
	case 1:
		/* Set "clock-frequency" */
		val = ambapp_dev_freq((ambapp_dev *)core->apbslv);
		return ambapp_prop_int(buf, prop_str_clockfrequency, val);
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numspi);

	return 0;
}


#define ENTRY_OR_DEFAULT(array, idx, dflt) \
	(idx < sizeof(array) / sizeof(array[0]) ? array[idx] : dflt)

static int grgpio_irqgen_array[] = {
	CONFIG_GRGPIO_IRQGEN
};
#define GRGPIO_NBITS(idx) \
	ENTRY_OR_DEFAULT(grgpio_nbits_array, idx, 0);

static int grgpio_nbits_array[] = {
	CONFIG_GRGPIO_NBITS
};
#define GRGPIO_IMASKGEN(idx) \
	ENTRY_OR_DEFAULT(grgpio_imaskgen_array, idx, 0x00000000);

static int grgpio_imaskgen_array[] = {
	CONFIG_GRGPIO_IMASKGEN
};
#define GRGPIO_IRQGEN(idx) \
	ENTRY_OR_DEFAULT(grgpio_irqgen_array, idx, -1);

#define GRGPIO_IMAP_MASK	0x1f1f1f1f

#define GRGPIO_IRQMODE_STRAIGHT	1
#define GRGPIO_IRQMODE_SINGLE	2
#define GRGPIO_IRQMODE_MAP	3

#define GRGPIO_MAX_PINS		32
#define GRGPIO_IRQMAP_NOIRQ	0xffffffff

struct grgpio_regs {
	unsigned int data;		/* 0x00 */
	unsigned int output;		/* 0x04 */
	unsigned int dir;		/* 0x08 */
	unsigned int imask;		/* 0x0c */
	unsigned int ipolarity;		/* 0x10  */
	unsigned int iedge;		/* 0x14 */
	unsigned int bypass;		/* 0x18 */
	unsigned int capability;	/* 0x1c */
	unsigned int imap[8];		/* 0x20-0x3c */
};

struct grgpio_fixup_priv {
	unsigned int nbits;
	unsigned int imaskgen;
	unsigned int irqmode;
	unsigned int irqmap[GRGPIO_MAX_PINS];
};

int grgpio_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	struct grgpio_fixup_priv *gp =
		(struct grgpio_fixup_priv *)core->fixup_priv;
	static char *prop_str_nbits;
	static char *prop_str_irqmap;
	static char *prop_str_gpiocontroller;
	static char *prop_str_ngpiocells;

	switch (index) {
	case 0:
		PROM_STRDUP_ONCE(prop_str_nbits, "nbits");
		return ambapp_prop_int(buf, prop_str_nbits, gp->nbits);
	case 1:
		PROM_STRDUP_ONCE(prop_str_irqmap, "irqmap");
		return ambapp_prop_uintarray(buf, prop_str_irqmap,
					     gp->irqmap, gp->nbits);
	case 2:
		PROM_STRDUP_ONCE(prop_str_gpiocontroller, "gpio-controller");
		return ambapp_prop_empty(buf, prop_str_gpiocontroller);
	case 3:
		PROM_STRDUP_ONCE(prop_str_ngpiocells, "#gpio-cells");
		return ambapp_prop_int(buf, prop_str_ngpiocells, 2);
	default:
		return 0;
	}
}

static int grgpio_irqgen_to_irqmode(int irqgen)
{
	if (irqgen < 0)
		return 0;

	switch (irqgen) {
	case 0:
		return GRGPIO_IRQMODE_STRAIGHT;
	case 1:
		return GRGPIO_IRQMODE_SINGLE;
	default:
		return GRGPIO_IRQMODE_MAP;
	}
}

/* Probes:
 * - nbits, if not set
 * - imaskgen, if not set
 * - whether irqmode is IRQMODE_MAP for grgpiov1
 */
static void grgpio_probe(int idx, struct grgpio_regs *regs, struct grgpio_fixup_priv *gp)
{
	unsigned int imask;
	unsigned int iedge;
	unsigned int direction;
	unsigned int output;
	unsigned int lines;
	unsigned int i;

	/* Prevent interrupts during probe */
	imask = BYPASS_LOAD_PA(&regs->imask);
	BYPASS_STORE_PA(&regs->imask, 0x00000000);

	if (!gp->nbits) {
		/* Probe nbits, i.e. the number of lines */
		direction = BYPASS_LOAD_PA(&regs->dir);
		output = BYPASS_LOAD_PA(&regs->output);
		BYPASS_STORE_PA(&regs->output, ~direction);
		lines = direction | BYPASS_LOAD_PA(&regs->output);
		BYPASS_STORE_PA(&regs->output, output);
		for (i = 0; (lines & (1 << i)) && i < 32; i++)
			;
		gp->nbits = i;
	}

	if (!gp->imaskgen) {
		/* Probe which pins have irq enabled */
		iedge = BYPASS_LOAD_PA(&regs->iedge);
		BYPASS_STORE_PA(&regs->imask, 0x00000000);
		BYPASS_STORE_PA(&regs->iedge, 0xffffffff);
		gp->imaskgen = BYPASS_LOAD_PA(&regs->iedge);
		BYPASS_STORE_PA(&regs->iedge, iedge);
	}

	if (!gp->irqmode) {
		unsigned int read;
		unsigned int readback;

		/* Probe if there are imap registers */
		read = BYPASS_LOAD_PA(&regs->imap[0]);
		BYPASS_STORE_PA(&regs->imap[0], ~read & GRGPIO_IMAP_MASK);
		readback = BYPASS_LOAD_PA(&regs->imap[0]);
		if(read != readback) {
			gp->irqmode = GRGPIO_IRQMODE_MAP;
			BYPASS_STORE_PA(&regs->imap[0], read);
		} else {
			/*
			 * irqgen == 0 and irqgen == 1 cannot be distinguished
			 * for version 1 of the core without going outside the
			 * grgpio core. Therefore, this has to be configured
			 * from the outside instead
			 */
			DBG_PRINTF1("grgpio%d: interrupts disabled - cannot distinguish irqgen 0 from 1\n",
				    idx);
		}
	}

	/* Restore original imask */
	BYPASS_STORE_PA(&regs->imask, imask);
}

/* Sets up the irqmap as reported to Linux. Does not set up any registers */
static void grgpio_setup_interrupts(ambapp_apbdev *dev,
				    struct grgpio_regs *regs,
				    struct grgpio_fixup_priv *gp)
{
	int pirq;
	int i;
	int j;


	int off = 0;

	/* Fill in irq arrays */
	pirq = dev->irq[0];
	_memset(dev->irq, 0, sizeof(dev->irq));
	for (i = 0; i < gp->nbits; i++) {
		int irq = 0;

		if (gp->irqmode && gp->imaskgen & (1 << i)) {
			if (gp->irqmode == GRGPIO_IRQMODE_SINGLE) {
				irq = pirq;
			} else {
				/* int off; */
				if (gp->irqmode == GRGPIO_IRQMODE_MAP) {
					unsigned int map;
					j = i / 4;
					map = BYPASS_LOAD_PA(&regs->imap[j]);
					off = (map >> (8*(3-i))) & 0x1f;
				} else {
					off = i;
				}
				irq = pirq + off;
			}
		}
		if (irq) {
			for (j = 0; dev->irq[j] && dev->irq[j] != irq; j++)
				; /* Skip taken slots */
			dev->irq[j] = irq;
			gp->irqmap[i] = j;
		} else {
			gp->irqmap[i] = GRGPIO_IRQMAP_NOIRQ;
		}
	}

	/*
	 * Make sure that no irq are allowed to overlap with an eirq. This would
	 * prevent Linux from even booting even if it is never triggered.
	 */
	for (i = 0; i < 32 && dev->irq[i] != 0; i++) {
		/* Check if dev->irq[i] overlaps with an eirq */
		for (j = 0; eirqs[j]; j++)
			if (dev->irq[i] == eirqs[j])
				break;
		if (!eirqs[j])
			continue;

		/* Overlap with eirq - remove from mapping... */
		for (j = 0; j < GRGPIO_MAX_PINS; j++) {
			if (gp->irqmap[j] == i) {
				gp->irqmap[j] = GRGPIO_IRQMAP_NOIRQ;
			} else if(gp->irqmap[j] > i && gp->irqmap[j] < 32) {
				gp->irqmap[j]--; /* due to next step */
			}
		}

		/* ... and from interrupts */
		for (j = i+1; j < 32 && dev->irq[j] != 0; j++)
			dev->irq[j-1] = dev->irq[j];
		dev->irq[j-1] = 0;
	}

}

/* Probe grgpio for nbits and interrupts */
int grgpio_fixup_core(ambapp_core *core)
{
	struct grgpio_regs *regs;
	struct grgpio_fixup_priv *gp;
	ambapp_apbdev *dev;
	int irqgen;

	/* Static solution as core subindex is not generally available */
	static unsigned int totalcores = 0;
	int idx = totalcores++;

	dev = core->apbslv;
	regs = (struct grgpio_regs *)dev->address;
	gp = startup_malloc(sizeof(*gp));

	/* TODO: Make it possible for many cores to have different values */
	gp->nbits = GRGPIO_NBITS(idx);
	gp->imaskgen = GRGPIO_IMASKGEN(idx);
	irqgen = GRGPIO_IRQGEN(idx);

	if (core->apbslv->ver == 0) {
		irqgen = 0;
	} else if (core->apbslv->ver >= 2) {
		unsigned int cap = BYPASS_LOAD_PA(&regs->capability);

		gp->nbits = (cap & 0x1f) + 1;
		irqgen = (cap >> 8) & 0x1f;
	}
	gp->irqmode = grgpio_irqgen_to_irqmode(irqgen);

	if (CONFIG_GRGPIO_PROBE && (!gp->nbits || !gp->imaskgen || gp->irqmode))
		grgpio_probe(idx, regs, gp);
	grgpio_setup_interrupts(dev, regs, gp);

#ifdef CONFIG_DEBUG
	{
		int i;
		DBG_PRINTF3("grgpio%d:\n   nbits=0x%x, imaskgen=0x%x\n",
			    idx, gp->nbits, gp->imaskgen);
		DBG_PRINTF0("   irqs: ");
		for (i = 0; dev->irq[i]; i++)
			DBG_PRINTF2("%s0x%x", (i == 0 ? "" : ", "), dev->irq[i]);
		DBG_PRINTF0("\n   interrupt map: ");
		for (i = 0; i < gp->nbits; i++) {
			if (gp->irqmap[i] < 32)
				DBG_PRINTF2("%s0x%x", (i == 0 ? "" : ", "), gp->irqmap[i]);
			else
				DBG_PRINTF2("%s%s", (i == 0 ? "" : ", "), "-");
		}
		DBG_PRINTF0("\n");
	}
#endif
	core->fixup_priv = gp;

	return 0;
}

void gptimer_init(ambapp_core *core, int level)
{
#ifdef CONFIG_HRTIMERS
	/* Count the number of GPTIMER cores the system */
	if (level == 0)
		gptimer_core_cnt++;
#endif
	/* Only process the first GPTIMER core */
	if (gptimer_inited)
		return;

	gptimer_inited = 1;
	gptimer = (struct gptimer_regs *)core->apbslv->address;

#ifndef CONFIG_BOOTLOADER_FREQ
	{
		struct ambapp_bus *ainfo = &ambapp_plb;
		unsigned long timer_freq, scaler;
		struct apb_bus *apbbus;

		scaler = BYPASS_LOAD_PA(&gptimer->scaler_reload);
		if (scaler == 0)
			while (1) ; /* Frequency never initialized */
		timer_freq = int_mul(scaler + 1, 1000000);

		/* Set the frequency for the AHB bus that GPTIMER is bridged
		 * from. The APB frequency is always the same as the AHB
		 * frequency.
		 */
		apbbus = &ainfo->apbs[core->apbslv->bus_idx];
		ambapp_freq_init(apbbus->parent_bus_idx, timer_freq);
	}
#endif
}

#ifdef CONFIG_PINMUX

/**
 * pinmux_gr740_allows_dev - Check if a device is allowed due to pin
 * multiplexing.
 *
 * Func    Remove condition
 * UART0   GPIO_15 == 0
 * UART1   GPIO_15 == 0
 * CAN0    GPIO_15 == 0
 * CAN1    GPIO_15 == 0
 * SPW_DBG GPIO_15 == 0
 * GR1553B GPIO_15 == 0
 * GRETH1  MEM_IFWIDTH == 0 || MEM_IFWIDTH == 1 && PCI_MODE_ENABLE == 1
 * PCI	   MEM_IFWIDTH == 0 || MEM_IFWIDTH == 1 && PCI_MODE_ENABLE == 0
 *
 * Return values:
 *  1 = Allowed
 *  0 = Not allowed
 */
int pinmux_gr740_allows_dev(ambapp_dev *dev, int num) {
	uint32_t ret = 1;
	uint32_t bootstrap;

	if (!amba_systemid_is_gr740() || !gpreg)
		return ret;

	bootstrap = BYPASS_LOAD_PA(&gpreg->reg);
	switch (dev->device) {
	case GAISLER_GRCAN:
	case GAISLER_APBUART:
	case GAISLER_GR1553B:
		/* Bootstrap signal GPIO_15:
		 * If this signal is LOW then the PROM/IO interface is enabled
		 * Otherwise the PROM/IO interface pins are routed to their
		 * alternative functions.
		 */
		if ((bootstrap & GPREG_GR740_BOOTSTRAP_GPIO_15) == 0)
			ret = 0;

		break;
	case GAISLER_ETHMAC:
		/* Only applicable to greth1 */
		if (num != 1)
			break;
		/* Remove greth1 if sdram iface is selected
		 * or if pci mode is enabled
		 */
		if ((bootstrap & GPREG_GR740_BOOTSTRAP_MEM_IFWIDTH) == 0)
			ret = 0;
		else if (bootstrap & GPREG_GR740_BOOTSTRAP_PCIMODE_EN)
			ret = 0;

		break;
	case GAISLER_GRPCI2:
	case GAISLER_GRPCI2_DMA:
		/* Remove pci node if sdram iface is selected
		 * or if pci mode is disabled.
		 */
		if ((bootstrap & GPREG_GR740_BOOTSTRAP_MEM_IFWIDTH) == 0)
			ret = 0;
		else if ((bootstrap & GPREG_GR740_BOOTSTRAP_PCIMODE_EN) == 0)
			ret = 0;

		break;
	default:
		break;
	}

	return ret;
}

/**
 * pinmux_allows_core - Check if a core is allowed due to pin multiplexing.
 *
 * Return values:
 *  1 = Allowed
 *  0 = Not allowed
 */
static int pinmux_allows_core(ambapp_core *core, int num)
{
	int ret = 1;
	ambapp_dev *dev = ambapp_get_first_dev(core);

	if (amba_systemid_is_gr740())
		ret = pinmux_gr740_allows_dev(dev, num);

	return ret;
}

#else
static int pinmux_allows_core(ambapp_core *core, int num)
{
	return 1;
}
#endif

static int apbuart_initied = 0;

/* Find first APBUART for console */
int apbuart_apb_fixup(ambapp_dev *dev)
{
	if (!apbuart_initied++) {
		ambapp_apbdev *apbdev = (ambapp_apbdev *)dev;
		apbuart = (struct apbuart_regs *)apbdev->address;
	}
	return 0;
}

int apbuart_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numuarts = -1;

	switch (index) {
	case 0:
		numuarts++;
		break;
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numuarts);

	return 0;
}

int apbuart_fixup_node(ambapp_core *core, struct node *node)
{
	static int numnodes = 0;
	int ret = 0;

	if (!pinmux_allows_core(core, numnodes))
		ret = REMOVE_NODE;

	numnodes++;
	return ret;
}

/* Turn off GRETH RX/TM DMA operations. This should not have to be done here,
 * however during debug (no proper shutdown or reset/boot) the GRETH may still
 * be active. This may lead to DMA overwrites of early Linux initalization
 * memory since the GRETH may overwrite them.
 */
void greth_init(ambapp_core *core, int level)
{
	struct greth_regs {
		uint32_t ctrl;
		uint32_t status;
		/* ... */
	} *regs;

	if (level == 0 && core->apbslv) {
		/* Turn off RX/TX DMA */
		regs = (struct greth_regs *)core->apbslv->address;
		BYPASS_STORE_PA(&regs->ctrl, BYPASS_LOAD_PA(&regs->ctrl)&~0xf);
		/* Clear status */
		BYPASS_STORE_PA(&regs->status, 0xffffffff);
	}
}

int greth_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numgreths = -1;

	switch (index) {
	case 0:
		numgreths++;
		if (amba_systemid_is_gr740()) {
			/*
			 * The two greth cores shares MDIO bus but greth0 is
			 * nevertheless supposed to only use the PHY with addr 1
			 * and greth1 the PHY with addr 2.
			 */
			static char *prop_str_forcephy;
			int subindex = numgreths;
			int phyaddr = subindex + 1;

			PROM_STRDUP_ONCE(prop_str_forcephy, "fixed-phy-addr");
			return ambapp_prop_int(buf, prop_str_forcephy, phyaddr);
		}
		break;
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numgreths);

	return 0;
}

int greth_fixup_node(ambapp_core *core, struct node *node)
{
	static int numnodes = 0;
	int ret = 0;

	if (!pinmux_allows_core(core, numnodes))
		ret = REMOVE_NODE;

	numnodes++;
	return ret;
}

void canmux_init(ambapp_core *core, int level)
{
	struct canmux_regs {
		uint32_t ctrl;
	} *regs;

	if (level == 0 && core->apbslv && amba_systemid == GR712RC_SYSTEMID) {
		regs = (struct canmux_regs *)core->apbslv->address;

		/* Use both CAN_OC cores */
		BYPASS_STORE_PA(&regs->ctrl, 3);
	}
}

int grusbdc_fixup_core(ambapp_core *core)
{
	ambapp_ahbdev *dev = core->ahbslv;

	if (dev && (dev->userdef[0] & 0x1)) { /* sepirq set */
		dev->irq[1] = (dev->userdef[0]) >> 4 & 0xf; /* irqi */
		dev->irq[2] = (dev->userdef[0]) >> 8 & 0xf; /* irqo */
	}

	return 0;
}

int grcan_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numgrcan = -1;

	switch (index) {
	case 0:
		numgrcan++;
		break;
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numgrcan);

	return 0;
}

int grcan_fixup_node(ambapp_core *core, struct node *node)
{
	static int numnodes = 0;
	int ret = 0;

	if (!pinmux_allows_core(core, numnodes))
		ret = REMOVE_NODE;

	numnodes++;
	return ret;
}

int l4stat_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numl4stat = -1;

	switch (index) {
	case 0:
		numl4stat++;
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numl4stat);

	return 0;
}

int gr1553b_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numgr1553b = -1;

	switch (index) {
	case 0:
		numgr1553b++;
		break;
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numgr1553b);

	return 0;
}

int gr1553b_fixup_node(ambapp_core *core, struct node *node)
{
	static int numnodes = 0;
	int ret = 0;

	if (!pinmux_allows_core(core, numnodes))
		ret = REMOVE_NODE;

	numnodes++;
	return ret;
}

int spwrouter_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numspwrtr = -1;

	switch (index) {
	case 0:
		numspwrtr++;
		break;
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numspwrtr);

	return 0;
}

int ftmctrl_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	/* Use static as core subindex is not generally available. */
	static int numftmctrl = -1;

	switch (index) {
	case 0:
		numftmctrl++;
		break;
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numftmctrl);

	return 0;
}

int grpci2_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	static int numgrpci2 = -1;

	switch (index) {
	case 0:
		numgrpci2++;
		break;
	default:
		break;
	}

	if (grcg_is_clkgating_enabled())
		return grcg_add_consumer_props_for_core(buf, core, numgrpci2);

	return 0;
}

int grpci2_fixup_node(ambapp_core *core, struct node *node)
{
	static int numnodes = 0;
	int ret = 0;

	if (!pinmux_allows_core(core, numnodes))
		ret = REMOVE_NODE;

	numnodes++;
	return ret;
}

int grpci2_dma_fixup_node(ambapp_core *core, struct node *node)
{
	static int numnodes = 0;
	int ret = 0;

	if (!pinmux_allows_core(core, numnodes))
		ret = REMOVE_NODE;

	numnodes++;
	return ret;
}

#ifdef CONFIG_GRWATCHDOG_WDT

void grwatchdog_init(ambapp_core *core, int level)
{
	if (!grwatchdog_inited && core->apbslv) {
		grwatchdog = (struct grwatchdog_regs *)core->apbslv->address;
		grwatchdog_inited = 1;
	}
}

#endif

struct amba_driver {
	unsigned short vendor;
	unsigned short device;
	unsigned char ver_min;
	unsigned char ver_max;
	int (*fixup_dev)(ambapp_dev *dev);
	int (*fixup_core)(ambapp_core *core);
	int (*fixup_prop)(ambapp_core *core, struct prop_std *buf, int index);
	void (*init)(ambapp_core *core, int level);
	unsigned int (*fixup_dev_count)(struct ambapp_pnp_axb *axb, void *bus,
					int type);
	int (*fixup_virtual_dev)(ambapp_dev *dev, void *axb, void *bus,
				 void **pdevp);
	ambapp_core *cores;
	int (*fixup_node)(ambapp_core *core, struct node *node);
};

/* Driver for a special version */
#define AMBAPP_DRV_VER(ven, dev, vmin, vmax, fixdev, fixcore, fixprop, init, fixnode) \
	{ven, dev, vmin, vmax, fixdev, fixcore, fixprop, init, NULL, NULL, NULL, fixnode}
/* Driver for all versions */
#define AMBAPP_DRV(ven, dev, fixdev, fixcore, fixprop, init, fixnode) \
	{ven, dev, 0, 0xff, fixdev, fixcore, fixprop, init, NULL, NULL, NULL, fixnode}
/* Driver for all versions with extra fixup functions */
#define AMBAPP_DRV_EXTRA_FIXUP(ven, dev, fixdev, fixcore, fixprop, init, \
			       fixdevcnt, fixvirtdev, fixnode)                   \
	{ven, dev, 0, 0xff, fixdev, fixcore, fixprop, init,             \
			fixdevcnt, fixvirtdev, NULL, fixnode}

struct amba_driver amba_drivers[] = {
 /*					   dev, core, prop, init */
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_IRQMP, irqmp_dev, NULL, NULL, NULL, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_GPTIMER, gptimer_apb_fixup, NULL,    \
			gptimer_fixup_prop, gptimer_init, gptimer_fixup_node),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_LEON5, NULL, NULL, NULL, leon_init, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_LEON4, NULL, NULL, NULL, leon_init, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_LEON3, NULL, NULL, NULL, leon_init, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_LEON3FT, NULL, NULL, NULL, leon_init, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_APBUART, apbuart_apb_fixup, NULL,
			apbuart_fixup_prop, NULL, apbuart_fixup_node),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_ETHMAC, NULL, NULL,     \
 			greth_fixup_prop, greth_init, greth_fixup_node),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_I2CMST, NULL, NULL,     \
			i2c_fixup_prop, NULL, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_SPICTRL, NULL, NULL,     \
			spi_fixup_prop, NULL, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_GPIO, NULL, grgpio_fixup_core,     \
			grgpio_fixup_prop, NULL, NULL),
 AMBAPP_DRV_EXTRA_FIXUP(VENDOR_GAISLER, GAISLER_CANAHB, NULL, NULL,     \
			canoc_ahb_fixup_prop, NULL,                     \
			canoc_ahb_fixup_dev_count, canoc_ahb_fixup_virtual, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_CANMUX, NULL, NULL,     \
			NULL, canmux_init, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_USBDC, NULL, grusbdc_fixup_core,     \
 			NULL, NULL, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_CLKGATE, NULL, NULL,     \
			grcg_fixup_prop, NULL, grcg_fixup_node),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_GRCAN, NULL, NULL,     \
			grcan_fixup_prop, NULL, grcan_fixup_node),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_L4STAT, NULL, NULL,     \
			l4stat_fixup_prop, NULL, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_GR1553B, NULL, NULL,     \
			gr1553b_fixup_prop, NULL, gr1553b_fixup_node),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_SPWROUTER, NULL, NULL,     \
			spwrouter_fixup_prop, NULL, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_FTMCTRL, NULL, NULL,     \
			ftmctrl_fixup_prop, NULL, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_GRPCI2, NULL, NULL,     \
 			grpci2_fixup_prop, NULL, grpci2_fixup_node),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_GPREG, NULL, NULL,     \
			NULL, gpreg_init, NULL),
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_GRPCI2_DMA, NULL, NULL,     \
			NULL, NULL, grpci2_dma_fixup_node),
#ifdef CONFIG_GRWATCHDOG_WDT
 AMBAPP_DRV(VENDOR_GAISLER, GAISLER_GRWATCHDOG, NULL, NULL,     \
			NULL, grwatchdog_init, NULL),
#endif
};
#define AMBAPP_DRV_NUM (sizeof(amba_drivers)/sizeof(struct amba_driver))

/* Fixup the count of AMBA PnP devices
 *
 * Note: To remove the device, instead return 1 in ambapp_dixup_dev. This is to
 * add to the number of devices in conjunction with ambapp_fixup_virtual_dev.
 *
 * axb   - struct ambapp_pnp_ahb * or struct ambapp_pnp_apb * for device
 * bus   - struct ahb_bus * or struct apb_bus * for device
 * type  - Type of the device
 *
 * Return values:
 *  The number of extra devices that will created.
 */
unsigned int ambapp_fixup_dev_count(struct ambapp_pnp_axb *axb, void *bus, int type)
{
	struct amba_driver *drv;
	int i;

	ambapp_dev dev_;
	ambapp_dev *dev = &dev_;
	dev->vendor = ambapp_pnp_vendor(axb->id);
	dev->device = ambapp_pnp_device(axb->id);
	dev->ver = ambapp_pnp_ver(axb->id);

	for (i = 0, drv = amba_drivers; i < AMBAPP_DRV_NUM; i++, drv++) {
		if ((drv->device == dev->device) &&
		    (drv->fixup_dev_count != NULL) &&
		    (drv->vendor == dev->vendor) &&
		    ((dev->ver >= drv->ver_min) &&
		     (dev->ver <= drv->ver_max))) {
			return drv->fixup_dev_count(axb, bus, type);
		}
	}

	return 0;
}

/* Fixup virtual AMBA PnP device information
 *
 * dev   - device struct for actual device
 * axb   - struct ambapp_pnp_ahb * or struct ambapp_pnp_apb * for actual device
 * bus   - struct ahb_bus * or struct apb_bus * for actual device
 * pdevp - pointer to struct ambapp_ahbdev *pdev in ambapp_scan_bus_ahb, or
 *         pointer to struct ambapp_apbdev *pdev in ambapp_scan_bus_apb
 * Return values:
 *  0 = Proceed as usual
 *  1 = Skip original device but do not touch variables
 */
int ambapp_fixup_virtual_dev(ambapp_dev *dev, void *axb, void *bus,
			     void **pdevp)
{
	struct amba_driver *drv;
	int i;

	for (i = 0, drv = amba_drivers; i < AMBAPP_DRV_NUM; i++, drv++) {
		if ((drv->device == dev->device) &&
		    (drv->fixup_virtual_dev != NULL) &&
		    (drv->vendor == dev->vendor) &&
		    ((dev->ver >= drv->ver_min) &&
		     (dev->ver <= drv->ver_max))) {
			return drv->fixup_virtual_dev(dev, axb, bus, pdevp);
		}
	}

	return 0;
}

/* Fixup AMBA PnP device information
 *
 * Return values:
 *  0 = Proceed as usual
 *  1 = Skip device
 */
int ambapp_fixup_dev(ambapp_dev *dev)
{
	struct amba_driver *drv;
	int i;

	for (i = 0, drv = amba_drivers; i < AMBAPP_DRV_NUM; i++, drv++) {
		if ((drv->device == dev->device) &&
		    (drv->fixup_dev != NULL) &&
		    (drv->vendor == dev->vendor) &&
		    ((dev->ver >= drv->ver_min) &&
		    (dev->ver <= drv->ver_max))) {
			return drv->fixup_dev(dev);
		}
	}

	return 0;
}

/*
 * Fix up versions that are DEV_VER_NONE. Other than unset version
 * fields all should already match at this point, no checks for that are done.
 */
static void ambapp_fixup_core_versions(ambapp_core *core)
{
	int version = DEV_VER_NONE;

	/* Gather version */
	if (core->ahbmst && core->ahbmst->ver != DEV_VER_NONE)
		version = core->ahbmst->ver;
	if (core->ahbslv && core->ahbslv->ver != DEV_VER_NONE)
		version = core->ahbslv->ver;
	if (core->apbslv && core->apbslv->ver != DEV_VER_NONE)
		version = core->apbslv->ver;

	/* If we have found none, set version to 0 */
	if (version == DEV_VER_NONE)
		version = 0;

	/* Set version */
	if (core->ahbmst)
		core->ahbmst->ver = version;
	if (core->ahbslv)
		core->ahbslv->ver = version;
	if (core->apbslv)
		core->apbslv->ver = version;
}

/* Fixup AMBA PnP core information
 *
 * Return values:
 *  0 = Proceed as usual
 *  1 = Skip core
 */
int ambapp_fixup_core(ambapp_core *core)
{
	struct amba_driver *drv;
	ambapp_dev *dev;
	int i;

	if (core->fixup_versions)
		ambapp_fixup_core_versions(core);

	/* Find a device so that we get a VENDOR:DEVICE:VERSION */
	if (core->ahbmst)
		dev = (ambapp_dev *)core->ahbmst;
	else if (core->ahbslv)
		dev = (ambapp_dev *)core->ahbslv;
	else if (core->apbslv)
		dev = (ambapp_dev *)core->apbslv;
	else
		while (1) ; /* this should not happen */

	for (i = 0, drv = amba_drivers; i < AMBAPP_DRV_NUM; i++, drv++) {
		if ((drv->device == dev->device) &&
		    (drv->fixup_core != NULL) &&
		    (drv->vendor == dev->vendor) &&
		    ((dev->ver >= drv->ver_min) &&
		    (dev->ver <= drv->ver_max))) {
			return drv->fixup_core(core);
		}
	}

	return 0;
}

/* Add properties for an ambapp_core
 *
 * core  - the core in question
 * buf   - property to fill in when adding a new property
 * index - for each core, this function should be called with an incrementing
 *         index (starting at 0) until 0 is returned
 *
 * Return values:
 *  0 = No more properties to be added
 * >0 = The size of the property struct to be added
 */
int ambapp_fixup_prop(ambapp_core *core, struct prop_std *buf, int index)
{
	struct amba_driver *drv;
	int i;
	ambapp_dev *dev = ambapp_get_first_dev(core);

	for (i = 0, drv = amba_drivers; i < AMBAPP_DRV_NUM; i++, drv++) {
		if ((drv->device == dev->device) &&
		    (drv->fixup_prop != NULL) &&
		    (drv->vendor == dev->vendor) &&
		    ((dev->ver >= drv->ver_min) &&
		     (dev->ver <= drv->ver_max))) {
			return drv->fixup_prop(core, buf, index);
		}
	}

	return 0;
}

int ambapp_node_fixup(ambapp_core *core, struct node *node)
{
	struct amba_driver *drv;
	int i;
	ambapp_dev *dev = ambapp_get_first_dev(core);

	if (dev->vendor == VENDOR_GAISLER) {
#ifdef CONFIG_APBUART_LINUX_MAX
		/* One you the first N APBUART cores */
		static int apbuart_cnt = 0;
		if (dev->device == GAISLER_APBUART) {
			if (apbuart_cnt++ >= CONFIG_APBUART_LINUX_MAX)
				return 1;
		}
#endif

#ifdef CONFIG_MINIMAL_CORES
		/* Ignore other cores that what is required */
		if (dev->device != GAISLER_APBUART &&
		    dev->device != GAISLER_IRQMP &&
		    dev->device != GAISLER_GPTIMER)
			return 1;
#endif

		for (i = 0, drv = amba_drivers; i < AMBAPP_DRV_NUM;
		     i++, drv++) {
			if ((drv->device == dev->device) &&
			    (drv->fixup_node != NULL) &&
			    (drv->vendor == dev->vendor) &&
			    ((dev->ver >= drv->ver_min) &&
			     (dev->ver <= drv->ver_max))) {
				return drv->fixup_node(core, node);
			}
		}
	}
	return 0;
}

/* Set root node and prepare for adding children */
void ambapp_bus_set_root_node(struct ambapp_bus *ainfo, int busidx,
			      struct node *node)
{
	ainfo->ahbsn[busidx] = node;
	ainfo->ahbsnn[busidx] = &node->child;
}

/* Insert Node last in chain of siblings */
void ambapp_bus_add_child_node(struct ambapp_bus *ainfo, int busidx,
			       struct node *node)
{
	*(ainfo->ahbsnn[busidx]) = node;
	ainfo->ahbsnn[busidx] = &node->sibling;
}

/*  */
struct ambapp_bridge_driver {
	int vendor;		/* VENDOR */
	int device;		/* DEVICE */
	unsigned char ver_min;	/* Version Minimum */
	unsigned char ver_max;	/* Version Maximum */
	unsigned char type;	/* 0=AHB-AHB, 1=AHB-APB */
	void (*create)(struct ambapp_bus *ainfo,
		struct ahb_bus *parent,
		ambapp_ahbdev *bridge);
};

/* proc.c */
extern unsigned long ipi_num;
struct propa_ptr props_ambapp_templ[] = {
	PROPA_PTR("device_type", "ambapp", 7),
	PROPA_PTR("name", "ambapp0", 8),
	PROPA_PTR("systemid", &amba_systemid, 4),
	PROPA_PTR("ioarea", 0, 4),
	PROPA_PTR_END("ipi_num", &ipi_num, 4),
};
#define NAMEIDX   1
#define IOAREAIDX 3

struct node *ahb2ahb_create_bus_node(struct ambapp_bus *ainfo, unsigned long ioarea, int bus_idx) 
{
	struct node *n = 0;
#ifndef FLATOFTREE
	struct propa_ptr *pa;

	/* create a bus node */
	n = (struct node *)prom_calloc(sizeof(struct node));
	/* initialize properties from template */
	pa = (struct propa_ptr *)prom_calloc(sizeof(props_ambapp_templ));
	_memcpy((void*)pa, (void*)&props_ambapp_templ, sizeof(props_ambapp_templ));
	n->props = (struct prop *)pa;
	/* customize ioarea */
	pa[IOAREAIDX].value = prom_calloc(sizeof(int));
	*(int *)pa[IOAREAIDX].value = ioarea;
	/* customize name */
	pa[NAMEIDX].value = prom_calloc(8);
	_memcpy((void*)pa[NAMEIDX].value,"ambapp",6);
	((char *)pa[NAMEIDX].value)[6] = '0' + bus_idx;
	ambapp_bus_set_root_node(ainfo, bus_idx, n);
#endif
	return n;
}

/*
 * Here the versions can be filled in as IOMMU master fields has no version
 * information. They default to 0. Even though we can later on match a master
 * with an unknown version to a slave with a known version, this is needed for
 * when there is only a master and no slaves, e.g. for PCI DMA masters.
 *
 * Returns whether the version of the ahb struct is trustworthy after the call
 * to this function.
 */
static int ahb2ahb_bus_create_iommu_fixup(
	struct ambapp_bus *ainfo, struct ahb_bus *parent, ambapp_ahbdev *bridge,
	struct ambapp_pnp_ahb *ahb, int index)
{
	if (((amba_systemid == GR740_REV0_SYSTEMID) ||
	     (amba_systemid == GR740_REV1_SYSTEMID)) &&
	    (index <= 9)) {
		int pciver;
		int spwver = 1;

		if (amba_systemid == GR740_REV1_SYSTEMID) {
			pciver = 2;
		} else {
			pciver = 1;
		}
		if (index == 0)
			*(unsigned int *)&ahb->id |= pciver << 5; /* PCI master */
		else if (index == 1)
			*(unsigned int *)&ahb->id |= pciver << 5; /* PCI DMA master */
		else if (index >= 4 && index <= 7)
			*(unsigned int *)&ahb->id |= spwver << 5; /* SpaceWire masters */

		/*
		 * We can rely upon all versions, including the default zeroes
		 * that we did not change.
		 */
		return 1;
	}

	return 0;
}

/* AHB-AHB IOMMU Creation */
void ahb2ahb_bus_create_iommu(struct ambapp_bus *ainfo, struct ahb_bus *parent,
			ambapp_ahbdev *bridge)
{
	struct ahb_bus *subbus;
	struct iommu_regs *regs = 0;
	ambapp_ahbdev *pdev;
	int j;

	if (ainfo->ahb_buses >= AMBA_AHB_MAX) {
#ifdef CONFIG_DEBUG
		DBG_PRINTF0("Warning! Detected too many AHB buses. Use -max-ahb-buses to adjust the max number of buses.\n");
#endif
		return;
	}

	/* Add new bus to AMBA Information */
	subbus = &ainfo->ahbs[ainfo->ahb_buses];
	subbus->bus_idx = ainfo->ahb_buses++;
	subbus->parent_bus_idx = parent->bus_idx;

	/* save the bus-idx the io-mmu should node-link */
	bridge->extra = subbus->bus_idx;
	
	/* Get info from Bridge's PnP custom information */
	subbus->ioarea = -2; /* mark as ioarea, already scanned */
	subbus->ffact = 1;
	subbus->dir = 1; /* up */
	subbus->isiommu = 1;
	
	/* get per bus node */
	ahb2ahb_create_bus_node(ainfo, 0, subbus->bus_idx);

	/* scan for reg address */
	for (j = 0; j < 4; j++) {
		if (bridge->address[j] && bridge->type[j] == AMBA_TYPE_AHBIO) {
			regs = (struct iommu_regs *)bridge->address[j];
			break;
		}
	}

	/* scan the pnp information */
	subbus->mst_cnt = 0;
	if (regs) {
		subbus->msts = (ambapp_ahbdev *)startup_malloc(16 * sizeof(ambapp_ahbdev));
		pdev = &subbus->msts[0];
		for (j = 0; j < 16; j++) {
			uint32_t mst;
			_memcpy4_pa((void *)&mst, (void *)&regs->mstconf[j], 4);
			if (mst) {
				struct ambapp_pnp_ahb ahb_buf;
				int gotver;

				_memset((void*)&ahb_buf, 0, sizeof(ahb_buf));
				*(int *)&(ahb_buf.id) = mst & 0xffffffe0;
				*(int *)&(ahb_buf.mbar[0]) = 0; /* mst */

				/* perform fixups */
				gotver = ahb2ahb_bus_create_iommu_fixup(
						ainfo, parent, bridge, &ahb_buf, j);

				/* Build a AHB Master Device */
				pdev->dev_type = DEV_AHB_MST;
				ambapp_dev_init_ahb(subbus, &ahb_buf, pdev, gotver);

				if (ambapp_fixup_dev((ambapp_dev *)pdev) == 0) {
					subbus->mst_cnt++;
					pdev++;
				} 
			}
                }
	}
	
}

/* AHB-AHB Bridge Creation */
void ahb2ahb_bus_create(struct ambapp_bus *ainfo, struct ahb_bus *parent,
			ambapp_ahbdev *bridge)
{
	struct ahb_bus *subbus;
	unsigned long ioarea;
	int i;

	if (ainfo->ahb_buses >= AMBA_AHB_MAX) {
#ifdef CONFIG_DEBUG
		DBG_PRINTF0("Warning! Detected too many AHB buses. Use -max-ahb-buses to adjust the max number of buses\n");
#endif
		return;
	}

	/* Is there a PnP area behind the bridge?*/
	ioarea = bridge->userdef[1];
	if (ioarea == 0)
		return;

	/* Check that bus hasn't been scanned already */
	for (i = 0; i < ainfo->ahb_buses; i++)
		if (ainfo->ahbs[i].ioarea == ioarea) {
			/* prevent uplink slave to be selected in ambapp_core_create */
			bridge->irq[0] |= 0x80;
			return;
		}

	
	/* Add new bus to AMBA Information */
	subbus = &ainfo->ahbs[ainfo->ahb_buses];
	subbus->bus_idx = ainfo->ahb_buses++;
	subbus->parent_bus_idx = parent->bus_idx;

	/* save the bus-idx the bridge should node-link */
	bridge->extra = subbus->bus_idx;

	/* Get info from Bridge's PnP custom information */
	subbus->ioarea = ioarea;
	subbus->ffact = (bridge->userdef[0] & AMBAPP_FLAG_FFACT) >> 4;
	subbus->dir = (bridge->userdef[0] & AMBAPP_FLAG_FFACT_DIR);

	/* get per bus node */
	ahb2ahb_create_bus_node(ainfo, ioarea, subbus->bus_idx);
	
}

/* AHB->APB Bridge */
void ahb2apb_bus_create(struct ambapp_bus *ainfo, struct ahb_bus *parent,
			ambapp_ahbdev *bridge)
{
	struct apb_bus *subbus;

	if (ainfo->apb_buses >= AMBA_APB_MAX) {
#ifdef CONFIG_DEBUG
		DBG_PRINTF0("Warning! Detected too many APB buses. Use -max-apb-buses to adjust the max number of buses.\n");
#endif
		return;
	}

	subbus = &ainfo->apbs[ainfo->apb_buses];
	subbus->bus_idx = ainfo->apb_buses++;
	subbus->parent_bus_idx = parent->bus_idx;
	subbus->ahb_bus_idx = bridge->bus_idx;
	
	/* Get info from Bridge's PnP custom information */
	subbus->ioarea = bridge->address[0];
}

struct ambapp_bridge_driver bridgedrvs[] = {
	{
		.vendor = VENDOR_GAISLER,
		.device = GAISLER_AHB2AHB,
		.ver_min = 1,
		.ver_max = 0xff,
		.type = 0,
		.create = ahb2ahb_bus_create
	},
	{
		.vendor = VENDOR_GAISLER,
		.device = GAISLER_APBMST,
		.ver_min = 0,
		.ver_max = 0xff,
		.type = 1,
		.create = ahb2apb_bus_create
	},
	{
		.vendor = VENDOR_GAISLER,
		.device = GAISLER_GRIOMMU,
		.ver_min = 0,
		.ver_max = 0xff,
		.type = 0,
		.create = ahb2ahb_bus_create_iommu
	},
	{
		.vendor = VENDOR_GAISLER,
		.device = GAISLER_L2CACHE,
		.ver_min = 0,
		.ver_max = 0xff,
		.type = 0,
		.create = ahb2ahb_bus_create
	},
	{
		.vendor = VENDOR_GAISLER,
		.device = GAISLER_APB3MST,
		.ver_min = 0,
		.ver_max = 0xff,
		.type = 1,
		.create = ahb2apb_bus_create
	},
};
#define AMBAPP_BRIDGE_DRV_NUM \
		(sizeof(bridgedrvs) / sizeof(struct ambapp_bridge_driver))

static int ambapp_is_apbmst(ambapp_dev *a)
{
	return (a->vendor == VENDOR_GAISLER && (a->device == GAISLER_APBMST ||
						a->device == GAISLER_APB3MST));
}


/************************/

void _memcpy4_pa(void *dst, void *src, int len)
{
	unsigned long *d, *s, *end;

	/* only request a even count words */
	if (len & 0x3)
		while (1) ;

	d = dst;
	s = src;
	end = (unsigned long *)(src + len);
	while (s < end) {
		*d = BYPASS_LOAD_PA(s);
		d++;
		s++;
	}
}

/* Detect a driver for AHB<->AHB, AHB->AHB or AHB->APB bridge */
static struct ambapp_bridge_driver *ambapp_bridge_detect(ambapp_ahbdev *dev)
{
	int i;
	struct ambapp_bridge_driver *drv;

	for (i = 0; i < AMBAPP_BRIDGE_DRV_NUM; i++) {
		drv = &bridgedrvs[i];

		if ((drv->device == dev->device) &&
		    (drv->vendor == dev->vendor) &&
		    ((dev->ver >= drv->ver_min) &&
		    (dev->ver <= drv->ver_max)))
			return drv;
	}

	return NULL;
}

void putsx(char*);
void puthex (unsigned int h);
void ambapp_dev_init_ahb(
	struct ahb_bus *bus,
	struct ambapp_pnp_ahb *ahb,
	ambapp_ahbdev *dev,
	int got_version
	)
{
	int bar;
	unsigned int addr, mask, mbar, type;

	/* Setup device struct */
	dev->vendor = ambapp_pnp_vendor(ahb->id);
	dev->device = ambapp_pnp_device(ahb->id);
	if (got_version)
		dev->ver = ambapp_pnp_ver(ahb->id);
	else
		dev->ver = DEV_VER_NONE;
	_memset(&dev->irq[0], 0, sizeof(dev->irq));
	dev->extra = 0;
	dev->irq[0] = ambapp_pnp_irq(ahb->id);
	dev->bus_idx = bus->bus_idx;
	dev->userdef[0] = (unsigned int)ahb->custom[0];
	dev->userdef[1] = (unsigned int)ahb->custom[1];
	dev->userdef[2] = (unsigned int)ahb->custom[2];

	/* Memory BARs */
	for (bar = 0; bar < 4; bar++) {
		mbar = ahb->mbar[bar];
		if (mbar == 0) {
			addr = 0;
			mask = 0;
			type = 0;
		} else {
			addr = ambapp_pnp_mbar_start(mbar);
			type = ambapp_pnp_mbar_type(mbar);
			if (type == AMBA_TYPE_AHBIO) {
				/* AHB I/O area is releative IO_AREA */
				addr = ambapp_pnp_ahbio_adr(addr, bus->ioarea);
				mask = (((unsigned int)(ambapp_pnp_mbar_mask(~mbar)<<8)|0xff))+1;
			} else {
				/* AHB memory area, absolute address */
				mask = (~((unsigned int)(ambapp_pnp_mbar_mask(mbar)<<20)))+1;
			}
		}
        // laur
        if(addr == 0x40000000)
            addr = CONFIG_RAM_START; //0x44000000;
		dev->address[bar] = addr;
        putsx("ahb slave "); puthex(addr);
		dev->mask[bar] = mask;
		dev->type[bar] = type;
	}
}

void ambapp_dev_init_apb(
	struct apb_bus *bus,
	struct ambapp_pnp_apb *apb,
	ambapp_apbdev *dev)
{
	/* Setup device struct */
	dev->bus_idx = bus->bus_idx;
	dev->vendor = ambapp_pnp_vendor(apb->id);
	dev->device = ambapp_pnp_device(apb->id);
	dev->ver = ambapp_pnp_ver(apb->id);
	_memset(&dev->irq[0], 0, sizeof(dev->irq));
	dev->extra = 0;
	dev->irq[0] = ambapp_pnp_irq(apb->id);
	dev->bus_idx = bus->bus_idx;
	dev->extra = bus->ahb_bus_idx;
	dev->address = ambapp_pnp_apb_start(apb->iobar, bus->ioarea);
	dev->mask = ambapp_pnp_apb_mask(apb->iobar);
    putsx("apb slave "); puthex(dev->address);
}

/* Finds for AHBMST, AHBSLV and APBSLV devices, counts then (returns count)
 * puts the addresses into pnp_entries in order
 */
static int ambapp_scan_bus(
	unsigned long pnparea,
	int max,
	int entry_size,
	void **pnp_entries,
	int *tot_dev_size /* I/O */
	)
{
	void *entry;
	unsigned long id;
	int cnt, i, dev_size = *tot_dev_size;

	cnt = 0;
	*tot_dev_size = 0;
	entry = (void *)pnparea;
	for (i = 0; i < max; i++) {
		_memcpy4_pa(&id, entry, 4);
		if (id != 0) {
			/* A device present here */
			pnp_entries[cnt++] = entry;
			*tot_dev_size = *tot_dev_size + dev_size;
		}
		entry += entry_size;
	}
	return cnt;
}

/* Scan one AHB bus, both AHB Masters and AHB Slaves */
static void ambapp_scan_bus_ahb(
	struct ambapp_bus *ainfo,
	struct ahb_bus *bus
	)
{
	struct ambapp_pnp_ahb *ahbs[64], ahb_buf, **pahb;
	struct ambapp_pnp_axb *axb;
	ambapp_ahbdev *pdev;
	int i, size, max_entries, vfix;
	unsigned int delta_count, pnp_cnt;
	unsigned long ioarea;
	struct ambapp_bridge_driver *busdrv;

	/* scan first bus for 64 devices, rest for 16 devices */
	if (bus->bus_idx == 0)
		max_entries = 64;
	else
		max_entries = 16;

	/* AHB MASTERS */
	ioarea = bus->ioarea | AMBA_CONF_AREA;
	size = sizeof(ambapp_ahbdev);
	pnp_cnt = ambapp_scan_bus(ioarea, max_entries,
				  sizeof(struct ambapp_pnp_ahb), (void **)ahbs,
				  &size);

	delta_count = 0;
	for (i = 0; i < pnp_cnt; i++) {
		_memcpy4_pa(&ahb_buf, ahbs[i], sizeof(struct ambapp_pnp_ahb));
		axb = (struct ambapp_pnp_axb *)&ahb_buf;
		delta_count += ambapp_fixup_dev_count(axb, bus, DEV_AHB_MST);
	}
	bus->mst_cnt = pnp_cnt + delta_count;
	size += delta_count * sizeof(ambapp_ahbdev);
	bus->msts = (ambapp_ahbdev *)startup_malloc(size);

	for (i = 0, pahb = &ahbs[0], pdev = &bus->msts[0];
	     i < pnp_cnt; i++, pahb++, pdev++) {
		_memcpy4_pa(&ahb_buf, *pahb, sizeof(struct ambapp_pnp_ahb));

		/* Build a AHB Master Device */
		pdev->dev_type = DEV_AHB_MST;
		ambapp_dev_init_ahb(bus, &ahb_buf, pdev, 1);

		vfix = ambapp_fixup_virtual_dev((ambapp_dev *)pdev,
						(void*)&ahb_buf, (void*)bus,
						(void**)&pdev);
		if (vfix == 1)
			/* skip the rest of the loop */
			continue;

		if (ambapp_fixup_dev((ambapp_dev *)pdev) == 1) {
			/* skip device */
			bus->mst_cnt--;
			pdev--;
			continue;
		}
	}

	/* AHB SLAVES */
	ioarea |= AMBA_AHBSLV_AREA;
	size = sizeof(ambapp_ahbdev);
	pnp_cnt = ambapp_scan_bus(ioarea, max_entries,
				  sizeof(struct ambapp_pnp_ahb), (void **)ahbs,
				  &size);

	delta_count = 0;
	for (i = 0; i < pnp_cnt; i++) {
		_memcpy4_pa(&ahb_buf, ahbs[i], sizeof(struct ambapp_pnp_ahb));
		axb = (struct ambapp_pnp_axb *)&ahb_buf;
		delta_count += ambapp_fixup_dev_count(axb, bus, DEV_AHB_SLV);
	}
	bus->slv_cnt = pnp_cnt + delta_count;
	size += delta_count * sizeof(ambapp_ahbdev);
	bus->slvs = (ambapp_ahbdev *)startup_malloc(size);

	for (i = 0, pahb = &ahbs[0], pdev = &bus->slvs[0];
	     i < pnp_cnt; i++, pahb++, pdev++) {
		_memcpy4_pa(&ahb_buf, *pahb, sizeof(struct ambapp_pnp_ahb));

		/* Build a AHB Slave Device */
		pdev->dev_type = DEV_AHB_SLV;
		ambapp_dev_init_ahb(bus, &ahb_buf, pdev, 1);

		vfix = ambapp_fixup_virtual_dev((ambapp_dev *)pdev,
						(void*)&ahb_buf, (void*)bus,
						(void**)&pdev);
		if (vfix == 1)
			/* skip the rest of the loop */
			continue;

		if (ambapp_fixup_dev((ambapp_dev *)pdev) == 1) {
			/* skip device */
			bus->slv_cnt--;
			pdev--;
			continue;
		}

		/* Detect if the device is a bridge. In that case just add the
		 * it, do not scan it. This is a one bus scan routine...
		 */
		busdrv = ambapp_bridge_detect(pdev);
		if (busdrv)
			busdrv->create(ainfo, bus, pdev);
	}
}

static void ambapp_scan_bus_apb(struct ambapp_bus *ainfo, struct apb_bus *bus)
{
	struct ambapp_pnp_apb *apbs[AMBA_APB_SLAVES], apb_buf, **papb;
	struct ambapp_pnp_axb *axb;
	ambapp_apbdev *pdev;
	int i, size, vfix;
	unsigned int delta_count, pnp_cnt;

	/* APB SLAVES */
	size = sizeof(ambapp_apbdev);
	pnp_cnt = ambapp_scan_bus(bus->ioarea | AMBA_CONF_AREA,
				  AMBA_APB_SLAVES,
				  sizeof(struct ambapp_pnp_apb),
				  (void **)apbs,
				  &size);

	delta_count = 0;
	for (i = 0; i < pnp_cnt; i++) {
		_memcpy4_pa(&apb_buf, apbs[i], sizeof(struct ambapp_pnp_apb));
		axb = (struct ambapp_pnp_axb *)&apb_buf;
		delta_count += ambapp_fixup_dev_count(axb, bus, DEV_APB_SLV);
	}
	bus->slv_cnt = pnp_cnt + delta_count;
	size += delta_count * sizeof(ambapp_apbdev);
	bus->slvs = (ambapp_apbdev *)startup_malloc(size);

	for (i = 0, papb = &apbs[0], pdev = &bus->slvs[0];
		i < pnp_cnt; i++, papb++, pdev++) {
		_memcpy4_pa(&apb_buf, *papb, sizeof(struct ambapp_pnp_apb));

		/* Build a APB Slave Device */
		pdev->dev_type = DEV_APB_SLV;
		ambapp_dev_init_apb(bus, &apb_buf, pdev);

		vfix = ambapp_fixup_virtual_dev((ambapp_dev *)pdev,
						(void*)&apb_buf, (void*)bus,
						(void**)&pdev);
		if (vfix == 1)
			/* skip the rest of the loop */
			continue;

		if (ambapp_fixup_dev((ambapp_dev *)pdev) == 1) {
			/* skip device */
			bus->slv_cnt--;
			pdev--;
			continue;
		}

		/* APB never have bridges */
	}
}

/* Create Root bus and initialize AMBA information */
void ambapp_bus_init(
	struct ambapp_bus *ainfo,
	unsigned long ioarea)
{
	int i;

	_memset(ainfo, 0, sizeof(ainfo));
	ainfo->ahbs[0].parent_bus_idx = 0xff; /* No parent */
	ainfo->ahbs[0].ioarea = ioarea;
	ainfo->ahb_buses = 1;

	for (i = 0; i < AMBA_AHB_MAX; i++) {
		if (ainfo->ahbs[i].ioarea == -2) /* iommu */
			continue;
		if (ainfo->ahbs[i].ioarea == 0)
			break;
		ambapp_scan_bus_ahb(ainfo, &ainfo->ahbs[i]);
	}

	for (i = 0; i < AMBA_APB_MAX; i++) {
		if (ainfo->apbs[i].ioarea == 0)
			break;
		ambapp_scan_bus_apb(ainfo, &ainfo->apbs[i]);
	}

#ifdef CONFIG_BOOTLOADER_FREQ
	/* Register Hardwcoded Bus frequency for AHB Bus 0 */
	ambapp_freq_init(0, CONFIG_BOOTLOADER_FREQ);
#endif
}

/* AMBA Core Counter */
static int ambapp_core_cnt = 0;

int ambapp_for_each_dev(struct ambapp_bus *ainfo, int type,
				ambapp_dev_f func, void *arg)
{
	int ret, i, j;
	struct ahb_bus *ahbbus;
	struct apb_bus *apbbus;
	ambapp_ahbdev *ahb;
	ambapp_apbdev *apb;

	/* AMBA AHB Masters */
	if (type & 0x1) {
		for (i = 0, ahbbus = ainfo->ahbs;
		     i < ainfo->ahb_buses; i++, ahbbus++) {
			for (j = 0, ahb = ahbbus->msts;
			     j < ahbbus->mst_cnt; j++, ahb++) {
				ret = func(ainfo, (ambapp_dev *)ahb, arg);
				if (ret != 0)
					return ret;
			}
		}
	}

	/* AMBA AHB Slaves */
	if (type & 0x2) {
		for (i = 0, ahbbus = ainfo->ahbs;
		     i < ainfo->ahb_buses; i++, ahbbus++) {
			for (j = 0, ahb = ahbbus->slvs;
			     j < ahbbus->slv_cnt; j++, ahb++) {
				ret = func(ainfo, (ambapp_dev *)ahb, arg);
				if (ret != 0)
					return ret;
			}
		}
	}

	/* 3. Find all AMBA cores with only an AMBA APB Slave device */
	if (type & 0x4) {
		for (i = 0, apbbus = ainfo->apbs;
		     i < ainfo->apb_buses; i++, apbbus++) {
			for (j = 0, apb = apbbus->slvs;
			     j < apbbus->slv_cnt; j++, apb++) {
				ret = func(ainfo, (ambapp_dev *)apb, arg);
				if (ret != 0)
					return ret;
			}
		}
	}

	return 0; /* All Devices Processed */
}

static int matching_dev_versions(int a, int b)
{
	return a == b || a == DEV_VER_NONE || b == DEV_VER_NONE;
}

ambapp_core *ambapp_core_create(
	struct ambapp_bus *ainfo,
	ambapp_dev *dev,
	int options)
{
	ambapp_core *core;
	struct ahb_bus *ahbbus;
	struct apb_bus *apbbus;
	ambapp_ahbdev *ahb;
	ambapp_apbdev *apb;
	int i, j;
	int vendor, device, ver;

	if (dev->irq[0] & 0x80) {
		/* This is the last time we will search this device,
		 * so we can unmark it as used.
		 */
		dev->irq[0] &= ~0x80;
		return NULL;
	}

	core = (ambapp_core *)startup_malloc(sizeof(ambapp_core));
	_memset(core, 0, sizeof(ambapp_core));

	vendor = dev->vendor;
	device = dev->device;
	ver = dev->ver;
	if (ver == DEV_VER_NONE)
		core->fixup_versions = 1;

#ifdef CONFIG_DEBUG
	core->vendor = vendor;
	core->device = device;
#endif

	/* Find a AHB Slave for Device */
	if (options & 0x2) {
		for (i = 0, ahbbus = ainfo->ahbs;
		     i < ainfo->ahb_buses; i++, ahbbus++) {
			for (j = 0, ahb = ahbbus->slvs;
			     j < ahbbus->slv_cnt; j++, ahb++) {
				if ((vendor == ahb->vendor) &&
				    (device == ahb->device) &&
				    matching_dev_versions(ver, ahb->ver) &&
				    ((ahb->irq[0] & 0x80) == 0)) {
					/* AHB Slave Device matching and not
					 * marked taken.
					 */
					ahb->irq[0] |= 0x80;
					core->ahbslv = ahb;
					if (ahb->ver == DEV_VER_NONE)
						core->fixup_versions = 1;
					goto ahbscandone;
				}
			}
		}
	}
ahbscandone:

	/* Find a APB Slave for Device */
	if (options & 0x4) {
		for (i = 0, apbbus = ainfo->apbs;
		     i < ainfo->apb_buses; i++, apbbus++) {
			for (j = 0, apb = apbbus->slvs;
			     j < apbbus->slv_cnt; j++, apb++) {
				if ((vendor == apb->vendor) &&
				    (device == apb->device) &&
				    matching_dev_versions(ver, apb->ver) &&
				    ((apb->irq[0] & 0x80) == 0)) {
					/* AHB Slave Device matching and not
					 * marked taken.
					 */
					apb->irq[0] |= 0x80;
					core->apbslv = apb;
					if (apb->ver == DEV_VER_NONE)
						core->fixup_versions = 1;
					goto apbscandone;
				}
			}
		}
	}
apbscandone:

	core->index = ambapp_core_cnt++;
	return core;
}

void ambapp_cores_init(struct ambapp_bus *ainfo)
{
	int i, j;
	ambapp_core *core, **pcore;
	struct ahb_bus *ahbbus;
	struct apb_bus *apbbus;
	ambapp_ahbdev *ahb;
	ambapp_apbdev *apb;

	/* Ee have all device information in RAM, next step is to pair
	 * AMBA devices together into AMBA "cores". Each core constist of
	 * a combination of max:
	 *   - one AHB Master
	 *   - one AHB Slave
	 *   - one APB Slave
	 *
	 * The devices are paired together by looking at the
	 * VENDOR:DEVICE:VERSION of devices. Each core is given an Index in
	 * which order they are found, starting at zero.
	 * To a avoid creating different cores associated with the same AMBA
	 * device each assigned device is marked as "taken" temporarily
	 * by setting bit 7 in IRQ.
	 */
	ambapp_core_cnt = 0;
	pcore = &ainfo->cores;

	/* 1. Find all AMBA cores with an AMBA AHB Master (an possibly an
	 *    AHB Slave and/or APB Slave)
	 */
	for (i = 0, ahbbus = ainfo->ahbs; i < ainfo->ahb_buses; i++, ahbbus++) {
		for (j = 0, ahb = ahbbus->msts;
		     j < ahbbus->mst_cnt; j++, ahb++) {
			core = ambapp_core_create(
				ainfo,
				(ambapp_dev *)ahb,
				0x6); /* AHB SLV and APB SLV */
			if (core == NULL)
				continue; /* device already used */
			core->ahbmst = ahb;
			if (ambapp_fixup_core(core) == 1) {
				_memset(core, 0, sizeof(core));
				continue;
			}

			*pcore = core;
			pcore = &core->next;
		}
	}

	/* 2. Find all AMBA cores with an AMBA AHB Slave (an possibly an
	 *    APB Slave) which does not have an AHB Master.
	 */
	for (i = 0, ahbbus = ainfo->ahbs; i < ainfo->ahb_buses; i++, ahbbus++) {
		for (j = 0, ahb = ahbbus->slvs;
		     j < ahbbus->slv_cnt; j++, ahb++) {
			core = ambapp_core_create(
				ainfo,
				(ambapp_dev *)ahb,
				0x4); /* APB SLV */
			if (core == NULL)
				continue; /* device already used */

			core->ahbslv = ahb;

			if (ambapp_fixup_core(core) == 1) {
				_memset(core, 0, sizeof(core));
				continue;
			}

			*pcore = core;
			pcore = &core->next;
		}
	}

	/* 3. Find all AMBA cores with only an AMBA APB Slave device */
	for (i = 0, apbbus = ainfo->apbs; i < ainfo->apb_buses; i++, apbbus++) {
		for (j = 0, apb = apbbus->slvs;
		     j < apbbus->slv_cnt; j++, apb++) {
			core = ambapp_core_create(
				ainfo,
				(ambapp_dev *)apb,
				0x0); /* Nothing */
			if (core == NULL)
				continue; /* device already used */

			core->apbslv = apb;

			if (ambapp_fixup_core(core) == 1) {
				_memset(core, 0, sizeof(core));
				continue;
			}

			*pcore = core;
			pcore = &core->next;
		}
	}

	*pcore = NULL;
	ainfo->core_cnt = ambapp_core_cnt;
}

struct ambapp_bus ambapp_plb;

/* Early console initialization (before AMBA Plug & Play) */
static void ambapp_earlycon_init()
{
	if (amba_systemid_is_gr740()) {
        putsx("amba_systemid_is_gr740\n");
		apbuart = (struct apbuart_regs *)GR740_DEBUG_UART;
	}
}

/* Initialize AMBA Structures but not bus frequency, pair devices into cores */
void ambapp_init(void)
{
	/* Get AMBA System ID */
	amba_systemid =
		BYPASS_LOAD_PA(CONFIG_AMBA_IO_AREA | AMBA_CONF_AREA | 0xff0);

	ambapp_earlycon_init();

	ambapp_bus_init(&ambapp_plb, CONFIG_AMBA_IO_AREA);

	ambapp_cores_init(&ambapp_plb);
}

void ambapp_drivers_init(void)
{
	struct ambapp_bus *ainfo = &ambapp_plb;
	struct amba_driver *drv;
	int i, lvl;
	ambapp_core *core, **ppcore;
	ambapp_dev *dev;

	/* match cores with drivers */
	core = ainfo->cores;
	while (core) {
		/* Find a device so that we get a VENDOR:DEVICE:VERSION */
		dev = ambapp_get_first_dev(core);

		for (i = 0, drv = amba_drivers;
		     i < AMBAPP_DRV_NUM; i++, drv++) {
			if ((drv->device == dev->device) &&
			    drv->init &&
			    (drv->vendor == dev->vendor) &&
			    ((dev->ver >= drv->ver_min) &&
			    (dev->ver <= drv->ver_max))) {
				/* Add core to end of list */
				ppcore = &drv->cores;
				while (*ppcore)
					ppcore = &(*ppcore)->next_in_drv;
				*ppcore = core;
				core->next_in_drv = NULL;
				break;
			}
		}
		core = core->next;
	}

	/* All initialization functions 5 times */
	for (lvl = 0; lvl < 5; lvl++) {
		for (i = 0, drv = amba_drivers;
		     i < AMBAPP_DRV_NUM; i++, drv++) {
			core = drv->cores;
			while (core) {
				drv->init(core, lvl);
				core = core->next_in_drv;
			}
		}
	}
}

static unsigned int ambapp_freq_calc(int dir, int ffact, unsigned int freq)
{
	if (dir == 0)
		return int_mul(freq, ffact);
	else
		return int_div(freq, ffact);
}

static void ambapp_subbus_freq_calc(struct ahb_bus *bus)
{
	struct ambapp_bus *ainfo = &ambapp_plb;
	struct ahb_bus *subbus;
	int i;

	for (i = 0; i < ainfo->ahb_buses; i++) {
		subbus = &ainfo->ahbs[i];
		if (subbus == bus)
			continue;
		if (subbus->parent_bus_idx == bus->bus_idx) {
			/* Subbus is slave */
			if (subbus->freq == 0) {
				/* freq already calculated */
				subbus->freq = ambapp_freq_calc(
							subbus->dir,
							subbus->ffact,
							bus->freq);
			}
			ambapp_subbus_freq_calc(subbus);
		}
	}
}

/* Set the frequency of an AMBA bus, used to determine all other buses
 * frequencies as well.
 *
 * 1. Determine the frequency of the root bus
 * 2. Iterate over all buses and determine their frequency relative
 *    to the parent bus
 */
void ambapp_freq_init(int ahb_bus_idx, unsigned long freq)
{
	struct ambapp_bus *ainfo = &ambapp_plb;
	struct ahb_bus *bus;
	int dir;

	bus = &ainfo->ahbs[ahb_bus_idx];
	bus->freq = freq;

	while (bus->parent_bus_idx != 0xff) {
		/* upstreams towards root bus means we swap order of frequency
		 * factor direction.
		 */
		dir = bus->dir;
		if (dir == 0)
			dir = 1;
		else
			dir = 0;
		freq = ambapp_freq_calc(dir, bus->ffact, freq);
		bus = &ainfo->ahbs[bus->parent_bus_idx];
		bus->freq = freq;
	}

	ambapp_subbus_freq_calc(&ainfo->ahbs[0]);
}

/* Get bus frequency for a device */
unsigned long ambapp_dev_freq(ambapp_dev *dev)
{
	struct ambapp_bus *ainfo = &ambapp_plb;
	int ahb_idx;

	if (dev->dev_type == DEV_APB_SLV)
		ahb_idx = ainfo->apbs[dev->bus_idx].parent_bus_idx;
	else
		ahb_idx = dev->bus_idx;

	return ainfo->ahbs[ahb_idx].freq;
}

#ifndef CONFIG_CUSTOM_NODES

void writehex(char *n, int v)
{
	v = v & 0xf;
	*n = v > 9 ? v - 10 + 'a' : v + '0';
}

void ambapp_prop_init(struct prop_std *buf, const char *prop_str)
{
	struct prop_data *dp = (struct prop_data *)buf;

	dp->name = (char *)prop_str;
	dp->next = NULL;
	dp->options = PO_DATA | PO_NEXT;
}

int ambapp_prop_intptr(struct prop_std *buf, const char *prop_str, int *value, int length)
{
	struct prop_ptr *dp = (struct prop_ptr *)buf;

	ambapp_prop_init(buf, prop_str);
	dp->options = PO_PTR | PO_NEXT;
	dp->length = length;
	dp->value = value;

	return sizeof(struct prop_ptr) + dp->length;
}

int ambapp_prop_int(struct prop_std *buf, const char *prop_str, int value)
{
	struct prop_data *dp = (struct prop_data *)buf;

	ambapp_prop_init(buf, prop_str);
	dp->data[0] = value;
	dp->length = sizeof(value);

	return sizeof(struct prop_data) + dp->length;
}

int ambapp_prop_str(struct prop_std *buf, const char *prop_str, const char *val_str)
{
	char *str = (char *)&buf->v.data[0];

	if (!val_str)
		return 0;
	_strcpy(str, (char *)val_str);
	buf->length = 1 + _strnlen(str, 64);
	ambapp_prop_init(buf, prop_str);

	return sizeof(struct prop_data) + buf->length;
}

int ambapp_prop_uintarray(struct prop_std *buf, const char *prop_str,
			  unsigned int *array, int len)
{
	struct prop_data *dp = (struct prop_data *)buf;

	ambapp_prop_init(buf, prop_str);
	dp->length = len * sizeof(*array);
	_memcpy((char *)dp->data, (char *)array, dp->length);

	return sizeof(struct prop_data) + dp->length;
}

int ambapp_prop_str_array(struct prop_std *buf, const char *prop_str,
			  const char *val_array[])
{
	char *str;
	int len;
	int i;

	ambapp_prop_init(buf, prop_str);
	str = (char *)&buf->v.data[0];
	buf->length = 0;
	for(i = 0; val_array[i]; i++) {
		const char *strval = val_array[i];

		len = 1 + _strnlen(strval, 64);
		_strcpy(str, (char *)strval);
		str += len;
		buf->length += len;
	}
	return sizeof(struct prop_data) + buf->length;

}

int ambapp_prop_empty(struct prop_std *buf, const char *prop_str)
{
	return ambapp_prop_uintarray(buf, prop_str, NULL, 0);
}

/* Create Name Property */
int ambapp_prop_name(ambapp_core *core, struct prop_std *buf)
{
	char *str;
	const char *device, *vendor;
	ambapp_dev *dev = ambapp_get_first_dev(core);

	str = (char *)&buf->v.data[0];

	/* Find Device and Vendor String from AMBA PnP Data Base */
	device = ambapp_device_id2str(ambapp_ids, dev->vendor, dev->device);
	vendor = ambapp_vendor_id2str(ambapp_ids, dev->vendor);
	if (device && vendor) {
		_strcpy(str, (char *)vendor);
		buf->length = _strnlen(vendor, 32);
		*(str + buf->length) = '_';
		_strcpy(str + buf->length + 1, (char *)device);
		buf->length += 2 + _strnlen(device, 32);
	} else {
		writehex(&str[0], dev->vendor >> 4);
		writehex(&str[1], dev->vendor);
		str[2] = '_';
		writehex(&str[3], dev->device >> 8);
		writehex(&str[4], dev->device >> 4);
		writehex(&str[5], dev->device);
		str[6] = '\0';
		str[7] = 0;
		buf->length = 7;
	}

	ambapp_prop_init(buf, prop_str_name);

	return sizeof(struct prop_data) + buf->length;
}

int ambapp_prop_vendor(ambapp_core *core, struct prop_std *buf)
{
	ambapp_dev *dev = ambapp_get_first_dev(core);

	return ambapp_prop_int(buf, prop_str_vendor, dev->vendor);
}

int ambapp_prop_device(ambapp_core *core, struct prop_std *buf)
{
	ambapp_dev *dev = ambapp_get_first_dev(core);

	return ambapp_prop_int(buf, prop_str_device, dev->device);
}

int ambapp_prop_version(ambapp_core *core, struct prop_std *buf)
{
	ambapp_dev *dev = ambapp_get_first_dev(core);

	return ambapp_prop_int(buf, prop_str_version, dev->ver);
}

int ambapp_prop_devmsk(ambapp_core *core, struct prop_std *buf)
{
	unsigned int devmsk = 0;

	if (core->apbslv)
		devmsk |= 0x1;
	if (core->ahbslv)
		devmsk |= 0x2;
	if (core->ahbmst)
		devmsk |= 0x4;
	return ambapp_prop_int(buf, prop_str_devmsk, (int)devmsk);
}

int ambapp_prop_interrupts_add(ambapp_dev *dev, int *buf, int index)
{
	int i;

	if (!dev)
		return index;

	for (i = 0; i < 32 && dev->irq[i] != 0 && index < 32; i++)
		buf[index++] = dev->irq[i];
	return index;
}

int ambapp_prop_interrupts(ambapp_core *core, struct prop_std *buf)
{
	int i;
	struct prop_data *dp = (struct prop_data *)buf;

	i = 0;
	i = ambapp_prop_interrupts_add((ambapp_dev *)core->apbslv, dp->data, i);
	i = ambapp_prop_interrupts_add((ambapp_dev *)core->ahbslv, dp->data, i);
	i = ambapp_prop_interrupts_add((ambapp_dev *)core->ahbmst, dp->data, i);

	if (i == 0)
		return 0; /* no such property */

	ambapp_prop_init(buf, prop_str_interrupts);
	dp->length = i << 2;

	return sizeof(struct prop_data) + dp->length;
}

int ambapp_prop_reg(ambapp_core *core, struct prop_std *buf)
{
	int i, j;
	struct prop_data *dp = (struct prop_data *)buf;

	i = 0;
	if (core->apbslv) {
		dp->data[i++] = core->apbslv->address;
		dp->data[i++] = core->apbslv->mask;
	}
	if (core->ahbslv) {
		for (j = 0; j < 4; j++) {
			if (core->ahbslv->mask[j] > 0) {
				dp->data[i++] = core->ahbslv->address[j];
				dp->data[i++] = core->ahbslv->mask[j];
			}
		}
	}

	if (i == 0)
		return 0; /* no reg property */


	ambapp_prop_init(buf, prop_str_reg);
	dp->length = i << 2;

	return sizeof(struct prop_data) + dp->length;
}

int ambapp_prop_userdef(ambapp_core *core, struct prop_std *buf)
{
	int i;
	struct prop_data *dp = (struct prop_data *)buf;

	i = 0;
	if (core->ahbslv) {
		dp->data[i++] = core->ahbslv->userdef[0];
		dp->data[i++] = core->ahbslv->userdef[1];
		dp->data[i++] = core->ahbslv->userdef[2];
	}
	if (core->ahbmst) {
		dp->data[i++] = core->ahbmst->userdef[0];
		dp->data[i++] = core->ahbmst->userdef[1];
		dp->data[i++] = core->ahbmst->userdef[2];
	}

	if (i == 0)
		return 0; /* no reg property */

	ambapp_prop_init(buf, prop_str_userdef);
	dp->length = i << 2;

	return sizeof(struct prop_data) + dp->length;
}

int ambapp_prop_freq(ambapp_core *core, struct prop_std *buf)
{
	int i;
	struct prop_data *dp = (struct prop_data *)buf;

	ambapp_prop_init(buf, prop_str_freq);
	i = 0;
	if (core->apbslv)
		dp->data[i++] = ambapp_dev_freq((ambapp_dev *)core->apbslv);
	if (core->ahbslv)
		dp->data[i++] = ambapp_dev_freq((ambapp_dev *)core->ahbslv);
	if (core->ahbmst)
		dp->data[i++] = ambapp_dev_freq((ambapp_dev *)core->ahbmst);
	dp->length = i << 2;

	return sizeof(struct prop_data) + dp->length;
}

int ambapp_prop_index(ambapp_core *core, struct prop_std *buf)
{
	return ambapp_prop_int(buf, prop_str_index, core->index);
}

int ambapp_prop_ampopts(ambapp_core *core, struct prop_std *buf)
{
#ifndef CONFIG_AMPOPTS
	return 0;
#else
	struct amp_opt *a;

	a = &ampopts[0];
	while (a->idx != -1) {
		if (a->idx == core->index) {
			/* Found Matching AMP String, create property */
			return ambapp_prop_int(buf, prop_str_ampopts,
					       a->val);
		}
		a++;
	}

	/* no AMP option for this core */
	return 0;
#endif
}

/* Create Description Property... very optional :) */
int ambapp_prop_description(ambapp_core *core, struct prop_std *buf)
{
	/* Find Device description from AMBA PnP Data Base */
	ambapp_dev *dev = ambapp_get_first_dev(core);
	const char *desc = ambapp_device_id2desc(ambapp_ids,
						 dev->vendor, dev->device);

	return ambapp_prop_str(buf, prop_str_description, desc);
}

int (*ambapp_prop_creators[])(ambapp_core *core, struct prop_std *buf) = {
	ambapp_prop_name,
	ambapp_prop_vendor,
	ambapp_prop_device,
	ambapp_prop_version,
	ambapp_prop_devmsk,
	ambapp_prop_interrupts,
	ambapp_prop_reg,
	ambapp_prop_userdef,
	ambapp_prop_freq,
	ambapp_prop_index,
	ambapp_prop_ampopts,
	ambapp_prop_description,
};
#define PROP_CREATORS_NUM (sizeof(ambapp_prop_creators)/sizeof(void (*)(void)))

/**
 * node_has_child: Go through the children of the node and check if there is
 * a child with the expected name.
 *
 * node       - The parent node
 * child_name - The name of the child to be found
 * name_len   - max length of the child_name
 * out        - If provided it will point to the found child node

 * Return values:
 *  0 = No child found
 *  1 = Found a child
 */
int node_has_child(struct node *node, const char *child_name, int name_len,
		   struct node **out)
{
	struct node *n;
	int proplen = _strnlen(child_name, name_len) + 1;
	char tmp[proplen];

	n = node->child;
	while (n) {
		/* Check if the length of the properties match, then go and
		 * copy the property into tmp and then compare
		 */
		if (no_proplen((int)n, prop_str_name) == proplen &&
			no_getprop((int)n, prop_str_name, tmp) == 1 &&
			_strcmp(tmp, (char*)child_name) == 0) {
			if (out)
				*out = n;
			return 1;
		}
		/* Go on and check the siblings */
		n = n->sibling;
	}
	return 0;
}

#ifdef HASXML
extern struct xml_match xml_matches[];
extern struct xml_node xml_nodes[];
extern int xml_values[];
extern int xml_corehandles[];
extern int xml_nodehandles[];
extern int xml_matchcount;
extern int xml_nodecount;


void xml_nodehandle_apply(struct xml_node *xn, struct node *node)
{
	int i;
	/* Fill in nodehandle values for which this node is the
	 * target
	 */
	for (i = 0; i < xn->nhtarget_count; i++) {
		int vindex = xml_nodehandles[xn->nhtarget_start + i];
		xml_values[vindex] = (int)node;
	}
}

void xml_nodehandle_init(void)
{
	int i;
	struct xml_node *xn;

	for (i = 0; i < xml_nodecount; i++) {
		xn = &xml_nodes[i];
		/* Skip placeholder nodes as we don't know the address */
		if (xn->flags & XMLNODE_PLH)
			continue;

		/* Set value to which this node is target */
		xml_nodehandle_apply(xn, &xn->node);
	}
}

void xml_nodes_init_hook(struct ambapp_bus *ainfo)
{
	ambapp_core *core = ainfo->cores;
	ambapp_core *cn;

       /*
	* Initiate subindex for cores with the same vendor and device by
	* looping through all cores and for each core increasing subindex of
	* all cores behind it of the same type.
	*
	* E.g. the third found uart has its subindex increased once by the
	* first, and once by the second, making its subindex 2 as intended.
	*/
	while (core) {
		ambapp_dev *a = ambapp_get_first_dev(core);
		cn = core->next;
		while (cn) {
			ambapp_dev *b = ambapp_get_first_dev(cn);
			if (a->device == b->device &&
			    a->vendor == b->vendor) {
				cn->subindex_for_xml++;
			}
			cn = cn->next;
		}
		core = core->next;
	}
	xml_nodehandle_init();
}

/*
 * Do modifications to node node according to the specifications of a xml_node
 * xn. Properties are added only if addprop is non-zero.
 *
 * If new properties are added to the core top level, *lastprop will be changed
 * to point to the last added one.
 */
void xml_xmlnode_apply(struct node *node, struct xml_node *xn,
		       int addprop, struct prop_std **lastprop)
{
	struct propa_ptr *xnprops = (struct propa_ptr *)xn->node.props;
	struct prop_std *prop = NULL;
	int pi;

	/* Add, replace or remove core top level properties */
	for (pi = 0; addprop && pi < xn->propcount; pi++) {
		struct propa_ptr *p = &xnprops[pi];

		/* Make sure no property has that name already */
		prop_del(node, p->name);

		/* PO_REMOVE flag indicates removal only */
		if (p->options & PO_REMOVED)
			continue;

		prop = (struct prop_std *)prom_malloc(sizeof(struct prop_std));
		prop->options = PO_PTR | PO_NEXT;
		prop->length = p->length;
		prop->name = p->name;
		prop->next = NULL;
		prop->v.value = p->value;
		prop_add(node, (struct prop *)prop);
	}
	if (prop && lastprop)
		*lastprop = prop;

	/*
	 * Add all new nodes by adding the child of the placeholder
	 * node. All other new nodes are already chained through sibling
	 * and child pointers from that child.
	 */
	node_add_child(node, xn->node.child);

	xml_nodehandle_apply(xn, node);
}

int xml_core_matches(ambapp_core *core, struct xml_match *m)
{
	ambapp_dev *a = ambapp_get_first_dev(core);

	if (!(m->flags & XMLMATCH_CORE))
		return 0;

	if (m->vendor && m->device) {
		/* Match by vendor, device, core subindex */
		if (m->vendor == a->vendor && m->device == a->device &&
		    (m->index == -1 || m->index == core->subindex_for_xml)) {
			return 1;
		}
	} else if (core->index == m->coreindex) {
		/* Match by overall core index */
		return 1;
	}
	return 0;
}

/*
 * Do modifications to node n or core core according to the specifications
 * produced from an -xml <xmlfile> passed xml file.
 *
 * If new properties are added to the core top level, *lastprop will be changed
 * to point to the last added one.
 *
 * A non-zero return value indicates that core should be removed.
 */
int xml_node_create_hook(ambapp_core *core, struct node *node,
			 struct prop_std **lastprop)
{
	int mi;
	struct xml_match *m;
	struct xml_node *xn;

	for(mi = 0; mi < xml_matchcount; mi++) {
		m = &xml_matches[mi];
		if (!xml_core_matches(core, m))
				continue;

		/* Return 1 to indicate core removal */
		if (m->flags & XMLMATCH_REMOVE)
			return 1;

		xn = &xml_nodes[m->placeholder];
		xml_xmlnode_apply(node, xn, 1, lastprop);
	}

	return 0;
}

void xml_create_bus_subnodes(struct ambapp_bus *ainfo)
{
	struct node *node;
	struct xml_match *m;
	struct xml_node *xn;
	int mi;

	for(mi = 0; mi < xml_matchcount; mi++) {
		m = &xml_matches[mi];
		if (!(m->flags & XMLMATCH_BUS))
			continue;

		/* Only additions of nodes are supported. To add support for
		 * adding properties, bus properties needs to be converted from
		 * array properties to linked list properties. */
		node = ainfo->ahbsn[m->bus];
		if (node) {
			xn = &xml_nodes[m->placeholder];
			xml_xmlnode_apply(node, xn, 0, NULL);
		}
	}
}

#else /* !HASXML */

void xml_nodes_init_hook(struct ambapp_bus *ainfo)
{
	;
}

int xml_node_create_hook(ambapp_core *core, struct node *node,
			 struct prop_std **lastprop)
{
	return 0;
}

void xml_create_bus_subnodes(struct ambapp_bus *ainfo)
{
	;
}

#endif /* HASXML */

/* Add a AMBA Core to the end of the node tree */
struct node *ambapp_node_create(ambapp_core *core)
{
	struct node *n;
	int i, size;
	unsigned int prop_buf[64];
	struct prop_std *prop, *buf = (struct prop_std *)prop_buf;

	n = (struct node *)prom_malloc(sizeof(struct node));
	n->child = NULL;
	n->sibling = NULL;
	n->props = NULL;
	prop = NULL;

	for (i = 0; i < PROP_CREATORS_NUM; i++) {
		size = ambapp_prop_creators[i](core, buf);
		if (size == 0)
			continue;
		prop_add_dup(n, buf, size, &prop);
	}
	for (i = 0; ; i++) {
		size = ambapp_fixup_prop(core, buf, i);
		if (size == 0)
			break;
		prop_add_dup(n, buf, size, &prop);
	}

	if (xml_node_create_hook(core, n, &prop))
		return NULL;

	if (prop)
		prop->options |= PO_END;

	return n;
}

struct node *pnodes_amba = NULL;

void ambapp_nodes_init(struct node *b0)
{
	struct ambapp_bus *ainfo = &ambapp_plb;
	struct node *n; int busidx = 0;
	ambapp_core *core = ainfo->cores;

	xml_nodes_init_hook(ainfo);

	/* ambapp0 */
	ambapp_bus_set_root_node(ainfo, 0, b0);

	core = ainfo->cores;
	while (core) {
		n = ambapp_node_create(core);

		if (n && ambapp_node_fixup(core, n) == 0) {

#ifndef FLATOFTREE
			static struct ambapp_bridge_driver *b;
			ambapp_dev *a = ambapp_get_first_dev(core);
		
			/* add it to the bus it belongs to */
			busidx = a->bus_idx;
			if (!ainfo->ahbsn[busidx]) {
				busidx = 0;
			}
			
			switch (a->dev_type) {
			case DEV_APB_SLV:
				if (a->dev_type == DEV_APB_SLV) {
					/* use the ahb-bus-idx that the AHB-APB bridge is connected to */
					if (!ainfo->ahbsn[busidx = a->extra])
						busidx = 0;
				}
				break;
			case DEV_AHB_MST:
			case DEV_AHB_SLV:
				if (core->ahbslv
				    && (!ambapp_is_apbmst(a))
				    && (b = ambapp_bridge_detect((ambapp_ahbdev *)a))) {
					/* bridges connect with their slave */
					a = (ambapp_dev *)core->ahbslv;
					busidx = a->bus_idx;
					/* dev.extra is the bridge to be child */
					if (busidx < a->extra) {
						node_add_child(n, ainfo->ahbsn[a->extra]);
					}
				} 
				break;
			}
#else
			busidx = 0;
#endif

			/* Insert Node last in chain */
			ambapp_bus_add_child_node(ainfo, busidx, n);
		}

		core = core->next;
	}

	xml_create_bus_subnodes(ainfo);
}
#endif
