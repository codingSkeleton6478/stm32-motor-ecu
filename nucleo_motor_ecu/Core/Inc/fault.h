#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

/* 0x200 STATUS의 B4 값과 같게 맞춤 */
typedef enum {
    ECU_STATE_STOP  = 0,
    ECU_STATE_RUN   = 1,
    ECU_STATE_FAULT = 2
} ecu_state_t;

/* 고장 코드 비트 (0x200 B5). 여러 고장이 동시에 켜질 수 있게 비트로 둔다 */
#define FAULT_CAN_TIMEOUT  (0x01U)   /* SR-02: 유효 CMD 100ms 없음 */
#define FAULT_CAN_E2E      (0x02U)   /* SR-03: 체크섬·alive 오류 3회 연속 */
#define FAULT_OVERCURRENT  (0x04U)   /* SR-04: 과전류 200ms 지속 */
#define FAULT_ENCODER      (0x08U)   /* SR-05: 듀티 40% 초과인데 300ms 동안 카운트 변화 10 미만 */
#define FAULT_CURR_SENSOR  (0x10U)   /* SR-08: INA219 읽기 실패 200ms 지속 (전류 감시 불가) */

void        fault_init(void);
void        fault_step_10ms(void);   /* TIM2 ISR에서 ctrl_step_10ms() 바로 앞에 호출 */
/* main loop가 INA219를 읽을 때마다(20ms) 호출. I2C는 블로킹이라 ISR에서 못 읽으므로 값만 넘겨받는다 */
void        fault_set_current(uint16_t current_ma, uint8_t valid);
ecu_state_t fault_get_state(void);
uint8_t     fault_get_code(void);

#endif