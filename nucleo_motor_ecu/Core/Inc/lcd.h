#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_i2c.h"

#define LCD_ADDR_7BIT   (0x27U) /* 스캔 결과. PCF8574T, A0~A2 패드 비움*/
#define LCD_COLS        (16U)
#define LCD_ROWS        (2U)

/* 함수 프로토타입*/
HAL_StatusTypeDef lcd_init(I2C_HandleTypeDef *hi2c);     //전원 켠 뒤 1번. 약 60ms 걸림(블로킹)
HAL_StatusTypeDef lcd_clear(void);       //화면 지우기. 약2ms
HAL_StatusTypeDef lcd_set_cursor(uint8_t row, uint8_t col); // row 0~1, col 0~15
HAL_StatusTypeDef lcd_print(const char *str);   //현재 커서 위치부터 문자열 출력
HAL_StatusTypeDef lcd_print_line(uint8_t row, const char *str); //한 줄을 통째로 덮어쓰기

#endif //LCD_H