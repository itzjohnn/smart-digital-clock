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
#include "dht11.h"
#include "ui.h"

#include <stdio.h>

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

// Storage for climate data
DHT11_Data_t climate_data = {0};
HAL_StatusTypeDef dht_status = HAL_ERROR;

// Current display contrast state to prevent redundant I2C bus traffic
uint8_t current_contrast = 0x80;

// Previous button states for falling-edge detection (1 = unpressed, 0 = pressed)
uint8_t prev_btn1 = 1;
uint8_t prev_btn2 = 1;
uint8_t prev_btn3 = 1;
uint8_t prev_btn4 = 1;

// Display sleep / screen-saver tracking
uint8_t is_sleeping = 0;
uint32_t dark_start_tick = 0;
#define SLEEP_TIMEOUT_MS  30000 // 30 seconds of darkness before sleep


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

  // Visual Pass/Fail Indication
  // If at least 2 devices ACK (OLED and RTC), blink PC9 (Green LED) 5 times rapidly
  // If fewer than 2 devices reply, toggle PC8 (Blue LED) rapidly as a warning
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

  // Initialize OLED
    SH1106_Init(&hi2c1);
    SH1106_Clear();
    SH1106_UpdateScreen(&hi2c1);

  // Initialize DHT11 pin
    DHT11_Init();

  // Initialize UI module
    UI_Init(&hi2c1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  int8_t last_second = -1;

  while (1)
  {
    // Button Sampling & Edge Detection
    // Read raw levels (active-low: 0 = pressed, 1 = released)
    uint8_t raw_btn1 = HAL_GPIO_ReadPin(BUTTONS_GPIO_PORT, BUTTON1_PIN);
    uint8_t raw_btn2 = HAL_GPIO_ReadPin(BUTTONS_GPIO_PORT, BUTTON2_PIN);
    uint8_t raw_btn3 = HAL_GPIO_ReadPin(BUTTONS_GPIO_PORT, BUTTON3_PIN);
    uint8_t raw_btn4 = HAL_GPIO_ReadPin(BUTTONS_GPIO_PORT, BUTTON4_PIN);

    // Falling-edge detection: was HIGH (1), now LOW (0)
    uint8_t btn1_pressed = (prev_btn1 == GPIO_PIN_SET && raw_btn1 == GPIO_PIN_RESET);
    uint8_t btn2_pressed = (prev_btn2 == GPIO_PIN_SET && raw_btn2 == GPIO_PIN_RESET);
    uint8_t btn3_pressed = (prev_btn3 == GPIO_PIN_SET && raw_btn3 == GPIO_PIN_RESET);
    uint8_t btn4_pressed = (prev_btn4 == GPIO_PIN_SET && raw_btn4 == GPIO_PIN_RESET);

    // Store history for next cycle
    prev_btn1 = raw_btn1;
    prev_btn2 = raw_btn2;
    prev_btn3 = raw_btn3;
    prev_btn4 = raw_btn4;
    
    // Detects button press to wake up and flash screen
    uint8_t any_button_pressed = (btn1_pressed || btn2_pressed || btn3_pressed || btn4_pressed);

    // If sleeping and a button was pressed, wake up and ignore menu action for this click
    if (is_sleeping && any_button_pressed)
    {
      is_sleeping = 0;
      SH1106_SetSleep(&hi2c1, 0);
      dark_start_tick = HAL_GetTick(); // Reset dark timer
    }
    else if (!is_sleeping)
    {
      // Send button events to UI state machine normally
      UI_ProcessButtons(btn1_pressed, btn2_pressed, btn3_pressed, btn4_pressed);
    }

    // Telemetry and Alarm Evaluation
    if (UI_GetState() == UI_STATE_CLOCK)
    {
      DS3231_GetTime(&hi2c1, &current_time);

      // Check if alarm time matched
      UI_CheckAlarm(&current_time);

      if (current_time.seconds != last_second)
      {
        last_second = current_time.seconds;

        if (current_time.seconds % 2 == 0)
        {
          dht_status = DHT11_Read(&climate_data);
        }
      }
    }

    // Automatic wake if alarm just fired while asleep
    if (is_sleeping && UI_IsAlarmTriggered())
    {
      is_sleeping = 0;
      SH1106_SetSleep(&hi2c1, 0);
      dark_start_tick = HAL_GetTick();
    }

    // Service non-blocking buzzer
    UI_ServiceAlarmBuzzer();

    // Ambient Light & Dynamic Display Contrast
    HAL_ADC_Start(&hadc);
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK)
    {
      adc_val = HAL_ADC_GetValue(&hadc);
    }
    HAL_ADC_Stop(&hadc);

    if (adc_val < 800)
    {
      // Tracks how long darkness persists
      if (dark_start_tick == 0)
      {
        dark_start_tick = HAL_GetTick();
      }
      else if (!is_sleeping && (HAL_GetTick() - dark_start_tick >= SLEEP_TIMEOUT_MS))
      {
        // Dark threshold exceeded; display enters deep sleep
        is_sleeping = 1;
        SH1106_SetSleep(&hi2c1, 1);
      }
    }
      else
      {
        // Room has light; reset timer and display wakes up if asleep
        dark_start_tick = 0;
        if (is_sleeping)
        {
          is_sleeping = 0;
          SH1106_SetSleep(&hi2c1, 0);
        }
      }

    // Map 12-bit ADC (0 to 4095) into 3 discrete contrast tiers
  if (!is_sleeping)
  {
    uint8_t target_contrast;
    if (adc_val < 800)
    {
      target_contrast = 0x00;
    }
    else if (adc_val < 2200)
    {
      target_contrast = 0x50;
    }
    else
    {
      target_contrast = 0xFF;
    }

    // Only transmit over I2C if the tier actually changed
    if (target_contrast != current_contrast)
    {
      current_contrast = target_contrast;
      SH1106_SetContrast(&hi2c1, current_contrast);
    }
  }

    // Render Active Screen (only when display is awake)
    if (!is_sleeping)
    {
    UI_Update(&current_time, &climate_data, dht_status);
    }

    // Fast responsive polling delay for tactile debounce
    HAL_Delay(20);

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
