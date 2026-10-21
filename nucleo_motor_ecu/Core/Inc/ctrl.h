#ifndef CTRL_H
#define CTRL_H

#include <stdbool.h>
#include <stdint.h>

/* 속도 제어 모드 */
typedef enum {
    CTRL_MODE_OPEN = 0,   /* d 명령: 듀티 직접 지정 (Day 4~5 방식) */
    CTRL_MODE_PI          /* r 명령: 목표 rpm을 PI로 추종 */
} ctrl_mode_t;

void    ctrl_init(void);
void    ctrl_step_10ms(void);              /* TIM2 ISR에서 10ms마다 호출 */

void    ctrl_set_duty(int16_t duty);       /* 개루프 모드로 전환 */
void    ctrl_set_target_rpm(float rpm);    /* PI 모드로 전환 */
void    ctrl_stop(void);                   /* 개루프 듀티 0 */
void    ctrl_set_gains(float kp, float ki);

ctrl_mode_t ctrl_get_mode(void);
float   ctrl_get_target_rpm(void);
int32_t ctrl_get_rpm_x10(void);
int16_t ctrl_get_duty(void);
float   ctrl_get_kp(void);
float   ctrl_get_ki(void);

#endif