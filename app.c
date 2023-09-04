/* Test app for daedalus-os */
#include <stdio.h>
#include <stdarg.h>
#include "daedalus_os.h"

#define TASK_STACK_SZ 0xF

static struct os_mutex stdout_mtx;
static struct os_event events;

void os_printf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	if (os_mutex_acquire(&stdout_mtx, OS_SEC_TO_TICKS(100)) == OS_SUCCESS) {
		vprintf(format, args);
		os_mutex_release(&stdout_mtx);
	}
	va_end(args);
}

void *task1(void *data)
{
	while (1) {
		os_sim_thread_sched_check();

		os_printf("%s sleeping...\n", (char *)data);
		os_task_sleep(OS_SEC_TO_TICKS(3));
		os_printf("Setting event...\n");
		os_event_set(&events, 5);
	}

	return NULL;
}

void *task2(void *data)
{
	while (1) {
		os_sim_thread_sched_check();

		os_printf("%s waiting for event...\n", (char *)data);
		if (os_event_wait(&events, 5, OS_SEC_TO_TICKS(5)) == OS_SUCCESS)
			os_printf("%s recognized event!\n", (char *)data);
		else
			os_printf("%s: Event not signalled.\n", (char *)data);
	}
}

int main(void)
{
	// Tasks just share stack for now until implement context switching
	os_task_stack task_stack[TASK_STACK_SZ];
	os_mutex_create(&stdout_mtx);
	os_event_create(&events);

	os_init();
	os_task_create(task1, "Task A", task_stack, TASK_STACK_SZ, 2);
	os_task_create(task2, "Task B", task_stack, TASK_STACK_SZ, 1);
	os_task_create(task2, "Task C", task_stack, TASK_STACK_SZ, 1);
	os_task_create(task2, "Task D", task_stack, TASK_STACK_SZ, 1);
	os_start();

	os_sim_main_wait();
}
