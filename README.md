# daedalus-os (In Development)
daedalus-os is a simple RTOS I'm developing mainly for learning purposes. The goal is to support the core features one would expect from an RTOS (preemptive scheduling, mutexes, semaphores, queues, etc) without introducing too much extra functionality or optimization tricks, in order for other beginners to more easily understand how an RTOS works. daedalus-os currently only targets ARM Cortex-M MCUs but perhaps in the future I'll work on supporting more.

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
:heavy_check_mark: Context switching   
:heavy_check_mark: ISR safe functions  
:x: Task messages  
:x: Low-power idle task  

## Build
daedalus-os is meant to be built alongside a larger project. Simply include daedalus_os.c in your project's source folder and daedalus_os.h in your project's include folder.

## Run
Please see app.c as an example of how to use daedalus-os in your project.

## License
daedalus-os is licensed under the MIT license and is completely free to use and modify.
