# daedalus-os
daedalus-os is a simple RTOS I developed mainly for learning purposes.

The goal is to support the core features one would expect from an RTOS (preemptive scheduling, mutexes, semaphores, queues, etc)
without introducing too much extra functionality or optimization tricks, in order for other beginners to more easily understand how an RTOS works.

daedalus-os currently only targets ARM Cortex-M MCUs but perhaps in the future I'll work on supporting more.

## Status
While most of the functionality is all there, and a decent amount of testing has been done, much more thorough testing needs to be completed before this
can be considered a reliable RTOS. In addition, the kernel itself is likely vulnerable to race conditions as I have to figure out how I want to handle
marking critical sections inside the kernel. I fear my overall design might need to be rethought because if I just disable all interrupts while inside
a kernel function, then the PendSV interrupt won't trigger (which is vital for scheduling/context switches). I'll have to think about this some more.

## Features (X is yet to be implemented)
:heavy_check_mark: Preemptive scheduling  
:heavy_check_mark: Round-robin scheduling for tasks of same priority  
:heavy_check_mark: Mutexes  
:heavy_check_mark: Mutex priority inheritance  
:heavy_check_mark: Semaphores  
:heavy_check_mark: Queues  
:heavy_check_mark: Event groups  
:heavy_check_mark: Fully static memory allocation  
:heavy_check_mark: Context switching   
:heavy_check_mark: ISR safe functions  
:x: Task messages  
:x: Low-power idle task  

## Build
daedalus-os is meant to be built alongside a larger project. Simply include `daedalus_os.c` in your project's source folder and `daedalus_os.h` in your project's include folder.

## Run
Here is an example of how you could incorporate daedalus-os into your project. Please see `daedalus_os.h` for the complete API.

```c
#include "uart.h"
#include "led.h"
#include "daedalus_os.h"

#define TASK_STACK_SZ 0xFF

static struct os_mutex uart_mtx;
void safe_uart(const char *str)
{
    if (os_mutex_acquire(&uart_mtx, OS_MSEC_TO_TICKS(100)) == OS_SUCCESS) {
        uart_write_str(str);
        os_mutex_release(&uart_mtx);
    }
}

void print_task(void *data)
{
    const char *msg = data;
    while (1) {
        safe_uart(msg);
        os_task_sleep(OS_SEC_TO_TICKS(1));
    }
}

void blink_task(void *data)
{
    while (1) {
        led_toggle();
        os_task_sleep(OS_MSEC_TO_TICKS(100));
    }
}

int main(void) {
    uart_init(9600);
    led_enable();

    os_task_stack pt1_stk[TASK_STACK_SZ];
    os_task_stack pt2_stk[TASK_STACK_SZ];
    os_task_stack pt3_stk[TASK_STACK_SZ];
    os_task_stack bt_stk[TASK_STACK_SZ];

    os_init();

    os_mutex_create(&uart_mtx);
    os_task_create(print_task, "I am Task A!\n", pt1_stk, TASK_STACK_SZ, 1);
    os_task_create(print_task, "I am Task B!\n", pt2_stk, TASK_STACK_SZ, 1);
    os_task_create(print_task, "I am Task C!\n", pt3_stk, TASK_STACK_SZ, 1);
    os_task_create(blink_task, NULL, bt_stk, TASK_STACK_SZ, 1);

    os_start();
}
```

## License
daedalus-os is licensed under the MIT license and is completely free to use and modify.
