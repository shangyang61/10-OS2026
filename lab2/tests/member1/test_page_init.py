"""Run the delivered page_init function with a bounded mock physical memory.

This tests the real C function body; it does not simulate RISC-V execution.
Usage: python3 tests/test_page_init.py (requires native gcc).
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'kern/mm/pmm.c').read_text(encoding='utf-8')
body = source.split('static void page_init(void) {', 1)[1].split('/* pmm_init', 1)[0]
body = 'static void page_init(void) {' + body
prelude = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#define PGSIZE 4096ULL
#define DRAM_BASE 0x80000000ULL
#define KERNEL_BEGIN_PADDR (DRAM_BASE + 8192)
#define PHYSICAL_MEMORY_END (DRAM_BASE + sizeof(arena))
#define PPN(x) ((uintptr_t)(x) / PGSIZE)
#define ROUNDDOWN(x,n) ((x) - (x) % (n))
#define ROUNDUP(x,n) ROUNDDOWN((x)+(n)-1,n)
typedef struct link { struct link *prev, *next; } list_entry_t;
struct Page { int ref; uint64_t flags; unsigned int property; list_entry_t page_link; };
static _Alignas(4096) unsigned char arena[2*1024*1024];
#define PHYSICAL_MEMORY_OFFSET ((uintptr_t)arena - DRAM_BASE)
#define PADDR(x) ((uintptr_t)(x) - va_pa_offset)
#define end test_kernel_end
char *test_kernel_end;
static struct Page *pages;
static size_t npage;
static const size_t nbase = DRAM_BASE / PGSIZE;
static uint64_t va_pa_offset, input_base, input_size;
static size_t registered;
static unsigned calls;
static jmp_buf failure;
static uint64_t get_memory_base(void) { return input_base; }
static uint64_t get_memory_size(void) { return input_size; }
#define cprintf(...) ((void)0)
#define panic(...) longjmp(failure, 1)
#define SetPageReserved(p) ((p)->flags |= 1)
static void set_page_ref(struct Page *p,int ref) { p->ref=ref; }
static void list_init(list_entry_t *p) { p->prev=p->next=p; }
static struct Page *pa2page(uintptr_t pa) { assert(PPN(pa)>=nbase && PPN(pa)<npage); return pages+PPN(pa)-nbase; }
static void init_memmap(struct Page *p,size_t n) {
    assert(n>0); calls++; registered=n;
    uintptr_t begin=(uintptr_t)(p-pages+nbase)*PGSIZE;
    assert(begin >= PADDR(pages)+(npage-nbase)*sizeof(*pages));
    assert(begin+n*PGSIZE <= input_base+input_size);
    assert(begin+n*PGSIZE <= PHYSICAL_MEMORY_END);
    for(size_t i=0;i<n;i++) { assert(p[i].flags==1); p[i].flags=0; }
}
'''
# The kernel's linker array is represented by a configurable pointer in the mock.
body = body.replace('extern char end[];', '')
main = r'''
static void run(const char *label,uint64_t base,uint64_t size,size_t kernel_end,int expect_panic,int expect_empty) {
    input_base=base; input_size=size; test_kernel_end=(char *)arena+kernel_end;
    registered=0; calls=0; pages=NULL; npage=0;
    memset(arena,0xa5,sizeof(arena));
    int panicked=setjmp(failure);
    if(!panicked) page_init();
    assert(!!panicked==expect_panic);
    if(!panicked) {
        assert((calls==0)==expect_empty);
        assert(npage==PPN(ROUNDDOWN(base+size<PHYSICAL_MEMORY_END ? base+size : PHYSICAL_MEMORY_END,PGSIZE)));
        assert(((uintptr_t)pages%PGSIZE)==0);
        assert(pa2page(PADDR(test_kernel_end-1))->flags==1);
        assert(pa2page(PADDR(pages))->flags==1);
        assert(va_pa_offset==PHYSICAL_MEMORY_OFFSET);
    }
    printf("PASS %s\n",label);
}
int main(void) {
 run("normal",DRAM_BASE,sizeof(arena),12345,0,0);
 run("partial tail page",DRAM_BASE,sizeof(arena)-123,12345,0,0);
 run("clamped upper bound",DRAM_BASE,4*1024*1024,12345,0,0);
 run("missing base",0,sizeof(arena),12345,1,0);
 run("missing size",DRAM_BASE,0,12345,1,0);
 run("overflow",DRAM_BASE,UINT64_MAX,12345,1,0);
 run("wrong DRAM base",DRAM_BASE+4096,sizeof(arena)-4096,12345,1,0);
 run("kernel outside memory",DRAM_BASE,8192,12345,1,0);
 run("descriptors do not fit",DRAM_BASE,16384,12345,1,0);
 run("descriptors fit without free page",DRAM_BASE,12288,8192,0,1);
 puts("10/10 page_init boundary tests passed");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp)/'check.c'
    exe = Path(tmp)/'check'
    c.write_text(prelude+body+main,encoding='utf-8')
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

