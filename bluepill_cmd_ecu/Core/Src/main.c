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
#include "can_if.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LED_PERIOD_MS   (500U) //LED 깜빡임 주기(ms),매직 넘버 대신 이름을 붙여둠
#define LED_FAST_MS     (100U) //송신 막힘(ACK 없음) 표시용 빠른 깜빡임
#define CMD_PERIOD_MS   (10U) // 0x100 송신 주기: 메시지 설계표 10ms
#define SEQ_STEP_MS     (3000U) // 자동 속도 시퀀스 한 단계 유지 시간 3초
#define STATUS_TIMEOUT_MS (100u) // 0x200이 100ms(5주기) 넘게 없으면 상대 상태를 모름
#define NUCLEO_STATE_FAULT (2U) 
#define FI03_PORT  GPIOB         /* FI-03 스위치: BOOT1 점퍼 가운데 핀 = R1 경유 PB2 */
#define FI03_PIN   GPIO_PIN_2    /* 캡 −(GND) = 평상시, 캡 +(3.3V) = 체크섬 오염 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

/* USER CODE BEGIN PV */
//고정 속도 시퀀스 (가변저항 대신). 3V 최고 약 58 rpm이라 그 안에서 0/20/35/20 rpm
static const uint16_t k_seq_rpm[] = {0U, 20U, 35U, 20U};
#define SEQ_LEN (sizeof(k_seq_rpm) / sizeof(k_seq_rpm[0])) //배열 길이 4
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
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
  MX_CAN_Init();
  /* USER CODE BEGIN 2 */
  if (can_if_init() != HAL_OK) {
    Error_Handler();   /* CAN 시작 실패 -> 여기서 멈춤 (LED가 깜빡이지 않음) */
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /*HAL_GetTick(): SysTick이 1ms마다 올려 주는 카운터(부팅 후 경과ms)
    static uint32_t s_led_last_ms = 0U; //마지막으로 토글한 시각, static이라 루프를 돌아도 값이 유지됨
    uint32_t now = HAL_GetTick();

    //뺄셈으로 결과 시간을 비교하면 49.7일 뒤 카운터가 0으로 넘어가도(오버플로) 계산이 맞다.
    if((now - s_led_last_ms) >= LED_PERIOD_MS)
    {
      s_led_last_ms = now;
      //LED핀/LED_GIPO_Port는 CubeMx에서 PC13에 붙인 레이블 LED로 main.h에 자동 생성된 이름임
      HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }       Day8 LED 깜빡임용으로 쓰던 코드*/

    static uint32_t s_led_last_ms = 0U;
    static uint32_t s_cmd_last_ms = 0U;
    static uint32_t s_seq_last_ms = 0U;
    static uint32_t s_seq_idx = 0U;      /* 지금 보내는 시퀀스 단계 0~3 */
    uint32_t now = HAL_GetTick();

    /* 3초마다 다음 목표 rpm으로 (마지막 다음은 처음으로) */
    if ((now - s_seq_last_ms) >= SEQ_STEP_MS) {
      s_seq_last_ms = now;
      s_seq_idx = (s_seq_idx + 1U) % SEQ_LEN;
    }

    /* 10ms마다 0x100 송신 */
    if ((now - s_cmd_last_ms) >= CMD_PERIOD_MS) {
      s_cmd_last_ms = now;
      /* BOOT1 캡이 + 쪽(PB2 = High)이면 이번 프레임은 체크섬을 틀리게 보냄 */
      can_if_set_fi03((HAL_GPIO_ReadPin(FI03_PORT, FI03_PIN) == GPIO_PIN_SET) ? 1U : 0U);
      (void)can_if_send_cmd(k_seq_rpm[s_seq_idx]);  /* 실패는 can_if 안에서 셈 */
    }

    /* LED 3가지 표시
       - 계속 켜짐: ACK 없음(버스 끊김) / - 100ms 깜빡임: Nucleo FAULT 또는 상태 수신 끊김 / - 500ms: 정상 */
    if (can_if_tx_stuck() != 0U) {
      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);   /* PB12 Low = 켜짐 (active-low) */
    } else {
      uint32_t led_period =
          ((can_if_get_nucleo_state() == NUCLEO_STATE_FAULT) ||
           (can_if_get_status_age_ms() > STATUS_TIMEOUT_MS)) ? LED_FAST_MS : LED_PERIOD_MS;
      if ((now - s_led_last_ms) >= led_period) {
        s_led_last_ms = now;
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
      }
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
}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 4;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_15TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

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
  /* LED(PB12)용 GPIOB 클럭. 클럭이 꺼져 있으면 GPIOB 레지스터 쓰기가 무시된다 (RM0008 RCC_APB2ENR.IOPBEN) */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* FI-03 스위치: 입력. 레벨은 BOOT1 점퍼 캡이 R1을 통해 정해 주므로 내부 풀업/풀다운 없음 */
  GPIO_InitStruct.Pin  = FI03_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(FI03_PORT, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
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
