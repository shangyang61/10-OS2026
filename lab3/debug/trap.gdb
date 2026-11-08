set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote 127.0.0.1:1235
hbreak trap
continue
set $tf = (struct trapframe *)$a0
printf "\n=== Hardware-to-C Trap frame ===\n"
printf "trapframe size: %lu bytes\n", sizeof(struct trapframe)
printf "tf=%#lx saved_sp=%#lx\n", $tf, $tf->gpr.sp
printf "status=%#lx epc=%#lx cause=%#lx\n", $tf->status, $tf->epc, $tf->cause
if sizeof(struct trapframe) != 288
  quit 1
end
if $tf->cause != 0x8000000000000005
  quit 1
end
delete breakpoints
hbreak __trapret
continue
set $tf = (struct trapframe *)$sp
set $expected_sp = $tf->gpr.sp
set $expected_epc = $tf->epc
set $expected_sie = ($tf->status >> 4) & 2
printf "\n=== Before RESTORE_ALL ===\n"
printf "frame_sp=%#lx original_sp=%#lx delta=%lu\n", $sp, $expected_sp, $expected_sp-(unsigned long)$sp
printf "ticks=%lu\n", ticks
if $expected_sp - (unsigned long)$sp != 288
  quit 1
end
delete breakpoints
hbreak *$expected_epc
continue
printf "\n=== After sret ===\n"
printf "pc=%#lx sp=%#lx sstatus=%#lx\n", $pc, $sp, $sstatus
if $pc != $expected_epc || $sp != $expected_sp
  quit 1
end
if ($sstatus & 2) != $expected_sie
  quit 1
end
printf "[GDB] Trap frame, timer cause, SP, EPC and SIE restoration: PASS\n"
detach
quit
