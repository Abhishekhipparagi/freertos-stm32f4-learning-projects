/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Binary semaphore + queue producer-consumer demo
  *
  *  Master task (producer) enqueues randomized stationery orders.
  *  Slave task (consumer) dequeues and processes each order sequentially.
  *  Synchronization: binary semaphore (signal) + depth-1 queue (data).
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>       /* va_list, va_start, va_end              */
#include <stdlib.h>       /* rand()                                 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* Stationery item identifiers. ITEM_COUNT serves as array bound & modulus. */
typedef enum
{
    ITEM_PEN = 0,
    ITEM_PENCIL,
    ITEM_ERASER,
    ITEM_NOTEBOOK,
    ITEM_MARKER,
    ITEM_STAPLER,
    ITEM_FOLDER,
    ITEM_STICKY_NOTE,
    ITEM_COUNT              /* sentinel: always equals total item types */
} SupplyItem_e;

/* Queue payload: one work order from master to slave. */
typedef struct
{
    uint16_t       usOrderId;       /*  increasing sequence ID  1...N*/
    SupplyItem_e   eItem;           /* item type to distribute               */
    uint8_t        ucQuantity;      /* units to hand out (1..15)             */
} WorkOrder_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_TX_BUF_SIZE          256U   /* vConsolePrint() buffer    */
#define ORDER_QUEUE_DEPTH         1U     /* single-slot queue (back-pressure) */
#define MASTER_TASK_STACK_WORDS   500U   /* 500 words = 2000 bytes           */
#define SLAVE_TASK_STACK_WORDS    500U
#define MASTER_TASK_PRIORITY      3U     /* higher number = higher priority   */
#define SLAVE_TASK_PRIORITY       1U
#define MAX_ORDER_QUANTITY        15U    /* max units per order               */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static char              g_acPrintBuffer[UART_TX_BUF_SIZE]; /* shared format buffer        */
static SemaphoreHandle_t g_xOrderReadySemaphore = NULL;     /* binary sem: order-ready flag */
static QueueHandle_t     g_xOrderQueue          = NULL;     /* carries WorkOrder_t structs  */

/* Enum-indexed lookup table for printable item names. */
static const char *const g_apcItemNames[ITEM_COUNT] =
{
    [ITEM_PEN]         = "Pen",
    [ITEM_PENCIL]      = "Pencil",
    [ITEM_ERASER]      = "Eraser",
    [ITEM_NOTEBOOK]    = "Notebook",
    [ITEM_MARKER]      = "Marker",
    [ITEM_STAPLER]     = "Stapler",
    [ITEM_FOLDER]      = "Folder",
    [ITEM_STICKY_NOTE] = "Sticky Note",
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void vMasterTask(void *pvParameters);
static void vSlaveTask(void *pvParameters);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---- FreeRTOS error hooks ---- */

/* if Stack overflow detected - Inspect pcTaskName in debugger. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for (;;);
}

/* If Heap allocation failed. the below hook get executed, Increase configTOTAL_HEAP_SIZE. */
void vApplicationMallocFailedHook(void)
{
    for (;;);
}

/* ---- UART formatted print () ---- */

/* printf-style output over UART2. Uses vsnprintf */
static void vConsolePrint(const char *pcFormat, ...)
{
    va_list xArgs;
    va_start(xArgs, pcFormat);
    vsnprintf(g_acPrintBuffer, sizeof(g_acPrintBuffer), pcFormat, xArgs);
    va_end(xArgs);

    HAL_UART_Transmit(&huart2, (uint8_t *)g_acPrintBuffer,
                       (uint16_t)strlen(g_acPrintBuffer), HAL_MAX_DELAY);
}

/* ---- Master task (priority 3 — producer) ---- */

/*
 * Generates a random WorkOrder_t each iteration:
 *   1. Build order (random item + quantity)
 *   2. Enqueue via xQueueSend (blocks if slave hasn't consumed yet)
 *   3. Signal slave via xSemaphoreGive
 */
static void vMasterTask(void *pvParameters)
{
    (void)pvParameters;

    WorkOrder_t     xOrder;
    BaseType_t      xQueueStatus;
    static uint16_t usSequenceCounter = 0U;

    /* Binary semaphore starts "taken"; prime it once for clean startup. */
    xSemaphoreGive(g_xOrderReadySemaphore);

    for (;;)
    {
        /* Build random order */
        usSequenceCounter++;
        xOrder.usOrderId  = usSequenceCounter;
        xOrder.eItem      = (SupplyItem_e)(rand() % (int)ITEM_COUNT);
        xOrder.ucQuantity = (uint8_t)((rand() % MAX_ORDER_QUANTITY) + 1U);

        vConsolePrint("[MASTER] Order #%u  ->  Distribute %u x %s\r\n",
                      xOrder.usOrderId,
                      xOrder.ucQuantity,
                      g_apcItemNames[xOrder.eItem]);

        /* Enqueue order; blocks indefinitely if queue full */
        xQueueStatus = xQueueSend(g_xOrderQueue, &xOrder, portMAX_DELAY);

        if (xQueueStatus != pdPASS)
        {
            vConsolePrint("[MASTER] ERROR: enqueue failed for order #%u\r\n",
                          xOrder.usOrderId);
        }
        else
        {
            /* Signal slave that a new order is available */
            xSemaphoreGive(g_xOrderReadySemaphore);

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

/* ---- Slave task (priority 1 — consumer) ---- */

/*
 * Blocks on semaphore until master signals, then:
 *   1. Dequeue WorkOrder_t ()
 *   2. Distribute items one-by-one
 *   3. Print completion message
 *   4. Loop back to wait on semaphore
 */
static void vSlaveTask(void *pvParameters)
{
    (void)pvParameters;

    WorkOrder_t xReceivedOrder;
    BaseType_t  xQueueStatus;

    for (;;)
    {
        /* Block until master gives semaphore (zero CPU while waiting) */
        xSemaphoreTake(g_xOrderReadySemaphore, portMAX_DELAY);

        /* Non-blocking dequeue; semaphore guarantees data is present */
        xQueueStatus = xQueueReceive(g_xOrderQueue, &xReceivedOrder, 0);

        if (xQueueStatus == pdPASS)
        {
            /* Hand out items one at a time */
            for (uint8_t ucUnit = 1U; ucUnit <= xReceivedOrder.ucQuantity; ucUnit++)
            {
                vConsolePrint("  [SLAVE] Handing out %s %u of %u ...\r\n",
                              g_apcItemNames[xReceivedOrder.eItem],
                              ucUnit,
                              xReceivedOrder.ucQuantity);


                vTaskDelay(pdMS_TO_TICKS(50));
            }

            vConsolePrint("  [SLAVE] Order #%u COMPLETE  (%u x %s delivered)\r\n\r\n",
                          xReceivedOrder.usOrderId,
                          xReceivedOrder.ucQuantity,
                          g_apcItemNames[xReceivedOrder.eItem]);
        }
        else
        {
            /* Semaphore fired but queue empty — should not happen */
            vConsolePrint("  [SLAVE] WARNING: semaphore received but queue empty\r\n");
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
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

  vConsolePrint("\r\n===== Master-Slave Stationery Distribution Demo =====\r\n\r\n");

  /* Create synchronization primitives */
  g_xOrderReadySemaphore = xSemaphoreCreateBinary();
  g_xOrderQueue          = xQueueCreate(ORDER_QUEUE_DEPTH, sizeof(WorkOrder_t));

  if ((g_xOrderReadySemaphore != NULL) && (g_xOrderQueue != NULL))
  {
      xTaskCreate(vMasterTask, "Master", MASTER_TASK_STACK_WORDS,
                  NULL, MASTER_TASK_PRIORITY, NULL);
      xTaskCreate(vSlaveTask,  "Slave",  SLAVE_TASK_STACK_WORDS,
                  NULL, SLAVE_TASK_PRIORITY,  NULL);

      /* Start scheduler; does not return on success */
      vTaskStartScheduler();
  }

  /* Scheduler failed to start (insufficient heap, etc.) */
  vConsolePrint("[ERROR] Failed to create semaphore or queue.\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART2_UART_Init(void)
{

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
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}


static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);


  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);


  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|GPIO_PIN_15
                          |Audio_RST_Pin, GPIO_PIN_RESET);


  GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = PDM_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = I2S3_WS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(I2S3_WS_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = SPI1_SCK_Pin|SPI1_MISO_Pin|SPI1_MOSI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = BOOT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = CLK_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|GPIO_PIN_15
                          |Audio_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = I2S3_MCK_Pin|I2S3_SCK_Pin|I2S3_SD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = VBUS_FS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(VBUS_FS_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = OTG_FS_ID_Pin|OTG_FS_DM_Pin|OTG_FS_DP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = Audio_SCL_Pin|Audio_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);


  GPIO_InitStruct.Pin = MEMS_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
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
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
