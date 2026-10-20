#ifndef ENCODER_H
#define ENCODER_H
#include <stdint.h>
void    encoder_init(void);
void    encoder_update(void);     /* 10ms마다 호출 */
int16_t encoder_get_delta(void);  /* 직전 10ms 동안 카운트 */
int32_t encoder_get_total(void);  /* 누적 카운트 */
int32_t encoder_get_rpm_x10(void); /* 100ms 창 기준 RPM x10 */
#endif