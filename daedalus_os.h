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

/* Other DEFINES */
#define PRIORITY_GROUP_WIDTH 16
#define PRIORITY_TABLE_SZ (MAX_PRIORITY_LEVEL / PRIORITY_GROUP_WIDTH + 1)

/* Typedefs */
typedef void (*os_task_entry)(void *);
typedef uint32_t os_task_stack;

/* Macros */
#define OS_ENTER_CRITICAL() asm("nop")
#define OS_EXIT_CRITICAL() asm("nop")
#define OS_SEC_TO_TICKS(sec) ((sec) * CLOCK_RATE_HZ)

/* Enums */
enum OS_TASK_STATE {
	TASK_BLOCKED,
	TASK_READY
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
	uint8_t id;
};

struct os_mutex {
	struct os_tcb *holding_task;
	uint16_t holding_task_orig_pri;
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
bool os_mutex_acquire(struct os_mutex *mutex, uint16_t timeout_ticks);
void os_mutex_release(struct os_mutex *mutex);

#endif
