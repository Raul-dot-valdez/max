/*
 * AI Terra mower HMI — entry point.
 * Nothing vendor-specific here: pal_sys_init() is the port's job.
 */
#include "app_tasks.h"
#include "osal.h"
#include "pal_sys.h"

int main(void)
{
    pal_sys_init();  /* clocks, MPU, caches, FMC SDRAM (+ self-test), .sdram_bss zeroing */
    app_init();      /* storage, services, peripherals, asset cache */
    app_start();     /* tasks */
    osal_start();    /* never returns */
    for (;;) {
    }
}
