/* Extra tests are injected into an isolated build, never the submission init. */
#include <assert.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <riscv.h>
#include <stdio.h>
#include <../sync/sync.h>

extern uintptr_t lab3_probe_registers(void);

void lab3_probe_exceptions(void) {
    assert(!(read_csr(sstatus) & SSTATUS_SIE));
    assert(lab3_probe_registers() == 0);
    cprintf("[extra] all GPRs, SP and S-mode return: PASS\n");

    asm volatile(".option push\n.option rvc\n.2byte 0x9002\n.option pop" ::: "memory");
    cprintf("[extra] 16-bit breakpoint return: PASS\n");
    asm volatile(".option push\n.option norvc\n.4byte 0x00100073\n.option pop" ::: "memory");
    cprintf("[extra] 32-bit breakpoint return: PASS\n");
    asm volatile(".2byte 0x0000" ::: "memory");
    cprintf("[extra] 16-bit illegal instruction return: PASS\n");
    asm volatile(".4byte 0x0000007b" ::: "memory");
    cprintf("[extra] 32-bit illegal instruction return: PASS\n");
}

void lab3_probe_sync(void) {
    bool outer, inner;
    size_t before = nr_free_pages();
    assert(read_csr(sstatus) & SSTATUS_SIE);

    local_intr_save(outer);
    assert(outer && !(read_csr(sstatus) & SSTATUS_SIE));
    local_intr_save(inner);
    assert(!inner);
    struct Page *page = alloc_page();
    assert(page != NULL && !(read_csr(sstatus) & SSTATUS_SIE));
    free_page(page);
    local_intr_restore(inner);
    assert(!(read_csr(sstatus) & SSTATUS_SIE));
    local_intr_restore(outer);
    assert(read_csr(sstatus) & SSTATUS_SIE);

    page = alloc_page();
    assert(page != NULL && (read_csr(sstatus) & SSTATUS_SIE));
    free_page(page);
    assert((read_csr(sstatus) & SSTATUS_SIE) && nr_free_pages() == before);
    cprintf("[extra] nested interrupt state and page wrappers: PASS\n");
}
