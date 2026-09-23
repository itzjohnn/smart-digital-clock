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
#include "adc.h"
#include "i2c.h"
#include "gpio.h"
#include "ds3231.h"
#include "sh1106.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define BUZZER_PIN          GPIO_PIN_3
#define BUZZER_GPIO_PORT    GPIOA

#define BUTTON1_PIN         GPIO_PIN_0
#define BUTTON2_PIN         GPIO_PIN_1
#define BUTTON3_PIN         GPIO_PIN_2
#define BUTTON4_PIN         GPIO_PIN_3
#define BUTTONS_GPIO_PORT   GPIOB

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// Raw 12-bit ADC reading from light-dependent resistor (LDR)
uint32_t adc_val = 0;

// I2C Scanner Storage (DEBUGGER CHECK)
uint8_t found_devices = 0;
uint8_t detected_addrs[10] = {0};

// Live decoded timestamp snapshot polled continuously from DS3231
DS3231_Time_t current_time;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_ADC_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  // Enable GPIOA & GPIOB clocks
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // Configure PA3 as push-pull output
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStruct);

  // Configure PB0-PB3 as digital inputs with internal pull-ups
  GPIO_InitStruct.Pin = BUTTON1_PIN | BUTTON2_PIN | BUTTON3_PIN | BUTTON4_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BUTTONS_GPIO_PORT, &GPIO_InitStruct);

  // Calibrate ADC1
  HAL_ADCEx_Calibration_Start(&hadc);

  // Scan 7-bit addresses 1 through 127
  for (uint16_t addr = 1; addr < 128; addr++)
  {
    // HAL expects 7-bit addresses shifted left by 1
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2, 10) == HAL_OK)
    {
      if (found_devices < 10)
      {
        detected_addrs[found_devices++] = (uint8_t)addr;
      }
    }
  }

  /* Visual Pass/Fail Indication
  // If at least 2 devices ACK (OLED and RTC), blink PC9 (Green LED) 5 times rapidly.
  // If fewer than 2 devices reply, toggle PC8 (Blue LED) rapidly as a warning.
  */
  if (found_devices >= 2)
  {
    for (int i = 0; i < 5; i++)
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
      HAL_Delay(80);
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
      HAL_Delay(80);
    }
  }
  else
  {
    for (int i = 0; i < 10; i++)
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
      HAL_Delay(50);
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);
      HAL_Delay(50);
    }
  }

  // Initialize clock (TEMP CHECK)
  DS3231_Time_t init_time = 
  { 
    .seconds      = 50,
    .minutes      = 59,
    .hours        = 23,
    .day_of_week  = 1,
    .day_of_month = 20,
    .month        = 9,
    .year         = 26
  };
  DS3231_SetTime(&hi2c1, &init_time);

  // Initialize OLED
    SH1106_Init(&hi2c1);

    // Draw a test border around the entire display
    for (int16_t x = 0; x < SH1106_WIDTH; x++)
    {
      SH1106_DrawPixel(x, 0, SH1106_COLOR_WHITE);                 // Top border
      SH1106_DrawPixel(x, SH1106_HEIGHT - 1, SH1106_COLOR_WHITE); // Bottom border
    }
    for (int16_t y = 0; y < SH1106_HEIGHT; y++)
    {
      SH1106_DrawPixel(0, y, SH1106_COLOR_WHITE);                 // Left border
      SH1106_DrawPixel(SH1106_WIDTH - 1, y, SH1106_COLOR_WHITE);  // Right border
    }

    // Draw an 'X' across screen to test diagonal coordinates
    for (int16_t i = 0; i < SH1106_HEIGHT; i++)
    {
      SH1106_DrawPixel(i * 2, i, SH1106_COLOR_WHITE);
      SH1106_DrawPixel((SH1106_WIDTH - 1) - (i * 2), i, SH1106_COLOR_WHITE);
    }

    // Push framebuffer to physical screen
    SH1106_UpdateScreen(&hi2c1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // Reads pin states (active-low: RESET = pressed)
    uint8_t btn1 = (HAL_GPIO_ReadPin(BUTTONS_GPIO_PORT, BUTTON1_PIN) == GPIO_PIN_RESET);
    uint8_t btn2 = (HAL_GPIO_ReadPin(BUTTONS_GPIO_PORT, BUTTON2_PIN) == GPIO_PIN_RESET);
    uint8_t btn3 = (HAL_GPIO_ReadPin(BUTTONS_GPIO_PORT, BUTTON3_PIN) == GPIO_PIN_RESET);
    uint8_t btn4 = (HAL_GPIO_ReadPin(BUTTONS_GPIO_PORT, BUTTON4_PIN) == GPIO_PIN_RESET);

    // Sample ADC Channel 1 (LDR)
    HAL_ADC_Start(&hadc);
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK)
    {
      adc_val = HAL_ADC_GetValue(&hadc);
    }
    HAL_ADC_Stop(&hadc);

    uint8_t is_dark = (adc_val < 1500);

    // Blue LED (PC8): Active if Button 1 OR Button 3 OR dark detected
    if (btn1 || btn3 || is_dark)
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
    }
    else
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);
    }
    // Green LED (PC9): Active if Button 2 OR Button 3
    if (btn2 || btn3)
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
    }
    else
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
    }

    // Buzzer (PA3): Active if Button 4
    if (btn4)
    {
      HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_PIN, GPIO_PIN_SET);
    }
    else
    {
      HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    }

    HAL_Delay(50); // 50 ms delay

    // Poll time
    DS3231_GetTime(&hi2c1, &current_time);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI14;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI14State = RCC_HSI14_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI14CalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
