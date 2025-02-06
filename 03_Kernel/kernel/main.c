#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "process.h"

extern struct ioqueue g_keyboardIOQueue;
void thread_A(void *arg);
void process_A();
int a = 0;

int main(void) {
    put_str("\nkernel start!\n");
    idt_init();
    timer_init();
    memory_init();
    thread_init();
    console_init();
    keyboard_init();
    tss_init();

    thread_create("ThreadA", 20, thread_A, NULL);
    // thread_create("process_A", 20, process_A, NULL);
    process_execute(process_A, "process_A");
    interrupt_enable();
    while(1) {
        // console_put_str("Main  ");
    }
    return 0;
}

void thread_A(void *arg) {
    while(1) {
        console_put_str("thread_A, a=");
        console_put_int(a);
        console_put_char('\n');
    }
}

void process_A(void *arg) {
    while(1) {
        a++;
    }
}