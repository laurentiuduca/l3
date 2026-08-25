/* SYMBOLS PROVIDED BY THE LINKER SCRIPT */

#ifndef __LINKSCRIPT_H__
#define __LINKSCRIPT_H__

/* MMU Tables provided by the Linker script */
extern unsigned long _mmu_ctx_table[256];
extern unsigned long _mmu_ctx0_level1[256];
extern unsigned long _mmu_ctx0_fc_level2[64];
extern unsigned long _mmu_ctx0_ffd_level3[64];

/* Linux kernel BSS (Virtual Addresses) */
extern char *__bss_start;
extern char *__bss_stop;

/* Location of Startup/PROM code */
extern int _startup_prom_start;
extern int _startup_prom_end;

extern unsigned long long _promstartup_move_start;
extern unsigned long long _promstartup_move_end;
extern unsigned long long _promstartup_move_start_va;
extern unsigned long long _promstartup_move_end_va;
extern unsigned long long _startup_heap_start;
extern unsigned long long _startup_heap_end;
extern unsigned long long _prom_heap_start;
extern unsigned long long _prom_heap_end;
extern unsigned long long _startup_start;
extern unsigned long long _prom_end;
extern unsigned long long _prom_data_start;
extern unsigned long long _linux_end_of_memory;

/* Used from virtual image */
extern unsigned long long _promstartup_stack_start;
extern unsigned long long _promstartup_stack_end;
extern unsigned long long _linux_end_of_memory;
extern unsigned long long _prom_start;
extern unsigned long long _prom_text_start;
extern unsigned long long _prom_text_end;
extern unsigned long long _prom_data_start;
extern unsigned long long _prom_data_end;
extern unsigned long long _prom_bss_start;
extern unsigned long long _prom_bss_end;
extern unsigned long long _prom_heap_start;
extern unsigned long long _prom_heap_end;
extern unsigned long long _prom_end;

#endif
