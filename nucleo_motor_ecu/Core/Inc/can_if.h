#ifndef CAN_IF_H
#define CAN_IF_H

#include "main.h"
#include <stdint.h>

#define CAN_ID_CMD     (0x100U)  /* 소형 보드 -> Nucleo: 목표 rpm (10ms) */
#define CAN_ID_STATUS  (0x200U)  /* Nucleo -> 소형 보드: 상태 (20ms, 6단계) */

/* 수신 통계: 1초 로그와 5단계 타임아웃 판단에 씀 */
typedef struct
{
    uint32_t rx_ok;         /* 체크섬·alive 모두 정상인 프레임 수 */
    uint32_t err_checksum;  /* 체크섬 틀림 -> 버림 */
    uint32_t err_alive;     /* alive counter가 이전 +1이 아님 -> 버림 */
    uint32_t err_dlc;       /* 길이가 4가 아님 -> 버림 */
    uint8_t  consec_err;    /* 연속 오류 횟수 (3회면 FAULT, 5단계) */
    uint16_t last_rpm;      /* 마지막 유효 목표 rpm */
    uint8_t  last_alive;    /* 마지막 유효 alive counter */
    uint32_t last_ok_ms;    /* 마지막 유효 프레임을 받은 시각 (HAL_GetTick, ms) */
} can_if_stats_t;

HAL_StatusTypeDef can_if_init(CAN_HandleTypeDef *h);  /* 필터 + 시작 + 수신 인터럽트 */
void can_if_get_stats(can_if_stats_t *out);           /* ISR이 바꾸는 값을 한 번에 복사 */

/*ISR(TIM2)에서 부르는 읽기 함수: 32비트 이하 값 하나라 한 번에 읽혀서
  인터럽트를 막지 않아도 된다.*/
  uint32_t can_if_get_last_ok_ms(void);
  uint8_t can_if_get_consec_err(void);
  uint16_t can_if_get_last_rpm(void);

  HAL_StatusTypeDef can_if_send_status(int16_t rpm, uint16_t current_ma, uint8_t state, uint8_t code);
#endif