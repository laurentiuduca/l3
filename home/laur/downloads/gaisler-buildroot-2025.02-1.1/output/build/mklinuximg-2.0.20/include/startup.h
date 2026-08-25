#ifndef __STARTUP_H__
#define __STARTUP_H__

#include <common.h>

extern struct common_data common;

extern unsigned long long _promstartup_stack_start;
extern unsigned long long _promstartup_stack_end;
extern unsigned long long _linux_end_of_memory;
extern unsigned long long _startup_start;
extern unsigned long long _startup_end;
extern unsigned long long _startup_text_start;
extern unsigned long long _startup_text_end;
extern unsigned long long _startup_data_start;
extern unsigned long long _startup_data_end;
extern unsigned long long _startup_bss_start;
extern unsigned long long _startup_bss_end;
extern unsigned long long _startup_heap_start;
extern unsigned long long _startup_heap_end;

void *startup_malloc(int size);

#endif
