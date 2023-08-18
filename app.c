/* Test app for daedalus-os */
#include <stdio.h>
#include "daedalus_os.h"

#define PRINT_STR_STACK_SZ 0xFF

void print_str(void *str) {
    printf("%s\n", (const char *)str);
}

int main(void) {
    task_stack print_str_stack[PRINT_STR_STACK_SZ];

    os_init();
    os_task_create(print_str, "Hello world", print_str_stack, PRINT_STR_STACK_SZ, 1);
    os_start();

    // Wouldn't normally get here, just for testing
    printf("Done.\n");
    return 0;
}
