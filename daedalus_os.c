#include <assert.h>
#include <stdio.h>
#include "daedalus_os.h"
#include "windows.h"

#define IDLE_TASK_STACK_SZ 16
#define READY_GROUP_WIDTH 16

/* Private */
// Task-related variables
static struct tcb tasks[MAX_NUM_TASKS];
static int task_count = 0;
static struct tcb *running_task = NULL;

// Helps quickly track which priorities have ready tasks
static struct tcb *priority_list[MAX_PRIORITY_LEVEL];
static uint16_t ready_group;
static uint16_t ready_table[MAX_PRIORITY_LEVEL / 16 + 1];

// Related to the idle stack
static task_stack idle_task_stack[IDLE_TASK_STACK_SZ];
static uint32_t ticks_in_idle = 0;

static void idle_task_entry(void *data) {
    (void)data;

    ticks_in_idle++;
    printf("Idling\n");
}

static void os_task_insert_priority_list(struct tcb *task) {
    if (!priority_list[task->priority]) {
        priority_list[task->priority] = task;
    } else {
        struct tcb *next = priority_list[task->priority]->next_task;
        priority_list[task->priority]->next_task = task;
        task->next_task = next;
    }
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
    struct tcb *task = priority_list[priority];
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

static void os_task_set_state(struct tcb *task, enum TASK_STATE state) {
    task->state = state;

    switch (state) {
    case TASK_READY:
        os_ready_list_set(task->priority);
        break;
    case TASK_BLOCKED:
        running_task = NULL;
        break;
    case TASK_RUNNING:
        os_ready_list_clear(task->priority);
        break;
    }
}

static struct tcb *os_get_next_ready_task(void) {
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
    priority *= READY_GROUP_WIDTH;
    while (tmp >>= 1)
        priority++;
    
    // If the running task is the highest priority task, abort
    if (running_task && priority < running_task->priority)
        return NULL;
    
    // Now find task associated with that priority that is ready
    struct tcb *task;
    if (running_task && priority == running_task->priority && running_task->next_task)
        task = running_task->next_task;
    else
        task = priority_list[priority];
    assert(task); // Should have at least one task associated with that priority

    // Not this simple for round-robin...
    while (task->state != TASK_READY) {
        task = task->next_task;

        // Should always find a task that is ready if priority is greater than running task priority
        assert(task || (running_task && priority == running_task->priority));

        if (!task)
            task = priority_list[priority];
    }

    return task;
}

static void os_sw_context(struct tcb *next_task) {
    (void)next_task;

    // ASM store context of current task

    // ASM restore context of next task
}

static void os_schedule(void) {
    OS_ENTER_CRITICAL();

    struct tcb *next_task = os_get_next_ready_task();

    // Make sure there is a higher priority task that's ready
    if (next_task) {
        os_task_set_state(next_task, TASK_RUNNING);

        // Running task may have previously been set to blocked if scheduler was called through a sync function
        if (running_task)
            os_task_set_state(running_task, TASK_READY);

        os_sw_context(next_task);
        running_task = next_task;
    }

    // For now just execute task (would normally return to new context)
    running_task->entry(running_task->arg);

    OS_EXIT_CRITICAL();
}

static void ClockTick(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    (void)hwnd;
    (void)uMsg;
    (void)idEvent;
    (void)dwTime;

    os_schedule();
}

/* Public */
void os_init(void) {
    os_task_create(idle_task_entry, NULL, idle_task_stack, IDLE_TASK_STACK_SZ, 0);
}

void os_task_create(task_entry entry, void *arg, task_stack *stack_base, size_t stack_sz, uint8_t priority) {
    assert(task_count < MAX_NUM_TASKS);

    struct tcb task = {
        .entry = entry,
        .arg = arg,
        .stack_base = stack_base,
        .stack_sz = stack_sz,
        .priority = priority,
        .next_task = NULL
    };

    tasks[task_count] = task;
    os_task_insert_priority_list(&tasks[task_count]);
    os_task_set_state(&tasks[task_count], TASK_READY);
    task_count++;

    // Push entry as PC onto task's stack
    // Push stack_base+stack_sz as SP onto task's stack
    // Push other registers onto task's stack
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

/* Testing functions */
void os_task_set_state_pub(struct tcb *task, enum TASK_STATE state) {
    os_task_set_state(task, state);
}

/*
void os_mutex_init(struct mutex *mutex) {
    mutex->holding_task = NULL;
    mutex->blocked_count = 0;
}

void os_mutex_acquire(struct mutex *mutex) {
    OS_ENTER_CRITICAL();

    if (!mutex->holding_task) {
        mutex->holding_task = running_task;

        OS_EXIT_CRITICAL();
    } else {
        running_task->state = TASK_BLOCKED;
        mutex->blocked_list[mutex->blocked_count++] = running_task;

        OS_EXIT_CRITICAL();
        os_schedule(); // How prevent os_schedule from going on task's stack?
    }
}

void os_mutex_release(struct mutex *mutex) {
    OS_ENTER_CRITICAL();

    if (running_task != mutex->holding_task)
        return;
    
    mutex->holding_task = NULL;

    for (int i = 0; i < mutex->blocked_count; i++)
        mutex->blocked_list[i]->state = TASK_READY;
    mutex->blocked_count = 0;

    OS_EXIT_CRITICAL();
    os_schedule();
}
*/
