#ifndef INA219_H
#define INA219_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

#define INA219_ADDR_7BIT  (0x40U)   /* A0, A1 패드 비움 → 0x40 */

/* 함수 프로토타입(선언). 몸통은 ina219.c에 있다 */
HAL_StatusTypeDef ina219_init(I2C_HandleTypeDef *hi2c, uint16_t *config_out);
HAL_StatusTypeDef ina219_read(int16_t *current_ma_x10, uint16_t *bus_mv);
uint32_t ina219_get_err_count(void);     /* 누적 읽기 실패 횟수 */
uint32_t ina219_get_recover_count(void); /* I2C 리셋 횟수 */
uint8_t  ina219_is_valid(void);          /* 1: 마지막 읽기 성공, 0: 실패 중 */

#endif /* INA219_H */