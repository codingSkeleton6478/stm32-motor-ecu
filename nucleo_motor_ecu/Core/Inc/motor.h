#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <stdint.h>

void motor_init(void);
void motor_enable(bool on);
void motor_set(int16_t duty_permille);   /* -1000 ~ +1000, 부호 = 방향 */

#endif