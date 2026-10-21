#ifndef PI_H
#define PI_H

/* PI 속도 제어기
   단위: 목표·측정 = rpm, 출력 = 듀티 permille(-1000~+1000, motor_set과 같은 단위) */
typedef struct {
    float kp;       /* 비례 이득 [permille / rpm] */
    float ki;       /* 적분 이득 [permille / (rpm*s)] */
    float integ;    /* 적분 항 누적값 [permille] */
    float out_min;  /* 출력 하한 [permille] */
    float out_max;  /* 출력 상한 [permille] */
} pi_ctrl_t;

void  pi_init(pi_ctrl_t *pi, float kp, float ki, float out_min, float out_max);
void  pi_reset(pi_ctrl_t *pi);                                /* 적분 0으로 */
float pi_step(pi_ctrl_t *pi, float target, float measured, float dt);

#endif