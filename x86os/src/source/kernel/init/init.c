﻿
#include "comm/boot_info.h"
#include "comm/cpu_instr.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "core/task.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "tools/list.h"
#include "ipc/sem.h"
#include "core/memory.h"
#include "dev/console.h"
#include "dev/kbd.h"
#include "fs/fs.h"

static boot_info_t * init_boot_info;        // 启动信息

/**
 * 内核入口
 */
void kernel_init (boot_info_t * boot_info) {
    init_boot_info = boot_info;

    // 初始化CPU，再重新加载
    cpu_init();   // 对GDT表初始化，对互斥锁进行初始化
    irq_init();
    log_init();

    // 内存初始化要放前面一点，因为后面的代码可能需要内存分配
    memory_init(boot_info);
    fs_init();  // 文件系统初始化
    time_init();
    task_manager_init();
}


/**
 * @brief 移至第一个进程运行
 */
void move_to_first_task(void) {
    // 不能直接用Jmp far进入，因为当前特权级0，不能跳到低特权级的代码
    // 下面的iret后，还需要手动加载ds, fs, es等寄存器值，iret不会自动加载
    // 注意，运行下面的代码可能会产生异常：段保护异常或页保护异常。
    // 可根据产生的异常类型和错误码，并结合手册来找到问题所在
    task_t * curr = task_current();
    ASSERT(curr != 0);

    tss_t * tss = &(curr->tss);

    // 也可以使用类似boot跳loader中的函数指针跳转
    // 这里用jmp是因为后续需要使用内联汇编添加其它代码
    __asm__ __volatile__(
        // 模拟中断返回，切换入第1个可运行应用进程
        // 不过这里并不直接进入到进程的入口，而是先设置好段寄存器，再跳过去
        "push %[ss]\n\t"			// SS
        "push %[esp]\n\t"			// ESP
        "push %[eflags]\n\t"           // EFLAGS
        "push %[cs]\n\t"			// CS
        "push %[eip]\n\t"		    // ip
        "iret\n\t"
        ::[ss]"r"(tss->ss), 
          [esp]"r"(tss->esp), 
          [eflags]"r"(tss->eflags),
          [cs]"r"(tss->cs), 
          [eip]"r"(tss->eip)
    );
}



void init_main(void) {
    log_printf("==============================");
    log_printf("Kernel is running....");
    log_printf("Version: %s, name: %s", OS_VERSION, "tiny x86 os");
    log_printf("==============================");

    // 当IF位被设置为1时，表示中断是打开的，处理器可以响应中断请求。
    // 而当IF位被设置为0时，表示中断是禁止的，处理器将不会响应中断请求。
    // 0x6: 0110 表示中断打开的

    // 初始化任务
    task_first_init();
    move_to_first_task();
}
