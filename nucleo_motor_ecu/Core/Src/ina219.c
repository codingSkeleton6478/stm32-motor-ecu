#include "ina219.h"

/* ---- INA219 레지스터 주소 (TI INA219 데이터시트 레지스터 맵) ---- */
#define INA219_REG_CONFIG   (0x00U)  /* 설정 레지스터. 전원 켜질 때 기본값 0x399F */
#define INA219_REG_SHUNT    (0x01U)  /* 션트 전압. 부호 있는 16비트, 1 LSB = 10 uV */
#define INA219_REG_BUS      (0x02U)  /* 버스 전압. bit15..3이 값, 1 LSB = 4 mV */

#define INA219_BUS_SHIFT    (3U)     /* 버스 전압 하위 3비트(상태 비트)를 버리기 위한 시프트 */
#define INA219_BUS_LSB_MV   (4U)     /* 버스 전압 1 LSB = 4 mV */
#define INA219_TIMEOUT_MS   (5U)     /* I2C 한 번 읽기 제한 시간. 100kHz에서 4바이트는 1ms 안에 끝남 */
#define INA219_FAIL_LIMIT   (3U)     /* 연속 3번(= 60ms) 실패하면 I2C를 리셋 */

static I2C_HandleTypeDef *s_hi2c = NULL;   /* main.c의 hi2c1을 가리킴 (init에서 받음) */

/* 진단용 상태. static이라 이 파일 밖에서는 아래 get 함수로만 읽을 수 있다 */
static uint32_t s_err_count = 0U;      /* 누적 실패 횟수 */
static uint32_t s_recover_count = 0U;  /* I2C 리셋한 횟수 */
static uint32_t s_consec_fail = 0U;    /* 연속 실패 횟수 (성공하면 0으로) */
static uint8_t  s_valid = 0U;          /* 1 = 지금 값 믿을 수 있음, 0 = 실패 중 */

/* I2C 주변장치를 소프트웨어 리셋 후 다시 초기화한다.
 * 노이즈나 선 빠짐으로 BUSY가 걸려 풀리지 않을 때 쓴다.
 * SWRST(CR1 bit15)는 I2C 클럭이 켜져 있을 때만 먹으므로 DeInit(클럭 끔)보다 먼저 한다. 근거: RM0008 I2C_CR1 */
static void i2c_recover(void)
{
    s_hi2c->Instance->CR1 |= I2C_CR1_SWRST;    /* 1을 쓰면 I2C 내부 상태(BUSY 포함)가 리셋 상태로 */
    s_hi2c->Instance->CR1 &= ~I2C_CR1_SWRST;   /* 0으로 되돌려야 다시 동작 가능 */
    (void)HAL_I2C_DeInit(s_hi2c);              /* HAL 상태와 핀 설정을 초기 상태로 */
    (void)HAL_I2C_Init(s_hi2c);                /* CubeMX 설정(100kHz 등)으로 다시 초기화 */
    s_recover_count++;
}

/* 16비트 레지스터 하나 읽기.
 * Mem_Read는 [주소+W][레지스터 번호][재시작][주소+R][데이터 2바이트] 순서를 한 번에 처리한다.
 * INA219는 MSB를 먼저 보낸다(빅엔디언) → buf[0]이 상위 바이트 */
static HAL_StatusTypeDef read_reg(uint8_t reg, uint16_t *out)
{
    uint8_t buf[2] = {0U, 0U};
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(s_hi2c,
                                            (uint16_t)(INA219_ADDR_7BIT << 1), /* HAL은 8비트 형식 주소 */
                                            reg, I2C_MEMADD_SIZE_8BIT,         /* 레지스터 번호는 1바이트 */
                                            buf, 2U, INA219_TIMEOUT_MS);
    if (st == HAL_OK)
    {
        *out = (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);  /* 상위<<8 | 하위 */
    }
    return st;   /* 실패하면 *out은 건드리지 않는다 */
}

HAL_StatusTypeDef ina219_init(I2C_HandleTypeDef *hi2c, uint16_t *config_out)
{
    s_hi2c = hi2c;
    /* 설정은 전원 켜질 때 기본값(±320mV 범위, 12비트, 연속 측정)을 그대로 쓴다.
     * 우리 전류(최대 수백 mA × 0.1Ω = 수십 mV)는 이 범위에 충분히 들어간다.
     * 여기서는 읽어서 통신이 되는지만 확인한다 */
    return read_reg(INA219_REG_CONFIG, config_out);
}

HAL_StatusTypeDef ina219_read(int16_t *current_ma_x10, uint16_t *bus_mv)
{
    uint16_t raw_shunt = 0U;
    uint16_t raw_bus = 0U;
    HAL_StatusTypeDef st = read_reg(INA219_REG_SHUNT, &raw_shunt);
    if (st == HAL_OK)
    {
        st = read_reg(INA219_REG_BUS, &raw_bus);
    }

    if (st == HAL_OK)
    {
        /* 옴의 법칙: I = V / R = (raw × 10 uV) / 0.1 Ω = raw × 0.1 mA
         * → raw 값이 그대로 0.1 mA 단위 전류. 곱셈·나눗셈이 필요 없다.
         * 캘리브레이션 레지스터를 안 쓰는 이유: 션트 전압만 읽어 직접 계산하면 충분하고 설정 실수 위험이 없다 */
        *current_ma_x10 = (int16_t)raw_shunt;   /* 레지스터가 2의 보수라 int16_t로 바꾸면 음수(역방향)도 표현 */
        *bus_mv = (uint16_t)((raw_bus >> INA219_BUS_SHIFT) * INA219_BUS_LSB_MV);

        s_consec_fail = 0U;     /* 성공 → 연속 실패 초기화 */
        s_valid = 1U;
    }
    else
    {
        s_err_count++;
        s_consec_fail++;
        s_valid = 0U;           /* 실패 중에는 *current_ma_x10에 남은 옛 값을 믿지 않는다 */
        if (s_consec_fail >= INA219_FAIL_LIMIT)
        {
            i2c_recover();      /* 연속 실패 = 일시적 오류가 아니라 버스가 멈췄을 가능성 → 리셋 */
            s_consec_fail = 0U;
        }
    }
    return st;
}

/* 바깥(main, 나중에 fault 모듈)에서 상태를 읽는 함수. 변수를 직접 노출하지 않는다 */
uint32_t ina219_get_err_count(void)     { return s_err_count; }
uint32_t ina219_get_recover_count(void) { return s_recover_count; }
uint8_t  ina219_is_valid(void)          { return s_valid; }