# Mutex — Protecting Shared UART from Concurrent Access

Demonstrates the problem of shared resource corruption when two tasks with different priorities access the UART simultaneously, and how a FreeRTOS mutex solves it. Toggle a single `#define` to see garbled vs clean output — the most convincing way to understand why mutexes exist.

---

## The Problem

Two tasks print long strings to UART character by character. Task2 (high priority) sleeps for a random duration, then wakes up and **preempts** Task1 (low priority) mid-print. The result: interleaved characters from both strings on the terminal.

```
WITHOUT MUTEX — garbled output:

Task1 ::::: Hello from low-priTask2 ----- Hello from high-priority task, this
string can interrupt Task1 anytime if USE_MUTEX not defined
ority task, this is a task1's strTask2 ----- Hello from high-pri...
```

Task2 woke up while Task1 was in the middle of printing. Since Task2 has higher priority, the scheduler immediately switches to it. Task2 starts transmitting its own characters over the same UART — corrupting Task1's output.

## The Fix

Wrap the print operation inside a mutex lock. Even if Task2 wakes up with higher priority, it **blocks on the mutex** until Task1 finishes printing and releases it.

```
WITH MUTEX — clean output:

Task1 ::::: Hello from low-priority task, this is a task1's string to show the problem
Task2 ----- Hello from high-priority task, this string can interrupt Task1 anytime if USE_MUTEX not defined
Task1 ::::: Hello from low-priority task, this is a task1's string to show the problem
Task2 ----- Hello from high-priority task, this string can interrupt Task1 anytime if USE_MUTEX not defined
```

Every line prints completely before the other task gets access.

---

## What Is a Mutex

Mutex = **Mut**ual **Ex**clusion. Only one task can hold it at a time.

```
xSemaphoreTake(mutex)  →  Lock.   "I'm using the UART, everyone else wait."
xSemaphoreGive(mutex)  →  Unlock. "I'm done, next task can have it."

If already locked  →  Take() BLOCKS until the holder calls Give().
```

---

## How to Test

Toggle the `#define` in `main.c` to switch between garbled and clean output:

```c
/* Comment out  = garbled output (no protection)  */
/* Uncomment    = clean output   (mutex protection) */
#define USE_MUTEX
```

Rebuild, flash, and compare the UART output. The difference is immediately visible.

---

## How It Works

```
Task2 (Priority 2 — HIGH)              Task1 (Priority 1 — LOW)
  |                                       |
  Take(mutex) ← acquires lock            |  (blocked, lower priority)
  |                                       |
  Print string char by char               |
  |                                       |
  Give(mutex) ← releases lock            |
  |                                       |
  vTaskDelay(random 0-500ms)              Take(mutex) ← acquires lock
  |  (sleeping, yields CPU)               |
  |                                       Print string char by char
  |                                       |
  WAKES UP (higher priority)              |  ← Task2 wants to run but...
  |                                       |
  Take(mutex) ← BLOCKS!                  |  ...Task1 holds the mutex
  |  (waits, even though higher pri)      |
  |                                       Give(mutex) ← releases lock
  |                                       |
  Take(mutex) ← NOW acquires             |  (lower priority, preempted)
  |                                       |
  Print string char by char               |
  ...                                     ...
```

The key insight: **the mutex overrides normal priority rules**. Task2 has higher priority but still waits because Task1 holds the lock. This is what makes shared resource access safe.

---

```
+------------------------+---------------------+----------------------+
| Property               | Mutex               | Binary Semaphore     |
+------------------------+---------------------+----------------------+
| Purpose                | Protect shared       | Signal between       |
|                        | resources            | tasks                |
+------------------------+---------------------+----------------------+
| Ownership              | YES - only the task  | NO - any task can    |
|                        | that Take()s can     | Give()               |
|                        | Give()               |                      |
+------------------------+---------------------+----------------------+
| Priority Inheritance   | YES - prevents       | NO                   |
|                        | priority inversion   |                      |
+------------------------+---------------------+----------------------+
| Typical use            | UART, SPI, I2C,      | ISR-to-task signal,  |
|                        | shared memory,       | event notification   |
|                        | global variables     |                      |
+------------------------+---------------------+----------------------+
| This project           |        ✓             |                      |
+------------------------+---------------------+----------------------+
```

### Priority Inheritance (Mutex Exclusive Feature)

Without priority inheritance, a dangerous scenario called **priority inversion** can occur:

```
Without priority inheritance:
  Task-High (pri 3) needs mutex
  Task-Low  (pri 1) holds mutex
  Task-Mid  (pri 2) is running, preempts Task-Low
  → Task-High is STUCK waiting because Task-Mid keeps preempting Task-Low
  → Task-Low can never finish and release the mutex

With priority inheritance (FreeRTOS mutex):
  Task-High (pri 3) needs mutex
  Task-Low  (pri 1) holds mutex
  → Kernel temporarily BOOSTS Task-Low to priority 3
  → Task-Mid (pri 2) cannot preempt Task-Low anymore
  → Task-Low finishes quickly, releases mutex
  → Task-High gets the mutex immediately
  → Task-Low drops back to priority 1
```

FreeRTOS mutexes have priority inheritance built in. Binary semaphores do not.

---

## Why Character-by-Character Printing?

The strings are printed one character at a time deliberately:

```c
pcChar = pcTask1String;
while (*pcChar != '\0')
{
    HAL_UART_Transmit(&huart2, (uint8_t *)pcChar, 1, HAL_MAX_DELAY);
    pcChar++;
}
```

This maximizes the window of opportunity for preemption. If the entire string were sent in one `HAL_UART_Transmit()` call, the corruption would still happen but would be harder to observe because the HAL internally disables interrupts during DMA transfers on some configurations. Character-by-character makes the race condition obvious and reproducible.

---

## Why Random Delay?

```c
vTaskDelay(pdMS_TO_TICKS(rand() % 500));
```

Task2 sleeps for 0–500 ms randomly. This simulates real-world unpredictability where you never know exactly when a high-priority task will need the shared resource. Sometimes Task1 finishes before Task2 wakes (looks fine by luck), sometimes it doesn't (garbled). The randomness proves that **"it works most of the time" is not safe enough** — you need a mutex for guaranteed correctness.

---

## Key FreeRTOS APIs Used

| API | Context | Purpose |
|---|---|---|
| `xSemaphoreCreateMutex()` | main | Create mutex (initial state = available) |
| `xSemaphoreTake()` | Task | Lock — acquire exclusive UART access |
| `xSemaphoreGive()` | Task | Unlock — release UART for other tasks |
| `xTaskCreate()` | main | Create Task1 (pri 1) and Task2 (pri 2) |
| `vTaskDelay()` | Task | Task2: random sleep. Task1: 100 ms between prints. |

---

## Hardware Setup

| Component | Pin | Configuration |
|---|---|---|
| USART2 TX | PA2 | Alternate function, 115200 baud |
| USART2 RX | PA3 | Alternate function |
 All output is over UART — connect a serial terminal at **115200 baud, 8N1**.

---

## Project Structure

```
MUTEX_DEMONSTRATION/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       └── main.c              ← Task1, Task2, mutex toggle via #define
├── ThirdParty/
│   └── FreeRTOS/               ← Kernel source (manual integration)
└── Drivers/                    ← HAL & CMSIS (auto-generated)
```

---

## Build & Run

1. Open the project in STM32CubeIDE
2. Build (`Ctrl+B`)
3. Flash to Discovery board
4. Open a serial terminal at **115200 baud, 8N1**
5. Observe clean output with `#define USE_MUTEX` enabled
6. Comment out `#define USE_MUTEX`, rebuild, reflash — observe garbled output
7. Compare both outputs to understand exactly what the mutex protects
