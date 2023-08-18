#ifndef DAEDALUS_OS_H
#define DAEDALUS_OS_H

#include <stdbool.h>
#include <stdint.h>

/* Config */
#define MAX_NUM_TASKS 32
#define MAX_PRIORITY_LEVEL 10

/* Typedefs */
typedef void (*task_entry)(void *);
typedef uint32_t task_stack;

/* Macros */
#define OS_ENTER_CRITICAL() asm("nop")
#define OS_EXIT_CRITICAL() asm("nop")

/* Public Functions */
void os_init(void);
bool os_task_create(task_entry entry, void *arg, task_stack *stack_base, size_t stack_sz, int priority);
void os_start(void);

#endif
