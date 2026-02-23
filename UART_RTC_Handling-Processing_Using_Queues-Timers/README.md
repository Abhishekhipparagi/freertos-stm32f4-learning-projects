# UART Menu System with RTC Configuration using Queues & Software Timers

A multi-task FreeRTOS application that implements an interactive UART-based menu system on the STM32F407VG Discovery board. Demonstrates real-world use of queues for inter-task communication, software timers for periodic actions, task notifications for event signaling, and a finite state machine for application flow control.

---

## Why This Project

 This project ties multiple primitives together into a cohesive, functional application — the kind of architecture found in production embedded systems. It covers how tasks, queues, timers, and ISRs cooperate in practice.

---

## Features

**Interactive UART Menu** — A terminal-based interface (115200 baud) with a main menu and two sub-menus. Navigate by sending single-character commands over a serial terminal (PuTTY, Tera Term, etc.).

**LED Effect Engine** — Four distinct blink patterns driven by FreeRTOS software timers. Each effect uses a pair of complementary 4-bit patterns that alternate every 500 ms.

**RTC Configuration** — Set time (12-hour format with AM/PM) and date through a guided, multi-step UART dialog. Input is validated before being applied to the hardware RTC.

**Live RTC Reporting** — Toggle a periodic 1-second timer that prints the current time and date to the ITM/SWO debug console. Useful for verifying the RTC without re-entering the menu.

---

## System Architecture

The application follows a producer-consumer pattern with a central command:

```
                                    +------------------+
                                    |    UART ISR      |
                                    | (1 byte at a time)|
                                    +--------+---------+
                                             |
                                        queue_uart_rx
                                        (raw bytes)
                                             |
                                             v
                                    +------------------+
                                    |  cmd_handler     |
                                    |  (parse + route) |
                                    +--------+---------+
                                             |
                              xTaskNotify (cmd pointer)
                                             |
                   +-------------------------+-------------------------+
                   |                         |                         |
                   v                         v                         v
          +----------------+       +------------------+      +------------------+
          |   menu_task    |       |    led_task       |      |    rtc_task       |
          | (main menu)    |       | (effect select)   |      | (time/date cfg)  |
          +-------+--------+       +--------+---------+      +--------+---------+
                  |                         |                          |
                  +------------+------------+--------------------------+
                               |
                          queue_print
                       (string pointers)
                               |
                               v
                      +------------------+
                      |   print_task     |
                      | (UART transmit)  |
                      +------------------+
```

**Data flow summary:**

1. UART ISR receives one byte at a time, pushes it into `queue_uart_rx`
2. When `\n` (newline) arrives, ISR notifies `cmd_handler` via `xTaskNotifyFromISR()`
3. `cmd_handler` drains the queue, assembles a command struct, and routes it to the active task based on the current FSM state
4. The active task processes the command and enqueues response strings into `queue_print`
5. `print_task` dequeues string pointers and transmits them over UART

This separation ensures that no task directly touches the UART peripheral for transmit — all output is serialized through a single print task, preventing data corruption from concurrent access.

---

## State Machine

Navigation between menus is controlled by a finite state machine. Only one state is active at a time, and `cmd_handler` uses it to decide which task receives the next command.

```
                        +----------------+
                        |  MAIN MENU     |
                        |  (STATE: 0)    |
                        +---+--------+---+
                            |        |
                    [0]     |        |     [1]
                            v        v
                 +----------+--+  +--+-------------+
                 | LED EFFECT  |  | RTC MENU        |
                 | (STATE: 1)  |  | (STATE: 2)      |
                 +-------------+  +--+---------+----+
                                     |    |    |
                              [0]    | [1]|    | [2]
                                     v    v    v
                              +------++ +--+-+ +---------+
                              | TIME  | |DATE| | REPORT  |
                              | CFG   | |CFG | | TOGGLE  |
                              | (S:3) | |(S:4)| | (S:5)  |
                              +-------+ +----+ +---------+

All sub-states return to MAIN MENU after completion.
```

States defined in code:

| State | Value | Description |
|---|---|---|
| `STATE_MAIN_MENU` | 0 | Top-level menu displayed |
| `STATE_LED_EFFECT` | 1 | Waiting for LED effect selection |
| `STATE_RTC_MENU` | 2 | RTC sub-menu displayed |
| `STATE_RTC_TIME_CONFIG` | 3 | Collecting hh:mm:ss:AM/PM from user |
| `STATE_RTC_DATE_CONFIG` | 4 | Collecting dd/mm/dow/yy from user |
| `STATE_RTC_REPORT` | 5 | Waiting for y/n to toggle reporting |

---

## UART Menu Interface

Connect a serial terminal at **115200 baud, 8N1**. The menus appear as:

```
  +------------------------------------+
  |     STM32F4 Discovery  v1.0        |
  |          MAIN MENU                 |
  +------------------------------------+
  | [0]  LED Control Panel             |
  | [1]  Clock & Calendar Settings     |
  | [2]  Exit                          |
  +------------------------------------+
  Select option >>
```

### LED Control Panel

```
  +------------------------------------+
  |        LED CONTROL PANEL           |
  +------------------------------------+
  | [1]    Sync Blink   (all toggle)   |
  | [2]    Dual Sweep   (alt. pairs)   |
  | [3]    Wave Split   (top/bottom)   |
  | [4]    Cross Fade   (diagonal)     |
  | [none] All LEDs OFF                |
  +------------------------------------+
  Select effect >>
```

LED effect patterns (PD12, PD13, PD14, PD15):

```
Effect 1 (Sync Blink):   ON  ON  ON  ON   <->  OFF OFF OFF OFF
Effect 2 (Dual Sweep):   ON  OFF ON  OFF  <->  OFF ON  OFF ON
Effect 3 (Wave Split):   ON  ON  OFF OFF  <->  OFF OFF ON  ON
Effect 4 (Cross Fade):   OFF ON  ON  OFF  <->  ON  OFF OFF ON
```

Each effect is driven by its own software timer firing every 500 ms(you can config it ). 

### RTC Configuration

Time entry is a guided multi-step process:

```
  Hour (1-12)    : 10
  Minutes (0-59) : 30
  Seconds (0-59) : 00
  AM=0 / PM=1    : 1

  [OK] Configuration saved successfully.
  Current Time : 10:30:00 [PM]
```

 Invalid input rejects the entire entry and returns to the main menu.

### Live RTC Reporting (ITM/SWO Console)

Option `[2]` in the RTC sub-menu toggles a periodic timer that prints the current time and date every 1 second to the **ITM/SWO debug console** (not UART).

```
  Enable live time report on ITM? (y/n) : y
```

Once enabled, the SWV ITM Data Console in STM32CubeIDE shows a continuous stream:

```
  10:30:01 [PM]    02-23-2026
  10:30:02 [PM]    02-23-2026
  10:30:03 [PM]    02-23-2026
  ...
```

**How to view ITM output in STM32CubeIDE:**

1. Start a debug session
2. Open **Window → Show View → SWV → SWV ITM Data Console**
3. Click the **Configure** (gear) icon → enable **Port 0**
4. Set SWO clock to match your SYSCLK (168 MHz)
5. Click **Start Trace** (red circle button)
6. Enable reporting from the RTC menu — output appears in the console
---

## FreeRTOS Objects

### Tasks (5 total)

| Task | Stack | Priority | Role |
|---|---|---|---|
| `menu_task` | 250 words | 2 | Displays main menu, delegates to sub-tasks |
| `cmd_task` | 250 words | 2 | Parses UART input, routes commands by FSM state |
| `print_task` | 250 words | 2 | Dequeues string pointers, transmits over UART |
| `led_task` | 250 words | 2 | Handles LED effect selection, starts/stops timers |
| `rtc_task` | 250 words | 2 | Multi-step RTC time and date configuration |

### Queues (2 total)

| Queue | Depth | Item Size | Direction |
|---|---|---|---|
| `queue_uart_rx` | 10 | 1 byte | UART ISR → cmd_handler |
| `queue_print` | 10 | pointer | Any task → print_task |

**Why two queues?** `queue_uart_rx` carries raw bytes (producer is ISR). `queue_print` carries string pointers (any task can enqueue, single task transmits — serialized access to UART TX).

### Software Timers (5 total)

| Timer | Period | Auto-Reload | Purpose |
|---|---|---|---|
| `timer_led[0]` | 500 ms | Yes | Effect 1: sync blink |
| `timer_led[1]` | 500 ms | Yes | Effect 2: dual sweep |
| `timer_led[2]` | 500 ms | Yes | Effect 3: wave split |
| `timer_led[3]` | 500 ms | Yes | Effect 4: cross fade |
| `timer_rtc_report` | 1000 ms | Yes | Periodic RTC output to ITM/SWO |

Each LED timer carries a unique **Timer ID** (1–4) set at creation via the `pvTimerID` parameter. The single `callback_led_effect()` function reads this ID to select the correct blink pattern — avoiding the need for four separate callback functions.

---

## ISR and Callback Details

### UART Receive — `HAL_UART_RxCpltCallback()`

Defined in `main.c` inside `USER CODE BEGIN 4`. This callback is invoked by the HAL each time one byte arrives on USART2.

```
Byte arrives on USART2
  |
  +-- Push byte into queue_uart_rx
  |     (if queue full: drop oldest byte to make room)
  |
  +-- Is byte == '\n'?
  |     YES -> xTaskNotifyFromISR(cmd_handler)
  |     NO  -> do nothing (wait for more bytes)
  |
  +-- Re-arm: HAL_UART_Receive_IT() for next byte
```

**Important:** The callback re-arms itself after every byte. If `HAL_UART_Receive_IT()` is not called at the end, the UART stops receiving.

**Queue-full handling:** When the queue is full, the oldest byte is discarded to make room. This ensures the final `\n` delimiter is never lost, which would cause `cmd_handler` to never wake up.

### No Wiring Needed in `stm32f4xx_it.c`

Unlike the button-based projects, the UART callback (`HAL_UART_RxCpltCallback`) is part of the HAL UART interrupt chain and is called automatically by the HAL from `USART2_IRQHandler()`. No manual wiring in `stm32f4xx_it.c` is required — the HAL handles the routing internally.


---

## Hardware Setup used (On-Board)

| Component | Pin | Configuration |
|---|---|---|
| Green LED | PD12 | GPIO Output |
| Orange LED | PD13 | GPIO Output |
| Red LED | PD14 | GPIO Output |
| Blue LED | PD15 | GPIO Output |
| USART2 TX | PA2 | Alternate function |
| USART2 RX | PA3 | Alternate function |
| RTC | Internal | LSI clock, 12-hour format |

**Serial connection:** Connect a USB-to-UART adapter to PA2/PA3, or use the ST-Link Virtual COM Port if available on your board variant. Terminal settings: **115200 baud, 8N1, send on Enter = \n**.

---

## Key Design Decisions

**Centralized print task** — All UART transmissions go through `queue_print` → `print_task`. This eliminates the risk of two tasks calling `HAL_UART_Transmit()` concurrently, which would corrupt the output. No mutex needed — the queue serializes access by design.

**Command struct passed by pointer** — `cmd_handler` passes the address of its local `uart_command_t` to the target task via `xTaskNotify(eSetValueWithOverwrite)`. This avoids copying the struct through a queue and works safely because `cmd_handler` blocks on the next `xTaskNotifyWait()` immediately after sending — so the struct remains valid while the receiver reads it.

**Single callback for all LED timers** — Instead of four separate callbacks, a single `callback_led_effect()` uses `pvTimerGetTimerID()` to determine which pattern to apply. The patterns are stored in a `const` lookup table, making it trivial to add new effects.

**Static string buffers for RTC display** — `rtc_show_on_uart()` uses `static char` buffers so the pointers enqueued into `queue_print` remain valid after the function returns. Without `static`, the stack-allocated buffers would be overwritten before `print_task` consumes them.

---

## FreeRTOS APIs Used

| API | Context | Purpose |
|---|---|---|
| `xTaskCreate()` | main | Create all five tasks |
| `xTaskNotify()` | Task | Wake a task or pass a pointer between tasks |
| `xTaskNotifyWait()` | Task | Block until notified (with optional value receive) |
| `xTaskNotifyFromISR()` | ISR | Wake cmd_handler when `\n` received |
| `xQueueCreate()` | main | Create byte queue and print queue |
| `xQueueSend()` | Task | Enqueue string pointer for printing |
| `xQueueSendFromISR()` | ISR | Enqueue raw UART byte from ISR |
| `xQueueReceive()` | Task | Dequeue in print_task and cmd_handler |
| `xQueueReceiveFromISR()` | ISR | Drop oldest byte when queue full |
| `xTimerCreate()` | main | Create LED effect and RTC report timers |
| `xTimerStart()` | Task | Activate a specific LED effect |
| `xTimerStop()` | Task | Stop all LED effects before starting a new one |
| `xTimerIsTimerActive()` | Task | Check if RTC report timer is already running |
| `pvTimerGetTimerID()` | Callback | Identify which LED effect timer fired |

---

## Project Structure

```
UART_RTC_Handling-Processing_Using_Queues-Timers/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       ├── main.c              ← All task logic, callbacks, helpers
│       └── stm32f4xx_it.c      ← USART2 IRQ (auto-generated, no manual edits)
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
5. The main menu appears — interact by typing option numbers and pressing Enter
6. To see RTC live reporting, enable ITM/SWO trace in the debugger (SWV ITM Data Console)

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| No menu appears on terminal | UART not connected or wrong baud | Verify PA2/PA3 wiring, set terminal to 115200/8N1 |
| Menu appears but input is ignored | Terminal not sending `\n` on Enter | Change terminal setting: "Send on Enter = LF" or "CR+LF" |
| LED effect doesn't change | Previous timer still running | `led_stop_all_timers()` is called before starting — check timer handles are valid |
| RTC time resets after power cycle | RTC running on LSI, no battery backup | Normal for Discovery board — VBAT not battery-backed by default |
| `[!] Invalid input` on valid entry | Multi-char input where single char expected | Ensure no extra spaces or characters before pressing Enter |
| ITM report not visible | SWV not configured in debugger | Enable SWV ITM Data Console, set SWO clock to match SYSCLK |
