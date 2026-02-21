/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_hal_gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  float temperature;
  float humidity;
  uint8_t aqi;
  uint16_t tvoc;
  uint16_t eco2;
} SensorData;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define aht21_addr (0x38 << 1) 
#define ens160_addr (0x53 << 1) 
#define ENS160_OPMODE 0x10 
#define ENS160_STATUS 0x20 
#define ENS160_AQI 0x21 
#define ENS160_TVOC_L 0x22 
#define ENS160_ECO2_L 0x24 
#define ENS160_TEMP_IN_L 0x13
#define ENS160_RH_IN_L 0x15
#define LOG_SECTOR_ADDR  0x08020000
#define LOG_SECTOR FLASH_SECTOR_5
#define LOG_RECORD_WORDS ((sizeof(SensorData)+3)/4)
#define LOG_RECORD_BYTES (LOG_RECORD_WORDS*4)
#define LOG_SECTOR_END (LOG_SECTOR_ADDR + 0x20000)
#define LOG_SECTOR_SIZE   0x20000        // 128 KB
#define LOG_SECTOR_END    (LOG_SECTOR_ADDR + (LOG_SECTOR_SIZE * 8 / 10))   // 80%

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile uint8_t dump_request = 0;
volatile uint8_t bt_connected = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void flash_log_sample(uint32_t *ptr, SensorData *s)
{
  HAL_FLASH_Unlock();

  uint32_t *p = (uint32_t*)s;

  for (int i = 0; i < (sizeof(SensorData)+3)/4; i++) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, *ptr, p[i]);
    *ptr += 4;
  }

  HAL_FLASH_Lock();
}
void transmit_flash_data(uint32_t start, uint32_t end)
{
    char line[64];

    while(start < end)
    {
        SensorData *s = (SensorData*)start;

        int ti = (int)(s->temperature * 100);
        int hi = (int)(s->humidity * 100);

        int len = snprintf(line,sizeof(line),
            "%d.%02d,%d.%02d,%d,%d,%d\r\n",
            ti/100, abs(ti%100),
            hi/100, abs(hi%100),
            s->aqi, s->tvoc, s->eco2);

        HAL_UART_Transmit(&huart1,(uint8_t*)line,len,HAL_MAX_DELAY);

        start += LOG_RECORD_BYTES;
    }
}
void erase_flash_sector(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t err;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = LOG_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&erase,&err);
    HAL_FLASH_Lock();
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint32_t flash_ptr = LOG_SECTOR_ADDR;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  while(flash_ptr < LOG_SECTOR_END &&
      *(uint32_t*)flash_ptr != 0xFFFFFFFF)
  {
    flash_ptr += 4;
  }
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  uint8_t opmode[2] = {ENS160_OPMODE, 0x02};
  HAL_I2C_Master_Transmit(&hi2c1, ens160_addr, opmode, 2, 100);
  volatile uint8_t  aqi_g = 0;
  volatile uint16_t tvoc_g = 0;
  volatile uint16_t eco2_g = 0;
  volatile uint8_t  status_dbg = 0;
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(1000);

  while (1) {
    if(dump_request) {
      dump_request = 0;
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
      HAL_Delay(200);
      uint32_t t0 = HAL_GetTick();
      bt_connected = 0;

      // wait up to 45s for BT STATE interrupt
      while(HAL_GetTick() - t0 < 45000)
      {
        if(bt_connected)
          break;
      }
      if(bt_connected)
      {
        transmit_flash_data(LOG_SECTOR_ADDR, flash_ptr);
        HAL_UART_Transmit(&huart1, (uint8_t*)"END\r\n", 5, HAL_MAX_DELAY);
        erase_flash_sector();
        flash_ptr = LOG_SECTOR_ADDR;
        for(int i=0;i<3;i++) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(200);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(200);
        }
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
      }
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
      continue;
    }
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};
    HAL_I2C_Master_Transmit(&hi2c1, aht21_addr, cmd, 3, 100);
    HAL_Delay(150);
    uint8_t data[6];
    HAL_I2C_Master_Receive(&hi2c1, aht21_addr, data, 6, 100);
    uint32_t raw_hum = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);
    uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    float hum = (raw_hum * 100.0f) / 1048576.0f;
    float temp = ((raw_temp * 200.0f) / 1048576.0f) - 50.0f;
    HAL_Delay(150);
    uint16_t temp_in = (uint16_t)(temp * 64.0f);
    uint16_t hum_in = (uint16_t)(hum * 512.0f);

    uint8_t temp_ens[3] = {ENS160_TEMP_IN_L, (uint8_t)(temp_in & 0xFF), (uint8_t)(temp_in >> 8)};
    uint8_t hum_ens[3] = {ENS160_RH_IN_L, (uint8_t)(hum_in & 0xFF), (uint8_t)(hum_in >> 8)};
    HAL_I2C_Master_Transmit(&hi2c1, ens160_addr, temp_ens, 3, HAL_MAX_DELAY);
    HAL_I2C_Master_Transmit(&hi2c1, ens160_addr, hum_ens, 3, HAL_MAX_DELAY);

    uint8_t reg = ENS160_STATUS;
    uint8_t status;

    HAL_I2C_Master_Transmit(&hi2c1, ens160_addr, &reg, 1, HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(&hi2c1, ens160_addr, &status, 1, HAL_MAX_DELAY);
    status_dbg = status;
    if (status != 0) {
      volatile uint8_t aqi;
      uint8_t buf[2];

      reg = ENS160_AQI;
      HAL_I2C_Master_Transmit(&hi2c1, ens160_addr, &reg, 1, HAL_MAX_DELAY);
      HAL_I2C_Master_Receive(&hi2c1, ens160_addr, &aqi, 1, HAL_MAX_DELAY);

      reg = ENS160_TVOC_L;
      HAL_I2C_Master_Transmit(&hi2c1, ens160_addr, &reg, 1, HAL_MAX_DELAY);
      HAL_I2C_Master_Receive(&hi2c1, ens160_addr, buf, 2, HAL_MAX_DELAY);
      volatile uint16_t tvoc = (uint16_t)buf[0] | ((uint16_t)(buf[1]) << 8);

      reg = ENS160_ECO2_L;
      HAL_I2C_Master_Transmit(&hi2c1, ens160_addr, &reg, 1, HAL_MAX_DELAY);
      HAL_I2C_Master_Receive(&hi2c1, ens160_addr, buf, 2, HAL_MAX_DELAY);
      volatile uint16_t eco2 = (uint16_t)buf[0] | ((uint16_t)(buf[1]) << 8);
      aqi_g = aqi;
      tvoc_g = tvoc;
      eco2_g = eco2;

      SensorData sample;

      sample.temperature = temp;
      sample.humidity = hum;
      sample.aqi = aqi;
      sample.tvoc = tvoc;
      sample.eco2 = eco2;
      if(flash_ptr + LOG_RECORD_BYTES <= LOG_SECTOR_END){
        flash_log_sample(&flash_ptr, &sample);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(1000);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
      }
      int temp_i = (int)(temp * 100);
      int hum_i = (int)(hum * 100);

      char line[64];

      int len = snprintf(line, sizeof(line),
         "%d.%02d,%d.%02d,%d,%d,%d\r\n",
         temp_i/100, abs(temp_i%100),
         hum_i/100, abs(hum_i%100),
         aqi, tvoc, eco2);
      
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
      HAL_Delay(2000);
      HAL_Delay(10000);


    }
  }
}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* USER CODE END 3 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the WakeUp
  */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_3)
    {
        dump_request = 1;
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    }
    if(GPIO_Pin == GPIO_PIN_1)   // Bluetooth STATE
    {
        bt_connected = 1;
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }

}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
