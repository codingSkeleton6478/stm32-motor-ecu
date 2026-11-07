#include "lcd.h"

/* ---- PCF8574 핀 → LCD 신호 매핑 (이 백팩의 일반적인 배선) ---- */
#define LCD_BIT_RS   (0x01U)   /* P0: 0=명령, 1=문자 데이터 */
#define LCD_BIT_RW   (0x02U)   /* P1: 0=쓰기. 우리는 쓰기만 하므로 항상 0 */
#define LCD_BIT_EN   (0x04U)   /* P2: High→Low 순간에 LCD가 데이터를 읽음 */
#define LCD_BIT_BL   (0x08U)   /* P3: 백라이트. 1이면 켜짐 */

/* ---- HD44780 명령 (HD44780 데이터시트 명령표) ---- */
#define LCD_CMD_CLEAR        (0x01U)  /* 화면 지우고 커서 0,0. 실행 시간 최대 1.52ms */
#define LCD_CMD_ENTRY_INC    (0x06U)  /* 글자 쓰면 커서 오른쪽으로, 화면은 안 밀림 */
#define LCD_CMD_DISPLAY_OFF  (0x08U)
#define LCD_CMD_DISPLAY_ON   (0x0CU)  /* 화면 켬, 커서 안 보임, 깜빡임 없음 */
#define LCD_CMD_FUNC_4BIT_2L (0x28U)  /* 4비트 버스, 2줄, 5x8 글꼴 */
#define LCD_CMD_SET_DDRAM    (0x80U)  /* 커서 위치 설정 명령. 하위 7비트가 주소 */
#define LCD_ROW1_OFFSET      (0x40U)  /* 2번째 줄은 DDRAM 주소 0x40부터 시작 */

#define LCD_TIMEOUT_MS       (10U)

static I2C_HandleTypeDef *s_hi2c = NULL;

/* 4비트(nibble) 하나를 LCD에 넣는다.
 * EN=1로 한 번, EN=0으로 한 번 보내서 High→Low 하강 에지를 만든다.
 * EN High 폭은 최소 450ns가 필요한데(HD44780), I2C 100kHz에서 1바이트 전송이 약 90us라 자동으로 충분하다 */
static HAL_StatusTypeDef write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (uint8_t)(((nibble & 0x0FU) << 4) | LCD_BIT_BL | rs);  /* D4~D7은 P4~P7 → 4칸 왼쪽 */
    uint8_t buf[2];
    buf[0] = (uint8_t)(data | LCD_BIT_EN);   /* EN=1: 데이터 올려놓기 */
    buf[1] = data;                           /* EN=0: 이 순간 LCD가 읽음 */
    return HAL_I2C_Master_Transmit(s_hi2c, (uint16_t)(LCD_ADDR_7BIT << 1), buf, 2U, LCD_TIMEOUT_MS);
}

/* 1바이트를 상위 4비트 → 하위 4비트 순서로 보낸다 (4비트 모드 규칙) */
static HAL_StatusTypeDef write_byte(uint8_t value, uint8_t rs)
{
    HAL_StatusTypeDef st = write_nibble((uint8_t)(value >> 4), rs);
    if (st == HAL_OK)
    {
        st = write_nibble((uint8_t)(value & 0x0FU), rs);
    }
    /* 대부분 명령의 실행 시간은 37us인데, 다음 I2C 전송 자체가 그보다 길어서 따로 기다리지 않는다 */
    return st;
}

static HAL_StatusTypeDef send_cmd(uint8_t cmd)  { return write_byte(cmd, 0U); }          /* RS=0 */
static HAL_StatusTypeDef send_data(uint8_t ch)  { return write_byte(ch, LCD_BIT_RS); }   /* RS=1 */

HAL_StatusTypeDef lcd_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef st;
    s_hi2c = hi2c;

    HAL_Delay(50U);   /* 전원 켜진 뒤 LCD 내부 리셋 대기 (데이터시트: 40ms 이상) */

    /* LCD가 지금 8비트 모드인지 4비트 모드 중간인지 모르는 상태다.
     * 0x3을 3번 보내 확실히 8비트 모드로 맞춘 뒤(데이터시트 "Initializing by Instruction"),
     * 0x2를 보내 4비트 모드로 전환한다 */
    st = write_nibble(0x03U, 0U);
    HAL_Delay(5U);                          /* 4.1ms 이상 */
    if (st == HAL_OK) { st = write_nibble(0x03U, 0U); }
    HAL_Delay(1U);                          /* 100us 이상 */
    if (st == HAL_OK) { st = write_nibble(0x03U, 0U); }
    HAL_Delay(1U);
    if (st == HAL_OK) { st = write_nibble(0x02U, 0U); }   /* 여기부터 4비트 모드 */
    HAL_Delay(1U);

    /* 이제부터는 1바이트를 4비트씩 두 번에 보낸다 */
    if (st == HAL_OK) { st = send_cmd(LCD_CMD_FUNC_4BIT_2L); }
    if (st == HAL_OK) { st = send_cmd(LCD_CMD_DISPLAY_OFF); }
    if (st == HAL_OK) { st = lcd_clear(); }
    if (st == HAL_OK) { st = send_cmd(LCD_CMD_ENTRY_INC); }
    if (st == HAL_OK) { st = send_cmd(LCD_CMD_DISPLAY_ON); }
    return st;
}

HAL_StatusTypeDef lcd_clear(void)
{
    HAL_StatusTypeDef st = send_cmd(LCD_CMD_CLEAR);
    HAL_Delay(2U);   /* clear만 실행 시간이 1.52ms로 길어서 기다린다 */
    return st;
}

HAL_StatusTypeDef lcd_set_cursor(uint8_t row, uint8_t col)
{
    if ((row >= LCD_ROWS) || (col >= LCD_COLS))
    {
        return HAL_ERROR;   /* 범위 밖 요청은 거부 (엉뚱한 메모리 위치에 쓰는 것 방지) */
    }
    uint8_t addr = (uint8_t)(((row == 0U) ? 0x00U : LCD_ROW1_OFFSET) + col);
    return send_cmd((uint8_t)(LCD_CMD_SET_DDRAM | addr));
}

HAL_StatusTypeDef lcd_print(const char *str)
{
    HAL_StatusTypeDef st = HAL_OK;
    uint32_t n = 0U;
    /* 한 줄(16칸)을 넘기지 않도록 최대 16글자까지만 */
    while ((str[n] != '\0') && (n < LCD_COLS) && (st == HAL_OK))
    {
        st = send_data((uint8_t)str[n]);
        n++;
    }
    return st;
}

/* 한 줄 전체(16칸)를 덮어쓴다. 글자가 16칸보다 짧으면 나머지를 공백으로 채운다.
 * 이유: 주기 갱신에서 lcd_clear()를 쓰면 매번 2ms를 기다리고 화면이 깜빡인다.
 *       대신 같은 자리에 덮어쓰고, 이전 글자가 남지 않게 공백으로 지운다 */
HAL_StatusTypeDef lcd_print_line(uint8_t row, const char *str)
{
    HAL_StatusTypeDef st = lcd_set_cursor(row, 0U);
    uint8_t ended = 0U;                       /* 문자열 끝('\0')을 만났는지 */
    for (uint32_t n = 0U; (n < LCD_COLS) && (st == HAL_OK); n++)
    {
        if ((ended == 0U) && (str[n] == '\0'))
        {
            ended = 1U;                       /* 이후로는 str을 더 읽지 않는다 */
        }
        st = send_data((ended != 0U) ? (uint8_t)' ' : (uint8_t)str[n]);
    }
    return st;
}