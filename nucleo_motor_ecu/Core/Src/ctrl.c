#include "ctrl.h"
#include "encoder.h"
#include "motor.h"
#include "pi.h"

#define CTRL_DT_S        (0.01f)    /* 제어 주기 10ms = TIM2 1ms x 10 */
#define CTRL_OUT_MIN     (0.0f)     /* 정회전만: 감속은 마찰로 (역방향 구동 안 함) */
#define CTRL_OUT_MAX     (1000.0f)  /* motor_set 상한과 같음 */
#define CTRL_KP_DEFAULT  (50.0f)     /* 튜닝 결과 2026-10-21: 한계 Kp 약 100의 절반 */
#define CTRL_KI_DEFAULT  (300.0f)     /* 튜닝 결과 2026-10-21: 0->35 rpm 약 1초, 오버슈트 없음 */

static pi_ctrl_t s_pi;
static volatile ctrl_mode_t s_mode = CTRL_MODE_OPEN;
static volatile float   s_target_rpm = 0.0f;
static volatile int16_t s_open_duty = 0;
static volatile int16_t s_duty = 0;          /* 실제로 motor_set에 넣은 값 */
static volatile int32_t s_rpm_x10 = 0;
static volatile uint8_t s_reset_req = 0U;    /* main -> ISR: 적분 초기화 요청 */

void ctrl_init(void)
{
    pi_init(&s_pi, CTRL_KP_DEFAULT, CTRL_KI_DEFAULT, CTRL_OUT_MIN, CTRL_OUT_MAX);
}

/* ISR 안: 엔코더 읽기 -> PI 계산 -> PWM 갱신.
   motor_set은 이 함수에서만 부른다(main과 ISR이 동시에 GPIO·CCR을 건드리지 않게). */
void ctrl_step_10ms(void)
{
    int16_t duty;

    encoder_update();
    s_rpm_x10 = encoder_get_rpm_x10();

    if (s_reset_req != 0U) {
        pi_reset(&s_pi);
        s_reset_req = 0U;
    }

    if (s_mode == CTRL_MODE_PI) {
        if (s_target_rpm <= 0.0f) {
            pi_reset(&s_pi);         /* 목표 0: 출력 0, 적분도 비움 */
            duty = 0;
        } else {
            float meas = (float)s_rpm_x10 / 10.0f;
            duty = (int16_t)pi_step(&s_pi, s_target_rpm, meas, CTRL_DT_S);
        }
    } else {
        duty = s_open_duty;
    }

    motor_set(duty);
    s_duty = duty;
}

void ctrl_set_duty(int16_t duty)
{
    s_open_duty = duty;
    s_mode = CTRL_MODE_OPEN;
}

void ctrl_set_target_rpm(float rpm)
{
    if (s_mode != CTRL_MODE_PI) {
        s_reset_req = 1U;            /* 개루프에서 넘어올 때 이전 적분 버림 */
    }
    s_target_rpm = rpm;
    s_mode = CTRL_MODE_PI;
}

void ctrl_stop(void)
{
    ctrl_set_duty(0);
    s_target_rpm = 0.0f;
}

/* float 한 개 쓰기는 32비트 STR 한 번이라 ISR과 섞여도 값이 깨지지 않는다 */
void ctrl_set_gains(float kp, float ki)
{
    s_pi.kp = kp;
    s_pi.ki = ki;
    s_reset_req = 1U;
}

ctrl_mode_t ctrl_get_mode(void)     { return s_mode; }
float   ctrl_get_target_rpm(void)   { return s_target_rpm; }
int32_t ctrl_get_rpm_x10(void)      { return s_rpm_x10; }
int16_t ctrl_get_duty(void)         { return s_duty; }
float   ctrl_get_kp(void)           { return s_pi.kp; }
float   ctrl_get_ki(void)           { return s_pi.ki; }