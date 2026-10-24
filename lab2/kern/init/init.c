#include <console.h>
#include <defs.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <dtb.h>
#include <slub.h>
#include <member3_check.h>

void member2_page_check(void);

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);
void print_kerninfo(void);

/* *
 * print_kerninfo - print the information about kernel, including the location
 * of kernel entry, the start addresses of data and text segements, the start
 * address of free memory and how many memory that kernel has used.
 * */
void print_kerninfo(void) {
    extern char etext[], edata[], end[];
    cprintf("Special kernel symbols:\n");
    cprintf("  entry  0x%016lx (virtual)\n", (uintptr_t)kern_init);
    cprintf("  etext  0x%016lx (virtual)\n", etext);
    cprintf("  edata  0x%016lx (virtual)\n", edata);
    cprintf("  end    0x%016lx (virtual)\n", end);
    cprintf("Kernel executable memory footprint: %dKB\n",
            (end - (char*)kern_init + 1023) / 1024);
}

int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);

    print_kerninfo();

    // grade_backtrace();
    pmm_init();  // init physical memory management
    /* member2_page_check asserts Best-Fit specific choices (exact physical
     * address of the best block, one-call allocation of the whole free
     * region), so it must not run under Buddy. member3_buddy_test covers the
     * page layer for the Buddy configuration instead. */
#if !defined(USE_BUDDY_PMM)
    member2_page_check();
#endif
    slub_init();
    slub_check();
#if defined(USE_MEMBER3_TESTS)
#if defined(USE_BUDDY_PMM)
    member3_buddy_test();
#endif
    member3_slub_test();
#endif

    /* do nothing */
    while (1)
        ;
}

