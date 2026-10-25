# ============================================================================
# 【代码归属】课程实验框架为主；成员二、成员三增加了编译开关
# 【判定依据】按 lab2.docx 的任务划分与代码内容判断，不依赖可能被清除的 lab2 / YOUR CODE 标记。
# ============================================================================
# 【答辩可以直接这样念】
#
# “make 做三件事：交叉编译所有 .c/.S，链接成 bin/kernel，
# 再用 objcopy 变成纯二进制 bin/ucore.img。make qemu 启动 QEMU，
# make debug 额外加 -s -S，停在第一条指令等 GDB。
# 我们加了三个开关：PORTABLE_BOOT 适配现代 QEMU，
# PMM 选 Best-Fit 或 Buddy，MEMBER3_TESTS 控制是否跑成员三测试。
# 这三个都是编译期宏，切换前必须 make clean，否则会复用旧目标文件。”
#
# ============================================================================
# 【本组改动】成员二增加 PORTABLE_BOOT；成员三增加 PMM 和 MEMBER3_TESTS。
# ============================================================================
# ============================================================================
# Makefile —— Lab2 构建总控（答辩常问：make 到底做了什么）
# ============================================================================
# 【三条命令，三个层次】
#   make        : 交叉编译所有 .c/.S → 链接出 ELF 内核 bin/kernel
#                 → objcopy 成纯二进制 bin/ucore.img
#   make qemu   : 启动 QEMU，用 OpenSBI 当固件，把镜像加载到 0x80200000
#   make debug  : 同上，但 QEMU 停在第一条指令等 GDB（-s -S）
#
# 【为什么要交叉编译】
#   主机是 x86_64，内核跑在 RISC-V 上，所以必须用
#   riscv64-unknown-elf-gcc/ld/objcopy。
#
# 【三个编译期开关（答辩重点）】
#   PORTABLE_BOOT=1  现代 QEMU/OpenSBI 用 -kernel 启动，并加低地址同址映射
#   PMM=best_fit     默认，课程评分路径（best_fit_pmm_manager）
#   PMM=buddy        换用 Buddy System（buddy_pmm_manager）
#   MEMBER3_TESTS=1  额外运行成员三的独立 Buddy/SLUB 测试
#
#   注意：这些都是编译期宏，切换时必须 make clean，
#   否则旧目标文件会被复用，链接出"半套配置"的内核。
# ============================================================================
PROJ	:= lab2
EMPTY	:=
SPACE	:= $(EMPTY) $(EMPTY)
SLASH	:= /

V       := @

#ifndef GCCPREFIX
GCCPREFIX := riscv64-unknown-elf-
#endif

ifndef QEMU
QEMU := qemu-system-riscv64
endif

# 默认走课程 QEMU 4.1.1 的 -device loader 启动方式；
# 现代 QEMU/OpenSBI 用 PORTABLE_BOOT=1（-kernel + 启动过渡同址映射）。
# Keep the course QEMU 4.1.1 boot path by default. Set to 1 for modern OpenSBI.
PORTABLE_BOOT ?= 0
ifeq ($(PORTABLE_BOOT),1)
DEFS += -DBOOT_IDENTITY_MAP
endif

# 页分配器选择：
#   默认 Best-Fit，保证课程评分路径不变；
#   PMM=buddy 时同一个内核改用 Buddy System。
#   切换必须 make clean（编译期宏不会被 Make 当作依赖追踪）。
PMM ?= best_fit
ifeq ($(PMM),buddy)
DEFS += -DUSE_BUDDY_PMM
else ifeq ($(PMM),best_fit)
DEFS += -DUSE_BEST_FIT_PMM
else
$(error PMM must be best_fit or buddy, got '$(PMM)')
endif

# MEMBER3_TESTS=1 时运行成员三独立测试：
#   PMM=buddy 时跑 Buddy 测试，任何配置都跑 SLUB 测试。
#   默认关闭，保证评分路径的输出与原来一致。
MEMBER3_TESTS ?= 0
ifeq ($(MEMBER3_TESTS),1)
DEFS += -DUSE_MEMBER3_TESTS
endif

ifndef SPIKE
SPIKE := spike
endif

# eliminate default suffix rules
.SUFFIXES: .c .S .h

# delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# define compiler and flags
HOSTCC		:= gcc
HOSTCFLAGS	:= -Wall -O2

GDB		:= $(GCCPREFIX)gdb

CC		:= $(GCCPREFIX)gcc
# -mcmodel=medany：位置无关的 medium-any 模型，镜像加载到哪里都能用 PC 相对寻址
# -nostdinc：不用宿主机标准头，全部用 libs/ 下的自实现
# -ffunction-sections/-fdata-sections + 链接时 --gc-sections：丢弃未用代码
CFLAGS  := -mcmodel=medany -std=gnu99 -Wno-unused -Werror 
CFLAGS	+= -fno-builtin -Wall -O2 -nostdinc $(DEFS)
CFLAGS	+= -fno-stack-protector -ffunction-sections -fdata-sections
CFLAGS	+= -g
CFLAGS2 = $(CFLAGS) -D ucore_test
CTYPE	:= c S
LD      := $(GCCPREFIX)ld
LDFLAGS	:= -m elf64lriscv
LDFLAGS	+= -nostdlib --gc-sections

OBJCOPY := $(GCCPREFIX)objcopy
OBJDUMP := $(GCCPREFIX)objdump

COPY	:= cp
MKDIR   := mkdir -p
MV		:= mv
RM		:= rm -f
AWK		:= awk
SED		:= sed
SH		:= sh
TR		:= tr
TOUCH	:= touch -c

OBJDIR	:= obj
BINDIR	:= bin

ALLOBJS	:=
ALLDEPS	:=
TARGETS	:=

include tools/function.mk

listf_cc = $(call listf,$(1),$(CTYPE))

# for cc
add_files_cc = $(call add_files,$(1),$(CC),$(CFLAGS) $(3),$(2),$(4))
add_files_cc2 = $(call add_files,$(1),$(CC),$(CFLAGS2) $(3),$(2),$(4))
create_target_cc = $(call create_target,$(1),$(2),$(3),$(CC),$(CFLAGS))

# for hostcc
add_files_host = $(call add_files,$(1),$(HOSTCC),$(HOSTCFLAGS),$(2),$(3))
create_target_host = $(call create_target,$(1),$(2),$(3),$(HOSTCC),$(HOSTCFLAGS))

cgtype = $(patsubst %.$(2),%.$(3),$(1))
objfile = $(call toobj,$(1))
asmfile = $(call cgtype,$(call toobj,$(1)),o,asm)
outfile = $(call cgtype,$(call toobj,$(1)),o,out)
symfile = $(call cgtype,$(call toobj,$(1)),o,sym)

# for match pattern
match = $(shell echo $(2) | $(AWK) '{for(i=1;i<=NF;i++){if(match("$(1)","^"$$(i)"$$")){exit 1;}}}'; echo $$?)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
# include kernel/user

INCLUDE	+= libs/

CFLAGS	+= $(addprefix -I,$(INCLUDE))

LIBDIR	+= libs

$(call add_files_cc,$(call listf_cc,$(LIBDIR)),libs,)

# -------------------------------------------------------------------
# kernel

KINCLUDE	+= kern/debug/ \
			   kern/driver/ \
			   kern/mm/ \
			   kern/arch/

KSRCDIR		+= kern/init \
			   kern/libs \
			   kern/debug \
			   kern/driver \
			   kern/mm

KCFLAGS		+= $(addprefix -I,$(KINCLUDE))

ifeq ($(MAKECMDGOALS),test)
$(call add_files_cc2,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))
else
$(call add_files_cc,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))
endif
KOBJS	= $(call read_packet,kernel libs)

# create kernel target
kernel = $(call totarget,kernel)

$(kernel): tools/kernel.ld

$(kernel): $(KOBJS)
	@echo + ld $@
	$(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
	@$(OBJDUMP) -S $@ > $(call asmfile,kernel)
	@$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)

$(call create_target,kernel)

# -------------------------------------------------------------------
# create ucore.img
UCOREIMG	:= $(call totarget,ucore.img)

ifeq ($(PORTABLE_BOOT),1)
QEMU_BOOT_OPTS = -kernel $(UCOREIMG)
else
QEMU_BOOT_OPTS = -device loader,file=$(UCOREIMG),addr=0x80200000
endif


$(UCOREIMG): $(kernel)
	$(OBJCOPY) $(kernel) --strip-all -O binary $@

$(call create_target,ucore.img)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

$(call finish_all)

IGNORE_ALLDEPS	= clean \
				  dist-clean \
				  grade \
				  touch \
				  print-.+ \
				  handin

ifeq ($(call match,$(MAKECMDGOALS),$(IGNORE_ALLDEPS)),0)
-include $(ALLDEPS)
endif

# files for grade script

TARGETS: $(TARGETS)

.DEFAULT_GOAL := TARGETS
.PHONY: qemu spike test
# 启动 QEMU：virt 机器、无图形、OpenSBI 固件、按启动配置加载镜像。
qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		$(QEMU_BOOT_OPTS)

# -s -S：开 GDB 服务（1234 端口）并停在第一条指令，配合 make gdb 使用。
debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		$(QEMU_BOOT_OPTS)\
		-s -S

# 连接 make debug 启动的 QEMU。答辩演示时可在 0x80200000 下断点。
gdb:
	riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'

test: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		$(QEMU_BOOT_OPTS)
spike: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(SPIKE) $(UCOREIMG)

.PHONY: grade touch

GRADE_GDB_IN	:= .gdb.in
GRADE_QEMU_OUT	:= .qemu.out
HANDIN			:= proj$(PROJ)-handin.tar.gz

TOUCH_FILES		:=

MAKEOPTS		:= --quiet --no-print-directory

GRADE_TMP	:= /tmp/lab2_grade

grade:
	$(V)$(MAKE) $(MAKEOPTS) clean
	$(V)$(RM) -rf $(GRADE_TMP)
	$(V)$(MKDIR) $(GRADE_TMP)
	$(V)tar -C . --exclude=.git --exclude=.vscode --exclude=vcs -cf - . | tar -C $(GRADE_TMP) -xf -
	$(V)python3 tools/inject.py $(GRADE_TMP)
	$(V)cd $(GRADE_TMP) && $(SH) tools/grade.sh
	$(V)$(RM) -rf $(GRADE_TMP)

touch:
	$(V)$(foreach f,$(TOUCH_FILES),$(TOUCH) $(f))

print-%:
	@echo $($(shell echo $(patsubst print-%,%,$@) | $(TR) [a-z] [A-Z]))

.PHONY: clean dist-clean handin packall tags
clean:
	$(V)$(RM) $(GRADE_GDB_IN) $(GRADE_QEMU_OUT) cscope* tags
	-$(RM) -r $(OBJDIR) $(BINDIR)

dist-clean: clean
	-$(RM) $(HANDIN)

handin: packall
	@echo Please visit http://learn.tsinghua.edu.cn and upload $(HANDIN). Thanks!

packall: clean
	@$(RM) -f $(HANDIN)
	@tar -czf $(HANDIN) `find . -type f -o -type d | grep -v '^\.*$$' | grep -vF '$(HANDIN)'`

tags:
	@echo TAGS ALL
	$(V)rm -f cscope.files cscope.in.out cscope.out cscope.po.out tags
	$(V)find . -type f -name "*.[chS]" >cscope.files
	$(V)cscope -bq 
	$(V)ctags -L cscope.files
