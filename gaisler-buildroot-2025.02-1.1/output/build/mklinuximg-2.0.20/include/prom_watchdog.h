/* Watchdog functions
 *
 * (C) Copyright 2024 Frontgrade Gaisler
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

#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include <config.h>

#ifdef CONFIG_WATCHDOG

#define WATCHDOG_TIMEOUT_NOT_SET 0

enum ping_id {
    PING_ID_PROM_NO,
    PING_ID_UART,
    PING_ID_BOOT,
    PING_ID_MAX
};

struct watchdog_prop_conf {
	unsigned int user_timeout_sec;
	unsigned int hw_timeout_ms;
	const char *compatible;
};

void watchdog_init(void);
void watchdog_ping(enum ping_id id);
void watchdog_set_timeout(unsigned int waittime_ms);

struct watchdog_prop_conf *watchdog_get_prop_conf(unsigned int ambapp_device_id,
						  unsigned int core_idx);
#endif

#endif
