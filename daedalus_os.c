#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "daedalus_os.h"

// Port-specific (Cortex-M3)
#define STK_BASE 0xE000E010
#define STK_CTRL ((*(volatile uint32_t *)(STK_BASE + 0x00)))
#define STK_LOAD ((*(volatile uint32_t *)(STK_BASE + 0x04)))
#define STK_VAL ((*(volatile uint32_t *)(STK_BASE + 0x08)))
#define STK_CTRL_CPU_CLK (1 << 2)
#define STK_CTRL_EN_INT (1 << 1)
#define STK_CTRL_ENABLE (1 << 0)
#define SCB_ICSR ((*(volatile uint32_t *)(0xE000ED04)))
#define PENDSV_SET (1 << 28)
#define SW_CONTEXT() do { \
    SCB_ICSR |= PENDSV_SET; \
    asm("isb"); \
} while (0)

// Task-related variables
static struct os_tcb tasks[MAX_NUM_TASKS];
static int task_count = 0;
static struct os_tcb *running_task = NULL;
static struct os_tcb *prev_task = NULL;
static struct os_tcb *ready_list[MAX_PRIORITY_LEVEL];
static uint8_t highest_priority = 0;

// Related to the idle task
#define IDLE_TASK_STACK_SZ 32
static os_task_stack idle_os_task_stack[IDLE_TASK_STACK_SZ];
static uint32_t ticks_in_idle = 0;





/***************************************************************************************************
 * Private/Helper Functions
 **************************************************************************************************/
static void os_idle_task_entry(void *data)
{
	(void)data;
	while (1) {
		ticks_in_idle++;
	}
}

static void os_list_insert_task(struct os_tcb *task, struct os_tcb **list)
{
	if (!*list) {
		*list = task;
	} else {
		(*list)->prev_task = task;
		task->next_task = *list;
		*list = task;
	}
}

static void os_list_remove_task(struct os_tcb *task, struct os_tcb **list)
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

static struct os_tcb *os_list_get_high_pri(const struct os_tcb *list)
{
	const struct os_tcb *task = list;
	if (!task)
		return NULL;

	const struct os_tcb *high_task = task;
	while (task) {
		if (task->priority > high_task->priority)
			high_task = task;

		task = task->next_task;
	}

	return (struct os_tcb *)high_task;
}

static void os_task_set_state(struct os_tcb *task, enum OS_TASK_STATE state)
{
	task->state = state;

	switch (state) {
	case TASK_READY:
		os_list_insert_task(task, &ready_list[task->priority]);
		break;
	case TASK_BLOCKED:
		break;
	}
}

static enum OS_STATUS os_task_wait(uint16_t timeout_ticks, struct os_tcb **blocked_list)
{
	// Don't wait
	if (timeout_ticks == 0)
		return OS_TIMEOUT;
	
	running_task->waiting = true;

	os_list_remove_task(running_task, &ready_list[running_task->priority]);
	os_list_insert_task(running_task, blocked_list);

	os_task_sleep(timeout_ticks);

	// TODO: But if task timed out we never removed from block list... fix that
	bool waiting = running_task->waiting;
	running_task->waiting = false;

	return waiting ? OS_TIMEOUT : OS_SUCCESS;
}

static void os_task_wake(struct os_tcb *task, struct os_tcb **list)
{
	task->waiting = false;
	task->timeout = 0;
	os_list_remove_task(task, list);
	os_task_set_state(task, TASK_READY);
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
	// Starting first task (maybe set running task in os_start)
	if (!running_task)
		return ready_list[priority];
	
	// Allows for round-robin
	if (running_task->state != TASK_BLOCKED && running_task->priority == priority
		&& running_task->next_task)
		return running_task->next_task;

	// If no other tasks ready, return NULL so we don't do a context switch
	return ((running_task != ready_list[priority]) ? ready_list[priority] : NULL);
}

static void os_schedule(void)
{
	uint8_t highest_ready_pri = os_get_highest_ready_pri();
	struct os_tcb *next_task = os_get_next_ready_task(highest_ready_pri);

	// Make sure there is a higher priority task that's ready before context switch
	if (next_task) {
		prev_task = running_task;
		running_task = next_task;
		SW_CONTEXT();
	}
}

static void os_tick(void)
{
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

static void os_list_wake_high_pri(struct os_tcb **list)
{
	// In task_wake we remove from blocked list and add to ready list
	struct os_tcb *next_task = os_list_get_high_pri(*list);
	if (next_task) {
		os_task_wake(next_task, list);
		os_schedule();
	}
}



/***************************************************************************************************
 * General OS Functions
 **************************************************************************************************/
void os_init(void)
{
	os_task_create(os_idle_task_entry, NULL, idle_os_task_stack, IDLE_TASK_STACK_SZ, 0);
}

void os_start(void)
{
	// Start SysTick so it fires at the rate specified by OS_CLK_HZ
	STK_LOAD |= ((CPU_CLK_HZ / OS_CLK_HZ) - 1);
	STK_VAL = 0;
	STK_CTRL |= (STK_CTRL_CPU_CLK | STK_CTRL_EN_INT | STK_CTRL_ENABLE);

	/* Todo: Figure out a better way to handle below. Want to be able to call os_schedule()
	immediately and have PendSV automaticlaly return into correct mode. */

	// Set PSP as stack pointer in thread mode
	asm volatile(
		"mov R0, #0x02\n"
		"msr control, R0\n"
	);

	// Set Idle as running task then context switch into it
	running_task = &tasks[0];
	SW_CONTEXT();
}



/***************************************************************************************************
 * Task Functions
 **************************************************************************************************/
uint8_t os_task_create(os_task_entry entry, void *arg, os_task_stack *stack_base, size_t stack_sz,
			uint8_t priority)
{
	assert(task_count < MAX_NUM_TASKS);

	struct os_tcb task = {
		.entry = entry,
		.arg = arg,
		.stack_pntr = stack_base + stack_sz,
		.priority = priority,
		.next_task = NULL,
		.prev_task = NULL,
		.timeout = 0,
		.waiting = false,
		.wait_flags = 0,
		.id = task_count
	};

	/* We need to manually hand-jam parts of the task's stack so it can be loaded in initially.
	   When an exception (in our case, PendSV) is entered, the stack frame is built as follows:

	   0x20 - SP begin  (-0)
	   0x1C - xPSR      (-1)
	   0x18 - PC        (-2)
	   0x14 - LR        (-3)
	   0x10 - R12       (-4)
	   0x0C - R3        (-5)
	   0x08 - R2        (-6)
	   0x04 - R1        (-7)
	   0x00 - R0        (-8)

	   So, we need to manually build this first. We only care about PSR, which we need to ensure
	   the Thumb-mode bit is set, PC which we set to the task's entry point, LR which we set to
	   ensure the exception returns to the correct mode, and finally R0 which represents the
	   argument to the task's entry function.

	   If the SP isn't double-word aligned (8 bytes) the hardware will align it, so we need to
	   take that into account below when we hand-jam into the appropriate index. First we check
	   if the initial stack-pointer will be 8 byte aligned or not, and adjust the offset
	   appropriately. Then we start placing the needed values into the appropriate spots in the
	   stack. */
	if (((int)task.stack_pntr % 8) != 0)
		task.stack_pntr--;
	*(task.stack_pntr - 1) = 0x1000000; // Sets Thumb-mode bit
	*(task.stack_pntr - 2) = (uint32_t)task.entry;
	*(task.stack_pntr - 3) = 0xFFFFFFFD; // Sets Thread mode with PSP
	*(task.stack_pntr - 8) = (uint32_t)task.arg;
	task.stack_pntr -= 16; // Decrement stack pointer to simulate 16 registers being pushed
	
	tasks[task_count] = task;
	os_task_set_state(&tasks[task_count++], TASK_READY);

	if (priority > highest_priority)
		highest_priority = priority;

	return task.id;
}

void os_task_sleep(uint16_t ticks)
{    
	// Other functions that set task waiting will have already removed it from ready list
	if (!running_task->waiting)
		os_list_remove_task(running_task, &ready_list[running_task->priority]);
	
	running_task->timeout = ticks;
	os_task_set_state(running_task, TASK_BLOCKED);

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



/***************************************************************************************************
 * Mutex Functions
 **************************************************************************************************/
void os_mutex_create(struct os_mutex *mutex)
{
	mutex->holding_task = NULL;
	mutex->blocked_list = NULL;
}

enum OS_STATUS os_mutex_acquire(struct os_mutex *mutex, uint16_t timeout_ticks)
{
	if (mutex->holding_task) {
		// Priority inheritance
		// TODO: Mostly redundant as os_task_wait removes from ready list
		// We just don't want it removing from the wrong ready list
		if (mutex->holding_task->priority < running_task->priority) {
			os_list_remove_task(mutex->holding_task,
						&ready_list[mutex->holding_task->priority]);

			mutex->holding_task->priority = running_task->priority;

			os_list_insert_task(mutex->holding_task,
						&ready_list[mutex->holding_task->priority]);
		}

		if (os_task_wait(timeout_ticks, &mutex->blocked_list) == OS_TIMEOUT)
			return OS_TIMEOUT;
	}
	
	mutex->holding_task = running_task;
	mutex->holding_task_orig_pri = running_task->priority;
	return OS_SUCCESS;
}

void os_mutex_release(struct os_mutex *mutex)
{
	mutex->holding_task->priority = mutex->holding_task_orig_pri;
	mutex->holding_task = NULL;
	os_list_wake_high_pri(&mutex->blocked_list);
}



/***************************************************************************************************
 * Semaphore Functions
 **************************************************************************************************/
void os_semph_create(struct os_semph *semph, uint8_t count)
{
	semph->count = count;
	semph->blocked_list = NULL;
}

enum OS_STATUS os_semph_take(struct os_semph *semph, uint16_t timeout_ticks)
{
	if (semph->count <= 0 && os_task_wait(timeout_ticks, &semph->blocked_list) == OS_TIMEOUT)
		return OS_TIMEOUT;
	
	semph->count--;
	return OS_SUCCESS;
}

void os_semph_give(struct os_semph *semph)
{
	semph->count++;
	os_list_wake_high_pri(&semph->blocked_list);
}



/***************************************************************************************************
 * Queue Functions
 **************************************************************************************************/
void os_queue_create(struct os_queue *queue, size_t length, uint8_t *storage, size_t item_sz)
{
	queue->size = length * item_sz;
	queue->item_sz = item_sz;
	queue->storage = storage;
	queue->head = 0;
	queue->tail = 0;
	queue->full = false;
	queue->rec_blocked_list = NULL;
	queue->ins_blocked_list = NULL;
}

enum OS_STATUS os_queue_insert(struct os_queue *queue, const void *item, uint16_t timeout_ticks)
{
	if (queue->full && os_task_wait(timeout_ticks, &queue->ins_blocked_list) == OS_TIMEOUT)
		return OS_TIMEOUT;

	memcpy(queue->storage + queue->head, item, queue->item_sz);
	queue->head = (queue->head + queue->item_sz) % queue->size;

	if (queue->head == queue->tail)
		queue->full = true;

	os_list_wake_high_pri(&queue->rec_blocked_list);
	return OS_SUCCESS;
}

enum OS_STATUS os_queue_retrieve(struct os_queue *queue, void *item, uint16_t timeout_ticks)
{
	bool queue_empty = !queue->full && queue->head == queue->tail;
	if (queue_empty && os_task_wait(timeout_ticks, &queue->rec_blocked_list) == OS_TIMEOUT)
		return OS_TIMEOUT;
	
	memcpy(item, queue->storage + queue->tail, queue->item_sz);
	queue->tail = (queue->tail + queue->item_sz) % queue->size;
	queue->full = false;

	os_list_wake_high_pri(&queue->ins_blocked_list);
	return OS_SUCCESS;
}



/***************************************************************************************************
 * Event Group Functions
 **************************************************************************************************/
void os_event_create(struct os_event *event)
{
	event->flags = 0;
	event->blocked_list = NULL;
}

void os_event_set(struct os_event *event, uint8_t flags)
{
	event->flags |= flags;

	// Wake up ALL tasks waiting for flags to be set
	struct os_tcb *task = event->blocked_list;
	bool task_woken = false;
	while (task) {
		struct os_tcb *next = task->next_task;

		if (task->wait_flags == flags) {
			os_task_wake(task, &event->blocked_list);
			task_woken = true;
		}
		
		task = next;
	}

	if (task_woken)
		os_schedule();
}

// Waits for ALL flags in event to be set, always clears flags on exit
enum OS_STATUS os_event_wait(struct os_event *event, uint8_t flags, uint16_t timeout_ticks)
{
	if ((event->flags & flags) != flags) {
		running_task->wait_flags = flags;

		if (os_task_wait(timeout_ticks, &event->blocked_list) == OS_TIMEOUT)
			return OS_TIMEOUT;
	}
	
	// Clear flags before returning
	event->flags &= ~flags;
	return OS_SUCCESS;
}



/***************************************************************************************************
 * Port-Specific (Cortex-M3) Functions
 **************************************************************************************************/
void SysTick_Handler(void)
{
	os_tick();
}

void PendSV_Handler(void)
{
	// Store old context
	if (prev_task) {
		asm volatile(
			"mrs r0, psp\n"
			"mov r2, %0\n"
			"stmdb r0!, {r4-r11}\n"
			"str r0, [r2]\n"
			: : "r"(&prev_task->stack_pntr)
		);
	}

	// Load new context
	asm volatile(
		"mov r2, %0\n"
		"ldr r0, [r2]\n"
		"ldmia r0!, {r4-r11}\n"
		"msr psp, r0\n"
		: : "r"(&running_task->stack_pntr)
	);
}
