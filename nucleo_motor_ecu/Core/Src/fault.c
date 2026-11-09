#include "fault.h"
#include "main.h"     /* HAL_GetTick */
#include "can_if.h"
#include "ctrl.h"
#include "motor.h"

#define CAN_TIMEOUT_MS    (100U)  /* SR-02: 유효 CMD가 10주기(100ms) 넘게 없으면 고장 */
#define CAN_E2E_ERR_MAX   (3U)    /* SR-03: 체크섬·alive 오류 3회 연속이면 고장 */

/* TIM2 ISR이 쓰고 main(로그·LCD)이 읽으므로 volatile */
static volatile ecu_state_t s_state = ECU_STATE_FAULT;
static volatile uint8_t     s_code  = 0U;

void fault_init(void)
{
    /* 부팅 직후에는 유효 프레임을 아직 못 받았으므로 출력을 끈 상태로 시작한다 */
    motor_enable(false);
    ctrl_stop();
    s_state = ECU_STATE_STOP;
    s_code  = 0U;
}

void fault_step_10ms(void)
{
    uint32_t age  = HAL_GetTick() - can_if_get_last_ok_ms();  /* 마지막 유효 프레임 후 경과 ms */
    uint8_t  cons = can_if_get_consec_err();
    uint16_t rpm  = can_if_get_last_rpm();
    uint8_t  detected = 0U;

    /* SR-02: CAN timeout -> safe state */
    if (age > CAN_TIMEOUT_MS)      { detected |= FAULT_CAN_TIMEOUT; }
    /* SR-03: E2E 오류 연속 3회 -> safe state */
    if (cons >= CAN_E2E_ERR_MAX)   { detected |= FAULT_CAN_E2E; }

    if (detected != 0U) {
        s_code |= detected;        /* 고장 코드는 복귀할 때까지 유지(래치) */
        s_state = ECU_STATE_FAULT;
    }

    if (s_state == ECU_STATE_FAULT) {
        /* SR-04: 통신이 정상이고 목표가 0일 때만 복귀. 통신이 살아나자마자 갑자기 도는 것을 막는다 */
        if ((detected == 0U) && (rpm == 0U)) {
            s_code  = 0U;
            s_state = ECU_STATE_STOP;
        } else {
            /* 안전 상태: STBY Low(TB6612 출력 Hi-Z) + 듀티 0. 매 주기 다시 써서 확실히 유지 */
            motor_enable(false);
            ctrl_stop();
            return;
        }
    }

    /* 통신 정상: CAN 목표값을 PI 제어에 넘긴다 */
    motor_enable(true);
    ctrl_set_target_rpm((float)rpm);
    s_state = (rpm == 0U) ? ECU_STATE_STOP : ECU_STATE_RUN;
}

ecu_state_t fault_get_state(void) { return s_state; }
uint8_t     fault_get_code(void)  { return s_code; }