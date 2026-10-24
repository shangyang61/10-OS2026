# From the lab2 directory: gdb-multiarch -x ../debug/kernel.gdb
set pagination off
set architecture riscv:rv64
file bin/kernel
target remote localhost:1234
set logging file kernel_gdb_session.log
set logging overwrite on
set logging on
break kern_init
break page_init
continue

