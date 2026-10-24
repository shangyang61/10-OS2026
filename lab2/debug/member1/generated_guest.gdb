set pagination off
set confirm off
set architecture riscv:rv64
file bin/kernel
target remote localhost:1235
break page_init
continue
printf "\nGUEST page_init entry\n"
bt
x/5i $pc
step
next
break test_page_init
continue
printf "\nGUEST initialization complete\n"
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
