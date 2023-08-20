/* Test app for daedalus-os */
#include <stdio.h>
#include "daedalus_os.h"

#define TASK_STACK_SZ 0xF

void print_task(void *str) {
    printf("%s\n", (const char *)str);
}

void delay_task(void *str) {
    printf("%s\n", (const char *)str);
    os_task_delay(OS_SEC_TO_TICKS(5));
}

void yield_task(void *str) {
    printf("%s\n", (const char *)str);
    os_task_yield();
}

int main(void) {
    // Tasks just share stack for now until implement context switching
    os_task_stack task_stack[TASK_STACK_SZ];

    os_init();
    os_task_create(print_task, "Round-robin printer A", task_stack, TASK_STACK_SZ, 1);
    os_task_create(print_task, "Round-robin printer B", task_stack, TASK_STACK_SZ, 1);
    os_task_create(yield_task, "Round-robin yielder C", task_stack, TASK_STACK_SZ, 1);
    os_task_create(delay_task, "I am HIGH-PRIORITY, so I preempt, print and delay!", task_stack, TASK_STACK_SZ, 100);

    os_start();

    return 0;
}
