"""Run host-QEMU and guest-kernel GDB sessions inside os2026, save raw logs."""
from pathlib import Path
import os
import signal
import socket
import subprocess
import time

root = Path(__file__).resolve().parents[1]
lab = root / 'verification/lab2'
qemu = Path('/home/vmuser/src/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64')
qsource = qemu.parents[1]
logs = root / 'logs'
host_script = root / 'debug/generated_host.gdb'
host_script.write_text('''set pagination off
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
            print("\\nEVIDENCE " + self.label + " hit=" + str(self.hits))
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
printf "\\nHOST single stepping the first kernel instruction translation\\n"
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
''')
guest_script = root / 'debug/generated_guest.gdb'
guest_script.write_text('''set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote localhost:1235
break page_init
continue
printf "\\nGUEST page_init entry\\n"
bt
x/5i $pc
step
next
break test_page_init
continue
printf "\\nGUEST initialization complete\\n"
p/x va_pa_offset
p/x npage
p/x nbase
p/x pages
p sizeof(struct Page)
p/x &end
p/x $satp
x/gx (char *)&boot_page_table_sv39+4088
x/6i kern_init
detach
quit
''')
with (logs/'os2026_host_gdb.log').open('w') as hostlog:
    host = subprocess.Popen(['gdb','-q','-batch','-x',str(host_script),'--args',str(qemu),
        '-machine','virt','-nographic','-bios','default',
        '-device','loader,file=bin/ucore.img,addr=0x80200000',
        '-gdb','tcp:127.0.0.1:1235','-S'], cwd=lab,stdout=hostlog,stderr=subprocess.STDOUT,
        start_new_session=True)
    try:
        ready=False
        for _ in range(100):
            if host.poll() is not None: break
            with socket.socket() as sock:
                if sock.connect_ex(('127.0.0.1',1235)) == 0:
                    ready=True
                    break
            time.sleep(.1)
        if not ready: raise RuntimeError('Host GDB/QEMU did not open guest debugging port; inspect host log')
        with (logs/'os2026_guest_gdb.log').open('w') as guestlog:
            guest = subprocess.run(['/opt/riscv/bin/riscv64-unknown-elf-gdb','-q','-batch',
                '-x',str(guest_script)],cwd=lab,stdout=guestlog,stderr=subprocess.STDOUT,timeout=60)
        print('guest_gdb_exit=',guest.returncode)
        time.sleep(2)
    finally:
        if host.poll() is None:
            os.killpg(host.pid,signal.SIGTERM)
            host.wait(timeout=10)
print('Saved host and guest GDB logs under', logs)

