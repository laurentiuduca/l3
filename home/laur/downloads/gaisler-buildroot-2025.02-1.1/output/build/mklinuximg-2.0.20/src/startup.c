/* STARTUP PROM initilization that will be overwritten when Linux boots.
 */

#include <common.h>
#include <prom.h>
#include <startup.h>
#include <ambapp.h>
#include <prom_watchdog.h>
#ifdef CONFIG_HRTIMERS
#include <cpucounter.h>
#endif

static unsigned long startup_malloc_end = (unsigned long)&_startup_heap_end;

/* Grow Downwards malloc, no free routine, memory overwritten by Linux ,
 * minimum aligned to 4bytes
 */
void *startup_malloc(int size)
{
	size = (size + 3) & ~3;
	if ((startup_malloc_end - size) < (unsigned long)&_startup_heap_start) {
		/*
		 * ERROR: Resize startup heap size with -startupheap option that
		 * sets up CONFIG_STARTUP_HEAP_SIZE.
		 */
		while (1)
			;
	}
	startup_malloc_end -= size;
	return (void *)startup_malloc_end;
}

/*** Clear Linux BSS ***/
void linux_bss_init(void)
{
	unsigned long long *l;
	unsigned char *c;
	unsigned long end;

	/* Clear double word-wise */
	end = common.linux__bss_stop_va;
	l = (unsigned long long *)common.linux__bss_start_va;
	if (((unsigned long)l & 0x7) == 0) {
		while ((unsigned long)l < (end & ~0x7)) {
			*l = 0;
			l++;
		}
	}
	/* clear last bytes, byte-wise */
	c = (unsigned char *)l;
	while ((unsigned long)c < common.linux__bss_stop_va) {
		*c = (unsigned char)0;
		c++;
	}
}

/* Entry Point  */
void __attribute__ ((__section__(".startup.main.text")))
startup_main(void)
{
	/* Set Kernel Arguments */
	common.karg0 = (int)&romvector;
	common.karg1 = 0;

	/* Clear kernel BSS */
	linux_bss_init();

	/* Do AMBA scanning, create Devices and pair device together into
	 * cores
	 */
	ambapp_init();

	/* Do system initialization if needed. GPTIMER driver should find
	 * frequency of at least one bus.
	 */
	ambapp_drivers_init();

#ifdef CONFIG_WATCHDOG
	/* Init watchdog */
	watchdog_init();
#endif

#ifdef CONFIG_HRTIMERS
	LEON_UP_COUNTER_INIT();
#endif
	/* Create PROM nodes of all AMBA cores */
	ambapp_nodes_init(opprom_get_ambapp0());

	/* Init PROM structure */
	opprom_init();
}
