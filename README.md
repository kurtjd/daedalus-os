# daedalus-os (In Development)
daedalus-os is a simple RTOS I'm developing mainly for learning purposes. The goal is to support the core features one would expect from an RTOS (preemptive scheduling, mutexes, semaphores, queues, etc) without introducing too much extra functionality or optimization tricks, in order for other beginners to more easily understand how an RTOS works. I am planning to target Cortex-M MCUs, but currently testing is being performed via OS threads (to simulate context switching on a desktop).

## Planned Features (X is yet to be implemented)
:heavy_check_mark: Preemptive scheduling  
:heavy_check_mark: Round-robin scheduling for tasks of same priority  
:heavy_check_mark: Mutexes  
:heavy_check_mark: Mutex priority inheritance  
:heavy_check_mark: Semaphores  
:heavy_check_mark: Queues  
:heavy_check_mark: Event groups  
:heavy_check_mark: Fully static memory allocation  
:heavy_check_mark: Desktop simulator (uses OS threads to simulate context switching)  
:x: Task messages  
:x: Low-power idle task  
:x: Context switching   
:x: ISR safe functions

## Build
Simply run `make`. You may need to modify the make file to explicitly link pthreads if building with the USE_SIM flag.

## Run
If built with the USE_SIM flag (which is on by default), this can be ran from your desktop machine. Simply run `daedalus`. You may experiment with app.c to test different OS functions.

## License
daedalus-os is licensed under the MIT license and is completely free to use and modify.
