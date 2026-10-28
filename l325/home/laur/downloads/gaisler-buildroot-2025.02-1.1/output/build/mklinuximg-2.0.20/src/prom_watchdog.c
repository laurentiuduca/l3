#include <config.h>
#ifdef CONFIG_WATCHDOG
#include <prom_watchdog.h>
#include <ambapp.h>
#include <ambapp_ids.h>
#include <gptimer.h>
#include <grwatchdog.h>
#include <prom.h>
/* Allowed number of pings when building the device tree */
#define MAX_PROM_NO_PING_CNT 500

/* Allowed number of pings on UART transmission */
#define MAX_UART_PING_CNT 2000

/* Allowed number of pings on boot */
#define MAX_BOOT_PING_CNT 1

#define TIMER_IDX_INVALID -1

/**
 * struct ping_counters
 *
 * curr - The current ping count
 * max  - The maximum ping count
 */
struct ping_counters {
	unsigned int curr;
	const unsigned int max;
};

/**
 * struct watchdog_dev
 *
 * The watchdog device interface.
 *
 * ambapp_device_id - The amba plug n play id of the device
 * core_idx - The index of the core that is handled
 * priv - The private data of the device
 * init - Called on init (after ambapp_init) (can be NULL)
 * ping - The ping function called from different sources (can be NULL)
 * set_timeout - Updates the timeout of the watchdog device (can be NULL)
 * prop_conf - Contains property information
 */
struct watchdog_dev {
	const unsigned int ambapp_device_id;
	const unsigned int core_idx;
	void *priv;
	void (*init)(const struct watchdog_dev *self);
	void (*ping)(const struct watchdog_dev *self);
	void (*set_timeout)(const struct watchdog_dev *self, unsigned int tval);
	struct watchdog_prop_conf *prop_conf;
};

/**
 * struct watchdog
 *
 * Connects the ping count structure with the watchdogs
 *
 * ping_counters - Holds current/max count of the different ping sources
 * wdogs         - The watchdog devices
 */
struct watchdog {
	struct ping_counters ping_counters[PING_ID_MAX];
	const struct watchdog_dev *wdogs[];
};

/* GPTIMER Watchdog */
#ifdef CONFIG_GPTIMER_WDT

/* Hardware timeout for the GR740 PLL watchdog  */
#define GPTIMER_WATCHDOG_GR740_HW_TIMEOUT 2000
/* Reload bit of the timer's ctrl register */
#define GPTIMER_CTRL_RD 4

/**
 * struct gptimer_wdt_priv
 * Private data of the gptimer watchdog.
 *
 * timer_idx - The index of the last timer of the first GPTIMER core.
 *             Is -1 (TIMER_IDX_INVALID) if initialized.
 * */
struct gptimer_wdt_priv {
	int timer_idx;
};

static struct gptimer_wdt_priv gptimer_wdt_priv = {
	.timer_idx = TIMER_IDX_INVALID
};

struct watchdog_prop_conf gptimer_wdt_prop_conf = {
	.user_timeout_sec = WATCHDOG_TIMEOUT_NOT_SET,
	.hw_timeout_ms = WATCHDOG_TIMEOUT_NOT_SET,
	.compatible = "gaisler,gptimer-wdt",
};

static void gptimer_wdt_init(const struct watchdog_dev *self)
{
	struct gptimer_wdt_priv *priv = self->priv;
	struct watchdog_prop_conf *prop_conf = self->prop_conf;
	int timer_idx = TIMER_IDX_INVALID;
	unsigned int hw_timeout_ms = WATCHDOG_TIMEOUT_NOT_SET;

	/* Sanity check the internal structure */
	if (self->ambapp_device_id != GAISLER_GPTIMER || self->core_idx != 0)
		return;

	/* Make sure that gptimer has been inited before use */
	if (!gptimer_inited)
		return;

	/* Fetch the nbr of timers from GPTIMER0 config register */
	timer_idx = BYPASS_LOAD_PA(&gptimer->config) & GPTIMER_CONF_TIMERS_MASK;

	if (timer_idx <= (sizeof(gptimer->timer) / sizeof(gptimer->timer[0]))) {
		/* The watchdog timer is connected to the last found timer */
		timer_idx = timer_idx - 1;
	}

	priv->timer_idx = timer_idx;

	/* Determine the hw_timeout*/
	if (amba_systemid_is_gr740()) {
		/* GR740 has a PLL watchdog with a timeout of 2 seconds */
		hw_timeout_ms = GPTIMER_WATCHDOG_GR740_HW_TIMEOUT;
	} else if (timer_idx != TIMER_IDX_INVALID) {
		/* Otherwise read the reload value and use it as timeout */
		hw_timeout_ms =
			BYPASS_LOAD_PA(&gptimer->timer[timer_idx].rld);
		hw_timeout_ms /= 1000;
	}

	/* Set the hw timeout. */
	prop_conf->hw_timeout_ms = hw_timeout_ms;

	/* Set the user timeout */
#ifdef CONFIG_GPTIMER_WDT_USER_TIMEOUT
	prop_conf->user_timeout_sec = CONFIG_GPTIMER_WDT_USER_TIMEOUT;
#endif

}

static void gptimer_wdt_ping(const struct watchdog_dev *self)
{
	struct gptimer_timer *timer;
	struct gptimer_wdt_priv *priv = self->priv;

	if (self->ambapp_device_id != GAISLER_GPTIMER || self->core_idx != 0)
		return;

	if (priv->timer_idx == TIMER_IDX_INVALID)
		return;

	timer = &gptimer->timer[priv->timer_idx];
	BYPASS_STORE_PA(&timer->ctrl,
			BYPASS_LOAD_PA(&timer->ctrl) | GPTIMER_CTRL_RD);
}

static void gptimer_wdt_set_timeout(const struct watchdog_dev *self,
				    unsigned int timeout_ms)
{
	struct gptimer_timer *timer;
	struct gptimer_wdt_priv *priv = self->priv;

	if (self->ambapp_device_id != GAISLER_GPTIMER || self->core_idx != 0)
		return;

	if (priv->timer_idx == TIMER_IDX_INVALID)
		return;

	timer = &gptimer->timer[priv->timer_idx];

	/* Set the reload value */
	BYPASS_STORE_PA(&timer->rld, timeout_ms * 1000);
	/* Reload the timer */
	BYPASS_STORE_PA(&timer->ctrl,
		BYPASS_LOAD_PA(&timer->ctrl) | GPTIMER_CTRL_RD);
}

static struct watchdog_dev gptimer_wdog = {
	.ambapp_device_id = GAISLER_GPTIMER,
	.core_idx = 0,
	.priv = &gptimer_wdt_priv,
	.init = gptimer_wdt_init,
	.ping = gptimer_wdt_ping,
	.set_timeout = gptimer_wdt_set_timeout,
	.prop_conf = &gptimer_wdt_prop_conf,
};

#endif /* GPTIMER Watchdog */

/* GRWATCHDOG */

#ifdef CONFIG_GRWATCHDOG_WDT

/* Overwritten by grwatchdog_init in startup_ambapp.c */
int grwatchdog_inited = 0;
struct grwatchdog_regs *grwatchdog = NULL;

struct grwatchdog_wdt_priv {
	int last_chtog;
	struct grwatchdog_regs *regs;
};

static struct grwatchdog_wdt_priv grwatchdog_wdt_priv;

static void grwatchdog_wdt_init(const struct watchdog_dev *self)
{
	struct grwatchdog_wdt_priv *priv = self->priv;

	if (self->ambapp_device_id != GAISLER_GRWATCHDOG || self->core_idx != 0)
		return;

	priv->last_chtog = -1;

	if (grwatchdog_inited)
		priv->regs = grwatchdog;

}

static void grwatchdog_wdt_ping(const struct watchdog_dev *self)
{
	struct grwatchdog_regs *wdog_regs;
	struct grwatchdog_wdt_priv *priv = self->priv;
	unsigned char chal, resp, chtog;
	unsigned int regval;

	if (self->ambapp_device_id != GAISLER_GRWATCHDOG || self->core_idx != 0)
		return;

	wdog_regs = priv->regs;

	if (!wdog_regs)
		return;

	/* Challenge time! */
	regval = BYPASS_LOAD_PA(&wdog_regs->status);

	/* Do we have a new challenge? Otherwise just bail out */
	chtog = regval & GRWATCHDOG_CHTOG_BIT ? 1 : 0;

	if (chtog == priv->last_chtog)
		return;

	priv->last_chtog = chtog;

	/* Read the current challenge value from the status register */
	chal = regval & GRWATCHDOG_CHAL_RESP_BITMASK;

	/*
	 * Calculate the response by rotating the value two bit positions
	 * to the left and inverting the lower two bits.
	 */
	resp = ((chal & 0x3f) << 2) | ((~chal & 0xc0) >> 6);

	/* Clear the old response */
	regval = BYPASS_LOAD_PA(&wdog_regs->resp);
	regval &= ~GRWATCHDOG_CHAL_RESP_BITMASK;

	/* Update the response */
	BYPASS_STORE_PA(&wdog_regs->resp, regval | resp);
}

static const struct watchdog_dev grwatchdog_wdog = {
	.ambapp_device_id = GAISLER_GRWATCHDOG,
	.core_idx = 0,
	.priv = &grwatchdog_wdt_priv,
	.init = grwatchdog_wdt_init,
	.ping = grwatchdog_wdt_ping,
	.set_timeout = NULL,
	.prop_conf = NULL,
};

#endif /* GRWATCHDOG */

static struct watchdog wdt_priv = {
	.ping_counters = {
		[PING_ID_PROM_NO] = { .curr = 0, .max = MAX_PROM_NO_PING_CNT},
		[PING_ID_UART] = { .curr = 0, .max = MAX_UART_PING_CNT},
		[PING_ID_BOOT] = { .curr = 0, .max = MAX_BOOT_PING_CNT},
	},
	.wdogs = {
#ifdef CONFIG_GPTIMER_WDT
		&gptimer_wdog,
#endif
#ifdef CONFIG_GRWATCHDOG_WDT
		&grwatchdog_wdog,
#endif
		NULL,
	}
};

#define FOR_EACH_WDOG(dog, wdog_list) \
	const struct watchdog_dev **dog = wdog_list; \
	for (; *dog; dog++)

void watchdog_init(void)
{
	FOR_EACH_WDOG(wdog, wdt_priv.wdogs)
		if ((*wdog)->init)
			(*wdog)->init(*wdog);
}

void watchdog_ping(enum ping_id id)
{
	struct ping_counters *ping_counters;

	if (id < 0 || id >= PING_ID_MAX)
		return;

	ping_counters = &wdt_priv.ping_counters[id];

	if (ping_counters->curr < ping_counters->max) {
		FOR_EACH_WDOG(wdog, wdt_priv.wdogs)
			if ((*wdog)->ping)
				(*wdog)->ping(*wdog);

		ping_counters->curr++;
	}
}

void watchdog_set_timeout(unsigned int timeout_ms)
{
	FOR_EACH_WDOG(wdog, wdt_priv.wdogs)
		if ((*wdog)->set_timeout)
			(*wdog)->set_timeout(*wdog, timeout_ms);

}

struct watchdog_prop_conf *watchdog_get_prop_conf(unsigned int ambapp_device_id,
						  unsigned int core_idx)
{
	FOR_EACH_WDOG(wdog, wdt_priv.wdogs)
		if ((*wdog)->ambapp_device_id == ambapp_device_id &&
		    (*wdog)->core_idx == core_idx)
			    return (*wdog)->prop_conf;

	return NULL;
}
#endif
