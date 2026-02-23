# Binary Semaphore — Master-Slave Producer-Consumer Demo

Demonstrates a FreeRTOS binary semaphore used as a signaling mechanism between a producer (Master) and consumer (Slave) task. The Master generates randomized stationery orders and the Slave processes them one at a time, synchronized via a binary semaphore and a depth-1 queue.

---

## What Is a Binary Semaphore

A binary semaphore is a flag with only two states: **0 (taken)** and **1 (given)**.

```
xSemaphoreGive()  →  sets flag to 1  (signal: "data is ready")
xSemaphoreTake()  →  sets flag to 0  (acknowledge: "got it, processing")

If flag == 0  →  Take() BLOCKS until someone calls Give()
```

It is not used for mutual exclusion (that's a mutex). A binary semaphore is used for **event signaling** — one task tells another that something happened.

---

## How It Works

Two tasks with different priorities communicate through a binary semaphore (signal) and a queue (data):

```
+---------------------+                  +---------------------+
|    MASTER (Pri 3)    |                  |    SLAVE (Pri 1)     |
|    Producer          |                  |    Consumer          |
+----------+----------+                  +----------+----------+
           |                                        |
    Build random order                    xSemaphoreTake()
           |                               (BLOCKS until signaled)
    xQueueSend(order)                               |
           |                                        |
    xSemaphoreGive() -------- signal --------->  WAKES UP
           |                                        |
    vTaskDelay(1s)                          xQueueReceive(order)
           |                                        |
    (next order)                            Process items 1 by 1
                                                    |
                                            Print completion
                                                    |
                                            Loop → Take() again
```

**Why both a semaphore AND a queue?** They serve different purposes. The queue carries the actual data (`WorkOrder_t` struct). The semaphore is the wake-up signal that tells the Slave "there's something in the queue now — go read it." Without the semaphore, the Slave would need to poll the queue or block on `xQueueReceive()` directly (which would also work, but this demo explicitly shows how binary semaphores are used for signaling).

---

## The Stationery Order System

The Master generates a `WorkOrder_t` each second with a random item and quantity:

```c
typedef struct {
    uint16_t     usOrderId;     /* sequence number 1, 2, 3... */
    SupplyItem_e eItem;         /* random item type           */
    uint8_t      ucQuantity;    /* 1 to 15 units              */
} WorkOrder_t;
```

Available items (8 types):

```
Pen, Pencil, Eraser, Notebook, Marker, Stapler, Folder, Sticky Note
```

The Slave receives each order and hands out items one by one with a 50 ms delay between each, simulating real work.

### Example UART Output

```
===== Master-Slave Stationery Distribution Demo =====

[MASTER] Order #1  ->  Distribute 7 x Notebook
  [SLAVE] Handing out Notebook 1 of 7 ...
  [SLAVE] Handing out Notebook 2 of 7 ...
  [SLAVE] Handing out Notebook 3 of 7 ...
  [SLAVE] Handing out Notebook 4 of 7 ...
  [SLAVE] Handing out Notebook 5 of 7 ...
  [SLAVE] Handing out Notebook 6 of 7 ...
  [SLAVE] Handing out Notebook 7 of 7 ...
  [SLAVE] Order #1 COMPLETE  (7 x Notebook delivered)

[MASTER] Order #2  ->  Distribute 3 x Pen
  [SLAVE] Handing out Pen 1 of 3 ...
  [SLAVE] Handing out Pen 2 of 3 ...
  [SLAVE] Handing out Pen 3 of 3 ...
  [SLAVE] Order #2 COMPLETE  (3 x Pen delivered)
```

---

## Synchronization Flow (Step by Step)

```
1. Startup
   - Binary semaphore created in "taken" state (count = 0)
   - Master calls Give() once to prime it (count → 1)

2. Master iteration:
   - Builds order (random item + quantity)
   - xQueueSend() → pushes order into depth-1 queue
   - xSemaphoreGive() → sets semaphore to 1 (wake signal)
   - vTaskDelay(1s) → sleeps, yields CPU to Slave

3. Slave iteration:
   - xSemaphoreTake() → was blocking, now wakes (semaphore → 0)
   - xQueueReceive() → dequeues the order (non-blocking, data guaranteed)
   - Processes items one by one (50 ms per item)
   - Prints completion
   - Loops back to Take() → blocks again until next Give()
```

---

## Priority and Scheduling

```
+--------+------------+----------------------------------------------+
| Task   | Priority   | Behavior                                     |
+--------+------------+----------------------------------------------+
| Master | 3 (higher) | Runs first, enqueues order, signals, sleeps  |
| Slave  | 1 (lower)  | Runs only when Master is blocked/delayed     |
+--------+------------+----------------------------------------------+
```

The Master has higher priority, so it always runs first when both are ready. But after calling `vTaskDelay(1000)`, it yields the CPU entirely — allowing the Slave to process the order uninterrupted. This is a classic **produce-then-yield** pattern.

---

## Back-Pressure via Depth-1 Queue

The queue depth is intentionally set to 1:

```c
#define ORDER_QUEUE_DEPTH  1U
```

This creates natural **back-pressure**: if the Slave is still processing an order, the queue is full and `xQueueSend()` in the Master will block until the Slave calls `xQueueReceive()`. This guarantees orders are never lost and the system stays synchronized without additional logic.

---

## Binary vs Counting Semaphore

```
+---------------------+---------------------+------------------------+
| Property            | Binary Semaphore    | Counting Semaphore     |
+---------------------+---------------------+------------------------+
| Counter range       | 0 to 1              | 0 to N                 |
| Purpose             | Event signaling     | Resource pool counting |
| Give() when max     | No effect (stays 1) | Increments counter     |
| Typical use         | "Wake up, data is   | "N spots available,    |
|                     |  ready"             |  take one"             |
| This project        |       ✓             |                        |
+---------------------+---------------------+------------------------+
```

---

## Key FreeRTOS APIs Used

| API | Context | Purpose |
|---|---|---|
| `xSemaphoreCreateBinary()` | main | Create semaphore (initial state = taken/0) |
| `xSemaphoreGive()` | Task | Signal Slave that an order is ready |
| `xSemaphoreTake()` | Task | Block until Master signals |
| `xQueueCreate()` | main | Create depth-1 queue for `WorkOrder_t` |
| `xQueueSend()` | Task | Enqueue order (blocks if queue full) |
| `xQueueReceive()` | Task | Dequeue order (non-blocking, semaphore guarantees data) |
| `xTaskCreate()` | main | Create Master and Slave tasks |
| `vTaskDelay()` | Task | Master: 1 s between orders. Slave: 50 ms per item. |

---

## Configuration

```c
#define ORDER_QUEUE_DEPTH       1U     /* single-slot queue (back-pressure)  */
#define MASTER_TASK_PRIORITY    3U     /* producer runs first                */
#define SLAVE_TASK_PRIORITY     1U     /* consumer runs when master sleeps   */
#define MAX_ORDER_QUANTITY      15U    /* max units per order (random 1-15)  */
```

---

## Hardware Setup

| Component | Pin | Configuration |
|---|---|---|
| USART2 TX | PA2 | Alternate function, 115200 baud |
| USART2 RX | PA3 | Alternate function |

No LEDs or buttons used. All output is over UART — connect a serial terminal at **115200 baud, 8N1**.

---

## Project Structure

```
BINARY_SEMAPHORE_DEMONSTRATION/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       └── main.c              ← Master/Slave tasks, semaphore, queue
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
5. Watch the Master generate orders and the Slave process them in real time
6. Try changing `ORDER_QUEUE_DEPTH` to 5 to allow the Master to queue multiple orders ahead
