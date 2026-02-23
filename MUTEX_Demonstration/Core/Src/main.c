/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @brief  Mutex Demo - Two tasks printing to UART
  *
  *  THE PROBLEM:
  *  - Task2 (high priority) prints its string, then sleeps for random ticks
  *  - Task1 (low priority) starts printing its string while Task2 sleeps
  *  - Task2 wakes up mid-print, PREEMPTS Task1, and starts printing
  *  - Result: GARBLED output on the serial terminal
  *
  *  THE FIX:
  *  - Wrap prints inside a mutex lock/unlock
  *  - Even if Task2 wakes up, it WAITS for Task1 to finish printing
  *  - Result: CLEAN output
  *
  *  HOW TO TEST:
  *  - Comment out #define USE_MUTEX   -> see garbled output
  *  - Uncomment  #define USE_MUTEX   -> see clean output
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
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




/* ===================================================================
 *  TOGGLE THIS TO SEE THE DIFFERENCE
 *  Comment out  = garbled output (no protection)
 *  Uncomment    = clean output   (mutex protection)
 * =================================================================== */
#define USE_MUTEX
static char g_acBuf[200];

/*
 * MUTEX (Mutual Exclusion):
 *
 *  - Only ONE task can "hold" it at a time
 *  - Other tasks trying to Take() will BLOCK until it's released
 *  - The task that Take()s MUST be the one that Give()s
 *
 *  Think of it like a bathroom key:
 *    - Take the key   = lock the door, you're inside
 *    - Give the key   = unlock, next person can enter
 *    - Only key holder can unlock (unlike semaphore)
 *
 *  Mutex vs Binary Semaphore:
 *    - Both are 0 or 1
 *    - Mutex has "ownership" (only taker can give)
 *    - Mutex has "priority inheritance" (prevents priority inversion)
 *    - Semaphore is for signaling between tasks
 *    - Mutex is for protecting shared resources
 */
static SemaphoreHandle_t g_xMutex = NULL;

/* The strings each task will try to print */
static const char *pcTask1String = "Task1 ::::: Hello from low-priority task, this is a task1's string to show the problem\r\n";
static const char *pcTask2String = "Task2 ----- Hello from high-priority task, this string can interrupt Task1 anytime if USE_MUTEX not defined\r\n";

/* ----  print helper ---- */
static void vPrint(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_acBuf, sizeof(g_acBuf), fmt, args);
    va_end(args);
    HAL_UART_Transmit(&huart2, (uint8_t *)g_acBuf,
                       (uint16_t)strlen(g_acBuf), HAL_MAX_DELAY);
}

/* =========================================================================
 *  Task1 - LOW priority (priority 1)
 *
 *  Runs whenever Task2 is blocked (sleeping).
 *  Prints its string character by character so you can clearly
 *  see when Task2 preempts it mid-print if USE_MUTEX not defined.
 * ========================================================================= */
static void vTask1(void *pvParam)
{

    const char *pcChar;

    for (;;)
    {
#ifdef USE_MUTEX
        /* LOCK - Task2 cannot interrupt our printing even if it wakes up */
        xSemaphoreTake(g_xMutex, portMAX_DELAY);
#endif

        /*
         * Print one character at a time.
         * Without mutex, Task2 can preempt us between ANY two characters.
         * With mutex, Task2 must wait until we release.
         */
        pcChar = pcTask1String;
        while (*pcChar != '\0')
        {
            HAL_UART_Transmit(&huart2, (uint8_t *)pcChar, 1, HAL_MAX_DELAY);
            pcChar++;
        }

#ifdef USE_MUTEX
        /* UNLOCK - now Task2 can take the mutex and print */
        xSemaphoreGive(g_xMutex);
#endif

        /* Small delay before printing again */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* =========================================================================
 *  Task2 - HIGH priority (priority 2)
 *
 *  Prints its string, then sleeps for a RANDOM number of ticks.
 *  When it wakes up, it preempts Task1 immediately (higher priority).
 *
 *  The random sleep is the KEY to this demo:
 *    - Short sleep = Task2 wakes up while Task1 is mid-print = GARBLE
 *    - Long sleep  = Task1 might finish in time = clean (by luck)
 *    - You never know when Task2 will wake up = unpredictable corruption
 * ========================================================================= */
static void vTask2(void *pvParam)
{

    const char *pcChar;

    for (;;)
    {
#ifdef USE_MUTEX
        /* LOCK - Task1 cannot be printing right now, or we wait for it */
        xSemaphoreTake(g_xMutex, portMAX_DELAY);
#endif

        /* Print one character at a time */
        pcChar = pcTask2String;
        while (*pcChar != '\0')
        {
            HAL_UART_Transmit(&huart2, (uint8_t *)pcChar, 1, HAL_MAX_DELAY);
            pcChar++;
        }

#ifdef USE_MUTEX
        /* UNLOCK */
        xSemaphoreGive(g_xMutex);
#endif

        /*
         * Sleep for RANDOM ticks (1 to 500).
         * This is what causes the unpredictable preemption.
         * Task1 starts running during this sleep.
         * When this expires, Task2 wakes up and PREEMPTS Task1.
         */
        vTaskDelay(pdMS_TO_TICKS(rand() % 500));
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
	MX_USART2_UART_Init();
	/* USER CODE BEGIN 2 */
#ifdef USE_MUTEX
    vPrint("\r\n=== Mutex ENABLED - output should be CLEAN ===\r\n\r\n");
    g_xMutex = xSemaphoreCreateMutex();
#else
    vPrint("\r\n=== Mutex DISABLED - output will be GARBLED ===\r\n\r\n");
#endif

    /* Task1 = low priority, Task2 = high priority */
    xTaskCreate(vTask1, "Task1-Low",  500, NULL, 1, NULL);
    xTaskCreate(vTask2, "Task2-High", 500, NULL, 2, NULL);

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
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

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
