#include "motor.h"
#include "main.h"

extern TIM_HandleTypeDef htim3;   /* main.c에서 CubeMX가 만든 TIM3 핸들 */

#define MOTOR_PERIOD_CNT   3200       /* ARR+1 */
#define MOTOR_DUTY_LIMIT   300        /* 첫 구동 안전: 30% 상한 */

void motor_init(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);   /* STBY Low: 출력 꺼짐 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

void motor_enable(bool on)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void motor_set(int16_t duty_permille)
{
    int32_t d = duty_permille;
    bool fwd = (d >= 0);
    if (d < 0)  { d = -d; }
    if (d > MOTOR_DUTY_LIMIT) { d = MOTOR_DUTY_LIMIT; }

    if (d == 0) {                     /* L/L: 공회전(free-run) */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
    } else if (fwd) {                 /* AIN1=H, AIN2=L: 정회전 */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    } else {                          /* AIN1=L, AIN2=H: 역회전 */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
    }

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1,
                          (uint32_t)((d * MOTOR_PERIOD_CNT) / 1000));
}