#include "can_if.h"

#define CMD_DLC        (4U)     /* B0-1 rpm, B2 alive, B3 checksum */
#define ALIVE_MASK     (0x0FU)  /* alive 0~15 순환 */
#define CHECKSUM_XOR   (0xFFU)  /* 보내는 쪽과 같은 식이어야 한다 */

static CAN_HandleTypeDef *s_hcan = NULL;

/* 아래 값은 RX0 ISR(콜백)이 쓰고 main이 읽으므로 volatile */
static volatile uint32_t s_rx_ok = 0U;
static volatile uint32_t s_err_cs = 0U;
static volatile uint32_t s_err_alive = 0U;
static volatile uint32_t s_err_dlc = 0U;
static volatile uint8_t  s_consec_err = 0U;
static volatile uint16_t s_last_rpm = 0U;
static volatile uint8_t  s_last_alive = 0U;
static volatile uint8_t  s_have_alive = 0U;   /* 첫 프레임은 비교할 이전 값이 없음 */
static volatile uint32_t s_last_ok_ms = 0U;

HAL_StatusTypeDef can_if_init(CAN_HandleTypeDef *h)
{
    CAN_FilterTypeDef f = {0};
    s_hcan = h;

    /* 필터: Day 8과 같이 전부 수신 -> FIFO0. ID 구분은 콜백에서 한다 */
    f.FilterBank           = 0U;
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh         = 0x0000U;
    f.FilterIdLow          = 0x0000U;
    f.FilterMaskIdHigh     = 0x0000U;   /* 마스크 0 = 어떤 비트도 비교 안 함 = 전부 통과 */
    f.FilterMaskIdLow      = 0x0000U;
    f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    f.FilterActivation     = ENABLE;
    f.SlaveStartFilterBank = 14U;       /* CAN2용. F103에는 없음 */
    if (HAL_CAN_ConfigFilter(s_hcan, &f) != HAL_OK) { return HAL_ERROR; }

    /* Normal 모드에서는 실제 버스(트랜시버 A의 CRX -> PA11)에서 recessive 11비트를 봐야 시작 */
    if (HAL_CAN_Start(s_hcan) != HAL_OK) { return HAL_ERROR; }

    s_last_ok_ms = HAL_GetTick();   /* 부팅 직후 바로 타임아웃으로 보지 않게 기준 시각 설정 */
    return HAL_CAN_ActivateNotification(s_hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* 오류 1회 처리: 종류별 카운트 + 연속 오류 +1 (255에서 멈춤) */
static void count_error(volatile uint32_t *cnt)
{
    (*cnt)++;
    if (s_consec_err < 255U) { s_consec_err++; }
}

/* RX0 ISR 안에서 실행: 검사하고 기록만 한다. printf·모터 제어는 하지 않음 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h)
{
    CAN_RxHeaderTypeDef rh;
    uint8_t d[8];
    uint8_t cs;
    uint8_t alive;

    if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &rh, d) != HAL_OK) { return; }
    if ((rh.IDE != CAN_ID_STD) || (rh.StdId != CAN_ID_CMD)) { return; }   /* 0x100만 처리 */

    if (rh.DLC != CMD_DLC) { count_error(&s_err_dlc); return; }

    /* E2E 검사 1: 체크섬. 비트가 깨졌거나 보낸 쪽 버그면 여기서 걸린다 */
    cs = (uint8_t)((d[0] ^ d[1] ^ d[2]) ^ CHECKSUM_XOR);
    if (cs != d[3]) { count_error(&s_err_cs); return; }

    /* E2E 검사 2: alive counter. 같은 값 반복(송신 쪽 멈춤) 또는 건너뜀(프레임 손실) 감지 */
    alive = (uint8_t)(d[2] & ALIVE_MASK);
    if ((s_have_alive != 0U) &&
        (alive != (uint8_t)((s_last_alive + 1U) & ALIVE_MASK))) {
        s_last_alive = alive;   /* 다음 프레임부터 다시 맞춰 보도록 기준만 갱신 */
        count_error(&s_err_alive);
        return;
    }

    /* 여기까지 오면 유효 프레임 */
    s_last_alive = alive;
    s_have_alive = 1U;
    s_last_rpm   = (uint16_t)((uint16_t)d[0] | ((uint16_t)d[1] << 8));  /* 리틀엔디언 복원 */
    s_last_ok_ms = HAL_GetTick();
    s_consec_err = 0U;          /* 정상 하나가 오면 연속 오류는 0부터 */
    s_rx_ok++;
}

void can_if_get_stats(can_if_stats_t *out)
{
    /* 복사하는 도중에 CAN 수신 인터럽트가 끼어들면 값이 섞이므로 그 인터럽트만 잠깐 막는다.
       TIM2(제어 루프) 인터럽트는 막지 않는다 */
    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    out->rx_ok        = s_rx_ok;
    out->err_checksum = s_err_cs;
    out->err_alive    = s_err_alive;
    out->err_dlc      = s_err_dlc;
    out->consec_err   = s_consec_err;
    out->last_rpm     = s_last_rpm;
    out->last_alive   = s_last_alive;
    out->last_ok_ms   = s_last_ok_ms;
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}
uint32_t can_if_get_last_ok_ms(void) { return s_last_ok_ms; }
uint8_t  can_if_get_consec_err(void) { return s_consec_err; }
uint16_t can_if_get_last_rpm(void)   { return s_last_rpm; }

#define STATUS_DLC  (6U)   /* B0-1 rpm, B2-3 mA, B4 상태, B5 고장 코드 */

HAL_StatusTypeDef can_if_send_status(int16_t rpm, uint16_t current_ma, uint8_t state, uint8_t code)
{
    CAN_TxHeaderTypeDef hdr = {0};   /* = {0}: 빠뜨린 필드가 쓰레기 값이 되지 않게 */
    uint8_t  d[STATUS_DLC];
    uint32_t mailbox;
    uint16_t r = (uint16_t)rpm;      /* int16의 비트 그대로 두 바이트로 나눈다(받는 쪽에서 int16로 복원) */

    if (HAL_CAN_GetTxMailboxesFreeLevel(s_hcan) == 0U) { return HAL_BUSY; }  /* 상대가 없으면 쌓지 않음 */

    hdr.StdId = CAN_ID_STATUS;
    hdr.IDE   = CAN_ID_STD;
    hdr.RTR   = CAN_RTR_DATA;
    hdr.DLC   = STATUS_DLC;
    hdr.TransmitGlobalTime = DISABLE;

    d[0] = (uint8_t)(r & 0xFFU);            /* 리틀엔디언 */
    d[1] = (uint8_t)(r >> 8);
    d[2] = (uint8_t)(current_ma & 0xFFU);
    d[3] = (uint8_t)(current_ma >> 8);
    d[4] = state;                           /* 0 STOP, 1 RUN, 2 FAULT */
    d[5] = code;                            /* bit0 CAN 타임아웃, bit1 E2E */
    return HAL_CAN_AddTxMessage(s_hcan, &hdr, d, &mailbox);
}