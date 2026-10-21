#include "cmd.h"
#include "main.h"
#include "ctrl.h"
#include "stm32f1xx_hal_uart.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart2;

#define CMD_LINE_MAX    16U
#define CMD_DUTY_MIN    (-1000L)
#define CMD_DUTY_MAX    (1000L)
#define CMD_RPM_MIN     (0L)
#define CMD_RPM_MAX     (50L)     /* 3V 무부하 최고 58.2 rpm(Day 5)보다 여유 */
#define CMD_GAIN_MAX    (500.0f)

static uint8_t s_rx_byte;
static char s_line[CMD_LINE_MAX];
static volatile uint8_t s_line_ready = 0U; //ISR가 세우고 main이 내림
static bool s_log_on = false;

/* "<글자><정수>" 형식을 읽음. 형식 오류나 범위 밖이면 false */
static bool parse_long(const char *s, long min, long max, long *out)
{
    char *end;
    long val;

    if (s[0] == '\0') { return false; }
    val = strtol(s, &end, 10);
    if ((*end != '\0') || (val < min) || (val > max)) { return false; }
    *out = val;
    return true;
}

/* "<글자><실수>" 형식(예: p5.5). 0 ~ CMD_GAIN_MAX */
static bool parse_gain(const char *s, float *out)
{
    char *end;
    float val;

    if (s[0] == '\0') { return false; }
    val = strtof(s, &end);
    if ((*end != '\0') || (val < 0.0f) || (val > CMD_GAIN_MAX)) { return false; }
    *out = val;
    return true;
}

static void cmd_parse(const char *line)
{
    long  val;
    float g;
    bool  ok = false;

    switch (line[0]) {
    case 'd':                                   /* d<듀티>: 개루프 */
        if (parse_long(&line[1], CMD_DUTY_MIN, CMD_DUTY_MAX, &val)) {
            ctrl_set_duty((int16_t)val);
            ok = true;
        }
        break;
    case 'r':                                   /* r<rpm>: PI 속도 제어 */
        if (parse_long(&line[1], CMD_RPM_MIN, CMD_RPM_MAX, &val)) {
            ctrl_set_target_rpm((float)val);
            ok = true;
        }
        break;
    case 'p':                                   /* p<Kp> */
        if (parse_gain(&line[1], &g)) {
            ctrl_set_gains(g, ctrl_get_ki());
            ok = true;
        }
        break;
    case 'i':                                   /* i<Ki> */
        if (parse_gain(&line[1], &g)) {
            ctrl_set_gains(ctrl_get_kp(), g);
            ok = true;
        }
        break;
    case 'l':                                   /* l1 / l0: CSV 로그 켜기/끄기 */
        if (parse_long(&line[1], 0L, 1L, &val)) {
            s_log_on = (val == 1L);
            if (s_log_on) { printf("t_ms,target_x10,rpm_x10,duty\r\n"); }
            ok = true;
        }
        break;
    case 's':                                   /* s: 정지 */
        if (line[1] == '\0') {
            ctrl_stop();
            ok = true;
        }
        break;
    case '?':                                   /* 상태. newlib-nano printf는 %f가 없어 x100 정수로 */
        if (line[1] == '\0') {
            printf("mode=%s target=%d rpm_x10=%ld duty=%d kp_x100=%ld ki_x100=%ld\r\n",
                   (ctrl_get_mode() == CTRL_MODE_PI) ? "PI" : "OPEN",
                   (int)ctrl_get_target_rpm(), (long)ctrl_get_rpm_x10(),
                   (int)ctrl_get_duty(),
                   (long)(ctrl_get_kp() * 100.0f), (long)(ctrl_get_ki() * 100.0f));
            return;
        }
        break;
    default:
        break;
    }
    printf(ok ? "OK\r\n" : "ERR\r\n");
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

bool cmd_log_on(void)
{
    return s_log_on;
}

/* 아래 두 콜백은 기존 그대로 */
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