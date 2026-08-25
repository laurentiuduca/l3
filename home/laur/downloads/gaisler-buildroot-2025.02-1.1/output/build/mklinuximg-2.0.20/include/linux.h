/* PROM definitions copied from Linux
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

#ifndef __LINUX_H__
#define __LINUX_H__

#include <stdint.h>

struct idprom {
	uint8_t id_format;
	uint8_t id_machtype;
	uint8_t id_ethaddr[6];
	int32_t id_date;
	uint32_t id_sernum:24;
	uint8_t id_cksum;
	uint8_t reserved[16];
};

struct linux_nodeops {
	int (*no_nextnode)(int node);
	int (*no_child)(int node);
	int (*no_proplen)(int node, const char *name);
	int (*no_getprop)(int node, const char *name, char *val);
	int (*no_setprop)(int node, const char *name, char *val, int len);
	char * (*no_nextprop)(int node, char *name);
};

struct linux_mlist_v0 {
	struct linux_mlist_v0 *theres_more;
	unsigned int start_adr;
	unsigned num_bytes;
};

struct linux_mem_v0 {
	struct linux_mlist_v0 **v0_totphys;
	struct linux_mlist_v0 **v0_prommap;
	struct linux_mlist_v0 **v0_available;
};

struct linux_bootargs_v2 {
	char **bootpath;
	char **bootargs;
	int *fd_stdin;
	int *fd_stdout;
};

struct linux_arguments_v0 {
	char *argv[8];
	char args[100];
	char boot_dev[2];
	int boot_dev_ctrl;
	int boot_dev_unit;
	int dev_partition;
	char *kernel_file_name;
	void *aieee1;
};

struct linux_dev_v0_funcs {
	int (*v0_devopen)(char *device_str);
	int (*v0_devclose)(int dev_desc);
	int (*v0_rdblkdev)(int dev_desc, int num_blks, int blk_st, char *buf);
	int (*v0_wrblkdev)(int dev_desc, int num_blks, int blk_st, char *buf);
	int (*v0_wrnetdev)(int dev_desc, int num_bytes, char *buf);
	int (*v0_rdnetdev)(int dev_desc, int num_bytes, char *buf);
	int (*v0_rdchardev)(int dev_desc, int num_bytes, int dummy, char *buf);
	int (*v0_wrchardev)(int dev_desc, int num_bytes, int dummy, char *buf);
	int (*v0_seekdev)(int dev_desc, long logical_offst, int from);
};

struct linux_dev_v2_funcs {
	int (*v2_inst2pkg)(int d);
	char* (*v2_dumb_mem_alloc)(char *va, unsigned sz);
	void (*v2_dumb_mem_free)(char *va, unsigned sz);

	char* (*v2_dumb_mmap)(char *virta, int which_io, unsigned paddr,
				unsigned sz);
	void (*v2_dumb_munmap)(char *virta, unsigned size);

	int (*v2_dev_open)(char *devpath);
	void (*v2_dev_close)(int d);
	int (*v2_dev_read)(int d, char *buf, int nbytes);
	int (*v2_dev_write)(int d, char *buf, int nbytes);
	int (*v2_dev_seek)(int d, int hi, int lo);


	void (*v2_wheee2)(void);
	void (*v2_wheee3)(void);
};

struct device_node;

struct linux_romvec {
	unsigned int pv_magic_cookie;
	unsigned int pv_romvers;
	unsigned int pv_plugin_revision;
	unsigned int pv_printrev;
	struct linux_mem_v0 pv_v0mem;
	struct linux_nodeops *pv_nodeops;
	char **pv_bootstr;
	struct linux_dev_v0_funcs pv_v0devops;
	char *pv_stdin;
	char *pv_stdout;
	int (*pv_getchar)(void);
	void (*pv_putchar)(int ch);
	int (*pv_nbgetchar)(void);
	int (*pv_nbputchar)(int ch);
	void (*pv_putstr)(char *str, int len);
	void (*pv_reboot)(char *bootstr);
	void (*pv_printf)(__const__ char *fmt, ...);
	void (*pv_abort)(void);
	__volatile__ int *pv_ticks;
	void (*pv_halt)(void);
	void (**pv_synchook)(void);
	union {
		void (*v0_eval)(int len, char *str);
		void (*v2_eval)(char *str);
	} pv_fortheval;

	struct linux_arguments_v0 **pv_v0bootargs;
	unsigned int (*pv_enaddr)(int d, char *enaddr);
	struct linux_bootargs_v2 pv_v2bootargs;
	struct linux_dev_v2_funcs pv_v2devops;

	int filler[15];

	void (*pv_setctxt)(int ctxt, char *va, int pmeg);
	int (*v3_cpustart)(unsigned int whichcpu, int ctxtbl_ptr,
				int thiscontext, char *prog_counter);
	int (*v3_cpustop)(unsigned int whichcpu);
	int (*v3_cpuidle)(unsigned int whichcpu);
	int (*v3_cpuresume)(unsigned int whichcpu);
};

#endif
