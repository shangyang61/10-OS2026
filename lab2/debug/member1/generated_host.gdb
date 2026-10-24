set pagination off
set confirm off
set print frame-arguments all
python
import gdb
class Observe(gdb.Breakpoint):
    def __init__(self, spec, label, names, condition=None, limit=5):
        super().__init__(spec)
        self.label, self.names, self.limit, self.hits = label, names, limit, 0
        self.predicate = condition
        if condition: self.condition = condition
    def stop(self):
        if self.predicate:
            try:
                if not bool(gdb.parse_and_eval(self.predicate)): return False
            except gdb.error: return False
        self.hits += 1
        if self.hits <= self.limit:
            print("\nEVIDENCE " + self.label + " hit=" + str(self.hits))
            for name in self.names:
                try: print(name + " = " + str(gdb.parse_and_eval(name)))
                except gdb.error as exc: print(name + " = unavailable: " + str(exc))
            gdb.execute("bt 5")
        return False
kernel = 'addr >= 0xffffffffc0200000 && addr < 0xffffffffc0208000'
Observe('get_physical_address', 'walk entry', ['addr','access_type','mmu_idx','env->satp'], kernel)
Observe('get_physical_address', 'before paging', ['addr','access_type','mmu_idx','env->satp'], 'addr >= 0x80200000 && addr < 0x80201000', 3)
Observe('target/riscv/cpu_helper.c:256', 'PTE loaded', ['addr','base','i','levels','ptshift','idx','pte_addr','pte','ppn'], kernel)
Observe('target/riscv/cpu_helper.c:337', 'translated page', ['addr','*physical','pte','ptshift'], kernel)
Observe('riscv_cpu_tlb_fill', 'TLB fill', ['address','access_type','mmu_idx'], 'address >= 0xffffffffc0200000 && address < 0xffffffffc0208000', 8)
Observe('tlb_set_page_with_attrs', 'TLB install', ['vaddr','paddr','prot','mmu_idx'], 'vaddr >= 0xffffffffc0200000 && vaddr < 0xffffffffc0208000', 8)
Observe('tlb_set_page_with_attrs', 'Bare TLB install', ['vaddr','paddr','prot','mmu_idx'], 'vaddr == 0x80200000', 2)
Observe('accel/tcg/cputlb.c:1271', 'C TLB read lookup', ['addr','tlb_addr','mmu_idx'], kernel, 8)
Observe('accel/tcg/cputlb.c:1030', 'instruction TLB lookup', ['addr','entry->addr_code','mmu_idx'], kernel, 8)
end
tbreak target/riscv/cpu_helper.c:256 if addr == 0xffffffffc02000d8
run
printf "\nHOST single stepping the first kernel instruction translation\n"
p/x addr
p/x pte
p ptshift
next
next
next
next
next
next
next
next
next
next
next
next
continue
