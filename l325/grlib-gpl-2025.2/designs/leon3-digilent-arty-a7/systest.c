//#include <stdio.h>
#include "gptimer.h"

#define abc(x) ((volatile unsigned int *)0x80000100)[x]
#define r(x) ((volatile unsigned int *)0x40000000)[x]

#define laurputc(val) while ((abc(1) & 4) == 0) { ; } \
            abc(0) = val;
//int laur();
#if 0
//----------------------------------------------------------------
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;

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
//------------------------------------------------------------------
//#include <stdint.h>

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

#define TIMERADDR 0x80000300

void gptimer_setuptestlaur(struct gptimer *lr, int irq)
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
}

unsigned int gptimer_gettimelaur(struct gptimer *lr)
{
    int i, inc, s=0, n;

    inc = (0xffffffff - lr->timer[0].counter);
    n = (lr->scalerload+1) + (lr->scalerload - lr->scalercnt);
    for(i = 0; i < n; i++)
        s += inc;

    return s;
    //return (0xffffffff - lr->timer[0].counter) * (lr->scalerload+1) + (lr->scalerload - lr->scalercnt);
}

void cache_disable() 
{
  asm(" sta %g0, [%g0] 2 ");
}

void cache_enable()
{
  asm(" set 0x81000f, %o0; sta %o0, [%g0] 2 ");
}
#endif
//-----------------------------------------------------------------------
int main()
{
    //int ret=0;
    laurputc(0x33);

        //struct gptimer *lr = (struct gptimer*)TIMERADDR;
        //unsigned int ticks=0;
        //gptimer_setuptestlaur(lr, 8);
        //ret = memtest_access_width(0x45000000);
        //putsx("ret="); puthex(ret);
        //ticks = gptimer_gettimelaur(lr); 
        //putsx("ticks="); puthex(ticks);    

    //putchar('\n');
	//printf("Hello World\n");

    //laur();
    while(1);

    return 0;
}

