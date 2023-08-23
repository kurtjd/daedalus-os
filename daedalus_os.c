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
static struct os_tcb *ready_list[MAX_PRIORITY_LEVEL];
static uint8_t highest_priority = 0;

// Related to the idle task
static os_task_stack idle_os_task_stack[IDLE_TASK_STACK_SZ];
static uint32_t ticks_in_idle = 0;

static void os_idle_task_entry(void *data)
{
	(void)data;

	ticks_in_idle++;
	printf("Idling\n");
}

static void os_task_insert_into_list(struct os_tcb *task, struct os_tcb **list)
{
	if (!*list) {
		*list = task;
	} else {
		(*list)->prev_task = task;
		task->next_task = *list;
		*list = task;
	}
}

static void os_task_remove_from_list(struct os_tcb *task, struct os_tcb **list)
{
	if (task->next_task)
		task->next_task->prev_task = task->prev_task;

	if (task->prev_task)
		task->prev_task->next_task = task->next_task;
	else
		*list = task->next_task;
	
	task->next_task = NULL;
	task->prev_task = NULL;
}

static void os_task_set_state(struct os_tcb *task, enum OS_TASK_STATE state)
{
	task->state = state;

	switch (state) {
	case TASK_READY:
		os_task_insert_into_list(task, &ready_list[task->priority]);
		break;
	case TASK_BLOCKED:
		running_task = NULL; // Consider if need to do this
		os_task_remove_from_list(task, &ready_list[task->priority]);
		break;
	}
}

static uint8_t os_get_highest_ready_pri(void)
{
	for (int i = highest_priority; i >= 0; i--) {
		if (ready_list[i])
			return i;
	}

	// Shouldn't get here...
	assert(false);
	return 0;
}

static struct os_tcb *os_get_next_ready_task(uint8_t priority)
{
	// Allows for round-robin
	if (running_task && running_task->priority == priority
		&& running_task->next_task)
		return running_task->next_task;

	if (running_task != ready_list[priority])
		return ready_list[priority];
	else
		return NULL;
}

static void os_sw_context(struct os_tcb *next_task)
{
	(void)next_task;

	// ASM store context of current task

	// ASM restore context of next task
}

static void os_schedule(void)
{
	OS_ENTER_CRITICAL();

	uint8_t highest_ready_pri = os_get_highest_ready_pri();
	struct os_tcb *next_task = os_get_next_ready_task(highest_ready_pri);

	// Make sure there is a higher priority task that's ready
	if (next_task) {
		running_task = next_task;
		os_sw_context(next_task);
	}

	OS_EXIT_CRITICAL();

	// For now just execute task (would normally return to new context)
	running_task->entry(running_task->arg);
}

static void os_mutex_assign(struct os_mutex *mutex, struct os_tcb *task)
{
	mutex->holding_task = task;
	mutex->holding_task_orig_pri = task->priority;
}

static bool os_mutex_handle_block(struct os_mutex *mutex,
					uint16_t timeout_ticks)
{
	// Priority inheritance: Set holding task to running task's priority
	mutex->holding_task->priority = running_task->priority;
	running_task->waiting = true;
	
	os_task_insert_into_list(running_task, &mutex->blocked_list);
	os_task_sleep(timeout_ticks);
	os_task_remove_from_list(running_task, &mutex->blocked_list);

	// Task either gets here via timeout or woken by mutex release
	// Commented out until context switch implemented
	/*if (!running_task->waiting) {
		os_mutex_assign(mutex, running_task);
		return true;
	}*/

	// Notifies caller that acquire timed out
	return false;
}

static struct os_tcb *os_mutex_get_high_pri_task(const struct os_mutex *mutex)
{
	struct os_tcb *task = mutex->blocked_list;
	if (!task)
		return NULL;

	struct os_tcb *high_task = task;
	while (task) {
		if (task->priority > high_task->priority) {
			high_task = task;
			task = task->next_task;
		}
	}

	return high_task;
}

static void os_tick(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	(void)hwnd;
	(void)uMsg;
	(void)idEvent;
	(void)dwTime;

	/* Naive approach, optimize later (don't want to have to loop over
	 * every task, only tasks in a timeout list) */
	for (int i = 0; i < task_count; i++) {
		struct os_tcb *task = &tasks[i];

		if (task->timeout > 0) {
			task->timeout--;
			if (task->timeout == 0)
				os_task_set_state(task, TASK_READY);
		}
	}

	os_schedule();
}

/* Public */
void os_init(void)
{
	os_task_create(os_idle_task_entry, NULL, idle_os_task_stack,
			IDLE_TASK_STACK_SZ, 0);
}

void os_start(void)
{
	os_schedule();
	// Enable SysTick interrupt

	// But for now use hacky Windows timer
	SetTimer(NULL, 0, 1000 / CLOCK_RATE_HZ, os_tick);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

uint8_t os_task_create(os_task_entry entry, void *arg,
			os_task_stack *stack_base, size_t stack_sz,
			uint8_t priority)
{
	assert(task_count < MAX_NUM_TASKS);

	struct os_tcb task = {
		.entry = entry,
		.arg = arg,
		.stack_base = stack_base,
		.stack_sz = stack_sz,
		.priority = priority,
		.next_task = NULL,
		.prev_task = NULL,
		.timeout = 0,
		.waiting = false,
		.id = task_count
	};

	tasks[task_count] = task;
	os_task_set_state(&tasks[task_count++], TASK_READY);

	// Push entry as PC onto task's stack
	// Push stack_base+stack_sz as SP onto task's stack
	// Push other registers onto task's stack

	if (priority > highest_priority)
		highest_priority = priority;

	return task.id;
}

void os_task_sleep(uint16_t ticks)
{    
	OS_ENTER_CRITICAL();
	running_task->timeout = ticks;
	os_task_set_state(running_task, TASK_BLOCKED);
	OS_EXIT_CRITICAL();

	os_schedule();
}

static void os_task_wake(struct os_tcb *task)
{
	OS_ENTER_CRITICAL();
	task->waiting = false;
	task->timeout = 0;
	os_task_set_state(task, TASK_READY);
	OS_EXIT_CRITICAL();

	os_schedule();
}

void os_task_yield(void)
{
	/* For now just immediately call scheduler. Yielding has little use in
	 * preemptive RTOS as highest priority task is already running. However
	 * useful if you have roud-robin tasks and want to immediately call the
	 * next one. */
	os_schedule();
}

const struct os_tcb *os_task_query(uint8_t task_id)
{
	return &tasks[task_id];
}

void os_mutex_create(struct os_mutex *mutex)
{
	mutex->holding_task = NULL;
	mutex->blocked_list = NULL;
}

bool os_mutex_acquire(struct os_mutex *mutex, uint16_t timeout_ticks)
{
	if (!mutex->holding_task) {
		os_mutex_assign(mutex, running_task);
		return true;
	} else {
		os_mutex_handle_block(mutex, timeout_ticks);
	}
}

void os_mutex_release(struct os_mutex *mutex)
{
	mutex->holding_task->priority = mutex->holding_task_orig_pri;
	mutex->holding_task = NULL;

	struct os_tcb *high_pri_task = os_mutex_get_high_pri_task(mutex);
	if (high_pri_task)
		os_task_wake(high_pri_task);
}
