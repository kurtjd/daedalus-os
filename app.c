/* Test app for daedalus-os */
#include <stdio.h>
#include "daedalus_os.h"

#define TASK_STACK_SZ 0xF

struct os_mutex mutex;

void print_task(void *str)
{
	printf("%s\n", (const char *)str);
}

void sleep_task(void *str)
{
	printf("%s\n", (const char *)str);
	os_task_sleep(OS_SEC_TO_TICKS(5));
}

void mutex_task(void *str)
{
	if (os_mutex_acquire(&mutex, OS_SEC_TO_TICKS(5))) {
		printf("%s got the mutex!\n", (const char *)str);
		os_task_sleep(OS_SEC_TO_TICKS(20));
		//os_mutex_release(&mutex); // cos no context switching yet...
	} else {
		printf("%s blocked getting mutex!\n", (const char *)str);
	}
}

int main(void)
{
	// Tasks just share stack for now until implement context switching
	os_task_stack task_stack[TASK_STACK_SZ];
	os_mutex_create(&mutex);

	os_init();
	os_task_create(print_task, "I just print", task_stack, TASK_STACK_SZ, 1);
	os_task_create(mutex_task, "Mutex A", task_stack, TASK_STACK_SZ, 2);
	os_task_create(mutex_task, "Mutex B", task_stack, TASK_STACK_SZ, 2);

	os_start();

	return 0;
}
