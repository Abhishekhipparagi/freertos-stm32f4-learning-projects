# Counting Semaphore — Parking Lot Simulation

Demonstrates FreeRTOS counting semaphores using a parking lot analogy: 3 parking spots shared between 5 cars. When all spots are taken, remaining cars block until a spot frees up.

---

## What Is a Counting Semaphore

A counting semaphore is a counter that ranges from 0 to a maximum value N.

```
xSemaphoreTake()  →  counter goes DOWN  (acquire a resource)
xSemaphoreGive()  →  counter goes UP    (release a resource)

When counter == 0   →  next Take() BLOCKS until someone calls Give()
```

Unlike a binary semaphore (0 or 1), a counting semaphore can go up to N, making it ideal for managing a **pool of identical resources** — in this case, parking spots.

---

## How It Works

At startup, the semaphore is created with a max count of 3 (all spots free). Five car tasks are created, each running in an infinite loop:

```
  +-------+     +-------+     +-------+     +-------+     +-------+
  | Honda |     |Toyota |     |  BMW  |     | Tesla |     |Suzuki |
  +---+---+     +---+---+     +---+---+     +---+---+     +---+---+
      |             |             |             |             |
      +-------------+-------------+-------------+-------------+
                              |
                    xSemaphoreTake()
                    (blocks if count == 0)
                              |
                    +---------+---------+
                    | Parking Lot       |
                    | Counting Sem = 3  |
                    | [ ]  [ ]  [ ]     |
                    +-------------------+
```

Each car's lifecycle:

```
Look for spot
  |
  +-- xSemaphoreTake(portMAX_DELAY)  →  blocks until a spot is free
  |
  PARKED (count decremented)
  |
  +-- Stay parked for 1-2 seconds
  |
  xSemaphoreGive()  →  spot released (count incremented)
  |
  LEFT
  |
  +-- Drive around for 2-3 seconds
  |
  Loop back to "Look for spot"
```

### Example UART Output

```
=== Parking: 3 spots, 5 cars ===

[Honda]  Looking for spot...
[Honda]  PARKED! (free: 2/3)
[Toyota] Looking for spot...
[Toyota] PARKED! (free: 1/3)
[BMW]    Looking for spot...
[BMW]    PARKED! (free: 0/3)
[Tesla]  Looking for spot...          ← blocks here, no spots available
[Suzuki] Looking for spot...          ← also blocks
[Honda]  LEFT (free: 1/3)

[Tesla]  PARKED! (free: 0/3)          ← unblocked, took Honda's spot
[Toyota] LEFT (free: 1/3)

[Suzuki] PARKED! (free: 0/3)          ← unblocked, took Toyota's spot
```

The key observation: when all 3 spots are occupied, Tesla and Suzuki are **blocked inside `xSemaphoreTake()`** — not spinning or polling. They consume zero CPU until a spot is released.

---

## Counting vs Binary Semaphore

```
+---------------------+-------------------+---------------------+
| Property            | Binary Semaphore  | Counting Semaphore  |
+---------------------+-------------------+---------------------+
| Counter range       | 0 to 1            | 0 to N              |
| Resources managed   | 1 (single access) | N (resource pool)   |
| Use case            | Event signaling   | Parking lots, pools |
|                     |                   | connection limits   |
| This project        |                   |        ✓            |
+---------------------+-------------------+---------------------+
```

---

## Configuration

Two `#define` values control the simulation. Change them to observe different behaviors:

```c
#define PARKING_SPOTS   3U    /* semaphore max count */
#define TOTAL_CARS      5U    /* number of car tasks */
```

| PARKING_SPOTS | TOTAL_CARS | Behavior |
|---|---|---|
| 3 | 5 | 2 cars always waiting (contention) |
| 5 | 5 | No contention — all cars park immediately |
| 1 | 5 | Essentially a binary semaphore — one car at a time |
| 3 | 3 | No waiting — every car always finds a spot |

---

## Staggered Task Start

Each car starts with a different delay to avoid all five printing simultaneously:

```c
vTaskDelay(pdMS_TO_TICKS(ulId * 1000));
```

Car 0 (Honda) starts immediately, Car 1 (Toyota) starts after 1 s, Car 2 (BMW) after 2 s, and so on. Parking and driving durations also vary by car ID to create realistic, non-uniform behavior.

---

## Key FreeRTOS APIs Used

| API | Context | Purpose |
|---|---|---|
| `xSemaphoreCreateCounting()` | main | Create semaphore with max count 3, initial count 3 |
| `xSemaphoreTake()` | Task | Acquire a parking spot (blocks if count == 0) |
| `xSemaphoreGive()` | Task | Release a parking spot |
| `uxSemaphoreGetCount()` | Task | Read current free spots (for UART display) |
| `xTaskCreate()` | main | Create 5 car tasks with unique IDs |
| `vTaskDelay()` | Task | Simulate parking duration and driving time |

---

## Hardware Setup

| Component | Pin | Configuration |
|---|---|---|
| USART2 TX | PA2 | Alternate function, 115200 baud |
| USART2 RX | PA3 | Alternate function |

No LEDs or buttons used in this project. All output is over UART — connect a serial terminal at **115200 baud, 8N1** to observe the simulation.

---

## Project Structure

```
COUNTING_SEMAPHORE_DEMONSTRATION/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       └── main.c              ← Semaphore creation, car tasks, UART print
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
5. Watch the parking lot simulation — cars park, wait, and leave in real time
6. Try changing `PARKING_SPOTS` to 1 to see single-access behavior
