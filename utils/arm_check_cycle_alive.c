#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>

#define _GNU_SOURCE

void
cycle_read(uint64_t *val0) {
    asm volatile(
        "mrs %0, PMCCNTR_EL0" "\n\t"
        : "=r" (*val0)
        :
        :
    );

}


int
main(void) {
    uint64_t val0, val1;
    asm volatile(
        "mrs %0, pmcr_el0"    "\n\t"
        "mrs %1, pmcntenset_el0"    "\n\t"
        : "=r" (val0), "=r" (val1)
        :
        :
    );
    printf("PMCR_EL0: 0x%x, PMCNTENSET_EL0: 0x%x\n", val0, val1);

    cycle_read(&val0);
    sleep(1);
    cycle_read(&val1);
    printf("\n%d 0x%x, 0x%x \n", sched_getcpu(), val0, val1);
    return 0;
}
