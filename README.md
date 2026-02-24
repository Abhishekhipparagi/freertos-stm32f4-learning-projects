# FreeRTOS on STM32F4 — Learning Projects

Hands-on FreeRTOS projects built from scratch on the STM32F407VG Discovery board. FreeRTOS kernel integrated manually (no CubeMX RTOS middleware) for full understanding of how the RTOS interacts with the hardware.

---

## Projects

| # | Project | FreeRTOS Concepts |
|---|---|---|
| 1 | [FreeRTOS Integration](./FREERTOS_INTEGRATION) | Manual kernel integration, heap selection, interrupt configuration |
| 2 | [Task Creation, Deletion, Delay & Notification](./TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION) | `xTaskCreate`, `vTaskDelete`, `vTaskDelay` vs `vTaskDelayUntil`, `xTaskNotifyFromISR` |
| 3 | [Binary Semaphore](./BINARY_SEMAPHORE_DEMONSTRATION) | `xSemaphoreCreateBinary`, producer-consumer signaling, queue + semaphore pattern |
| 4 | [Counting Semaphore](./COUNTING_SEMAPHORE_DEMONSTRATION) | `xSemaphoreCreateCounting`, resource pool management (parking lot simulation) |
| 5 | [Mutex](./MUTEX_Demonstration) | `xSemaphoreCreateMutex`, shared resource protection, priority inheritance |
| 6 | [UART + RTC with Queues & Timers](./UART_RTC_Handling-Processing_Using_Queues-Timers) | Queues, software timers, task notifications, FSM-based menu system |

Each project has its own detailed README inside its folder.

---

## Hardware

- **Board:** STM32F407VG Discovery
- **IDE:** STM32CubeIDE
- **RTOS:** FreeRTOS (manual integration, GCC/ARM_CM4F port, heap_4)
- **UART TO USB CONVERTER:** 115200 baud, 8N1 (STM32F4-USART2 — PA2/PA3)
- **ON-BOARD LEDS**

---

## FreeRTOS API Coverage

```
Tasks          xTaskCreate, vTaskDelete, vTaskDelay, vTaskDelayUntil
Notifications  xTaskNotify, xTaskNotifyWait, xTaskNotifyFromISR
Semaphores     xSemaphoreCreateBinary, xSemaphoreCreateCounting
Mutex          xSemaphoreCreateMutex (with priority inheritance)
Queues         xQueueCreate, xQueueSend, xQueueReceive, xQueueSendFromISR
Timers         xTimerCreate, xTimerStart, xTimerStop, pvTimerGetTimerID
```

---


---

## Author

**Abhishek Hipparagi**

Built while learning FreeRTOS for embedded systems development.
