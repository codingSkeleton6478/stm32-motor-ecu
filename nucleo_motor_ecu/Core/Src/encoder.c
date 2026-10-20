#include "encoder.h"
#include "stm32f1xx_hal.h"
#define ENC_CPR   3840   /* 실측 2026-10-20: 5바퀴 19453 카운트 */
#define ENC_WIN   10U    /* 10ms x 10 = 100ms 창 */


extern TIM_HandleTypeDef htim4;

static uint16_t s_prev_cnt = 0U;
static volatile int16_t s_delta = 0;
static volatile int32_t s_total = 0;
static int16_t  s_win[ENC_WIN] = {0};
static uint32_t s_win_idx = 0U;
static volatile int32_t s_win_sum = 0;

void encoder_init(void)
{
    (void)HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    s_prev_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
}

void encoder_update(void)
{
    uint16_t cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
    /* 16비트 뺄셈 결과를 int16로 해석: 65535->0 넘어감도 +1로 계산됨 */
    int16_t d = (int16_t)(uint16_t)(cnt - s_prev_cnt);
    s_prev_cnt = cnt;
    s_delta = d;
    s_total += d;
    s_win_sum -= s_win[s_win_idx];   /* 가장 오래된 10ms 값 빼고 */
    s_win[s_win_idx] = d;            /* 새 값 넣고 */
    s_win_sum += d;
    s_win_idx = (s_win_idx + 1U) % ENC_WIN;
}

int16_t encoder_get_delta(void) { return s_delta; }
int32_t encoder_get_total(void) { return s_total; }
/* rpm = (sum/CPR)회전 / 0.1s * 60s, 소수 1자리 위해 x10
   -> sum * 6000 / CPR  (FPU 없는 M3라 정수로 계산) */
int32_t encoder_get_rpm_x10(void)
{
    return (s_win_sum * 6000) / ENC_CPR;
}