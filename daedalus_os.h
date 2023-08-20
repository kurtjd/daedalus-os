#ifndef DAEDALUS_OS_H
#define DAEDALUS_OS_H

#include <stdbool.h>
#include <stdint.h>

/* Config (modified by user) */
#define MAX_NUM_TASKS 32
#define MAX_PRIORITY_LEVEL 31
#define CLOCK_RATE_HZ 2

/* Max Supported Values (should NOT be modified by user) */
#define MAX_SUPPORTED_PRIORITY_LEVEL 255
#define MAX_SUPPORTED_NUM_TASKS 255

/* Typedefs */
typedef void (*os_task_entry)(void *);
typedef uint32_t os_task_stack;

/* Macros */
#define OS_ENTER_CRITICAL() asm("nop")
#define OS_EXIT_CRITICAL() asm("nop")
#define OS_SEC_TO_TICKS(sec) ((sec) * CLOCK_RATE_HZ)

/* These will be made private, here for testing */
enum OS_TASK_STATE {
    TASK_BLOCKED,
    TASK_READY,
    TASK_RUNNING
};

// Consider rearranging members for efficient struct packing
struct os_tcb {
    os_task_entry entry;
    void *arg;
    os_task_stack *stack_base;
    size_t stack_sz;
    uint8_t priority;
    enum OS_TASK_STATE state;
    struct os_tcb *next_task;
    uint16_t timeout;
    uint8_t id;
};

struct os_mutex {
    struct os_tcb *holding_task;
    struct os_tcb *blocked_list[MAX_NUM_TASKS];
    int blocked_count;
};

/* Public Functions */
void os_init(void);
void os_start(void);
uint8_t os_task_create(os_task_entry entry, void *arg, os_task_stack *stack_base, size_t stack_sz, uint8_t priority);
void os_task_delay(uint16_t clock_ticks);
void os_task_yield(void);
const struct os_tcb *os_task_query(uint8_t task_id);

#endif
