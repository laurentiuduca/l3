/* Early boot code. Setup CPU, MMU tables and prepare STARTUP/PROM environment.
 * Most code here are executed in physical address space.
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
#include <laurgptimer.h>

#include <common.h>
#include <linkscript.h>

#ifdef CONFIG_WATCHDOG
#include <prom_watchdog.h>
#endif
/* Must not be overwritten by BSS clearer */
int __attribute__((section(".data"))) first_cpu_booted = 0;
unsigned long __attribute__((section(".data"))) end_of_mem; /* Set by head.S */

unsigned long start_ffd00000_pa;

/* Common Data, initialized by startup_prom_init */
struct common_data *pcommon = 0;

static inline __attribute__((always_inline))
void srmmu_set_mmureg(unsigned long regval)
{
	__asm__ __volatile__(B2B_INLINE_DOUBLE_NOP
			     "sta %0, [%%g0] %1\n\t"
			     B2B_INLINE_DOUBLE_NOP : :
			     "r" (regval), "i" (0x19) : "memory");

}

static inline __attribute__((always_inline))
unsigned long srmmu_get_mmureg(void)
{
	unsigned long retval;
	__asm__ __volatile__("lda [%%g0] %1, %0\n\t" :
			     "=r" (retval) :
			     "i" (0x19));
	return retval;
}

static inline __attribute__((always_inline))
void srmmu_set_ctable_ptr(unsigned long paddr)
{
	paddr = ((paddr >> 4) & 0xfffffff0);
	__asm__ __volatile__(B2B_INLINE_DOUBLE_NOP
			     "sta %0, [%1] %2\n\t"
			     B2B_INLINE_DOUBLE_NOP : :
			     "r" (paddr), "r" (0x00000100),
			     "i" (0x19) :
			     "memory");
}

static inline __attribute__((always_inline))
void srmmu_set_context(int context)
{
	__asm__ __volatile__(B2B_INLINE_DOUBLE_NOP
			     "sta %0, [%1] %2\n\t"
			     B2B_INLINE_DOUBLE_NOP : :
			     "r" (context), "r" (0x00000200),
			     "i" (0x19) : "memory");
}

static inline __attribute__((always_inline)) void leon3_enable_snooping(void)
{
	__asm__ __volatile__ ("lda [%%g0] 2, %%l1\n\t"
			      "set 0x800000, %%l2\n\t"
			      "or  %%l2, %%l1, %%l2\n\t"
			      "sta %%l2, [%%g0] 2\n\t"
			      B2B_INLINE_DOUBLE_NOP
			      : : : "l1", "l2");
}

static inline int leon3_processor_id(void)
{
	int cpuid;
	__asm__ __volatile__("rd %%asr17,%0\n\t"
			     "srl %0,28,%0" :
			     "=&r" (cpuid) : );
	return cpuid;
}

static inline void set_tbr(unsigned long regval)
{
	asm volatile ("mov %0, %%tbr\n\t" : : "r" (regval) : "memory");
}

static inline void leon_flush_cache_all(void)
{
	__asm__ __volatile__("flush\n\t" /* iflush */
			     B2B_INLINE_SINGLE_NOP
			     "sta %%g0, [%%g0] %0\n\t"
			     B2B_INLINE_DOUBLE_NOP : :
			     "i"(ASI_LEON_DFLUSH) : "memory");
}

static inline void leon_flush_tlb_all(void)
{
	leon_flush_cache_all();
	__asm__ __volatile__(B2B_INLINE_DOUBLE_NOP
			     "sta %%g0, [%0] %1\n\t"
			     B2B_INLINE_DOUBLE_NOP
			     : : "r"(0x400),
			     "i"(ASI_LEON_MMUFLUSH) : "memory");
}

void boot_init(void)
{
	/* disable mmu, should not be enabled at this point */
	srmmu_set_mmureg(0x00000000);
	__asm__ __volatile__("flush\n\t");
}

void  
putsx (unsigned char *s)
{
    volatile unsigned int *ubase; 
        ubase = (volatile unsigned int *) 0x80000100;
        while (s[0] != 0) {
            while ((ubase[1] & 4) == 0) {
                ;  
            }
            ubase[0] = *s;
            s++;
        }
}

void puthex (unsigned int h)
{
  volatile unsigned int *ubase;
  int i = 0;
  volatile char b[9], *s;
  for (i = 0;i < 8;i++,h<<=4) {
    char c = ((h & 0xf0000000) >> 28) & 0xf;
    if (c >= 10) {
      c += 'a' - 10;
    } else {
      c += '0';
    }
    b[i] = c;
  }
  b[8] = 0;
  s = b;
  putsx(b);
}

void boot_clr_region(unsigned long long *start, unsigned long long *end)
{
    //putsx("clear ");
    //puthex(start);
    //puthex(end);
	while (start < end) {
		*start = 0;
		start++;
	}
}

void cache_init(void)
{
	leon3_enable_snooping();
}

void fpu_init(void)
{
	/*
	 * Disable the FPU - This gives early warning of violations against the
	 * policy that mklinuximg should support systems with no FPU without
	 * need for soft floats.
	 */
	__asm__ __volatile__ ("mov %%psr, %%l1\n\t"
			      "set 0x1000, %%l2\n\t"
			      "not %%l2, %%l2\n\t"
			      "and %%l1, %%l2, %%l1\n\t"
			      "mov %%l1, %%psr\n\t"
			      "nop\n\t"
			      "nop\n\t"
			      "nop\n\t"
			      : : : "l1", "l2");
}

/* The Below Code sets up the Virtual Address Space as follows:
 *
 * Only Context 0 valid. Context 0 address map using 3 levels:
 *
 * 0x00000000-0xEFFFFFFF: 1:1 mapping to physical
 * 0xF0000000-0xFBFFFFFF: Map to main memory (low 192MB memory only)
 * 0xFC000000-0xFFCFFFFF: Not Mapped
 * 0xFFD00000-0xFFD3FFFF: Mapped to STARTUP/PROM code (last 128KB of RAM)
 * 0xFFD3FFFF-0xFFFFFFFF: Not Mapped
 */
void mmu_table_init(void)
{
	int i;
	unsigned long page_va_start, page_va_end, page_cnt;
	unsigned long page_pa_start, page_pa_end;

	for (i = 0; i < 256; i++) {
		if (i < 64) {
			_mmu_ctx0_fc_level2[i] = SRMMU_INVALID;
			_mmu_ctx0_ffd_level3[i] = SRMMU_INVALID;
		}
		_mmu_ctx_table[i] = SRMMU_INVALID;
		_mmu_ctx0_level1[i] = SRMMU_INVALID;
	}

	/* Setup Context Table, Context 0, point to level 1 Table */
	_mmu_ctx_table[0] = (((unsigned long)&_mmu_ctx0_level1[0] >> 4) &
				~SRMMU_ET_MASK) | SRMMU_ET_PTD;

	/* Setup Level1 Context0 Address space. 16MB/Entry
	 *
	 * 0x00000000-0xEFFFFFFF: 1:1 mapping (non-cachable)
	 * 0xF0000000-0xFBFFFFFF: Map to RAM memory (low memory only)
	 * 0xFC000000-0xFEFFFFFF: INVALID
	 * 0xFF000000-0xFFFFFFFF: To Level2, see below...
	 */
	for (i = 0; i < 240; i++) {
		_mmu_ctx0_level1[i] = (i << 20) | SRMMU_ACC_S_ALL |
					SRMMU_ET_PTE;
	}
	for (; i < 252; i++) {
		_mmu_ctx0_level1[i] = ((CONFIG_RAM_START >> 4)+((i-240)<<20)) |
					SRMMU_CACHE | SRMMU_ACC_S_ALL |
					SRMMU_ET_PTE;
	}
	for (; i < 255; i++)
		_mmu_ctx0_level1[i] = SRMMU_INVALID;
	_mmu_ctx0_level1[255] = ((unsigned long)&_mmu_ctx0_fc_level2[0] >> 4) |
				SRMMU_ET_PTD;

	/* Setup Level2, Context0, 0xFF000000-0xFFFFFFFF. 256KB/Entry
	 *
	 * 0xFF000000-0xFFD00000: INVALID
	 * 0xFFD00000-0xFFD3FFFF: To Level 3, see below
	 * 0xFFD3FFFF-0xFFFFFFFF: INVALID
	 */
	for (i = 0; i < 64; i++)
		_mmu_ctx0_fc_level2[i] = SRMMU_INVALID;
	_mmu_ctx0_fc_level2[CONFIG_LINUX_OPPROM_SEGMENT] =
		(((unsigned long)&_mmu_ctx0_ffd_level3[0]) >> 4) | SRMMU_ET_PTD;

	/* Setup Level3, Context0, 0xFFD00000-0xFFD3FFFF. 4KB/Entry
	 *
	 * 0xFFD00000-0xFFD3FFFF: Mapped to STARTUP/PROM code (End of RAM)
	 * 0xFFD3FFFF-0xFFFFFFFF: INVALID
	 */
	i = 0;
	page_va_start = (unsigned long)&_startup_start;
	page_va_end = (unsigned long)&_prom_end;
	page_cnt = (page_va_end - page_va_start) >> PAGE_SHIFT;
	page_pa_end = (end_of_mem + (PAGE_SIZE-1)) & ~PAGE_MASK;
	page_pa_start = page_pa_end - (page_cnt << PAGE_SHIFT);
	start_ffd00000_pa = page_pa_start;
	for (i = 0; i < page_cnt; i++) {
		_mmu_ctx0_ffd_level3[i] = (page_pa_start >> 4) | SRMMU_CACHE |
						SRMMU_ACC_S_ALL | SRMMU_ET_PTE;
		page_pa_start += PAGE_SIZE;
	}
	for (; i < 64; i++)
		_mmu_ctx0_ffd_level3[i] = SRMMU_INVALID;
}

static void mmu_init(void)
{
	/* Setup MMU */
	srmmu_set_ctable_ptr((unsigned long)&_mmu_ctx_table[0]);
	srmmu_set_context(0);
	__asm__ __volatile__("flush\n\t");

	/* Flush TLB */
	leon_flush_tlb_all();

	/* Enable MMU */
	srmmu_set_mmureg(0x00000001);

	/* Flush All Cache */
	leon_flush_cache_all();
}

void startup_prom_init(void)
{
	unsigned long long *src, *dst, *end;

	/* Clear BSS and Heaps */
	boot_clr_region(&_startup_heap_start, &_promstartup_move_start_va);
	boot_clr_region(&_promstartup_move_end_va, &_prom_heap_end);

	/* Relocate Initialize Startup environment */
	src = &_promstartup_move_start;
	dst = &_promstartup_move_start_va;
	end = &_promstartup_move_end;
	while (src < end) {
		*dst = *src;
		src++;
		dst++;
	}
}
#if 0
#define abc(x) ((volatile unsigned int *)0x80000100)[x]

#define o0 5
#define n0 1

#define laurputc(val) while ((abc(1) & 4) == 0) { ; } \
            abc(0) = val;

static unsigned int laurcas32(volatile unsigned int *addr,
                                 unsigned int expected,
                                 unsigned int desired)
{
    unsigned int old = desired;

    __asm__ __volatile__(
        "casa [%1] 0xA, %2, %0"
        : "=r"(old)
        : "r"(addr), "r"(expected), "0"(desired)
        : "memory");

    return old;
}
#endif

//------------------------------------------------------------------
#include <stdint.h>

int memtest_access_width(volatile void *base)
{
    volatile uint8_t  *p8  = (volatile uint8_t *)base;
    volatile uint16_t *p16 = (volatile uint16_t *)base;
    volatile uint32_t *p32 = (volatile uint32_t *)base;

    /*------------------------------------------------------------*/
    /* Test 32-bit access                                         */
    /*------------------------------------------------------------*/

    *p32 = 0x11223344;

    if (*p32 != 0x11223344)
        return -1;

    /* Big-endian byte order */

    if (p8[0] != 0x11) return -2;
    if (p8[1] != 0x22) return -3;
    if (p8[2] != 0x33) return -4;
    if (p8[3] != 0x44) return -5;

    /* Big-endian halfwords */

    if (p16[0] != 0x1122) return -6;
    if (p16[1] != 0x3344) return -7;

    /*------------------------------------------------------------*/
    /* Test byte enables                                          */
    /*------------------------------------------------------------*/

    *p32 = 0x00000000;

    p8[0] = 0xAA;
    if (*p32 != 0xAA000000) return -8;

    p8[1] = 0xBB;
    if (*p32 != 0xAABB0000) return -9;

    p8[2] = 0xCC;
    if (*p32 != 0xAABBCC00) return -10;

    p8[3] = 0xDD;
    if (*p32 != 0xAABBCCDD) return -11;

    /*------------------------------------------------------------*/
    /* Test halfword enables                                      */
    /*------------------------------------------------------------*/

    *p32 = 0;

    p16[0] = 0x1234;
    if (*p32 != 0x12340000) return -12;

    p16[1] = 0x5678;
    if (*p32 != 0x12345678) return -13;

    /*------------------------------------------------------------*/
    /* Verify partial writes don't disturb neighbours             */
    /*------------------------------------------------------------*/

    *p32 = 0xFFFFFFFF;

    p8[2] = 0x00;

    if (*p32 != 0xFFFF00FF)
        return -14;

    *p32 = 0xFFFFFFFF;

    p16[1] = 0x0000;

    if (*p32 != 0xFFFF0000)
        return -15;

    /*------------------------------------------------------------*/
    /* Walking byte test                                          */
    /*------------------------------------------------------------*/

    for (int i = 0; i < 4; i++) {

        *p32 = 0;

        p8[i] = 0xFF;

        uint32_t expected;

        switch (i) {
        case 0: expected = 0xFF000000; break;
        case 1: expected = 0x00FF0000; break;
        case 2: expected = 0x0000FF00; break;
        default: expected = 0x000000FF; break;
        }

        if (*p32 != expected)
            return -20 - i;
    }

    return 0;
}
//#if 0
#define TIMERADDR 0x80000300

int gptimer_setuptestlaur(struct laurgptimer *lr, int irq)
{
        int i, j, ntimers;

        ntimers = lr->configreg & 0x7;
        lr->scalerload = -1;
        
        /* enable first timer to make sure the scaler is ticking */
        lr->timer[0].counter = -1;
        lr->timer[0].control = 0x1;

        if (lr->scalercnt == lr->scalercnt) {
            putsx("gptimer error\n");
            return -1;
        }

        return 0;
}

unsigned int gptimer_gettimelaur(struct laurgptimer *lr)
{
    int i, inc, s=0, n;

    inc = (0xffffffff - lr->timer[0].counter);
    n = (lr->scalerload+1) + (lr->scalerload - lr->scalercnt);
    for(i = 0; i < n; i++)
        s += inc;

    return s;
    //return (0xffffffff - lr->timer[0].counter) * (lr->scalerload+1) + (lr->scalerload - lr->scalercnt);
}
//#endif
void cache_disable() 
{
  asm(" sta %g0, [%g0] 2; nop; nop ");
}

void cache_enable()
{
  asm(" set 0x81000f, %o0; sta %o0, [%g0] 2 ");
}

/* C Entry point
 *
 *  - Setup basic MMU Tables for Linux boot and STARTUP/PROM code
 *  - Init Cache and MMU
 *  - Initialize STARTUP/PROM environment (in virtual addresses)
 *  - Run STARTUP/PROM initialization
 *  - Boot into Linux kernel
 */
int boot_main(int arg0, int arg1, int arg2)
{
	unsigned long ps_move_va;

    //volatile unsigned int x = o0;
    //unsigned int old=0x31;
    //putsx("laurputc");
    //laurputc(old);
    //old = laurcas32(&x, o0, 20);
    //puthex(old); puthex(x);

        int ret=0, r=0;
        //struct laurgptimer *lr = (struct laurgptimer*)TIMERADDR;
        //unsigned int ticks=0;
        //r=gptimer_setuptestlaur(lr, 8);
        //cache_disable();
        ret = memtest_access_width((void *)0x45000000);
        //cache_enable();
        //ticks = gptimer_gettimelaur(lr); 
        putsx("ret="); puthex(ret);
        //putsx("r="); puthex(r);
        //putsx("ticks="); puthex(ticks);

	boot_init();

	cache_init();

    // laur
	//fpu_init();

	/* Only Master CPU initialize early MMU Table */
	if (first_cpu_booted == 0) {
		mmu_table_init();
    }
    
    putsx("args \n"); puthex(arg0);
    putsx("args \n"); puthex(arg1);
    putsx("args \n"); puthex(arg2);
     
	mmu_init();

	/* MMU is now. Virtual=Physical address where we are executing the
	 * boot/head code.
	 *
	 * Master CPU continue setup before entering kernel.
	 *
	 * Slave CPUs assume Linux is already running
	 */
	if (first_cpu_booted == 0) {
		first_cpu_booted = 1;

		/* Init STARTUP/PROM Environment */
        //putsx("startup_prom_init\n");
		startup_prom_init();
        
        //putsx("after startup_prom_init\n");
		/* ps_move section virtual address */
		ps_move_va = start_ffd00000_pa +
				((unsigned long)&_promstartup_move_start_va -
				(unsigned long)&_startup_start);

		/* Calculate address of common (virtual address) */
		pcommon = (void *)(ps_move_va +
				((unsigned long)&_prom_data_start -
				(unsigned long)&_promstartup_move_start));

		/* Safe to access common structure now */
		pcommon->arg0 = arg0;
		pcommon->arg1 = arg1;
		pcommon->arg2 = arg2;
		pcommon->end_of_mem = end_of_mem;
		pcommon->boot_cpu = leon3_processor_id();
		pcommon->end_of_mem_linux_pa = ps_move_va +
			((unsigned long)&_linux_end_of_memory -
			(unsigned long)&_promstartup_move_start);
#ifdef CONFIG_NO_LINUX
		pcommon->linux__bss_start_va = 0;
		pcommon->linux__bss_stop_va = 0;
#else
		pcommon->linux__bss_start_va = (unsigned long)&__bss_start;
		pcommon->linux__bss_stop_va = (unsigned long)&__bss_stop;
#endif
        //putsx("after boot main\n");
		return 1; /* Run Startup/PROM code */
	} else
		return 0; /* Continue boot kernel directly */
}

void boot_kernel(void)
{
	void (*kernel)(int arg0, int arg1);

#ifdef CONFIG_WATCHDOG
	/* Ping watchdog before booting Linux */
	watchdog_ping(PING_ID_BOOT);
#endif

	/* Boot into Linux kernel (Virtual Address). Assume Reset-trap vector
	 * is entry point.
	 */
	kernel = (void (*)(int, int)) KERNBASE_ENTRY_VA;
#ifdef CONFIG_NO_LINUX
	asm volatile ("ta 0x1" : : ); /* No point in going further, so stop */
#else
	set_tbr((unsigned long)kernel);
	kernel(pcommon->karg0, pcommon->karg1);
#endif
}

#ifdef FORCE_GRMON_LOAD_SYM
char _force_grmon_load_sym[] = FORCE_GRMON_LOAD_SYM;
#endif
