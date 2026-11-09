#ifndef CAN_IF_H
#define CAN_IF_H

#include <stdint.h>
#include "main.h" // HAL_StatusTypeDef 등 HAL타입
#include "stm32f1xx_hal_def.h"

#define CAN_ID_CMD      (0x100U) //소형 보드 -> 뉴클레오 보드: 목표 rpm(10ms)
#define CAN_ID_STATUS   (0x200U)
//뉴클레오 보드 -> 소형 보드: 상태(20ms, 6단계)

HAL_StatusTypeDef can_if_init(void);
HAL_StatusTypeDef can_if_send_cmd(uint16_t target_rpm);
uint8_t can_if_tx_stuck(void); //송신 메일박스 3개가 다 찼으면 1(아무도 ack안하는 상태)
uint32_t can_if_get_tx_fail(void); //메일 박스가 꽉 차서 fail된 횟수
uint8_t  can_if_get_nucleo_state(void);   /* 마지막 0x200의 B4 (0 STOP, 1 RUN, 2 FAULT) */
uint32_t can_if_get_status_age_ms(void);  /* 마지막 0x200 이후 경과 ms */
#endif