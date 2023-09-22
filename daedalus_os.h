#ifndef DAEDALUS_OS_H
#define DAEDALUS_OS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/***************************************************************************************************
 * Config (Modified By User)
 **************************************************************************************************/
#define MAX_NUM_TASKS 32
#define MAX_PRIORITY_LEVEL 31
#define OS_CLK_HZ 100
#define CPU_CLK_HZ 72000000UL
/***************************************************************************************************
 * End Config (Do NOT modify below this)
 **************************************************************************************************/



/***************************************************************************************************
 * Public Typedefs
 **************************************************************************************************/
typedef void (*os_task_entry)(void *);
typedef uint32_t os_task_stack;



/***************************************************************************************************
 * Public Macros
 **************************************************************************************************/
#define OS_ENTER_CRITICAL() asm("CPSID I")
#define OS_EXIT_CRITICAL() asm("CPSIE I")
#define OS_MSEC_TO_TICKS(msec) (((msec) * OS_CLK_HZ) / 1000)
#define OS_SEC_TO_TICKS(sec) (OS_MSEC_TO_TICKS((sec) * 1000))
#define OS_QUEUE_SZ(length, item_sz) ((length) * (item_sz))



/***************************************************************************************************
 * Public Enums
 **************************************************************************************************/
enum OS_TASK_STATE {
	TASK_BLOCKED,
	TASK_READY
};

enum OS_STATUS {
	OS_SUCCESS,
	OS_FAILED,
	OS_TIMEOUT
};



/***************************************************************************************************
 * Public Structures
 **************************************************************************************************/
// TODO: Consider rearranging members for efficient struct packing
struct os_tcb {
	os_task_entry entry;
	void *arg;
	os_task_stack *stack_pntr;
	uint8_t priority;
	enum OS_TASK_STATE state;
	struct os_tcb *next_task;
	struct os_tcb *prev_task;
	uint16_t timeout;
	bool waiting;
	uint8_t wait_flags;
	uint8_t id;
};

struct os_mutex {
	struct os_tcb *holding_task;
	uint8_t holding_task_orig_pri;
	struct os_tcb *blocked_list;
};

struct os_semph {
	uint8_t count;
	struct os_tcb *blocked_list;
};

struct os_queue {
	size_t size;
	size_t head;
	size_t tail;
	size_t item_sz;
	uint8_t *storage;
	bool full;
	struct os_tcb *rec_blocked_list;
	struct os_tcb *ins_blocked_list;
};

struct os_event {
	uint8_t flags;
	struct os_tcb *blocked_list;
};



/***************************************************************************************************
 * Public Functions
 **************************************************************************************************/
/*
Initializes the OS. Must be called first.
*/
void os_init(void);

/*
Starts the OS. Should be called after all tasks have been created.
*/
void os_start(void);

/*
Creates a new task. Returns the ID of the new task.

entry - function pointer to task's entry function
arg - pointer to data to be passed to task's entry function
stack_base - pointer to the base (lowest address) of task's stack
stack_sz - the size of the stack in number of 32-bit words
priority - the priority of the task
*/
uint8_t os_task_create(os_task_entry entry, void *arg,
			os_task_stack *stack_base, size_t stack_sz,
			uint8_t priority);

/*
Tells the current task to sleep for the specified number of OS clock ticks.
*/
void os_task_sleep(uint16_t ticks);

/*
Causes the scheduler to immediately run.
*/
void os_task_yield(void);

/*
Returns the task control block of the task with the given id. Mainly only useful for debugging.
*/
const struct os_tcb *os_task_query(uint8_t task_id);

/*
Creates and initializes the given mutex.
*/
void os_mutex_create(struct os_mutex *mutex);

/*
The running task will attempt to acquire the given mutex, sleeping the specified number of ticks
if it is not available.

Returns OS_SUCCESS if successful, OS_TIMEOUT otherwise.
*/
enum OS_STATUS os_mutex_acquire(struct os_mutex *mutex, uint16_t timeout_ticks);

/*
The running task will relinquish the given mutex.
Causes the scheduler to run if any tasks were waiting on the mutex.
*/
void os_mutex_release(struct os_mutex *mutex);

/*
Creates and initializes the given semaphore with the given count.
*/
void os_semph_create(struct os_semph *semph, uint8_t count);

/*
The running task will attempt to decrement the semaphore, sleeping the specified number of ticks
if the count is currently at 0.

Returns OS_SUCCESS if successful, OS_TIMEOUT otherwise.
*/
enum OS_STATUS os_semph_take(struct os_semph *semph, uint16_t timeout_ticks);

/*
The running task will increment the given semaphore.
*/
void os_semph_give(struct os_semph *semph);

/*
An ISR-safe version of os_semph_take.

Returns OS_SUCCESS if successful, OS_FAILED otherwise (does not wait).
*/
enum OS_STATUS os_semph_take_isr(struct os_semph *semph);

/*
An ISR-safe version of os_semph_give.
*/
void os_semph_give_isr(struct os_semph *semph);

/*
Creates and initializes the given queue.
The SIZE of the queue storage buffer must be a product of its length and item size.

queue - the queue to be created
length - the max number of elements the queue can hold
storage - the storage buffer for the queue
item_sz - the size of an individual item in the queue in bytes 
*/
void os_queue_create(struct os_queue *queue, size_t length, uint8_t *storage, size_t item_sz);

/*
Copies the given item into the queue, sleeping the specified number of ticks if the queue
is currently full.

Returns OS_SUCESS if successful, OS_TIMEOUT otherwise.
*/
enum OS_STATUS os_queue_insert(struct os_queue *queue, const void *item, uint16_t timeout_ticks);

/*
Removes and copies an element from the given queue into the given item, sleeping the specified
number of ticks if the queue is currently empty.

Returns OS_SUCCESS if successful, OS_TIMEOUT otherwise.
*/
enum OS_STATUS os_queue_retrieve(struct os_queue *queue, void *item, uint16_t timeout_ticks);

/*
An ISR-safe version of os_queue_insert.

Returns OS_SUCCESS if successful, OS_FAILED otherwise (does not wait).
*/
enum OS_STATUS os_queue_insert_isr(struct os_queue *queue, const void *item);

/*
An ISR-safe version of os_queue_retrieve.

Returns OS_SUCCESS if successful, OS_FAILED otherwise (does not wait).
*/
enum OS_STATUS os_queue_retrieve_isr(struct os_queue *queue, void *item);

/*
Create and intialize the given event group.
*/
void os_event_create(struct os_event *event);

/*
Sets the given bits (or flags) of the given event group.
*/
void os_event_set(struct os_event *event, uint8_t flags);

/*
The running task will sleep the specified number of ticks or until the specified flags in the given
event group are set,

Returns OS_SUCCESS if successful, OS_TIMEOUT otherwise.
*/
enum OS_STATUS os_event_wait(struct os_event *event, uint8_t flags, uint16_t timeout_ticks);

/*
An ISR-safe version of os_event_set.
*/
void os_event_set_isr(struct os_event *event, uint8_t flags);

#endif
