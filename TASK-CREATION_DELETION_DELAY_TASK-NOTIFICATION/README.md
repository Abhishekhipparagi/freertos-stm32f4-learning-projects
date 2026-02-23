# Task Creation, Deletion, Delay & Task Notification

Demonstrates four fundamental FreeRTOS concepts in a single project using the onboard LEDs and user button on the STM32F407VG Discovery board.

---

## Concepts Covered

**Task Creation** — Four tasks are created at startup using `xTaskCreate()`, each controlling a separate LED. All tasks run at equal priority (priority 2), so the scheduler uses round-robin to share CPU time between them.

**Task Deletion** — Tasks can delete themselves at runtime using `vTaskDelete(NULL)`. A chained deletion mechanism is implemented: the first button press deletes the Green LED task, and the second press deletes the Red LED task. The remaining tasks (Blue and Orange) continue running unaffected.

**Task Delay** — Two delay strategies are demonstrated side-by-side to highlight the difference:

```
vTaskDelay()      — Relative delay. Blocks for N ticks from the moment it is called.
                    actual_period = execution_time + delay
                    Slight drift over time. Used by Orange LED.

vTaskDelayUntil() — Absolute periodic delay. Compensates for execution time.
                    actual_period = exactly N ticks (no drift)
                    Used by Blue LED. Preferred for fixed-frequency tasks.
```

**Task Notification** — Used as a lightweight signaling mechanism from the button ISR to a task. `xTaskNotifyFromISR()` wakes the target task from `xTaskNotifyWait()`, replacing the need for a binary semaphore when only a simple wake-up signal (no data) is required.

---

## Hardware Setup

| Component | Pin | Configuration |
|---|---|---|
| Red LED | PD14 | GPIO Output (active high) |
| Green LED | PD12 | GPIO Output (active high) |
| Blue LED | PD15 | GPIO Output (active high) |
| Orange LED | PD13 | GPIO Output (active high) |
| User Button | PA0 | EXTI0, rising edge, ISR priority 6 |

No external wiring needed — all components are onboard the STM32F407VG Discovery.

---

## How It Works

### At Startup

Four tasks are created, all blinking their respective LEDs at ~1 Hz:

```
+------------+----------+--------------+---------------------+
| Task       | LED      | Delay Method | Deletable?          |
+------------+----------+--------------+---------------------+
| RED        | PD14     | Notify Wait  | Yes (2nd press)     |
| GREEN      | PD12     | Notify Wait  | Yes (1st press)     |
| BLUE       | PD15     | DelayUntil   | No (runs forever)   |
| ORANGE     | PD13     | Delay        | No (runs forever)   |
+------------+----------+--------------+---------------------+
```

### On Button Press (Deletion Chain)

A shared `task_to_delete_handle` pointer tracks which task to delete next. The ISR sends a notification to that task, which then cleans itself up:

```
Button Press 1:
  ISR notifies Green task
  Green task -> sets task_to_delete_handle = Red -> deletes itself
  Green LED stays ON permanently

Button Press 2:
  ISR notifies Red task
  Red task -> sets task_to_delete_handle = NULL -> deletes itself
  Red LED stays ON permanently

Button Press 3+:
  task_to_delete_handle == NULL -> ISR does nothing
```

### Shared Variable Protection

`task_to_delete_handle` is declared `volatile` and modified only inside `portENTER_CRITICAL()` / `portEXIT_CRITICAL()` blocks in task context, ensuring safe access from both tasks and the ISR.

---

## Important: Interrupt Wiring

The `button_interrupt_handler()` function defined in `main.c` is **not called automatically**. It must be invoked from the HAL EXTI callback in `stm32f4xx_it.c`:

```c
/* In stm32f4xx_it.c — inside USER CODE sections */

/* USER CODE BEGIN Includes */
extern void button_interrupt_handler(void);
/* USER CODE END Includes */

void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/* USER CODE BEGIN 1 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        button_interrupt_handler();
    }
}
/* USER CODE END 1 */
```

Without this wiring, button presses will trigger the EXTI interrupt but the FreeRTOS notification logic will never execute.

---

## Software Debounce

Mechanical buttons produce electrical bouncing — a single press can generate multiple rapid edges. The ISR implements a tick-based debounce:

```
Press detected
  |
  +-- Read current tick count (xTaskGetTickCountFromISR)
  +-- Compare with last valid press timestamp
  |
  +-- < 200 ms since last press?  -> Ignore (bounce)
  +-- >= 200 ms since last press? -> Process (valid press)
```

This avoids hardware debounce circuits and timer-based debounce, keeping the solution minimal.

---

## Key FreeRTOS APIs Used

| API | Context | Purpose |
|---|---|---|
| `xTaskCreate()` | main | Create tasks before scheduler starts |
| `vTaskDelete(NULL)` | Task | Task deletes itself |
| `vTaskDelay()` | Task | Relative delay (Orange LED) |
| `vTaskDelayUntil()` | Task | Absolute periodic delay (Blue LED) |
| `xTaskNotifyWait()` | Task | Block until notified (with timeout as delay) |
| `xTaskNotifyFromISR()` | ISR | Send notification from interrupt context |
| `portENTER_CRITICAL()` | Task | Disable interrupts to protect shared data |
| `portYIELD_FROM_ISR()` | ISR | Request context switch if higher-priority task woke |
| `xTaskGetTickCountFromISR()` | ISR | Read tick count safely from ISR (debounce) |

---

## Project Structure

```
TASK_CREATION_DELETION_DELAY_NOTIFICATION/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       ├── main.c              ← Task logic, ISR handler, hook functions
│       └── stm32f4xx_it.c      ← EXTI0 IRQ → calls button_interrupt_handler()
├── ThirdParty/
│   └── FreeRTOS/               ← Kernel source (manual integration)
└── Drivers/                    ← HAL & CMSIS (auto-generated)
```

---

## Build & Flash

1. Open the project in STM32CubeIDE
2. Build (`Ctrl+B`)
3. Flash to Discovery board (`Run → Debug` or `Ctrl+F11`)
4. Observe all four LEDs blinking at ~1 Hz
5. Press the user button — Green stops blinking and stays ON
6. Press again — Red stops blinking and stays ON
7. Blue and Orange continue blinking indefinitely
