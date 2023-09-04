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

#ifdef USE_SIM
static pthread_mutex_t sim_sched_mtx = PTHREAD_MUTEX_INITIALIZER;
static bool sim_sched = false;
#endif

#ifdef USE_SIM
static void *os_idle_task_entry(void *data)
#else
static void os_idle_task_entry(void *data)
#endif
{
	(void)data;
	while (1) {
		os_sim_thread_sched_check();
		ticks_in_idle++;
		printf("Idling\n");
	}

	return NULL;
}

static void os_sw_context(struct os_tcb *prev_task, struct os_tcb *next_task)
{
	#ifdef USE_SIM
	if (next_task->sim_started) {
		os_sim_thread_wake(next_task);
	} else {
		next_task->sim_started = true;
		pthread_create(NULL, NULL, next_task->entry, next_task->arg);
	}
	os_sim_thread_sleep(prev_task);
	#else
	(void)next_task;
	#endif

	// ASM store context of current task

	// ASM restore context of next task
}

static uint8_t os_get_highest_ready_pri(void)
{
	for (int i = highest_priority; i >= 0; i--) {
		if (ready_list[i]) {
			return i;
		}
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
	if (running_task != ready_list[priority]) {
		return ready_list[priority];
	} else {
		return NULL;
	}
}

static void os_schedule(void)
{
	OS_ENTER_CRITICAL();

	uint8_t highest_ready_pri = os_get_highest_ready_pri();
	struct os_tcb *next_task = os_get_next_ready_task(highest_ready_pri);

	// Make sure there is a higher priority task that's ready
	if (next_task) {
		struct os_tcb *prev_task = running_task;
		running_task = next_task;
		os_sw_context(prev_task, next_task);
	}

	OS_EXIT_CRITICAL();
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
		if (task->priority > high_task->priority) {
			high_task = task;
			task = task->next_task;
		}
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

static bool os_task_wait(uint16_t timeout_ticks, struct os_tcb *blocked_list)
{
	running_task->waiting = true;

	os_list_remove_task(running_task, &ready_list[running_task->priority]);
	os_list_insert_task(running_task, &blocked_list);
	os_task_sleep(timeout_ticks);

	bool waiting = running_task->waiting;
	running_task->waiting = false;
	return waiting;
}

static void os_task_wake(struct os_tcb *task, struct os_tcb *list)
{
	OS_ENTER_CRITICAL();
	task->waiting = false;
	task->timeout = 0;
	os_list_remove_task(task, &list);
	os_task_set_state(task, TASK_READY);
	OS_EXIT_CRITICAL();
}

static void os_list_wake_high_pri(struct os_tcb *list)
{
	struct os_tcb *next_task = os_list_get_high_pri(list);
	if (next_task) {
		os_task_wake(next_task, list);
		os_schedule();
	}
}

#ifdef USE_SIM
static void *os_tick(void *data)
#else
static void os_tick(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
#endif
{
	#ifdef USE_SIM
	(void)data;
	#else
	(void)hwnd;
	(void)uMsg;
	(void)idEvent;
	(void)dwTime;
	#endif

	while (1) {
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

		pthread_mutex_lock(&sim_sched_mtx);
		sim_sched = true;
		pthread_mutex_unlock(&sim_sched_mtx);
		/* TODO:
		-Set tick priority higher
		-Find way to make prev task sleep rather than signal thru sim_sched
		*/

		#ifdef USE_SIM
		usleep((1000 / CLOCK_RATE_HZ) * 1000);
		#endif
	}

	return NULL;
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
	#ifdef USE_SIM
	pthread_create(NULL, NULL, os_tick, NULL);
	#else
	SetTimer(NULL, 0, 1000 / CLOCK_RATE_HZ, os_tick);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	#endif
}

uint8_t os_task_create(os_task_entry entry, void *arg, os_task_stack *stack_base, size_t stack_sz,
			uint8_t priority)
{
	assert(task_count < MAX_NUM_TASKS);

	struct os_tcb task = {
		#ifdef USE_SIM
		.sim_cond = PTHREAD_COND_INITIALIZER,
		.sim_started = false,
		#endif

		.entry = entry,
		.arg = arg,
		.stack_base = stack_base,
		.stack_sz = stack_sz,
		.priority = priority,
		.next_task = NULL,
		.prev_task = NULL,
		.timeout = 0,
		.waiting = false,
		.wait_flags = 0,
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

	// Other functions that set task waiting will have already removed it from ready list
	if (!running_task->waiting) {
		os_list_remove_task(running_task, &ready_list[running_task->priority]);
	}
	
	running_task->timeout = ticks;
	os_task_set_state(running_task, TASK_BLOCKED);
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
	if (mutex->holding_task) {
		// Priority inheritance
		// TODO: Mostly redundant as os_task_wait removes from ready list
		// We just don't want it removing from the wrong ready list
		if (mutex->holding_task->priority < running_task->priority) {
			os_list_remove_task(mutex->holding_task, &ready_list[mutex->holding_task->priority]);
			mutex->holding_task->priority = running_task->priority;
			os_list_insert_task(mutex->holding_task, &ready_list[mutex->holding_task->priority]);
		}

		if (os_task_wait(timeout_ticks, mutex->blocked_list))
			return false;
	}
	
	mutex->holding_task = running_task;
	mutex->holding_task_orig_pri = running_task->priority;
	return true;
}

void os_mutex_release(struct os_mutex *mutex)
{
	mutex->holding_task->priority = mutex->holding_task_orig_pri;
	mutex->holding_task = NULL;
	os_list_wake_high_pri(mutex->blocked_list);
}

void os_semph_create(struct os_semph *semph, uint8_t count)
{
	semph->count = count;
	semph->blocked_list = NULL;
}

bool os_semph_take(struct os_semph *semph, uint16_t timeout_ticks)
{
	if (semph->count <= 0 && os_task_wait(timeout_ticks, semph->blocked_list))
		return false;
	
	semph->count--;
	return true;
}

void os_semph_give(struct os_semph *semph)
{
	semph->count++;
	os_list_wake_high_pri(semph->blocked_list);
}

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

bool os_queue_insert(struct os_queue *queue, const void *item, uint16_t timeout_ticks)
{
	if (queue->full && os_task_wait(timeout_ticks, queue->ins_blocked_list))
		return false;
	
	memcpy(queue->storage + queue->head, item, queue->item_sz);
	queue->head = (queue->head + queue->item_sz) % queue->size;

	if (queue->head == queue->tail)
		queue->full = true;
	
	os_list_wake_high_pri(queue->rec_blocked_list);
	return true;
}

bool os_queue_retrieve(struct os_queue *queue, void *item, uint16_t timeout_ticks)
{
	bool queue_empty = !queue->full && queue->head == queue->tail;
	if (queue_empty && os_task_wait(timeout_ticks, queue->rec_blocked_list))
		return false;
	
	memcpy(item, queue->storage + queue->tail, queue->item_sz);
	queue->tail = (queue->tail + queue->item_sz) % queue->size;
	queue->full = false;

	os_list_wake_high_pri(queue->ins_blocked_list);
	return true;
}

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
			os_task_wake(task, event->blocked_list);
			task_woken = true;
		}
		
		task = next;
	}

	if (task_woken)
		os_schedule();
}

// Waits for ALL flags in event to be set, always clears flags on exit
bool os_event_wait(struct os_event *event, uint8_t flags, uint16_t timeout_ticks)
{
	if ((event->flags & flags) != flags) {
		running_task->wait_flags = flags;

		if (os_task_wait(timeout_ticks, event->blocked_list))
			return false;
	}
	
	// Clear flags before returning
	event->flags &= ~flags;
	return true;
}

#ifdef USE_SIM
void os_sim_thread_sleep(struct os_tcb *task)
{
	if (!task)
		return;
	
	pthread_mutex_lock(&sim_sched_mtx);
	while (running_task->id != task->id)
		pthread_cond_wait(&task->sim_cond, &sim_sched_mtx);
	pthread_mutex_unlock(&sim_sched_mtx);
}

void os_sim_thread_wake(struct os_tcb *task)
{
	pthread_mutex_lock(&sim_sched_mtx);
	pthread_cond_signal(&task->sim_cond);
	pthread_mutex_unlock(&sim_sched_mtx);
}

void os_sim_thread_sched_check(void)
{
	pthread_mutex_lock(&sim_sched_mtx);
	if (sim_sched) {
		sim_sched = false;
		pthread_mutex_unlock(&sim_sched_mtx);
		os_schedule();
	} else {
		pthread_mutex_unlock(&sim_sched_mtx);
	}
}
#endif
