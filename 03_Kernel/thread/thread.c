#include "thread.h"
#include "list.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"

#define PG_SIZE (4096)
/* 主线程PCB */
static struct PCB_INFO* s_mainThreadPCB;
/* 就绪线程队列 */
static struct list s_readyThreadList;
/* 所有线程队列 */
static struct list s_allThreadList;
/* 线程结点 */
struct list_elem* g_curThreadTag;
/* 跳转执行函数：由汇编语言实现 */
extern void switch_to(struct PCB_INFO* cur_pcb, struct PCB_INFO* next_pcb);

/**************** 线程调度相关函数 *******************/
/* 获取当前线程pcb指针 */
struct PCB_INFO* get_curthread_pcb() {
    uint32_t esp; 
    asm ("mov %%esp, %0" : "=g" (esp));
    /* 取esp整数部分即pcb页起始地址 */
    return (struct PCB_INFO*)(esp & 0xfffff000);
}

void thread_schedule() {
    ASSERT(get_interrupt_status() == INTERRUPT_DISABLE);
    struct PCB_INFO* cur_thread_pcb = get_curthread_pcb(); 
    if (cur_thread_pcb->status == TASK_RUNNING) { // 若此线程只是cpu时间片到了,将其加入到就绪队列尾
        // ASSERT(!elem_find(&s_readyThreadList, &cur_thread_pcb->ready_list_tag));
        list_append(&s_readyThreadList, &cur_thread_pcb->ready_list_tag);
        cur_thread_pcb->ticks = cur_thread_pcb->priority;   // 重置 ticks 为其 priority
        cur_thread_pcb->status = TASK_READY;
    } else { 
        /* 若此线程需要某事件发生后才能继续上cpu运行,
        不需要将其加入队列, 因为当前线程不在就绪队列中。*/
    }
    ASSERT(!list_empty(&s_readyThreadList));
    g_curThreadTag = NULL;	  // thread_tag清空
    g_curThreadTag = list_pop(&s_readyThreadList);  // 取出队列的第一个线程跳转执行
    struct PCB_INFO* next_thread_pcb = GET_ENTRYPTR_FROM_LISTTAG(struct PCB_INFO, ready_list_tag, g_curThreadTag);
    next_thread_pcb->status = TASK_RUNNING;
    put_str("\nschedule next = ");
    put_int((int)next_thread_pcb);
    put_char('\n');
    switch_to(cur_thread_pcb, next_thread_pcb);
}

/**************** 线程阻塞相关函数 *******************/
/* 当前线程将自己阻塞 */
void thread_block(enum TASK_STATUS status) {
    /* stat取值为 TASK_BLOCKED, TASK_WAITING, TASK_HANGING, 也就是只有这三种状态才不会被调度*/
    ASSERT(((status == TASK_BLOCKED) || (status == TASK_WAITING) || (status == TASK_HANGING)));
    enum interrupt_status old_status = get_interrupt_status();
    interrupt_disable();
    struct PCB_INFO* cur_thread_pcb = get_curthread_pcb();
    cur_thread_pcb->status = status;  // 置其状态为stat 
    thread_schedule();                // 将当前线程换下处理器
    /* 待当前线程被解除阻塞后才继续运行下面的 set_interrupt_status */
    set_interrupt_status(old_status);
}

/* 将线程解除阻塞 */
void thread_unblock(struct PCB_INFO* pthread) {
    enum interrupt_status old_status = get_interrupt_status();
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));
    if (pthread->status != TASK_READY) {
        if (elem_find(&s_readyThreadList, &pthread->ready_list_tag)) {
            return;
        }
        pthread->status = TASK_READY;
        list_push(&s_readyThreadList, &pthread->ready_list_tag);    // 放到队列的最前面,使其尽快得到调度
    } 
    set_interrupt_status(old_status);
}

/**************** 线程创建相关函数 *******************/
/* 初始化线程基本信息 */
static void init_thread_pcb(struct PCB_INFO* thread_pcb, char* name, int prio) {
    memset(thread_pcb, 0, sizeof(*thread_pcb));
    strcpy(thread_pcb->name, name);

    if (thread_pcb == s_mainThreadPCB) {
        /* 由于把main函数也封装成一个线程, 并且它一直是运行的, 故将其直接设为 TASK_RUNNING */
        thread_pcb->status = TASK_RUNNING;
    } else {
        thread_pcb->status = TASK_READY;
    }

    /* self_kstack 是线程自己在内核态下使用的栈顶地址 */
    thread_pcb->self_kstack = (uint32_t*)((uint32_t)thread_pcb + PG_SIZE);
    thread_pcb->priority = prio;
    thread_pcb->ticks = prio;
    thread_pcb->elapsed_ticks = 0;
    thread_pcb->pgdir = NULL;
    thread_pcb->stack_magic = 0x19870916;  // 自定义的魔数
}
/* 由 thread_func 去执行 start_routine(arg) */
static void thread_running(thread_func start_routine, void* arg) {
    interrupt_enable();
    start_routine(arg);
}
/* 初始化线程栈 THREAD_STACK, 将待执行的函数和参数放到 THREAD_STACK 中相应的位置 */
static void init_thread_stack(struct PCB_INFO* thread_pcb, thread_func start_routine, void* arg) {
   /* 预留中断栈空间 */
   thread_pcb->self_kstack -= sizeof(struct INTR_STACK);
   /* 预留线程栈空间 */
   thread_pcb->self_kstack -= sizeof(struct THREAD_STACK);
   struct THREAD_STACK* kthread_stack = (struct THREAD_STACK*)thread_pcb->self_kstack;
   kthread_stack->eip = thread_running;
   kthread_stack->function = start_routine;
   kthread_stack->func_arg = arg;
   kthread_stack->ebp = 0;
   kthread_stack->ebx = 0;
   kthread_stack->esi = 0;
   kthread_stack->edi = 0;
}
/* 创建一优先级为prio的线程, 线程名为name, 线程所执行的函数是 function(func_arg) */
struct PCB_INFO* thread_create(char* name, int prio, thread_func start_routine, void* arg) {
    /* 从内核申请PCB对应的内存 */
    struct PCB_INFO* pcb = get_kernel_pages(1);

    init_thread_pcb(pcb, name, prio);
    init_thread_stack(pcb, start_routine, arg);
    /* 加入就绪线程队列 */
    // ASSERT(!elem_find(&s_readyThreadList, &pcb->ready_list_tag));
    list_append(&s_readyThreadList, &pcb->ready_list_tag);
    /* 加入全部线程队列 */
    // ASSERT(!elem_find(&s_allThreadList, &pcb->all_list_tag));
    list_append(&s_allThreadList, &pcb->all_list_tag);
    return pcb;
}

/**************** 内核进程 main 转换为线程相关操作  *******************/
/* 将kernel中的main函数完善为主线程 */
static void make_kernelmain_to_thread(void) {
    /* 因为main线程早已运行,咱们在loader.S中进入内核时的 mov esp,0xc009f000,
    *  就是为其预留了pcb,地址为0xc009e000,因此不需要通过get_kernel_page另分配一页 */
    s_mainThreadPCB = get_curthread_pcb();
    put_str("\ns_mainThreadPCB: ");
    put_int((uint32_t)s_mainThreadPCB);
    put_char('\n');
    init_thread_pcb(s_mainThreadPCB, "main", 31);
    /* main函数是当前线程, 当前线程不在 s_readyThreadList 中,
    * 所以只将其加在 s_allThreadList 中. */
    ASSERT(!elem_find(&s_allThreadList, &s_mainThreadPCB->all_list_tag));
    list_append(&s_allThreadList, &s_mainThreadPCB->all_list_tag);
}

/**************** 初始化 *******************/
void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&s_readyThreadList);
    list_init(&s_allThreadList);
    /* 将当前main函数创建为线程 */
    make_kernelmain_to_thread();
    put_str("thread_init done\n");
}
