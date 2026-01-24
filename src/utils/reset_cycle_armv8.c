#include <stdio.h>
#include <stdint.h>

void
read(uint64_t *val0, uint64_t *val1) {
    asm volatile(
        "mrs %0, PMCR_EL0" "\n\t"
        "mrs %1, PMCCNTR_EL0" "\n\t"
        : "=r" (*val0), "=r" (*val1)
        :
        :
    );

}

int
main(void) {
    uint64_t val0, val1;
    read(&val0, &val1);
    printf("PMCR_EL0: 0x%x\n", val0);
    printf("PMCCNTR_EL0: 0x%x\n", val1);
    //val0 = 1;
    val0 = 0;
    val0 = val0 | (1 << 31);
    asm volatile(
        "mrs x22, PMCR_EL0" "\n\t"
        "orr x22, x22, #0x7" "\n\t"
        "msr PMCR_EL0, x22"   "\n\t"
        "msr PMCNTENSET_EL0, %0"   "\n\t"
        :
        : "r" (val0)
        :
    );
    read(&val0, &val1);
    printf("PMCR_EL0: 0x%x\n", val0);
    printf("PMCCNTR_EL0: 0x%x\n", val1);
    return 0;
}
