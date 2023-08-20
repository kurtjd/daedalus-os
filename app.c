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
    //os_task_create(print_task, "Mmmm I'm task A!", print_task_stack, PRINT_TASK_STACK_SZ, 17);
    //os_task_create(print_task, "Mmmm I'm task B!", print_task_stack, PRINT_TASK_STACK_SZ, 17);
    //os_task_create(print_task, "Mmmm I'm task C!", print_task_stack, PRINT_TASK_STACK_SZ, 17);

    os_start();

    // Wouldn't normally get here, just for testing
    printf("\nDone.\n");
    return 0;
}
