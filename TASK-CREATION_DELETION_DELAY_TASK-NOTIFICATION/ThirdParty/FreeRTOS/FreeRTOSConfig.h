/*
 * ============================================================
 *  FreeRTOSConfig.h
 *  Board  : STM32F4 (Nucleo / Discovery / any STM32F4)
 *  IDE    : STM32CubeIDE / Keil / IAR
 *
 *  This is the SETTINGS FILE for FreeRTOS.
 *  Every FreeRTOS project must have exactly ONE of these files.
 *  It tells FreeRTOS how your hardware works and which
 *  features to turn ON or OFF.
 * ============================================================
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ============================================================
 *  Include standard integer types (uint8_t, uint32_t etc.)
 *  Only runs for C compilers — NOT for the assembler.
 *  IAR = __ICCARM__   GCC = __GNUC__   ARM Compiler = __CC_ARM
 * ============================================================ */
#if defined(__ICCARM__) || defined(__GNUC__) || defined(__CC_ARM)
/* Brings in uint8_t, uint16_t, uint32_t and similar standard types */
#include <stdint.h>
/* SystemCoreClock holds your CPU speed in Hz — set automatically by STM32 startup file */
extern uint32_t SystemCoreClock;
#endif

/* ============================================================
 *  SECTION 1 — SCHEDULER
 * ============================================================ */

/* 1 = PREEMPTIVE — FreeRTOS switches tasks automatically,
 0 = Cooperative — tasks must manually yield the CPU */
#define configUSE_PREEMPTION                    1

/* 1 = Tasks of EQUAL priority share CPU time — each gets 1 tick (1ms)
 0 = Task runs forever until it blocks or yields — others never run
 Always keep this 1 when you have multiple tasks at same priority */
#define configUSE_TIME_SLICING                  1

/* ============================================================
 *  SECTION 2 — CPU SPEED AND TICK RATE
 * ============================================================ */

/* Your CPU frequency in Hz — SystemCoreClock is filled automatically by STM32 startup code
 STM32F4 example = 168,000,000 Hz (168 MHz). Never hardcode a number here. */
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )

/* How many times per second the OS wakes up internally — its heartbeat
 1000 Hz = ticks every 1 millisecond, so vTaskDelay(500) = exactly 500 ms
 */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )

/* ============================================================
 *  SECTION 3 — TASK PRIORITIES
 * ============================================================ */

/* Total number of priority levels available — gives you levels 0, 1, 2, 3, 4
 Level 0 is reserved for the FreeRTOS Idle task, your tasks use levels 1 to 4
 Do not set this larger than needed — each extra level wastes a small amount of RAM */
#define configMAX_PRIORITIES                    ( 5 )

/* ============================================================
 *  SECTION 4 — MEMORY (STACK AND HEAP)
 * ============================================================ */

/* Minimum stack size for any task, measured in WORDS not bytes
 On STM32 (32-bit CPU): 1 word = 4 bytes, so 128 words = 512 bytes
 This is the bare minimum — give your own tasks 256 or 512 words each */
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )

/* Total RAM pool given to FreeRTOS for tasks, queues, semaphores and timers
 STM32F4 has 192 KB RAM total — starting with 50 KB for FreeRTOS is safe
 If you get a malloc failed error or crash at startup → increase this number */
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 50 * 1024 ) )

/* ============================================================
 *  SECTION 5 — TASK SETTINGS
 * ============================================================ */

/* Maximum characters allowed in a task name string including the null terminator
 Used only for debugging — visible in RTOS debugger and trace tools */
#define configMAX_TASK_NAME_LEN                 ( 16 )

/* 0 = 32-bit tick counter, handles delays up to ~49 days without wrapping — always use 0
 1 = 16-bit tick counter, wraps every 65 seconds  */
#define configUSE_16_BIT_TICKS                  0

/* 1 = Idle task yields the CPU immediately if another task at the same priority is ready
 Prevents Idle task from wasting CPU time — always keep this as 1 */
#define configIDLE_SHOULD_YIELD                 1

/* ============================================================
 *  SECTION 6 — FREERTOS FEATURES SWITCH ON OR OFF
 * ============================================================ */

/* Enable MUTEXES — protect shared hardware like UART or SPI from simultaneous access
 One task locks the mutex, uses the resource, then unlocks it for others
 Set to 1 (ON) — you will need this in almost every real project */
#define configUSE_MUTEXES                       1

/* Enable COUNTING SEMAPHORES — a counter that tasks can increment and decrement
 Useful for producer-consumer patterns where one task generates items another consumes
 Set to 1 (ON) */
#define configUSE_COUNTING_SEMAPHORES           1

/* Enable RECURSIVE MUTEXES — allows the same task to take the same mutex multiple times
 Required when a function that holds a mutex calls another function that also takes it
 Set to 1 (ON) */
#define configUSE_RECURSIVE_MUTEXES             1

/* Enable SOFTWARE TIMERS — automatically call a function after a set number of milliseconds
 Far lighter than creating a dedicated task just for timing purposes
 Set to 1 (ON) — very useful for periodic actions like sensor polling
 Set to 0 (ON) - not used in project*/
#define configUSE_TIMERS                        1

/* Priority of the hidden timer service task that executes all software timer callbacks
 configMAX_PRIORITIES minus 1 = highest available priority so timers always fire on time */
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )

/* Number of timer commands the queue can hold before it becomes full
 If you start many timers rapidly this buffer prevents losing commands
 10 slots is more than enough for beginner projects */
#define configTIMER_QUEUE_LENGTH                10

/* Stack size in WORDS for the timer service task
 MINIMAL_STACK_SIZE x 2 = 256 words = 1024 bytes, enough for simple callbacks
 Increase this if your timer callbacks use sprintf, floating point, or call many functions */
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/* Enable the trace facility so debugger tools can inspect task states and timing
 Required by FreeRTOS+Trace and Segger SystemView — almost no runtime overhead
 Set to 1 (ON) */
#define configUSE_TRACE_FACILITY                1

/* How many queues and semaphores can be registered with a name for the kernel debugger
 Affects only debug visibility in your IDE, not runtime behaviour — 8 is plenty */
#define configQUEUE_REGISTRY_SIZE               8

/* ============================================================
 *  SECTION 7 — SAFETY HOOKS — KEEP BOTH ON DURING DEVELOPMENT
 * ============================================================ */

/* 1 = Call vApplicationMallocFailedHook() whenever heap memory runs out
 You must write this function yourself — use it to print an error or blink an LED
 Saves hours of debugging unexplained crashes — always keep this ON */
#define configUSE_MALLOC_FAILED_HOOK            1

/* 2 = Most thorough stack overflow detection — checks a pattern at the bottom of each stack
 1 = Faster but less reliable method
 FreeRTOS calls vApplicationStackOverflowHook() when overflow is detected
 You must write this function — keep at 2 during all development and testing */
#define configCHECK_FOR_STACK_OVERFLOW          2

/* ============================================================
 *  SECTION 8 — HOOKS YOU DO NOT NEED YET — LEAVE ALL OFF
 * ============================================================ */

/* Idle hook — function called repeatedly when CPU has nothing else to do
 Used to put the CPU into low-power sleep mode
 Leave OFF (0) until you specifically need power saving */
#define configUSE_IDLE_HOOK                     0

/* Tick hook — function called on every single OS tick interrupt (every 1 ms)
 Very expensive — leave OFF (0) for now */
#define configUSE_TICK_HOOK                     0

/* Run-time statistics — measures how much CPU time each task consumes
 Requires a separate high-speed hardware timer to work correctly
 leave OFF (0) until you are comfortable with FreeRTOS */
#define configGENERATE_RUN_TIME_STATS           0

/* Application task tags — attach a custom integer value to any task for your own use
 Rarely needed in normal projects — leave OFF (0) */
#define configUSE_APPLICATION_TASK_TAG          0

/* Co-routines — an old lightweight alternative to full tasks, now obsolete
 Not used in modern FreeRTOS projects — leave OFF (0) and forget it exists */
#define configUSE_CO_ROUTINES                   0

/* Co-routine priority levels — only relevant when co-routines are enabled above
 Since co-routines are OFF this line has no effect — kept for completeness */
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

/* ============================================================
 *  SECTION 9 — API FUNCTIONS TO COMPILE INTO YOUR PROJECT
 *  1 = include this function (you can call it in your code)
 *  0 = exclude it to save a small amount of flash memory
 *  As a beginner set everything to 1 and optimise later
 * ============================================================ */

/* vTaskDelay(ticks) — put a task to sleep for N ticks — you will use this in every task */
#define INCLUDE_vTaskDelay                      1

/* vTaskDelayUntil() — precise periodic delay that compensates for execution time drift
 Better than vTaskDelay() when a task must run at exactly fixed time intervals */
#define INCLUDE_vTaskDelayUntil                 1

/* vTaskDelete(handle) — permanently remove a task and free its memory */
#define INCLUDE_vTaskDelete                     1

/* vTaskSuspend() and vTaskResume() — pause a task then wake it up again later */
#define INCLUDE_vTaskSuspend                    1

/* vTaskPrioritySet() — change a task's priority number while the system is running */
#define INCLUDE_vTaskPrioritySet                1

/* uxTaskPriorityGet() — read the current priority number of any task */
#define INCLUDE_uxTaskPriorityGet               1

/* xTaskGetCurrentTaskHandle() — returns the handle of whichever task is running right now */
#define INCLUDE_xTaskGetCurrentTaskHandle       1

/* xTaskGetIdleTaskHandle() — returns the handle of the built-in FreeRTOS Idle task */
#define INCLUDE_xTaskGetIdleTaskHandle          1

/* uxTaskGetStackHighWaterMark() — returns the minimum free stack words ever recorded
 Call this during development to check if you gave a task enough stack space
 If the returned number is very small then increase that task's stack size */
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* xTaskGetSchedulerState() — returns whether the scheduler is running, suspended, or not started */
#define INCLUDE_xTaskGetSchedulerState          1

/* xTimerPendFunctionCall() — safely call a function from inside an ISR using the timer task */
#define INCLUDE_xTimerPendFunctionCall          1

/* vTaskCleanUpResources() — internal FreeRTOS cleanup, include for completeness */
#define INCLUDE_vTaskCleanUpResources           1

/* pxTaskGetStackStart() — returns a pointer to the start of a task's stack memory
 Used by debugger tools and stack inspection utilities */
#define INCLUDE_pxTaskGetStackStart             1

/* ============================================================
 *  SECTION 10 — CORTEX-M INTERRUPT PRIORITIES (STM32 SPECIFIC)
 *
 *  WARNING — Wrong values here cause silent crashes and hard faults
 *
 *  STM32F4 uses 4 priority bits = 16 levels numbered 0 to 15
 *  Lower number = more urgent in hardware (0 is the most urgent of all)
 *
 *  Simple rule for your ISR functions:
 *    NVIC priority 5 to 15  →  SAFE to call FreeRTOS API (xQueueSendFromISR etc.)
 *    NVIC priority 0 to 4   →  NEVER call any FreeRTOS function — hard fault will result
 * ============================================================ */

/* Number of interrupt priority bits in the hardware — 4 bits on STM32F4 = 16 levels
 __NVIC_PRIO_BITS is defined automatically by the CMSIS device header if available */
#ifdef __NVIC_PRIO_BITS
    /* Use the value from the CMSIS STM32 device header file */
    #define configPRIO_BITS                     __NVIC_PRIO_BITS
#else
/* STM32F4 has 4 priority bits giving 16 levels from 0 (most urgent) to 15 (least urgent) */
#define configPRIO_BITS                     4
#endif

/* The least urgent (lowest urgency) interrupt priority level in the system
 FreeRTOS kernel interrupts SysTick and PendSV always run at this level
 They must be least urgent so they never block your time-critical ISRs */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15

/* The most urgent level from which you are still allowed to call FreeRTOS API safely
 Any ISR with priority 5, 6, 7 ... 15 may safely call xQueueSendFromISR() etc.
 Any ISR with priority 0, 1, 2, 3, 4 must NEVER call any FreeRTOS function */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

/* Interrupt priorities used by the kernel port layer itself.  These are generic
 to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/* FreeRTOS uses this internally to mask interrupts during critical sections
 NEVER set this to 0 — it would mask all interrupts and the system would freeze */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/* ============================================================
 *  SECTION 11 — ASSERT (DEVELOPMENT ERROR TRAP)
 * ============================================================ */

/* If condition x is false, disable all interrupts and spin in an infinite loop
 FreeRTOS calls this when it detects an internal error such as a NULL handle or bad parameter
 Your debugger will stop here and show you the exact file and line number of the problem
 Always keep this during development — only remove in the final production build */
#define configASSERT( x )   if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* ============================================================
 *  SECTION 12 — MAP FREERTOS INTERRUPT HANDLERS TO STM32 NAMES
 *
 *  STM32 requires specific function names in its interrupt vector table.
 *  FreeRTOS uses different internal names for the same interrupts.
 *  These three defines make the names match so the linker connects them.
 *
 *  IMPORTANT: If STM32CubeMX generated SysTick_Handler inside stm32f4xx_it.c
 *  you MUST delete that generated function — it will clash with this mapping
 *  and cause a duplicate symbol linker error.
 * ============================================================ */

/* SVC (SuperVisor Call) interrupt — FreeRTOS uses this to launch the very first task */
#define vPortSVCHandler     SVC_Handler

/* PendSV interrupt — FreeRTOS uses this to perform every context switch between tasks */
#define xPortPendSVHandler  PendSV_Handler

/* SysTick interrupt — fires every 1 ms or whatever you set configTICK_RATE_HZ to and drives the FreeRTOS internal tick counter */
#define xPortSysTickHandler SysTick_Handler

/*  include fot SEGGER SystemView FreeRTOS patch header for real-time task tracing */
//#include "SEGGER_SYSVIEW_FreeRTOS.h"
#endif /* FREERTOS_CONFIG_H */
