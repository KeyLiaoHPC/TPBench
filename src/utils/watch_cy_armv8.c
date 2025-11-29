#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>

#define _GNU_SOURCE

void
read(uint64_t *val0) {
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
    while(1){
        read(&val0);
        val1 = sched_getcpu();
        printf("Core %llu PMCCNTR_EL0: 0x%x\n", val1, val0);
    }
    return 0;
}
