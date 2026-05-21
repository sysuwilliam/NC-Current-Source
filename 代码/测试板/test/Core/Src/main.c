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
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ENABLE.h"
#include "DAC.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define Rs 1            //采样电阻
#define SEND_TIME 1000  //发送间隔
#define MAX_DAC   3.3f
#define MIN_DAC   0.1f
#define DAC_COUNT ((int)(MAX_DAC / MIN_DAC)) // 结果为整数 33
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint32_t adc_ui_original[2];        // 存放同步采样的 32 位原始数据
uint16_t ADC1_CH1 = 0;              // ADC1_CH1: PA1 V_OUT+
uint16_t ADC2_CH1 = 0;              // ADC2_CH1: PA0 V_OUT-
uint16_t ADC1_CH2 = 0;              // ADC1_CH2  PB0 V_SENSE

int counter = 0;                    //编码器值
char buff[50]="";

uint8_t DAC_CHANNEL_FLAG=0;
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC2_Init();
  MX_SPI1_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  DAC_Init(&hspi1);       //初始化DAC
  Safe_Init ();           //DAC1输出2.6f，DAC输出0，BUCK与SW均关闭

  HAL_TIM_Encoder_Start(&htim4,TIM_CHANNEL_ALL);      //旋转编码器
  HAL_Delay(20);

  //校准ADC
  HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_ADCEx_Calibration_Start(&hadc2);

  HAL_ADC_Start(&hadc2); // 先开启从机
  HAL_ADCEx_MultiModeStart_DMA(&hadc1, adc_ui_original, 2);//开启DMA
  HAL_TIM_Base_Start(&htim3);//同步时钟

  uint32_t last_tick = HAL_GetTick();
  uint32_t last_tick_buff = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 每 500ms 检测并处理
    if (HAL_GetTick() - last_tick_buff >= 500)
    {
      last_tick_buff = HAL_GetTick();

      uint16_t current_cnt = __HAL_TIM_GET_COUNTER(&htim4);

      if (current_cnt > DAC_COUNT && current_cnt <= 60)
      {
        counter = DAC_COUNT;
        __HAL_TIM_SET_COUNTER(&htim4, DAC_COUNT);
      }
      else if (current_cnt > 60 && current_cnt <= 100)
      {
        counter = 0;
        __HAL_TIM_SET_COUNTER(&htim4, 0);
      }
      else
      {
        counter = current_cnt;
      }
      float dac_voltage = counter * MIN_DAC;

      if (!DAC_CHANNEL_FLAG) {
        DAC_Set_Voltage(DAC_ADDR_CHANNEL_1, dac_voltage);
      }else {
        DAC_Set_Voltage(DAC_ADDR_CHANNEL_2, dac_voltage);
      }

      snprintf(buff, sizeof(buff), "Count: %d\r\n", counter);
      HAL_UART_Transmit(&huart1, (uint8_t *)buff, strlen(buff), 10);
    }



    BUCK_Enable();
    //==============旋转编码器=============//
    if (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET) {
      HAL_Delay(20);
      if (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET) {
        DAC_CHANNEL_FLAG = !DAC_CHANNEL_FLAG;
        while (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET);
        HAL_Delay(20);
      }
    }



    if (HAL_GetTick() - last_tick >= SEND_TIME) {
      last_tick = HAL_GetTick();

      // 从 32 位同步数据中拆分出 16 位的 ADC 原始值
      ADC1_CH1 = (uint16_t)(adc_ui_original[0] & 0xFFFF);          // ADC1 第1通道 (PA1)
      ADC2_CH1 = (uint16_t)((adc_ui_original[0] >> 16) & 0xFFFF);  // ADC2 第1通道 (PA0)
      ADC1_CH2 = (uint16_t)(adc_ui_original[1] & 0xFFFF);          // ADC1 第2通道 (PB0)

      // 统一转换为真实的物理引脚电压值 (0 - 3300 mV)
      uint32_t VOUT_P = (((uint32_t)ADC1_CH1 * 3300) / 4095)*11;
      uint32_t VOUT_N = (((uint32_t)ADC2_CH1 * 3300) / 4095)*11;
      uint32_t Vsense = ((uint32_t)ADC1_CH2 * 3300) / 4095;

      int I_actual = Vsense / Rs;
      int Vload = VOUT_P - VOUT_N;
      int VMOS = VOUT_N - Vsense;
      int RLOAD = Vload/I_actual;

      // 串口一次性输出 3 个通道的真实电压值
      char msg[256];
      snprintf(msg, sizeof(msg),
         "--------------------------------------------------\r\n"
         "VOUT_P: %4lu mV | VOUT_N: %4lu mV | Vsense: %4lu mV\r\n"
         "Vload : %d mV | VMOS  : %d mV\r\n"
         "I_actual: %d mA | RLOAD : %d Ohm\r\n",
         VOUT_P, VOUT_N, Vsense, Vload, VMOS, I_actual, RLOAD);
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 50);
    }
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

//=============按键中断==============//
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == KEY_3_Pin)
  {
    if (HAL_GPIO_ReadPin(KEY_3_GPIO_Port, KEY_3_Pin) == GPIO_PIN_RESET)
    {
      Safe_Off();
    }
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
