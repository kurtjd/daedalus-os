#ifndef DAEDALUS_OS_H
#define DAEDALUS_OS_H

#include <stdbool.h>
#include <stdint.h>

/* Config (modified by user) */
#define MAX_NUM_TASKS 32
#define MAX_PRIORITY_LEVEL 31

/* Max Supported Values (should NOT be modified by user) */
#define MAX_SUPPORTED_PRIORITY_LEVEL 255

/* Typedefs */
typedef void (*task_entry)(void *);
typedef uint32_t task_stack;

/* Macros */
#define OS_ENTER_CRITICAL() asm("nop")
#define OS_EXIT_CRITICAL() asm("nop")

/* Public Functions */
void os_init(void);
struct tcb *os_task_create(task_entry entry, void *arg, task_stack *stack_base, size_t stack_sz, uint8_t priority);
void os_start(void);

/* These will be made private, here for testing */
enum TASK_STATE {
    TASK_BLOCKED,
    TASK_READY,
    TASK_RUNNING
};

struct tcb {
    task_entry entry;
    void *arg;
    task_stack *stack_base;
    size_t stack_sz;
    uint8_t priority;
    enum TASK_STATE state;
    struct tcb *next_task;
};

struct mutex {
    struct tcb *holding_task;
    struct tcb *blocked_list[MAX_NUM_TASKS];
    int blocked_count;
};

/* Testing Functions */
void os_task_set_state_pub(struct tcb *task, enum TASK_STATE state);


#endif
