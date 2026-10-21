#ifndef CMD_H
#define CMD_H

#include <stdbool.h>

void cmd_init(void); // UART2 1바이트 수신 인터럽트 시작
void cmd_poll(void); // main loop에서 호출:한 줄이 다 오면 해석
bool cmd_log_on(void); // l1이면 true: main이 10ms CSV 로그 출력

#endif