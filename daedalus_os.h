#ifndef DAEDALUS_OS_H
#define DAEDALUS_OS_H

#define USE_SIM

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef USE_SIM
#include <pthread.h>
#include <unistd.h>
#endif

/* Config (modified by user) */
#define MAX_NUM_TASKS 32
#define MAX_PRIORITY_LEVEL 31
#define CLOCK_RATE_HZ 100

/* Typedefs */
#ifdef USE_SIM
typedef void* (*os_task_entry)(void *);
#else
typedef void (*os_task_entry)(void *);
#endif

typedef uint32_t os_task_stack;

/* Macros */
#define OS_ENTER_CRITICAL() asm("nop")
#define OS_EXIT_CRITICAL() asm("nop")
#define OS_MSEC_TO_TICKS(msec) (((msec) * CLOCK_RATE_HZ) / 1000)
#define OS_SEC_TO_TICKS(sec) (OS_MSEC_TO_TICKS((sec) * 1000))
#define OS_QUEUE_SZ(length, item_sz) ((length) * (item_sz))

/* Enums */
enum OS_TASK_STATE {
	TASK_BLOCKED,
	TASK_READY
};

enum OS_STATUS {
	OS_SUCCESS,
	OS_TIMEOUT
};

/* Structs */
// Consider rearranging members for efficient struct packing
struct os_tcb {
	os_task_entry entry;
	void *arg;
	os_task_stack *stack_base;
	size_t stack_sz;
	uint8_t priority;
	enum OS_TASK_STATE state;
	struct os_tcb *next_task;
	struct os_tcb *prev_task;
	uint16_t timeout;
	bool waiting;
	uint8_t wait_flags;
	uint8_t id;

	#ifdef USE_SIM
	pthread_cond_t sim_cond;
	bool sim_started;
	#endif
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

/* Public Functions */
void os_init(void);
void os_start(void);

uint8_t os_task_create(os_task_entry entry, void *arg,
			os_task_stack *stack_base, size_t stack_sz,
			uint8_t priority);
void os_task_sleep(uint16_t ticks);
void os_task_yield(void);
const struct os_tcb *os_task_query(uint8_t task_id);

void os_mutex_create(struct os_mutex *mutex);
enum OS_STATUS os_mutex_acquire(struct os_mutex *mutex, uint16_t timeout_ticks);
void os_mutex_release(struct os_mutex *mutex);

void os_semph_create(struct os_semph *semph, uint8_t count);
enum OS_STATUS os_semph_take(struct os_semph *semph, uint16_t timeout_ticks);
void os_semph_give(struct os_semph *semph);

void os_queue_create(struct os_queue *queue, size_t length, uint8_t *storage, size_t item_sz);
enum OS_STATUS os_queue_insert(struct os_queue *queue, const void *item, uint16_t timeout_ticks);
enum OS_STATUS os_queue_retrieve(struct os_queue *queue, void *item, uint16_t timeout_ticks);

void os_event_create(struct os_event *event);
void os_event_set(struct os_event *event, uint8_t flags);
enum OS_STATUS os_event_wait(struct os_event *event, uint8_t flags, uint16_t timeout_ticks);

#ifdef USE_SIM
void os_sim_thread_sleep(struct os_tcb *task);
void os_sim_thread_wake(struct os_tcb *task);
void os_sim_thread_sched_check(void);
void os_sim_main_wait(void);
#endif

#endif
