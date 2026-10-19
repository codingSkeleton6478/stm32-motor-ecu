#include "cmd.h"
#include "main.h"
#include "motor.h"
#include "stm32f1xx_hal_uart.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart2;

#define CMD_LINE_MAX    16U
#define CMD_DUTY_MIN    (-1000L)
#define CMD_DUTY_MAX    (1000L)

static uint8_t s_rx_byte;
static char s_line[CMD_LINE_MAX];
static volatile uint8_t s_line_ready = 0U; //ISR가 세우고 main이 내림
static int16_t s_duty = 0;

static void cmd_parse(const char *line)
{
    char *end;
    long val;

    if((line[0] == 'd') && (line[1] != '\0')) 
    {
        val = strtol(&line[1], &end, 10);
        if((*end != '\0') || (val<CMD_DUTY_MIN) || (val > CMD_DUTY_MAX))
        {
            printf("ERR\r\n");
        }   else
            {
                s_duty = (int16_t)val;
                motor_set(s_duty);
                printf("OK\r\n");
            }
    } else if((line[0] == 's') && (line[1] == '\0')) 
    {
        s_duty = 0;
        motor_set(0);
        printf("OK\r\n");
    } else if((line[0] == '?') && (line[1] == '\0'))
    {
        printf("duty=%d (limit 1000)\r\n", (int)s_duty) ;
    } else
    {
        printf("ERR\r\n");
    }
}

void cmd_init(void)
{
    (void)HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1U);
}

void cmd_poll(void)
{
    if(s_line_ready != 0U)
    {
        cmd_parse(s_line);
        s_line_ready = 0U;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    static uint8_t idx = 0U;

    if(huart == &huart2) {
        if(s_line_ready == 0U){
            if((s_rx_byte == '\r')  || (s_rx_byte == '\n')) {
                if(idx > 0U) {
                    s_line[idx] = '\0';
                    s_line_ready = 1U;
                }
                idx = 0U;
            } else if(idx < (CMD_LINE_MAX - 1U)) {
                s_line[idx] = (char)s_rx_byte;
                idx++;
            } else {
                idx = 0U;   //너무 긴 줄은 버림
            }
        }
        (void)HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1U); //다음 바이트
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        (void)HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1U);
    }
}