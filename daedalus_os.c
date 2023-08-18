#include "daedalus_os.h"

#define IDLE_TASK_STACK_SZ 16

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
    int priority;
    enum TASK_STATE state;
};

/* Private */
static struct tcb tasks[MAX_NUM_TASKS];
static int task_count = 0;
static struct tcb *current_task = NULL;

static task_stack idle_task_stack[IDLE_TASK_STACK_SZ];
static uint32_t ticks_in_idle = 0;

static void idle_task_entry(void *data) {
    (void)data;

    ticks_in_idle++;
}

// TODO: Optimize this so as to not have to loop over every task
static struct tcb *os_get_next_ready_task(void) {
    int max_priority = 0;
    struct tcb *max_task = NULL;

    for (int i = 0; i < task_count; i++) {
        struct tcb *task = &tasks[i];
        if (task->state == TASK_READY && task->priority > max_priority) {
            max_priority = task->priority;
            max_task = task;
        }
    }

    return max_task;
}

static void os_sw_context(struct tcb *new_task) {
    (void)new_task;

    // ASM store context of current task

    // ASM restore context of new task
}

static void os_schedule(void) {
    OS_ENTER_CRITICAL();

    struct tcb *next_task = os_get_next_ready_task();

    if (next_task) {
        next_task->state = TASK_RUNNING;
        current_task->state = TASK_READY;

        os_sw_context(next_task);
        current_task = next_task;
    }

    OS_EXIT_CRITICAL();
}

/* Public */
void os_init(void) {
    os_task_create(idle_task_entry, NULL, idle_task_stack, IDLE_TASK_STACK_SZ, 0);
    current_task = &tasks[0];
}

bool os_task_create(task_entry entry, void *arg, task_stack *stack_base, size_t stack_sz, int priority) {
    if (task_count >= MAX_NUM_TASKS)
        return false;

    struct tcb task = {
        .entry = entry,
        .arg = arg,
        .stack_base = stack_base,
        .stack_sz = stack_sz,
        .priority = priority,
        .state = TASK_READY
    };

    tasks[task_count++] = task;

    // Push entry as PC onto task's stack
    // Push stack_base+stack_sz as SP onto task's stack
    // Push other registers onto task's stack

    return true;
}

void os_start(void) {
    os_schedule();

    // Enable SysTick interrupt
}
