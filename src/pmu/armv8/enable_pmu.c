#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/module.h>

// PMUSERENR_EL0
#define PMUSER_ON   0x00000001
// PMCR_EL0
// Long cycle counter enabled
#define PMCR_LC     0x00000030
// Disable cycle counter when event counting is prohibited.
#define PMCR_DP     0x00000020
// Enable export of events in an IMPLEMENTATION DEFINED event stream.
#define PMCR_X      0x00000010
// Clock divider. 0: 1/1, 1: 1/64
#define PMCR_D      0x00000008
// Cycle counter reset.
#define PMCR_C      0x00000004
// Event counter reset.
#define PMCR_P      0x00000002
// Enable
#define PMCR_E      0x00000001
// PMCNTENCLR_EL0

static void
enable_counters(void *p){
    u32 reg_in = 0, reg_out = 0;
    u64 reg_in64 = 0, reg_out64 = 0;

    printk("=== On CPU #%d ===", smp_processor_id());
    if(smp_processor_id() == 0) printk("Enabling PMUSERENR_EL0.");
    reg_in= reg_in | PMUSER_ON;
    asm volatile(
        "msr pmuserenr_el0, %1"     "\n\t"
        "mrs %0, pmuserenr_el0"     "\n\t"
        : "=r" (reg_out)
        : "r" (reg_in)
        );
    if(smp_processor_id() == 0) printk("PMUSERENR_EL0 = %x", reg_out);
    asm volatile("mrs %0, pmcr_el0" : "=r" (reg_out));
    if(smp_processor_id() == 0) printk("PMCR_EL0 = %x", reg_out);
    if(smp_processor_id() == 0) printk("Setting PMCR_EL0.");
    // Enable, reset counter, resert cycle counter, long counter
    reg_in = 0x47;
    if(smp_processor_id() == 0) printk("reg_in = %x", reg_in);
    asm volatile(
        "msr pmcr_el0, %1"      "\n\t"
        "mrs %0, pmcr_el0"      "\n\t"
        : "=r" (reg_out)
        : "r" (reg_in)
        ); 
    if(smp_processor_id() == 0) printk("PMCR_EL0 = %x", reg_out);
    reg_in = 0x8000ffff;
    asm volatile(
        "msr pmcntenset_el0, %1"    "\n\t"
        "mrs %0, pmcntenset_el0"    "\n\t"
        : "=r" (reg_out)
        : "r" (reg_in)
        );
    if(smp_processor_id() == 0) printk("PMCNTENCLR = %x", reg_out);
}

static void
disable_counters(void *p){
    u32 reg_in = 0;
    asm volatile("msr pmuserenr_el0, %0": : "r" (reg_in));
    asm volatile("msr pmcr_el0, %0": : "r" (reg_in));
}

int init(void){
    on_each_cpu(enable_counters, NULL, 1);
    printk("PMU is on.");
    return 0;
}


void fini(void){
    on_each_cpu(disable_counters, NULL, 1);
    printk("PMU is off.");
}


MODULE_AUTHOR("Key Liao");
MODULE_LICENSE("Dual GPLv3");
MODULE_DESCRIPTION("Enables user-mode access to ARMv8-a PMU counters");
MODULE_VERSION("1.0");
module_init(init);
module_exit(fini);
