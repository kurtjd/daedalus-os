/* Test app for daedalus-os */
#include <stdio.h>
#include "daedalus_os.h"

#define TASK_STACK_SZ 0xF

struct os_mutex mutex;
struct os_semph semph;
struct os_queue queue;
struct os_event event;

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

void semph_task(void *str)
{
	if (os_semph_take(&semph, OS_SEC_TO_TICKS(5))) {
		printf("%s got the semaphore!\n", (const char *)str);
		os_task_sleep(OS_SEC_TO_TICKS(20));
		//os_semph_give(&mutex); // cos no context switching yet...
	} else {
		printf("%s blocked getting semaphore!\n", (const char *)str);
	}
}

void event_set_task(void *str)
{
	os_event_set(&event, 5);
	printf("Flags set!\n");
	os_task_sleep(OS_SEC_TO_TICKS(20));
}

void event_wait_task(void *str)
{
	if (os_event_wait(&event, 5, OS_SEC_TO_TICKS(10)))
		printf("Flags received!\n");
	else
		printf("Waiting on flags...\n");
}

int main(void)
{
	// Tasks just share stack for now until implement context switching
	os_task_stack task_stack[TASK_STACK_SZ];

	os_mutex_create(&mutex);
	os_semph_create(&semph, 3);
	os_event_create(&event);

	uint8_t queue_buf[OS_QUEUE_SZ(5, sizeof(int))];
	os_queue_create(&queue, 5, queue_buf, sizeof(int));

	os_init();
	os_task_create(print_task, "I just print", task_stack,
			TASK_STACK_SZ, 2);
	os_task_create(event_set_task, NULL, task_stack, TASK_STACK_SZ, 2);
	//os_task_create(event_wait_task, NULL, task_stack, TASK_STACK_SZ, 2);

	os_start();

	return 0;
}
