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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TASK_10MS_PERIOD    (10U)
#define TASK_100MS_PERIOD  (100U)
#define TASK_1000MS_PERIOD (1000U)
#define LED_PIN_POS        (5U)       // PA5 = 5번 핀
#define LED_PIN_MASK       (1UL << LED_PIN_POS) // 0x0020 5번 비트만 1인 값
#define LED_CRL_SHIFT      (LED_PIN_POS * 4U)   //CRL에서 PA5 설정이 시작하는 비트 = 20 (핀당 4비트)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
static volatile uint8_t g_btn_pressed = 0U;
static volatile uint32_t g_ms_tick = 0U;
static volatile uint32_t g_flag_10ms = 0U;
static volatile uint32_t g_flag_100ms = 0U;
static volatile uint32_t g_flag_1000ms = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;        // int(4바이트) → 1바이트 글자로 안전하게 변환
  (void)HAL_UART_Transmit(&huart2, &c, 1U, HAL_MAX_DELAY);    // 기존: USART2 → ST-LINK → PC 시리얼 모니터
  (void)HAL_UART_Transmit(&huart3, &c, 1U, HAL_MAX_DELAY);    // 추가: USART3 → PB10(D6) → 로직 분석기
  return ch;       // printf 규칙: 보낸 글자를 돌려줌
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == B1_Pin)
  {
    g_btn_pressed = 1U;
  }
}
// LD2(PA5)를 HAL 없이 레지스터로 설정한다. 근거: RM0008 7.3.7, 9.2.1 
static void LedReg_Init(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;   //GPIOA 클럭 켜기, 꺼져 있으면 아래 쓰기가 전부 무시됨
  GPIOA->CRL  &= ~(0xFUL << LED_CRL_SHIFT);   //PA5의 4비트(23:20)를 먼저 0000으로 지우기
  /* &= 다음에 |=하는 이유는 CRL은 핀 8개의 설정이 한 레지스터에 같이 들어있다
     그래서 PA5칸만 지우고 -> 새 값을 넣어야 다른핀 설정이 안 망가진다. 이걸 Read modify wirte라고함.*/
  GPIOA-> CRL |= (0x2UL << LED_CRL_SHIFT);    //0010 쓰기: MODE=10(출력 2MHz), CNF=00(push-pull)
  GPIOA-> BRR = LED_PIN_MASK;   // 시작은 꺼진 상태로
}
// LD2를 레지스터로 토글한다. 근거: RM0008 9.2.4(ODR), 9.2.5(BSRR), 9.2.6
static void LedReg_Toggle(void)
{
  if((GPIOA->ODR & LED_PIN_MASK) != 0U)   // ODR = 지금 내보내는 값. 5번 비트가 1이면 켜져 있는 것
  {
    GPIOA->BRR = LED_PIN_MASK;
  }
  else {
  {
    GPIOA->BSRR = LED_PIN_MASK;
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
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  uint32_t blink_ms = 500U;
  uint32_t blink_acc = 0U;
  uint32_t last_btn = 0U;
  uint32_t tick_count = 0U;
  uint32_t run_10ms = 0U;
  uint32_t run_100ms = 0U;

  LedReg_Init();          // LED를 레지스터로 다시 설정(HAL 설정을 덮어씀)
  printf("Day2 start\r\n");

  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if(g_btn_pressed != 0U)
    {
      g_btn_pressed = 0U;
      uint32_t now = HAL_GetTick();
      if((now - last_btn) >= 50U)
      {
        last_btn = now;
        blink_ms = (blink_ms == 500U) ? 100U : 500U;
        printf("blink %lu ms\r\n", (unsigned long)blink_ms);
      }
    }

    // 10ms 태스크: 다음 단계에서 PB0 타이밍 핀을 붙일 자리
    if(g_flag_10ms != 0U)
    {
      g_flag_10ms = 0U;     // 깃발을 먼저 내림
      HAL_GPIO_WritePin(DBG_TIMING_GPIO_Port, DBG_TIMING_Pin, GPIO_PIN_SET); //작업 시작 ->PBO High
      run_10ms++;           // 셀자 작업(지금은 횟수 세기뿐, 6일차에 PI 제어가 들어옴)
      HAL_GPIO_WritePin(DBG_TIMING_GPIO_Port, DBG_TIMING_Pin, GPIO_PIN_RESET); //작업 끝 ->PBO Low
    }

    // 100ms 태스크: LED 깜빡임
    if(g_flag_100ms != 0U)
    {
      g_flag_100ms= 0U;
      run_100ms++;
      blink_acc += TASK_100MS_PERIOD;   //100ms 흘렀음
      if(blink_acc >= blink_ms)         //정한 간격만큼 쌓이면
      {
        blink_acc = 0U;                    //누적 시간 초기화
        LedReg_Toggle();
        /*HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); //LED 토글, 레지스터로 LED 토글을 위해 잠시 비활성화*/
      }
    }

    // 1000ms 태스크: 상태 로그 (printf처럼 느린 일은 느린 태스크에)
    if(g_flag_1000ms !=0U)
    {
      g_flag_1000ms =0U;
      printf("tick %lu time2-%lu hal-%lu t10=%lu t100=%lu\r\n",
              (unsigned long)tick_count, (unsigned long)g_ms_tick,
              (unsigned long)HAL_GetTick(),
              (unsigned long)run_10ms, (unsigned long)run_100ms);
      tick_count++;
      run_10ms = 0U;        //다음 1초를 다시 셈
      run_100ms = 0U;
    }
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
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
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 63;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
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

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DBG_TIMING_GPIO_Port, DBG_TIMING_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DBG_TIMING_Pin */
  GPIO_InitStruct.Pin = DBG_TIMING_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DBG_TIMING_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    g_ms_tick++;
    if ((g_ms_tick % TASK_10MS_PERIOD) == 0U)
    {
      g_flag_10ms = 1U;
    }
    if ((g_ms_tick % TASK_100MS_PERIOD) == 0U)
    {
      g_flag_100ms = 1U;
    }
    if ((g_ms_tick % TASK_1000MS_PERIOD) == 0U)
    {
      g_flag_1000ms = 1U;
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
