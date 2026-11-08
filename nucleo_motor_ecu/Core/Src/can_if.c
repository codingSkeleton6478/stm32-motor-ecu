#include "can_if.h"
#include "stm32f1xx.h"
#include "stm32f1xx_hal_can.h"
#include "stm32f1xx_hal_def.h"
#include <stdint.h>

static CAN_HandleTypeDef *s_hcan = NULL; //main.C의 hcan을 가리킴(핸들은 main.c에 있으므로 포인터로 받는다)

//송신은 main, 수신은 ISR(콜백)에서 바뀌므로 volatile
static volatile uint32_t s_tx_ok   = 0U;
static volatile uint32_t s_tx_fail = 0U;
static volatile uint32_t s_rx_cnt  = 0U;
static volatile uint32_t s_rx_miss = 0U;
static volatile uint8_t  s_tx_seq  = 0U;   /* 다음에 보낼 카운터 */
static volatile uint8_t  s_last_rx = 0U;
static volatile uint8_t  s_exp_rx  = 0U;   /* 다음에 받기를 기대하는 카운터 */

HAL_StatusTypeDef can_if_init(CAN_HandleTypeDef *h)
{
    CAN_FilterTypeDef f = {0};
    s_hcan = h;

    //필터: bxCAN은 필터를 하나도 안 켜면 모든 프레임을 버린다.
    //ID/Mask 모드에서 Mask=0이면 어떤 비트도 비교 안함 ->전부 통과의 의미를 가짐
    f.FilterBank        =0U;    //F103은 필터 뱅크 0~13
    f.FilterMode        =CAN_FILTERMODE_IDMASK;
    f.FilterIdHigh      =0x0000U;
    f.FilterIdLow       =0x0000U;
    f.FilterMaskIdHigh  =0x0000U;   //마스크 0 -> 전부 수신하겠다는 의미
    f.FilterMaskIdLow   =0x0000U;   
    f.FilterFIFOAssignment = CAN_FILTER_FIFO0;    //통과한 프레임은 FIFO0로 -> RX0 인터럽트
    f.FilterActivation  =ENABLE;
    f.SlaveStartFilterBank = 14U;   //CAN2용 설정. F103에는 CAN2가 없어서 의미 없음
    if(HAL_CAN_ConfigFilter(s_hcan, &f) != HAL_OK) {return HAL_ERROR;}

    //초기화 모드 -> 정상 동작. 버스에서 11개 연속 recessive 비트를 봐야 시작된다.
    if(HAL_CAN_Start(s_hcan) != HAL_OK) { return HAL_ERROR;}
    
    //FIFO0에 메시지가 들어오면 인터럽트 -> HAL_CAN_RxFifo0MsgPendingCallback 호출
    return HAL_CAN_ActivateNotification(s_hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

HAL_StatusTypeDef can_if_send_test(void)
{
    CAN_TxHeaderTypeDef hdr = {0};
    uint8_t     data[8] = {0U};
    uint32_t    mailbox = 0U;

    //송신 메일박스는 3개뿐. 꽉 찼는데 넣으면 HAL_ERROR라 먼저 확인
    if(HAL_CAN_GetTxMailboxesFreeLevel(s_hcan) == 0U)
    {
        s_tx_fail++;
        return HAL_BUSY;
    }

    hdr.StdId = CAN_IF_TEST_ID;
    hdr.IDE   = CAN_ID_STD; //11비트 표준 ID
    hdr.RTR   = CAN_RTR_DATA;  //데이터 프레임(원격 요청 아님)
    hdr.DLC   = 8U;     //데이터 8바이트
    hdr.TransmitGlobalTime = DISABLE;

    data[0] = s_tx_seq;     //byte0 =  카운터, 나머지는 로직 분석기에 식별하기 위한 고정값
    data[1] = 0x11U; data[2] = 0x22U; data[3] = 0x33U;
    data[4] = 0x44U; data[5] = 0x55U; data[6] = 0x66U; data[7]=0x77U;

    if(HAL_CAN_AddTxMessage(s_hcan, &hdr, data, &mailbox) != HAL_OK)
    {
        s_tx_fail++;
        return HAL_ERROR;
    }
    s_tx_ok++;
    s_tx_seq++; //uint8_t라 255 다음 0으로 자연스럽게 돈다
    return HAL_OK;
}

//HAL의 weak 함수를 덮어 쓴다. RX0 ISR 안에서 실행 되므로 짧게: 꺼내서 세기만 하고 printf는 안함
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h)
{
    CAN_RxHeaderTypeDef rh;
    uint8_t d[8];

    if(HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &rh, d) != HAL_OK) {return;}

    if((rh.IDE == CAN_ID_STD) && (rh.StdId == CAN_IF_TEST_ID) && (rh.DLC == 8U))
    {
        if(d[0] != s_exp_rx) { s_rx_miss++;} //빠지거나 순서가 바뀐 프레임
        s_last_rx = d[0];
        s_exp_rx = (uint8_t)(d[0] + 1U);
        s_rx_cnt++;
    }
}

void can_if_get_stats(can_if_stats_t *out)
{
    out->tx_ok      = s_tx_ok;
    out->tx_fail    = s_tx_fail;
    out->rx_count   = s_rx_cnt;
    out->rx_miss    = s_rx_miss;
    out->last_tx    = (uint8_t)(s_tx_seq - 1U); //s_tx_seq는 출력값의 다음 값이라 1뺀다
    out->last_rx    = s_last_rx;
}

