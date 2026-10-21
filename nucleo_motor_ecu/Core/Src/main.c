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
#include "stm32f1xx_hal_adc_ex.h"
#include <stdint.h>
#include <stdbool.h>
#include "motor.h"
#include "cmd.h"
#include "encoder.h"
#include "ctrl.h"
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
#define ADC_MAX_COUNT      (4095U) //12비트 ADC 최대값
#define ADC_MA_LEN         (16U) //이동평균에 쓰는 샘플 수 
#define ADC_MA_SHIFT       (4U) // 16 = 2^4 -> 나누기 대신 4칸 오른쪽 이동

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
static volatile uint8_t g_btn_pressed = 0U;
static volatile uint32_t g_ms_tick = 0U;
static volatile uint32_t g_flag_10ms = 0U;
static volatile uint32_t g_flag_100ms = 0U;
static volatile uint32_t g_flag_1000ms = 0U;
static volatile uint16_t adc_buf[1];    // DMA가 여기에 최신 변환 값을 계속 덮어 씀(ISR/DMA와 main이 공유 -> volatile)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  uint32_t blink_ms = 500U;
  uint32_t blink_acc = 0U;
  uint32_t last_btn = 0U;
  uint32_t tick_count = 0U;
  uint32_t run_10ms = 0U;
  uint32_t run_100ms = 0U;

  LedReg_Init();          // LED를 레지스터로 다시 설정(HAL 설정을 덮어씀)
  uint16_t ma_buf[ADC_MA_LEN] = {0U}; // 최근 16개 샘플 보관
  uint32_t ma_sum = 0U; //16개의 합
  uint32_t ma_idx = 0U; // 다음에 덮어쓸 칸
  uint16_t adc_raw = 0U; //필터 전 값
  uint16_t adc_avg = 0U; //필터 후 값
  uint16_t raw_min = ADC_MAX_COUNT, raw_max = 0U; //1초 동안의 필터 전 최소/최대
  uint16_t avg_min = ADC_MAX_COUNT, avg_max = 0U; //1초 동안의 필터 후 최소/최대

  if(HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)  //F1 전용 자체 보정. 시작 전에 해야함
  {
    Error_Handler();
  }
  if(HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, 1U) != HAL_OK) // 변환 시작 + DMA가 adc_but[0]에 계속 저장
  {
    Error_Handler();
  }
   __HAL_DMA_DISABLE_IT(&hdma_adc1, (DMA_IT_TC | DMA_IT_HT));   // 완료·절반 완료 인터럽트만 끔. 전송 자체(DMA 복사)는 계속됨
  motor_init();
  motor_enable(true);
  encoder_init();
  ctrl_init();
  cmd_init();
  printf("Day6 start\r\n");

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
    cmd_poll();
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
      run_10ms++;           // 셀자 작업(지금은 횟수 세기뿐, 6일차에 PI 제어가 들어옴)
      adc_raw = adc_buf[0]; // DMA가 채워둔 최신 값을 한 번 집어 옴
      ma_sum -= ma_buf[ma_idx]; // 가장 오래된 값을 합에서 빼고
      ma_buf[ma_idx] = adc_raw; //그 자리에 새 값을 넣고
      ma_sum += adc_raw; //합에 더함
      ma_idx = (ma_idx + 1U) % ADC_MA_LEN; //다음칸 (15다음은0)
      adc_avg = (uint16_t)(ma_sum >> ADC_MA_SHIFT); // 합/16 = 평균
      if(adc_raw < raw_min) {raw_min = adc_raw;} //노이즈 측정: 최소/최대 갱신
      if(adc_raw > raw_max) {raw_max = adc_raw;}
      if(adc_avg < avg_min) {avg_min = adc_avg;}
      if(adc_avg > avg_max) {avg_max = adc_avg;}
      if (cmd_log_on())
      {
        printf("%lu,%d,%ld,%d\r\n", (unsigned long)g_ms_tick,
               (int)(ctrl_get_target_rpm() * 10.0f),
               (long)ctrl_get_rpm_x10(), (int)ctrl_get_duty());
      }

    }

    // 100ms 태스크: LED 깜빡임
    if(g_flag_100ms != 0U)
    {
      g_flag_100ms= 0U;
      run_100ms++;
      printf("rpm_x10=%ld\r\n", (long)encoder_get_rpm_x10());
      //printf("tot=%ld d=%d\r\n", (long)encoder_get_total(), (int)encoder_get_delta());
      //printf("adc raw=%4u avg=%4u pct=%3u%%\r\n", (unsigned)adc_raw,(unsigned)adc_avg, (unsigned)(((uint32_t)adc_avg*100U) / ADC_MAX_COUNT)); //0~4095 -> 0~100% 
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
      printf("noise raw=%u avg=%u\r\n", (unsigned)(raw_max - raw_min), (unsigned)(avg_max - avg_min));
      raw_min = ADC_MAX_COUNT; raw_max = 0U; //다음 1초를 다시 잼
      avg_min = ADC_MAX_COUNT; avg_max = 0U; 
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 3199;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_FALLING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 10;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 10;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

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
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, DBG_TIMING_Pin|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PA8 PA9 */
  GPIO_InitStruct.Pin = LD2_Pin|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DBG_TIMING_Pin PB5 */
  GPIO_InitStruct.Pin = DBG_TIMING_Pin|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
      HAL_GPIO_WritePin(DBG_TIMING_GPIO_Port, DBG_TIMING_Pin, GPIO_PIN_SET);   /* PB0 High: 제어 시작 */
      ctrl_step_10ms();  /* 엔코더 -> PI -> PWM */
      HAL_GPIO_WritePin(DBG_TIMING_GPIO_Port, DBG_TIMING_Pin, GPIO_PIN_RESET); /* PB0 Low: 제어 끝 */
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
