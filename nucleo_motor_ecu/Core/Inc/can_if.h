#ifndef CAN_IF_H
#define CAN_IF_H

#include "main.h"
#include "stm32f1xx_hal_def.h"
#include <stdint.h>

//루프백 시험 프레임 ID (11비트 표준ID)
#define CAN_IF_TEST_ID    (0x123U)

//통계: main에서 1초마다 출력용
typedef struct
{
    uint32_t tx_ok;      //송신 메일박스에 넣기 성공한 횟수
    uint32_t tx_fail;    //빈 메일박스가 없거나 HAL에러
    uint32_t rx_count;  //0x123 수신 횟수
    uint32_t rx_miss;   //받은 카운터가 기대값과 다른 횟수
    uint8_t  last_tx;    //마지막으로 보낸 카운터
    uint8_t  last_rx;     //마지막으로 받은 카운터
} can_if_stats_t;

HAL_StatusTypeDef   can_if_init(CAN_HandleTypeDef *h);  //필터 설정 + 시작+ 수신 인터럽트 켜기
HAL_StatusTypeDef can_if_send_test(void);   //0x123, DLC 8, byte0=카운터
void  can_if_get_stats(can_if_stats_t *out);

#endif