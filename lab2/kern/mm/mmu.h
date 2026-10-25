/*
 * ============================================================================
 * kern/mm/mmu.h —— 页表与地址翻译的基础定义（Lab2 答辩必问）
 * ============================================================================
 * 【这个文件在讲什么】
 *   RISC-V Sv39 把 64 位虚拟地址"有效部分"切成 39 位：
 *
 *     +-------9--------+-------9--------+--------9---------+----------12----------+
 *     |      VPN2      |      VPN1      |       VPN0       |  Offset within Page  |
 *     +----------------+----------------+------------------+----------------------+
 *     第 30~38 位        第 21~29 位        第 12~20 位         第 0~11 位
 *
 *   三级页表正好对应 VPN2 → VPN1 → VPN0，每级 512 项（2^9），每项 8 字节，
 *   所以一级页表恰好占 4 KiB（512 × 8 = 4096）。
 *
 * 【答辩要点一：为什么是三级而不是两级】
 *   39 = 9 + 9 + 9 + 12。每级 9 位 → 512 项；最后 12 位是页内偏移。
 *   若两级，每级要覆盖 13~14 位，页表项就不是 512 个这种规整结构了。
 *
 * 【答辩要点二：叶子项】
 *   页表项最低位 V=1 表示有效；R/W/X 任一为 1 表示这一项是"叶子"，
 *   直接给出物理页号，不再往下走。本实验的启动页表就是用一个 1 GiB 的
 *   叶子项（根表第 511 项）把内核直接映射起来。
 *
 * 【答辩要点三：PTE 里的物理页号】
 *   PTE 的 [53:10] 是 PPN。取出来左移 12 位才是物理地址：
 *   SV39_PTE_ADDR(pte) 先清掉低 10 位标志，再 <<3（等价于 <<12 后丢掉
 *   PTE 中已经右移 10 位的偏移）。这是 Lab2 最容易写错的一处。
 * ============================================================================
 */
#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#ifndef __ASSEMBLER__
#include <defs.h>
#endif

/* 一个物理页 4 KiB；PGSHIFT 是 log2(PGSIZE)，用于页号 ↔ 地址换算。 */
#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)

/* 物理/虚拟页号：地址右移 12 位。PPN 不区分虚拟还是物理，调用者自己保证语义。 */
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)

/* Sv39 的地址切分已经在文件头画过，这里保留英文原图方便对照。 */
// Sv39 linear address structure
// +-------9--------+-------9--------+--------9---------+----------12----------+
// |      VPN2      |      VPN1      |       VPN0       |  Offset within Page  |
// +----------------+----------------+------------------+----------------------+

// Sv39 in RISC-V64 uses 39-bit virtual address to access 56-bit physical address!
// Sv39 page table entry:
// +-------10--------+-------26-------+--------9----------+---------9--------+---2----+-------8-------+
// |    Reserved     |      PPN[2]     |      PPN[1]       |      PPN[0]     |Reserved|D|A|G|U|X|W|R|V|
// +-----------------+-----------------+-------------------+-----------------+--------+---------------+

/* 页目录/页表常量：每页 512 项，每项 8 字节。 */
/* page directory and page table constants */
#define SV39_NENTRY          512                     // page directory entries per page directory

#define SV39_PGSIZE          4096                    // bytes mapped by a page
#define SV39_PGSHIFT         12                      // log2(PGSIZE)
/* 一级页表覆盖的地址范围：512 × 4 KiB = 2 MiB，所以 PTSHIFT = 21。
 * 注意原框架这里写成了 SV39NENTRY（少一个下划线），是未使用的笔误，
 * 本次顺手修正为 SV39_NENTRY。 */
#define SV39_PTSIZE          (PGSIZE * SV39_NENTRY)  // bytes mapped by a page table
#define SV39_PTSHIFT         21                      // log2(PTSIZE)

/* 三个 VPN 字段在虚拟地址中的起始位。 */
#define SV39_VPN0SHIFT       12                      // offset of VPN0 in a linear address
#define SV39_VPN1SHIFT       21                      // offset of VPN1 in a linear address
#define SV39_VPN2SHIFT       30                      // offset of VPN2 in a linear address
#define SV39_PTE_PPN_SHIFT   10                      // offset of PPN in a physical address

/* 从虚拟地址取出某一级索引（& 0x1FF 就是取 9 位）。 */
#define SV39_VPN0(la) ((((uintptr_t)(la)) >> SV39_VPN0SHIFT) & 0x1FF)
#define SV39_VPN1(la) ((((uintptr_t)(la)) >> SV39_VPN1SHIFT) & 0x1FF)
#define SV39_VPN2(la) ((((uintptr_t)(la)) >> SV39_VPN2SHIFT) & 0x1FF)
#define SV39_VPN(la, n) ((((uintptr_t)(la)) >> 12 >> (9 * n)) & 0x1FF)

/* 反过来：由三级索引和页内偏移拼出虚拟地址。GDB 里手工验证映射时常用。 */
// construct linear address from indexes and offset
#define SV39_PGADDR(v2, v1, v0, o) ((uintptr_t)((v2) << SV39_VPN2SHIFT | (v1) << SV39_VPN1SHIFT | (v0) << SV39_VPN0SHIFT | (o)))

/* PTE → 物理地址：PTE 低 10 位是标志，PPN 从第 10 位开始。
 * 先 & ~0x1FF 清标志，再 <<3：因为 PPN 在 PTE 里已经右移过 10 位，
 * 补回 12-10=2 位……合起来就是 <<3。 */
// address in page table or page directory entry
#define SV39_PTE_ADDR(pte)   (((uintptr_t)(pte) & ~0x1FF) << 3)

/* 三级页表的下标编号，调试时用来指代"第几级"。 */
// 3-level pagetable
#define SV39_PT0                 0
#define SV39_PT1                 1
#define SV39_PT2                 2

/* ===== PTE 标志位（答辩常问：每一位是什么意思） =====
 *   V 有效   R 可读   W 可写   X 可执行
 *   U 用户态可访问   G 全局   A 已访问   D 已脏
 * 内核页表通常不置 U；置了 U 才允许 U 态访问。
 * W 不能单独置位：RISC-V 规定 W=1 时 R 必须也为 1。
 */
// page table entry (PTE) fields
#define PTE_V     0x001 // Valid
#define PTE_R     0x002 // Read
#define PTE_W     0x004 // Write
#define PTE_X     0x008 // Execute
#define PTE_U     0x010 // User
#define PTE_G     0x020 // Global
#define PTE_A     0x040 // Accessed
#define PTE_D     0x080 // Dirty
#define PTE_SOFT  0x300 // Reserved for Software

/* 常用权限组合。注意它们都带上了 V，否则这一项无效。 */
#define PAGE_TABLE_DIR (PTE_V)
#define READ_ONLY (PTE_R | PTE_V)
#define READ_WRITE (PTE_R | PTE_W | PTE_V)
#define EXEC_ONLY (PTE_X | PTE_V)
#define READ_EXEC (PTE_R | PTE_X | PTE_V)
#define READ_WRITE_EXEC (PTE_R | PTE_W | PTE_X | PTE_V)

#define PTE_USER (PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)

#endif /* !__KERN_MM_MMU_H__ */
