/* Test app for daedalus-os */
#include <stdio.h>
#include "daedalus_os.h"

#define PRINT_TASK_STACK_SZ 0xF

void print_task(void *str) {
    printf("%s\n", (const char *)str);
}

int main(void) {
    task_stack print_task_stack[PRINT_TASK_STACK_SZ];

    os_init();
    struct tcb *taskA = os_task_create(print_task, "Mmmm I'm task A!", print_task_stack, PRINT_TASK_STACK_SZ, 17);
    struct tcb *taskB = os_task_create(print_task, "Mmmm I'm task B!", print_task_stack, PRINT_TASK_STACK_SZ, 17);

    // Just manually call os_schedule (via os_start) to simulate systick interrupt
    for (int i = 0; i < 20; i++) {
        printf("%d: ", i);
        os_start();

        if (i == 0)
            os_task_set_state_pub(taskA, TASK_BLOCKED);
        else if (i == 1)
            os_task_set_state_pub(taskB, TASK_BLOCKED);
        else if (i == 10) {
            os_task_set_state_pub(taskA, TASK_READY);
            os_task_set_state_pub(taskB, TASK_READY);
        }
    }

    // Wouldn't normally get here, just for testing
    printf("\nDone.\n");
    return 0;
}
