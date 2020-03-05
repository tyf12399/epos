/**
 * vim: filetype=c:fenc=utf-8:ts=4:et:sw=4:sts=4
 *
 * Copyright (C) 2015 Hong MingJian<hongmingjian@gmail.com>
 * All rights reserved.
 *
 * This file is part of the EPOS.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 *
 */
#include <stddef.h>
#include "kernel.h"

/*中断向量表*/
void (*g_intr_vector[NR_IRQ])(uint32_t irq, struct context *ctx);

/*可用的物理内存区域*/
uint32_t g_ram_zone[RAM_ZONE_LEN];

uint32_t *PT  = (uint32_t *)USER_MAX_ADDR, //页表的指针
         *PTD = (uint32_t *)KERN_MIN_ADDR; //页目录的指针

/*默认的中断处理程序*/
void isr_default(uint32_t irq, struct context *ctx)
{
    //printk("IRQ=0x%02x\r\n", irq);
}

void start_user_task()
{
    calibrate_delay();

    uint32_t addr=0x08048000;
    uint32_t *p = (uint32_t *)addr;
    page_alloc_in_addr(addr, 1, VM_PROT_RW);

    *p++ = 0xe92d4008;
    *p++ = 0xe3a0005a;
    *p++ = 0xeb000001;
    *p++ = 0xeafffffc;

    *p++ = 0xeafffffa;

    *p++ = 0xe52d7004;
    *p++ = 0xe3a07ffa;
    *p++ = 0xef000000;
    *p++ = 0xe49d7004;
    *p++ = 0xe12fff1e;

    page_alloc_in_addr(USER_MAX_ADDR - (1024*1024), (1024*1024)/PAGE_SIZE, VM_PROT_RW);
    if(sys_task_create((void *)USER_MAX_ADDR, (void *)(addr+0x10), (void *)0x12345678) == NULL)
        printk("Failed\r\n");
}

/**
 * 机器无关（Machine Independent）的初始化
 */
void mi_startup()
{
    uint32_t i, brk;

    printk("Welcome to EPOS\r\n");
    printk("Copyright (C) 2005-2015, 2020 MingJian Hong<hongmingjian@gmail.com>\r\n");
    printk("All rights reserved.\r\n\r\n");

    {
        /*安装默认的中断处理程序*/
        for(i = 0; i < NR_IRQ; i++)
            g_intr_vector[i]=isr_default;

        /*安装定时器的中断处理程序*/
        g_intr_vector[IRQ_TIMER] = isr_timer;
        enable_irq(IRQ_TIMER);
    }

    /*
     * 初始化物理内存管理器
     */
    brk = PAGE_ROUNDUP( (uint32_t)(&end) );
    brk = init_frame(brk);

    /*
     * 初始化虚拟地址空间，为内核堆预留4MiB的地址空间
     */
    init_vmspace(brk+1024*PAGE_SIZE);

    /*
     * 初始化内核堆，大小为4MiB，由kmalloc/kfree管理.
     */
    init_kmalloc((uint8_t *)brk, 1024*PAGE_SIZE);

    /*
     * 把[MMIO_BASE, MMIO_BASE+16M)保留下来
     */
    page_alloc_in_addr(MMIO_BASE_VA, 4096, VM_PROT_RW);

    /*
     * 把[0xffff0000, 0xffff1000)保留下来，并映射到0x0
     */
    page_alloc_in_addr(0xffff0000, 1, VM_PROT_RW);
    page_map(0xffff0000, 0x0, 1, L2E_V|L2E_W|L2E_C);
    __asm__ __volatile__ (
             "mrc p15,0,r0,c1,c0,0\n\t"
             "orr r0, r0, #(1<<13) @SCTLR.V=1\n\t"
             "mcr p15,0,r0,c1,c0,0\n\t"
             :
             :
             : "r0"
    );

    /*
     * 内核已经被重定位到链接地址，取消恒等映射
     */
    for(i = 0; i < NR_KERN_PAGETABLE; i++)
        PTD[i] = 0;

    /*
     * 取消[KERNBASE+0x1000, KERNBASE+0x4000)的地址映射
     */
    for(i = 0x1000; i < LOADADDR-L1_TABLE_SIZE; i+=PAGE_SIZE)
        *vtopte(i+KERNBASE)=0;

    /* Invalidate the translation lookaside buffer (TLB)
     * ARM1176JZF-S manual, p. 3-86
     */
    __asm__ __volatile__("mcr p15, 0, %[data], c8, c7, 0" : : [data] "r" (0));

    /* Virtual memory layout
     *
     * 0xffff 0000 - 0xffff 1000 = Hivecs
     * 0xc400 0000 - 0xc500 0000 = Memory-mapped I/O
     * 0xc000 8000 - 0xc0?? ???? = kernel.img
     * 0xc000 4000 - 0xc000 8000 = Page directory (PTD)
     * 0xc000 1000 - 0xc000 4000 = Unused
     * 0xc000 0000 - 0xc000 1000 = Stacks for modes UND/ABT/IRQ/SVC
     * 0xbfc0 0000 - 0xc000 0000 = Page tables (PT)
     * 0x0040 0000 - 0xbfc0 0000 = User accessible memory
     * 0x0000 0000 - 0x0040 0000 = Unused
     */
    /* Physical memory layout
     *
     * 0x2000 0000 - 0x2100 0000 = Memory-mapped I/O
     * 0x00yy y000 - 0x2000 0000 = Free (managed)
     * 0x00xx x000 - 0x00yy y000 = (NR_KERN_PAGETABLE+4) page tables
     * 0x0000 8000 - 0x00xx x000 = kernel.img
     * 0x0000 4000 - 0x0000 8000 = Page directory
     * 0x0000 1000 - 0x0000 4000 = Free (un-managed)
     * 0x0000 0000 - 0x0000 1000 = Hivecs and stacks for modes UND/ABT/IRQ/SVC
     */

    /*
     * 初始化多线程子系统
     */
    init_task();

    /*
     * task0是系统空闲线程，已经由init_task创建。
     * 这里用run_as_task0手工切换到task0运行。
     * 由task0启动第一个用户线程，然后它将循环执行函数cpu_idle。
     */
    run_as_task0();
    start_user_task();

    while(1)
        cpu_idle();
}

