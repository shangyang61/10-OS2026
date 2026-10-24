# Host GDB: gdb /absolute/path/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
# Then: attach PID; source /absolute/path/qemu4_mmu.gdb; continue
set pagination off
set breakpoint pending on
handle SIGPIPE nostop noprint pass
set logging file qemu4_mmu_session.log
set logging overwrite on
set logging on
# Match the first kernel high-half mapping; do not confuse debugger memory reads
# with guest execution. Check bt for riscv_cpu_tlb_fill as the caller.
break get_physical_address if addr >= 0xffffffffc0200000 && addr < 0xffffffffc0201000
commands
silent
printf "\nMMU walk entry\n"
bt 5
p/x addr
p access_type
p mmu_idx
p/x env->satp
end

