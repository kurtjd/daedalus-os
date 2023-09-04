/* Test app for daedalus-os */
#include <stdio.h>
#include <stdarg.h>
#include "daedalus_os.h"

#define TASK_STACK_SZ 0xF

static struct os_mutex stdout_mtx;
static struct os_queue lol_q;

void os_printf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	if (os_mutex_acquire(&stdout_mtx, OS_SEC_TO_TICKS(100))) {
		vprintf(format, args);
		os_mutex_release(&stdout_mtx);
	}
	va_end(args);
}

void *task1(void *data)
{
	while (1) {
		os_sim_thread_sched_check();
		int hax = 69;

		if (os_queue_insert(&lol_q, &hax, OS_SEC_TO_TICKS(1))) {
			os_printf("I inserted in Q OwO\n");
		} else {
			os_printf("Failed insert 8==D T^T\n");
			exit(0);
		}
		
	}

	return NULL;
}

void *task2(void *data)
{
	while (1) {
		os_sim_thread_sched_check();

		int hax;
		os_printf("ok\n");
		if (os_queue_retrieve(&lol_q, &hax, OS_SEC_TO_TICKS(1))) {
			os_printf("I got: %d\n", hax);
		} else {
			os_printf("FUK\n");
			exit(0);
		}
	}
}

// Figure out queues...
int main(void)
{
	// Tasks just share stack for now until implement context switching
	os_task_stack task_stack[TASK_STACK_SZ];
	os_mutex_create(&stdout_mtx);

	uint8_t q_buf[OS_QUEUE_SZ(5, sizeof(int))];
	os_queue_create(&lol_q, 5, q_buf, sizeof(int));

	os_init();
	os_task_create(task1, "Task A", task_stack, TASK_STACK_SZ, 2);
	os_task_create(task2, "Task B", task_stack, TASK_STACK_SZ, 1);
	os_start();

	pthread_exit(NULL);
}
