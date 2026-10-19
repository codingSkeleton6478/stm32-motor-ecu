#ifndef CMD_H
#define CMD_H

void cmd_init(void); // UART2 1바이트 수신 인터럽트 시작
void cmd_poll(void); // main loop에서 호출:한 줄이 다 오면 해석

#endif