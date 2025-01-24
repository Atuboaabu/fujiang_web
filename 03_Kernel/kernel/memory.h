#ifndef _KERNEL_MEMORY_H_
#define _KERNEL_MEMORY_H_

#include "stdint.h"
#include "bitmap.h"
#include "lock.h"

#define PAGE_SIZE           (4096)
#define MEMORY_BITMAP_ADDR  (0xC009A000)
#define KERNEL_HEAP_START   (0xC0200000)

/* 页表属性定义 */
#define	 PG_P_1	  (1)  // 页表项或页目录项存在属性位
#define	 PG_P_0	  (0)  // 页表项或页目录项存在属性位
#define	 PG_RW_R  (0)  // R/W 属性位值, 读/执行
#define	 PG_RW_W  (2)  // R/W 属性位值, 读/写/执行
#define	 PG_US_S  (0)  // U/S 属性位值, 系统级
#define	 PG_US_U  (4)  // U/S 属性位值, 用户级

#define PDE_INDEX(vaddr) ((vaddr & 0xFFC00000) >> 22)
#define PTE_INDEX(vaddr) ((vaddr & 0x003FF000) >> 12)

/* 内存池标记,用于判断用哪个内存池 */
enum pool_flag {
   POOL_FLAG_KERNEL = 1,    // 内核内存池
   POOL_FLAG_USER   = 2     // 用户内存池
};

/* 页表项结构 PTE */
struct page_table_entry {
    uint32_t P : 1;     // 页表存在位：1表示存在
    uint32_t RW : 1;    // 读写权限：1表示可读可写，0表示只读
    uint32_t US : 1;    // 页表中页面的用户/特权级访问：1表示用户模式可访问；0表示仅内核模式访问
    uint32_t PWT : 1;   // 页表写入策略：1表示写直达，0表示写回
    uint32_t PCD : 1;   // 页表缓存禁用：1表示禁用，0表示启用
    uint32_t A : 1;     // 是否被使用，由处理器置位
    uint32_t D : 1;     // 页面是否被写入过
    uint32_t PS : 1;    // 页面大小：1 表示4MB页；0 表示4KB页
    uint32_t G : 1;     // 全局页：1表示全局页，不会被TLB刷新
    uint32_t AVL : 3;   // 有效标记：操作系统使用
    uint32_t PTN : 20;  // 物理页框号：物理页的其实地址
};

/* 页表项结构 PDE */
struct page_directory_entry {
    uint32_t P : 1;     // 页表存在位
    uint32_t RW : 1;    // 读写权限：1表示可读可写，0表示只读
    uint32_t US : 1;    // 页表中页面的用户/特权级访问
    uint32_t PWT : 1;   // 页表写入策略
    uint32_t PCD : 1;   // 页表缓存禁用
    uint32_t A : 1;     // 是否被使用，由处理器置位
    uint32_t Rev : 1;   // 保留位，必须为0
    uint32_t PS : 1;    // 页面大小：1 表示4MB页；0 表示4KB页
    uint32_t G : 1;     // 全局页
    uint32_t AVL : 3;   // 有效标记：操作系统使用
    uint32_t PTN : 20;  // 物理页框号：物理页的其实地址
};

struct memory_poll {
    uint32_t addr_start;        // 物理地址起始位置
    uint32_t size;              // 内存池大小（byte)
    struct bitmap pool_bitmap;  // 内存池对应的位图，用于管理物理内存
    struct lock lock;                // 内存池互斥锁
};

struct virtual_addr_pool {
    uint32_t addr_start;       // 虚拟地址起始位置
    struct bitmap pool_bitmap; // 虚拟地址对应的位图，用于管理虚拟地址
};

void memory_init();
void* get_kernel_pages(uint32_t cnt);
void* get_user_pages(uint32_t cnt);
void* bind_vaddr_with_mempool(enum pool_flag pf, uint32_t vaddr);
uint32_t vaddr_to_phyaddr(uint32_t vaddr);

#endif
