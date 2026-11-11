#include "fault.h"
#include "main.h"     /* HAL_GetTick */
#include "can_if.h"
#include "ctrl.h"
#include "motor.h"
#include "encoder.h"

/* ---- 판정 기준값 (근거는 docs/measurements.md) ---- */
#define CAN_TIMEOUT_MS      (100U)  /* SR-02: 유효 CMD가 10주기(100ms) 넘게 없으면 고장 */
#define CAN_E2E_ERR_MAX     (3U)    /* SR-03: 체크섬·alive 오류 3회 연속이면 고장 */

#define OC_LIMIT_MA         (380U)  /* SR-04: 정상 최대 약 315mA(Day 7 손 부하) x 약 2 */
#define OC_TICKS            (20U)   /* SR-04: 20 x 10ms = 200ms 지속. 출발 순간 최대 550mA는 짧아서 안 걸림 */

#define ENC_DUTY_MIN        (400)   /* SR-05: 듀티 40%(1000 만점에 400) 넘게 밀고 있을 때만 검사 */
#define ENC_WINDOW_TICKS    (30U)   /* SR-05: 30 x 10ms = 300ms 창 */
#define ENC_MIN_COUNTS      (10)    /* SR-05: 300ms 동안 10카운트 미만이면 "안 돈다". 듀티 40%면 정상은 약 400카운트 */

#define SENSOR_TICKS        (20U)   /* SR-08: INA219 무효가 200ms 지속되면 고장 */

/* TIM2 ISR이 쓰고 main(로그·LCD·0x200)이 읽으므로 volatile */
static volatile ecu_state_t s_state = ECU_STATE_FAULT;
static volatile uint8_t     s_code  = 0U;

/* main이 쓰고 ISR이 읽음. 각각 16비트·8비트 한 번 쓰기라 깨지지 않는다 */
static volatile uint16_t s_cur_ma    = 0U;
static volatile uint8_t  s_cur_valid = 0U;

/* 아래는 ISR 안에서만 쓰는 카운터라 volatile이 필요 없다 */
static uint8_t s_oc_ticks     = 0U;   /* 과전류가 연속으로 이어진 10ms 횟수 */
static uint8_t s_sensor_ticks = 0U;   /* INA219 무효가 연속으로 이어진 10ms 횟수 */
static uint8_t s_enc_ticks    = 0U;   /* 엔코더 창 안에서 지난 10ms 횟수 */
static int32_t s_enc_ref      = 0;    /* 창 시작 시점의 누적 카운트 */

void fault_init(void)
{
    /* 부팅 직후에는 유효 프레임을 아직 못 받았으므로 출력을 끈 상태로 시작한다 */
    motor_enable(false);
    ctrl_stop();
    s_state = ECU_STATE_STOP;
    s_code  = 0U;
}

void fault_set_current(uint16_t current_ma, uint8_t valid)
{
    s_cur_ma    = current_ma;
    s_cur_valid = valid;
}

/* SR-04 과전류 / SR-08 전류 센서 무효: "연속 N번"을 세서 짧은 튐은 무시한다 */
static uint8_t check_current(void)
{
    uint8_t det = 0U;

    if (s_cur_valid == 0U) {
        /* 센서 값을 믿을 수 없으면 과전류 판단을 하지 않고, 무효 상태가 오래가면 그 자체를 고장으로 본다 */
        s_oc_ticks = 0U;
        if (s_sensor_ticks < 255U) { s_sensor_ticks++; }
        if (s_sensor_ticks >= SENSOR_TICKS) { det |= FAULT_CURR_SENSOR; }   /* SR-08 */
    } else {
        s_sensor_ticks = 0U;
        if (s_cur_ma > OC_LIMIT_MA) {
            if (s_oc_ticks < 255U) { s_oc_ticks++; }
        } else {
            s_oc_ticks = 0U;     /* 한 번이라도 내려가면 처음부터 다시 셈 */
        }
        if (s_oc_ticks >= OC_TICKS) { det |= FAULT_OVERCURRENT; }          /* SR-04 */
    }
    return det;
}

/* SR-05 엔코더 고장: 모터를 세게 미는데(듀티 > 40%) 엔코더가 거의 안 변하면 신호가 없는 것 */
static uint8_t check_encoder(void)
{
    uint8_t det   = 0U;
    int32_t total = encoder_get_total();
    int32_t moved;

    if (ctrl_get_duty() > ENC_DUTY_MIN) {
        if (s_enc_ticks == 0U) { s_enc_ref = total; }   /* 창 시작: 기준 카운트 저장 */
        s_enc_ticks++;
        if (s_enc_ticks >= ENC_WINDOW_TICKS) {
            moved = total - s_enc_ref;
            if (moved < 0) { moved = -moved; }          /* 방향과 상관없이 움직인 양 */
            if (moved < ENC_MIN_COUNTS) { det |= FAULT_ENCODER; }
            s_enc_ticks = 0U;                            /* 다음 300ms 창 */
        }
    } else {
        s_enc_ticks = 0U;     /* 듀티가 낮으면 안 도는 게 정상일 수 있어 검사하지 않음 */
    }
    return det;
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
    /* SR-04, SR-08: 전류 */
    detected |= check_current();
    /* SR-05: 엔코더 */
    detected |= check_encoder();

    if (detected != 0U) {
        s_code |= detected;        /* 고장 코드는 복귀할 때까지 유지(래치) */
        s_state = ECU_STATE_FAULT;
    }

    if (s_state == ECU_STATE_FAULT) {
        /* SR-07: 지금 감지되는 고장이 없고 목표가 0일 때만 복귀. 통신이 살아나자마자 갑자기 도는 것을 막는다 */
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

    /* 정상: CAN 목표값을 PI 제어에 넘긴다 */
    motor_enable(true);
    ctrl_set_target_rpm((float)rpm);
    s_state = (rpm == 0U) ? ECU_STATE_STOP : ECU_STATE_RUN;
}

ecu_state_t fault_get_state(void) { return s_state; }
uint8_t     fault_get_code(void)  { return s_code; }