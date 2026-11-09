#include "can_if.h"
#include "stm32f1xx_hal_can.h"
#include "stm32f1xx_hal_def.h"
#include <stdint.h>

extern CAN_HandleTypeDef hcan; //main.c에서 CubeMX가 만든 핸들을 빌려씀

#define CMD_DLC     (4U) // B0-1 rpm, B2 alive, B3 checksum =4바이트
#define ALIVE_MASK  (0x0FU) //alive counter 0~15 순환(하위 4비트만)
#define CHECKSUM_XOR (0xFFU) //전부 0인 프레임이 체크섬을 통과하지 못하게 마지막에 반전


static uint8_t  s_alive = 0U; //다음에 보낼 alive counter
static uint32_t s_tx_fail = 0U;

//체크섬 = (B0 ^ B1 ^ B2) ^ 0xFF. 받는 쪽(뉴클레오)도 같은 식으로 계산해서 비교함
static uint8_t calc_checksum(const uint8_t *d)
{
    return (uint8_t)((d[0] ^ d[1] ^ d[2]) ^ CHECKSUM_XOR);
}

static volatile uint8_t  s_nucleo_state = 2U;   /* 받기 전에는 FAULT로 가정 (안전 쪽) */
static volatile uint32_t s_status_ms = 0U;      /* RX ISR이 쓰고 main이 읽음 -> volatile */

HAL_StatusTypeDef can_if_init(void)
{
    CAN_FilterTypeDef f = {0};

    /* Nucleo와 같은 "전부 수신" 필터 -> FIFO0. ID 구분은 콜백에서 */
    f.FilterBank           = 0U;
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterMaskIdHigh     = 0x0000U;
    f.FilterMaskIdLow      = 0x0000U;
    f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    f.FilterActivation     = ENABLE;
    f.SlaveStartFilterBank = 14U;
    if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK) { return HAL_ERROR; }

    if (HAL_CAN_Start(&hcan) != HAL_OK) { return HAL_ERROR; }
    s_status_ms = HAL_GetTick();
    /* FIFO0에 들어오면 USB_LP_CAN1_RX0 인터럽트 -> 아래 콜백 */
    return HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* RX0 ISR 안: 0x200에서 상태만 꺼내 기록 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h)
{
    CAN_RxHeaderTypeDef rh;
    uint8_t d[8];

    if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &rh, d) != HAL_OK) { return; }
    if ((rh.IDE == CAN_ID_STD) && (rh.StdId == CAN_ID_STATUS) && (rh.DLC >= 6U)) {
        s_nucleo_state = d[4];
        s_status_ms    = HAL_GetTick();
    }
}

uint8_t  can_if_get_nucleo_state(void)  { return s_nucleo_state; }
uint32_t can_if_get_status_age_ms(void) { return HAL_GetTick() - s_status_ms; }

HAL_StatusTypeDef can_if_send_cmd(uint16_t  target_rpm)
{
    CAN_TxHeaderTypeDef hdr = {0} ;
    uint8_t  d[CMD_DLC];
    uint32_t mailbox;
    HAL_StatusTypeDef st;
     /* bxCAN 송신 메일박스는 3개뿐(RM0008 24.7.1). ACK가 안 오면 자동 재전송하느라
       메일박스가 안 비어서 30ms 만에 꽉 찬다 -> 그때는 넣지 않고 실패만 센다 */
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U)
    {
        s_tx_fail++;
        return HAL_BUSY;
    }

    hdr.StdId = CAN_ID_CMD;          /* 0x100: Nucleo 콜백이 이 ID만 처리한다 */
    hdr.ExtId = 0U;
    hdr.IDE   = CAN_ID_STD;          /* 11비트 표준 ID (확장 29비트 아님) */
    hdr.RTR   = CAN_RTR_DATA;        /* 데이터 프레임 (원격 요청 아님) */
    hdr.DLC   = CMD_DLC;             /* 4바이트 */
    hdr.TransmitGlobalTime = DISABLE;

    d[0] = (uint8_t)(target_rpm & 0xFFU); //리틀엔디언: 아랫바이트 먼저
    d[1] = (uint8_t)(target_rpm >> 8);
    d[2] = s_alive;
    d[3] = calc_checksum(d); //B0~B2를 다 채운 뒤 계산

    st = HAL_CAN_AddTxMessage(&hcan, &hdr, d, &mailbox);
    if(st == HAL_OK) 
    {
        //실제로 메일박스에 넣었을 때만 +1, 못 보낸 주기는 번호를 건너뛰지 않는다.
        s_alive = (uint8_t)((s_alive + 1U) & ALIVE_MASK);
    } else
        {
            s_tx_fail++;
        }
        return st;
}

uint8_t can_if_tx_stuck(void)
{
    return (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U) ? 1U : 0U;
}

uint32_t can_if_get_tx_fail(void)   {return s_tx_fail;}



