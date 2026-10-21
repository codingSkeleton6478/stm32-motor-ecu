#include "pi.h"

void pi_init(pi_ctrl_t *pi, float kp, float ki, float out_min, float out_max)
{
    pi->kp      = kp;
    pi->ki      = ki;
    pi->out_min = out_min;
    pi->out_max = out_max;
    pi->integ   = 0.0f;
}

void pi_reset(pi_ctrl_t *pi)
{
    pi->integ = 0.0f;
}

/* u = Kp*e + ∫Ki*e dt
   Anti-windup(조건부 적분): 출력이 포화됐고 오차가 포화를 더 키우는 방향이면
   이번 주기 적분을 버린다. 포화 중에 적분이 계속 쌓이면 목표에 도달한 뒤에도
   쌓인 적분을 다 빼낼 때까지 출력이 안 내려가 큰 오버슈트가 난다. */
float pi_step(pi_ctrl_t *pi, float target, float measured, float dt)
{
    float err       = target - measured;
    float integ_new = pi->integ + (pi->ki * err * dt);
    float out       = (pi->kp * err) + integ_new;

    if (out > pi->out_max) {
        out = pi->out_max;
        if (err < 0.0f) { pi->integ = integ_new; }  /* 내려가는 방향이면 적분 허용 */
    } else if (out < pi->out_min) {
        out = pi->out_min;
        if (err > 0.0f) { pi->integ = integ_new; }  /* 올라가는 방향이면 적분 허용 */
    } else {
        pi->integ = integ_new;                       /* 포화 아님: 정상 적분 */
    }
    return out;
}