/* USER CODE BEGIN Header */
/*
 * Project : Task Creation, Deletion, Delay & Task Notification
 * Board   : STM32F407VG Discovery
 * RTOS    : FreeRTOS (manual integration, heap_4)
 *
 * Demonstrates four core FreeRTOS concepts using onboard LEDs:
 *
 *   1. Task Creation    — Four tasks via xTaskCreate(), each controls one LED.
 *   2. Task Deletion    — Button press triggers chained task deletion
 *                         (Green first, then Red) via vTaskDelete(NULL).
 *   3. Task Delay       — vTaskDelay() vs vTaskDelayUntil() compared side-by-side.
 *   4. Task Notification — ISR-to-task signaling .
 *
 * LED Mapping (GPIOD):
 *   PD12 — Green    PD13 — Orange    PD14 — Red    PD15 — Blue
 *
 * Button:
 *   PA0  — User button (EXTI0, rising edge, ISR priority 6)
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Task function prototypes */
static void task1_RED_LED(void *parameters);
static void task2_GREEN_LED(void *parameters);
static void task3_BLUE_LED(void *parameters);
static void task4_ORANGE_LED(void *parameters);
void button_interrupt_handler(void);

/* Task handles — needed to send notifications and manage deletion */
TaskHandle_t task1_RED_LED_handle;
TaskHandle_t task2_GREEN_LED_handle;
TaskHandle_t task3_BLUE_LED_handle;
TaskHandle_t task4_ORANGE_LED_handle;

/*
 * Points to the task that should be deleted on the next button press.
 * Marked volatile because it is shared between task context (critical section)
 * and ISR context — both must always see the latest value.
 *
 * Deletion chain: Green (1st press) -> Red (2nd press) -> NULL (done)
 */
TaskHandle_t volatile task_to_delete_handle = NULL;
BaseType_t status;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* FreeRTOS calls this automatically if a task overflows its stack.
 pcTaskName tells you WHICH task caused the overflow.
 The for(;;) hangs the program so you can see it in the debugger. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
	(void) xTask; /* Suppress unused variable warning */
	(void) pcTaskName; /* Suppress unused variable warning */
	for (;;)
		; /* Hang here — attach debugger to see which task crashed */
}

/* FreeRTOS calls this automatically when heap memory runs out.
 This happens when xTaskCreate, xQueueCreate, xSemaphoreCreate etc. fail
 because when configTOTAL_HEAP_SIZE is too small.
 The for(;;) hangs the program so you can catch it in the debugger. */
void vApplicationMallocFailedHook(void) {
	for (;;)
		; /* Hang here — debugger will stop at this line */
}

/* ---------------------------------------------------------------------------
 * RED LED (PD14) — Toggles every 1 s.
 *
 * Uses xTaskNotifyWait() with a 1000 ms timeout as a combined
 * "delay + signal check": blinks while no notification arrives,
 * but responds immediately when one does.
 *
 * On notification: turns LED ON permanently and deletes itself.
 * This is the SECOND task in the deletion chain (deleted after Green).
 * ---------------------------------------------------------------------------*/
static void task1_RED_LED(void *parameters)
{
	BaseType_t notify_status;

	while (1)
	{
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14);

		/* Wait up to 1000 ms for a notification.
		   If none arrives, timeout causes next toggle iteration. */
		notify_status = xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(1000));

		if (notify_status == pdTRUE)
		{
			/* Notification received — terminate this task.
			   Critical section protects shared task_to_delete_handle. */
			portENTER_CRITICAL();
			task_to_delete_handle = NULL;          /* End of deletion chain */
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
			portEXIT_CRITICAL();

			vTaskDelete(NULL);  /* Delete self — does not return */
		}
	}
}

/* ---------------------------------------------------------------------------
 * GREEN LED (PD12) — Toggles every 1 s.
 *
 * Same notification-as-delay pattern as Red task.
 *
 * On notification: hands off to Red (sets it as next deletion target),
 * turns LED ON permanently, and deletes itself.
 * This is the FIRST task in the deletion chain.
 * ---------------------------------------------------------------------------*/
static void task2_GREEN_LED(void *parameters)
{
	BaseType_t notify_status;

	while (1)
	{
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);

		notify_status = xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(1000));

		if (notify_status == pdTRUE)
		{
			portENTER_CRITICAL();
			task_to_delete_handle = task1_RED_LED_handle;  /* Red is next */
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
			portEXIT_CRITICAL();

			vTaskDelete(NULL);
		}
	}
}

/* ---------------------------------------------------------------------------
 * BLUE LED (PD15) — Toggles every 1 s using vTaskDelayUntil().
 *
 * vTaskDelayUntil() produces a precise periodic interval by compensating
 * for the task's own execution time:
 *     actual_period = exactly 1000 ms (regardless of execution time)
 *
 * Best suited for fixed-frequency tasks (sensor sampling, control loops).
 * Compare with Orange LED which uses vTaskDelay() (relative delay).
 * ---------------------------------------------------------------------------*/
static void task3_BLUE_LED(void *parameters)
{
	TickType_t last_wake_time;

	/* Capture current tick as reference — all future wakes are absolute offsets */
	last_wake_time = xTaskGetTickCount();

	while (1)
	{
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
		vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
	}
}

/* ---------------------------------------------------------------------------
 * ORANGE LED (PD13) — Toggles every 1 s using vTaskDelay().
 *
 * vTaskDelay() blocks for a duration relative to the moment it is called:
 *     actual_period = execution_time + 1000 ms  (slightly drifts over time)
 *
 * Fine for simple indicators; use vTaskDelayUntil() when precision matters.
 * ---------------------------------------------------------------------------*/
static void task4_ORANGE_LED(void *parameters)
{
	while (1)
	{
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

/* ---------------------------------------------------------------------------
 * Button ISR handler — called from HAL_GPIO_EXTI_Callback() on PA0 press.
 *
 * Software debounce: ignores presses within 200 ms of last valid press.
 *
 * Sends a task notification to the next task in the deletion chain.
 * Uses xTaskNotifyFromISR()
 * when only a wake-up signal is needed (no data transfer).
 * ---------------------------------------------------------------------------*/
void button_interrupt_handler(void)
{
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

	/* --- Software debounce --- */
	static TickType_t last_press_time = 0;
	TickType_t current_time = xTaskGetTickCountFromISR();

	if ((current_time - last_press_time) < pdMS_TO_TICKS(200))
	{
		return;  /* Bounce — ignore */
	}
	last_press_time = current_time;
	/* --- End debounce --- */

	if (task_to_delete_handle != NULL)
	{
		/* eNoAction = no value transfer, just wake the waiting task */
		xTaskNotifyFromISR(task_to_delete_handle, 0, eNoAction,
				&pxHigherPriorityTaskWoken);

		/* Yield immediately if notified task has higher priority */
		portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
	}
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	/* USER CODE BEGIN 2 */

	/*
	 * Create four tasks — all at priority 2 (equal = round-robin scheduling).
	 *  xTaskCreate(function,   name,    stack,   parameter,  priority,  handle);
	 ↑          ↑        ↑           ↑           ↑         ↑
	 what to    debug    how much    data to    who runs   task ID
	 run       name      RAM        pass in     first    (optional)  */

	status = xTaskCreate(task1_RED_LED, "TASK-1", 200, NULL, 2,
			&task1_RED_LED_handle);
	configASSERT(status = pdPASS);

	status = xTaskCreate(task2_GREEN_LED, "TASK-2", 200, NULL, 2,
			&task2_GREEN_LED_handle);
	configASSERT(status = pdPASS);

	/* Green is the first target in the deletion chain */
	task_to_delete_handle = task2_GREEN_LED_handle;

	status = xTaskCreate(task3_BLUE_LED, "TASK-3", 200, NULL, 2,
			&task3_BLUE_LED_handle);
	configASSERT(status = pdPASS);

	status = xTaskCreate(task4_ORANGE_LED, "TASK-4", 200, NULL, 2,
			&task4_ORANGE_LED_handle);
	configASSERT(status = pdPASS);

	//start the freeRTOS scheduler
	vTaskStartScheduler();

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 168;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin,
			GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOD,
	LD4_Pin | LD3_Pin | LD5_Pin | GPIO_PIN_15 | Audio_RST_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : CS_I2C_SPI_Pin */
	GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
	GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : PDM_OUT_Pin */
	GPIO_InitStruct.Pin = PDM_OUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : I2S3_WS_Pin */
	GPIO_InitStruct.Pin = I2S3_WS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(I2S3_WS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : SPI1_SCK_Pin SPI1_MISO_Pin SPI1_MOSI_Pin */
	GPIO_InitStruct.Pin = SPI1_SCK_Pin | SPI1_MISO_Pin | SPI1_MOSI_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : BOOT1_Pin */
	GPIO_InitStruct.Pin = BOOT1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : CLK_IN_Pin */
	GPIO_InitStruct.Pin = CLK_IN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin PD15
	 Audio_RST_Pin */
	GPIO_InitStruct.Pin = LD4_Pin | LD3_Pin | LD5_Pin | GPIO_PIN_15
			| Audio_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : I2S3_MCK_Pin I2S3_SCK_Pin I2S3_SD_Pin */
	GPIO_InitStruct.Pin = I2S3_MCK_Pin | I2S3_SCK_Pin | I2S3_SD_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : VBUS_FS_Pin */
	GPIO_InitStruct.Pin = VBUS_FS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(VBUS_FS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : OTG_FS_ID_Pin OTG_FS_DM_Pin OTG_FS_DP_Pin */
	GPIO_InitStruct.Pin = OTG_FS_ID_Pin | OTG_FS_DM_Pin | OTG_FS_DP_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
	GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : Audio_SCL_Pin Audio_SDA_Pin */
	GPIO_InitStruct.Pin = Audio_SCL_Pin | Audio_SDA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : MEMS_INT2_Pin */
	GPIO_InitStruct.Pin = MEMS_INT2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI0_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM6) {
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */

	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
