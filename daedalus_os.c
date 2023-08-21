#include <assert.h>
#include <stdio.h>
#include "daedalus_os.h"
#include "windows.h"

#define IDLE_TASK_STACK_SZ 16

/* Private */
// Task-related variables
static struct os_tcb tasks[MAX_NUM_TASKS];
static int task_count = 0;
static struct os_tcb *running_task = NULL;

// Helps quickly track which priorities have ready tasks
static struct os_tcb *priority_list[MAX_PRIORITY_LEVEL];
static uint16_t ready_group;
static uint16_t ready_table[PRIORITY_TABLE_SZ];

// Related to the idle task
static os_task_stack idle_os_task_stack[IDLE_TASK_STACK_SZ];
static uint32_t ticks_in_idle = 0;

static void os_idle_task_entry(void *data) {
    (void)data;

    ticks_in_idle++;
    printf("Idling\n");
}

// TODO: Merge these list functions
static void os_task_insert_priority_list(struct os_tcb *task) {
    if (!priority_list[task->priority]) {
        priority_list[task->priority] = task;
    } else {
        struct os_tcb *next = priority_list[task->priority]->next_task;
        priority_list[task->priority]->next_task = task;
        task->next_task = next;
    }
}

static void os_task_insert_block_list(struct os_tcb *task, struct os_tcb **block_list) {
    if (!*block_list) {
        *block_list = task;
    } else {
        /*struct os_tcb *next = (*block_list)->next_blocked;
        (*block_list)->next_blocked = task;
        task->next_blocked = next;*/
        (*block_list)->prev_blocked = task;
        task->next_blocked = *block_list;
        *block_list = task;
    }
}

static void os_task_remove_block_list(struct os_tcb *task, struct os_tcb **block_list) {
    if (task->next_blocked)
        task->next_blocked->prev_blocked = task->prev_blocked;

    if (task->prev_blocked)
        task->prev_blocked->next_blocked = task->next_blocked;
    else
        *block_list = task->next_blocked;
    
    task->next_blocked = NULL;
    task->prev_blocked = NULL;
}

static void os_ready_list_set(uint8_t priority) {
    // Find which bit in the ready group the priority of the task sits
    uint16_t ready_group_idx = priority >> 4;

    // Set that bit to indicate a priority in this index is ready
    ready_group |= (1 << ready_group_idx);

    // Finally set the bit to indicate this specific priority is ready
    ready_table[ready_group_idx] |= (1 << (priority & 0xF));
}

static void os_ready_list_clear(uint8_t priority) {
    struct os_tcb *task = priority_list[priority];
    assert(task != NULL); // We shouldn't ever call this on a priority that has no tasks assigned to it
    
    // Have to be sure there are no tasks at this priority level ready to run
    while (task) {
        if (task->state == TASK_READY)
            return;
        task = task->next_task;
    }

    uint16_t ready_group_idx = priority >> 4;
    ready_table[ready_group_idx] &= ~(1 << (priority & 0xF));

    // No priorities in this group are ready
    if (ready_table[ready_group_idx] == 0)
        ready_group &= ~(1 << ready_group_idx);
}

static void os_task_set_state(struct os_tcb *task, enum OS_TASK_STATE state) {
    task->state = state;

    switch (state) {
    case TASK_READY:
        os_ready_list_set(task->priority);
        break;
    case TASK_BLOCKED:
        running_task = NULL;
        os_ready_list_clear(task->priority);
        break;
    case TASK_RUNNING:
        os_ready_list_clear(task->priority);
        break;
    }
}

static struct os_tcb *os_get_next_ready_task(void) {
    // No task is ready to run
    if (ready_group == 0)
        return NULL;
    
    // First find the highest priority group that has a priority ready
    int priority = 0;
    int tmp = ready_group;
    while (tmp >>= 1)
        priority++;
    
    // Then find the highest priority that has a task ready
    tmp = ready_table[priority];
    assert(tmp != 0); // Shouldn't have a group ready but no priorities in that group ready
    priority *= PRIORITY_GROUP_WIDTH;
    while (tmp >>= 1)
        priority++;
    
    // If the running task is the highest priority task, abort
    if (running_task && priority < running_task->priority)
        return NULL;
    
    // Now find task associated with that priority that is ready
    struct os_tcb *task;
    if (running_task && priority == running_task->priority && running_task->next_task)
        task = running_task->next_task;
    else
        task = priority_list[priority];
    assert(task); // Should have at least one task associated with that priority

    while (task->state != TASK_READY) {
        task = task->next_task;

        // Should always find a task that is ready if priority is greater than running task priority
        assert(task || (running_task && priority == running_task->priority));

        if (!task)
            task = priority_list[priority];
    }

    return task;
}

static void os_sw_context(struct os_tcb *next_task) {
    (void)next_task;

    // ASM store context of current task

    // ASM restore context of next task
}

static void os_schedule(void) {
    OS_ENTER_CRITICAL();

    struct os_tcb *next_task = os_get_next_ready_task();

    // Make sure there is a higher priority task that's ready
    if (next_task) {
        os_task_set_state(next_task, TASK_RUNNING);

        // Running task may have previously been set to blocked if scheduler was called through a sync function
        if (running_task)
            os_task_set_state(running_task, TASK_READY);

        running_task = next_task;
        os_sw_context(next_task);
    }

    OS_EXIT_CRITICAL();

    // For now just execute task (would normally return to new context)
    running_task->entry(running_task->arg);
}

static void os_mutex_assign(struct os_mutex *mutex, struct os_tcb *task) {
    mutex->holding_task = task;
    mutex->holding_task_orig_pri = task->priority;
}

static void os_task_wake(struct os_tcb *task) {
    task->waiting = false;
    task->timeout = 0;
    os_task_set_state(task, TASK_READY);
}

static void ClockTick(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    (void)hwnd;
    (void)uMsg;
    (void)idEvent;
    (void)dwTime;

    // Naive approach, optimize later (don't want to have to loop over every task, only tasks in a timeout list)
    for (int i = 0; i < task_count; i++) {
        struct os_tcb *task = &tasks[i];
        if (task->timeout > 0)
            task->timeout--;
        
        if (task->timeout == 0)
            os_task_set_state(task, TASK_READY);
    }

    os_schedule();
}

/* Public */
void os_init(void) {
    os_task_create(os_idle_task_entry, NULL, idle_os_task_stack, IDLE_TASK_STACK_SZ, 0);
}

void os_start(void) {
    //os_schedule();

    // Enable SysTick interrupt

    // But for now use hacky Windows timer
    SetTimer(NULL, 0, 1000 / CLOCK_RATE_HZ, ClockTick);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

uint8_t os_task_create(os_task_entry entry, void *arg, os_task_stack *stack_base, size_t stack_sz, uint8_t priority) {
    assert(task_count < MAX_NUM_TASKS);

    struct os_tcb task = {
        .entry = entry,
        .arg = arg,
        .stack_base = stack_base,
        .stack_sz = stack_sz,
        .priority = priority,
        .next_task = NULL,
        .next_blocked = NULL,
        .prev_blocked = NULL,
        .timeout = 0,
        .waiting = false,
        .id = task_count
    };

    tasks[task_count] = task;
    os_task_insert_priority_list(&tasks[task_count]);
    os_task_set_state(&tasks[task_count], TASK_READY);
    task_count++;

    // Push entry as PC onto task's stack
    // Push stack_base+stack_sz as SP onto task's stack
    // Push other registers onto task's stack

    return task.id;
}

void os_task_sleep(uint16_t ticks) {    
    OS_ENTER_CRITICAL();
    running_task->timeout = ticks;
    os_task_set_state(running_task, TASK_BLOCKED);
    OS_EXIT_CRITICAL();

    os_schedule();
}

void os_task_yield(void) {
    // For now just immediately call scheduler
    // Yielding has little use in preemptive RTOS as highest priority task is already running
    // However useful if you have roud-robin tasks and want to immediately call the next one
    os_schedule();
}

const struct os_tcb *os_task_query(uint8_t task_id) {
    return &tasks[task_id];
}

void os_mutex_create(struct os_mutex *mutex) {
    mutex->holding_task = NULL;
    mutex->blocked_list = NULL;
}

bool os_mutex_acquire(struct os_mutex *mutex, uint16_t timeout_ticks) {
    if (!mutex->holding_task) {
        os_mutex_assign(mutex, running_task);
        return true;
    } else {
        // TODO: Handle priority inheritance (make holding task pri = running task pri)
        running_task->waiting = true;
        
        os_task_insert_block_list(running_task, &mutex->blocked_list);
        os_task_sleep(timeout_ticks);
        os_task_remove_block_list(running_task, &mutex->blocked_list);

        // Task either gets here via timeout or woken by mutex release
        // Commented out until context switch implemented
        /*if (!running_task->waiting) {
            os_mutex_assign(mutex, running_task);
            return true;
        }*/

        // Notifies caller that acquire timed out
        return false;
    }
}

void os_mutex_release(struct os_mutex *mutex) {
    mutex->holding_task = NULL;
    // TODO: Handle resetting priority back to original

    // Wanted to use tables like for readying tasks but tasks sharing priorities makes that difficult...
    struct os_tcb *task = mutex->blocked_list;
    if (task) {
        uint8_t high_pri = 0;
        struct os_tcb *high_task = NULL;

        while (task) {
            if (task->priority >= high_pri) {
                high_pri = task->priority;
                high_task = task;
                task = task->next_blocked;
            }
        }

        os_task_wake(high_task);
        os_schedule();
    }
}
