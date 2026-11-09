#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

/* 0x200 STATUS의 B4 값과 같게 맞춤 (6단계) */
typedef enum {
    ECU_STATE_STOP  = 0,
    ECU_STATE_RUN   = 1,
    ECU_STATE_FAULT = 2
} ecu_state_t;

/* 고장 코드 비트 (0x200 B5). 여러 고장이 동시에 켜질 수 있게 비트로 둔다 */
#define FAULT_CAN_TIMEOUT  (0x01U)   /* SR-02 */
#define FAULT_CAN_E2E      (0x02U)   /* SR-03 */

void        fault_init(void);
void        fault_step_10ms(void);   /* TIM2 ISR에서 ctrl_step_10ms() 바로 앞에 호출 */
ecu_state_t fault_get_state(void);
uint8_t     fault_get_code(void);

#endif