# STM32 10일 실습 가이드: 모터 제어 ECU 미니 프로젝트

> **10일 뒤 결과물**
> STM32가 모터 속도를 PI로 제어하고, CAN으로 명령과 상태를 주고받으며, 통신 끊김·과전류·엔코더 고장이 나면 모터를 멈추는(안전 상태) 미니 ECU를 만들어 GitHub에 올린다.
>
> **기준**
> - 하루 2~3시간
> - NUCLEO-F103RB = 모터 ECU(메인)
> - STM32F103 소형 보드 = 명령 ECU(CAN 상대)

---

## 0. 시작 전 체크 (Day 1 전 10분)

### 받은 부품과 용도

| 부품 | 수량 | 용도 | 쓰는 날 |
|---|---|---|---|
| NUCLEO-F103RB | 1 | 메인 보드(모터 ECU) | 전체 |
| STM32F103 소형 개발보드 (블루필 계열) | 1 | CAN 상대 노드(명령 ECU) | Day 8~10 |
| 엔코더 TT 모터 FIT0450 (6V, 160RPM, 120:1) | 1 | 속도 제어 대상 | Day 4~ |
| TB6612FNG 듀얼 모터 드라이버 | 1 | 모터 구동 | Day 4~ |
| 포텐셔미터 모듈 | 1 | 목표 속도 입력(ADC). **배송 대기 중 → `docs/pot_substitute.md`** | Day 3~ |
| 1602 I2C 캐릭터 LCD | 1 | 상태 표시 | Day 7~ |
| INA219 전류 센서 | 1 | 모터 전류 측정, 과전류 감지 | Day 7~ |
| SN65HVD230 CAN 트랜시버 | 2 | 보드 2개를 CAN으로 연결 | Day 9~ |
| USB 로직 분석기 24MHz 8ch | 1 | 신호 확인 (거의 매일) | Day 1~ |
| 브레드보드 830핀 | 1 | 배선 | 전체 |
| 점퍼 M/M 20cm, M/F | — | 배선 | 전체 |
| AA 4구 배터리 홀더(6V) + AA 건전지 4개 | 1 | 모터 전원 | Day 4~ |

### 꼭 확인할 4가지

1. **핀 헤더 납땜이 됐는지 확인한다.**
   - TB6612, INA219, CAN 모듈, 소형 보드는 핀 헤더가 따로 들어 있는 경우가 많다. 납땜이 안 돼 있으면 브레드보드에 꽂을 수 없다.
   - 인두가 없으면 학교 실습실이나 메이커스페이스를 **Day 4 전에** 예약해 둔다. Day 1~3은 납땜 없이 할 수 있다.
2. **AA 건전지 4개를 준비한다.** 모터를 USB 전원으로 돌리지 않는다.
3. **M/F 점퍼가 있는지 확인한다.**
   - 보드 양옆의 모포(Morpho) 헤더는 수(♂) 핀이라 M/F 점퍼가 있어야 연결된다.
   - 엔코더 B상(PB7), CAN(PA11·PA12), 소형 보드 SWD 배선에 필요하다.
4. **소형 보드 칩 각인을 확인한다.**
   - STM32F103C8T6이면 정상이다.
   - CKS32나 CS32 같은 호환 칩이면 CubeIDE가 연결을 거부할 수 있다. 대응은 Day 8에 있다.

### 미리 설치할 것

- **STM32CubeIDE**
- **PulseView (sigrok)**: 로직 분석기 프로그램. Windows에서는 설치 후 Zadig로 로직 분석기 드라이버를 WinUSB로 바꿔야 인식된다.
- **시리얼 터미널**: CubeIDE 내장 터미널이나 Tera Term
- **Git과 GitHub 저장소**: 저장소 이름 예: `stm32-motor-ecu`

### 전원·안전 규칙 (매일)

- **GND는 전부 하나로 묶는다.** 보드, 배터리 −, TB6612, 센서, 로직 분석기 모두 해당된다.
- **5V 신호는 FT(5V tolerant) 핀에만 넣는다.**
  - 이 계획에서 5V 신호가 들어가는 곳은 엔코더(PB6·PB7)와 I2C(PB8·PB9)뿐이다.
  - ADC 핀 PA0과 포텐셔미터에는 절대 5V를 넣지 않는다.
- **모터 배터리(6V)는 TB6612의 VM에만 연결한다.** 보드의 5V·3V3 핀에 연결하면 안 된다.
- **배선을 바꾸기 전에 USB와 배터리를 먼저 뺀다.**
- **모터 첫 구동은 바퀴를 띄운 상태에서 듀티 30%부터** 시작한다.

### 공통 코딩 규칙

- 모듈을 파일로 나눈다: `motor.c`, `encoder.c`, `pi.c`, `cmd.c`(UART 명령), `ina219.c`, `lcd.c`, `can_if.c`, `fault.c`
- 동적 메모리(malloc)를 쓰지 않는다. 정수는 `uint16_t`처럼 고정폭 타입을 쓴다.
- ISR은 짧게 쓴다. 블로킹 통신(I2C, UART printf)은 main loop에서만 한다.
- ISR과 main이 함께 쓰는 변수는 `volatile`로 선언한다.
- 매일 끝나면 커밋하고 태그(`day01`~`day10`)를 단다.

---

## 1. 최종 핀 맵 (NUCLEO-F103RB)

| 기능 | MCU 핀 | 보드 위치 | 연결 대상 | 시작일 |
|---|---|---|---|---|
| LED LD2 | PA5 | 내장 (D13) | — | Day 1 |
| 버튼 B1 | PC13 | 내장 | — | Day 1 |
| UART2 TX/RX | PA2/PA3 | 내장 (ST-LINK 가상 COM) | PC | Day 1 |
| 디버그 타이밍 핀 | PB0 | A3 | 로직 분석기 | Day 2 |
| 포텐셔미터 | PA0 (ADC1_IN0) | A0 | 포텐셔미터 OUT | Day 3 |
| 모터 PWM | PA6 (TIM3_CH1) | D12 | TB6612 PWMA | Day 4 |
| 모터 방향 1 | PA8 | D7 | TB6612 AIN1 | Day 4 |
| 모터 방향 2 | PA9 | D8 | TB6612 AIN2 | Day 4 |
| 드라이버 enable | PB5 | D4 | TB6612 STBY | Day 4 |
| 엔코더 A | PB6 (TIM4_CH1, FT) | D10 | 엔코더 A상 | Day 5 |
| 엔코더 B | PB7 (TIM4_CH2, FT) | CN7-21 (모포) | 엔코더 B상 | Day 5 |
| I2C SCL | PB8 (I2C1 리맵, FT) | D15 | LCD·INA219 SCL | Day 7 |
| I2C SDA | PB9 (I2C1 리맵, FT) | D14 | LCD·INA219 SDA | Day 7 |
| CAN RX | PA11 | CN10-14 (모포) | SN65HVD230 CRX | Day 8 |
| CAN TX | PA12 | CN10-12 (모포) | SN65HVD230 CTX | Day 8 |

### 전원 연결

| 모듈 | 전원 | 이유 |
|---|---|---|
| TB6612 VM | 배터리 + (6V). Day 7부터는 INA219를 거친다 | 모터 구동 전원 |
| TB6612 VCC | 3V3 | 로직 전원. MCU의 3.3V 신호를 그대로 받는다 |
| 엔코더 VCC | **5V** | 사양이 4.5~7.5V라서 3.3V로는 동작하지 않는다. 출력도 5V라 FT 핀(PB6·PB7)에 연결한다 |
| LCD VCC | 5V | LCD 구동 전원 |
| INA219 VCC | 3V3 | — |
| SN65HVD230 | 3V3 | 3.3V용 트랜시버 |
| 포텐셔미터 VCC | **3V3** | 5V를 넣으면 ADC 핀이 손상될 수 있다 |

> 모포 헤더 CN7·CN10의 핀 위치는 UM1724(아래 참고 자료)의 핀 배치 그림에서 확인한다.

---

## 2. 10일 일정 한눈에

| Day | 주제 | 결과물 | 우선순위 |
|---|---|---|---|
| 1 | 개발환경, GPIO, UART, 로직 분석기 | LED·버튼·printf, 첫 캡처 | 필수 |
| 2 | 타이머 인터럽트, 레지스터 직접 제어 | 1ms 스케줄러, ISR 시간 측정 | 필수 |
| 3 | ADC + DMA + 필터 | 포텐셔미터 0~100% | 필수 |
| 4 | PWM + TB6612 | 모터 개루프 구동, 최소 듀티 측정 | 필수 |
| 5 | 엔코더 모드 | CPR 측정, 듀티-RPM 표 | 필수 |
| 6 | PI 속도 제어 | 스텝 응답 그래프와 수치 | **핵심** |
| 7 | I2C: INA219 + LCD | 전류 측정, 상태 표시 | INA219 필수, LCD 선택 |
| 8 | CAN 루프백, 소형 보드 준비 | CAN 프레임 캡처 | 필수 |
| 9 | CAN 2노드, E2E 보호, 타임아웃 | 원격 속도 명령 | **핵심** |
| 10 | 고장 주입 테스트, 문서화 | 테스트 결과표, README | **핵심** |

> **일정이 밀리면:** LCD(Day 7)와 고장 주입 FI-06·FI-07(Day 10)을 뒤로 미룬다. 모터·엔코더·PI·CAN·고장 주입은 끝까지 지킨다. 한솔 전장 직무와 가장 가까운 부분이 이것이다.

---

## Day 1. 개발환경과 첫 신호 (납땜 불필요)

**목표:** LED·버튼·UART printf가 동작하고, 로직 분석기로 신호를 직접 본다.

1. CubeIDE에서 **New STM32 Project** → **Board Selector** → `NUCLEO-F103RB` 선택. "Initialize all peripherals with their default mode"에 Yes를 누른다.
   - 그러면 LD2(PA5), B1(PC13), USART2(115200)가 자동 설정된다.
2. **Clock Configuration** 탭에서 SYSCLK, APB1, APB1 Timer clocks, APB2 값을 적어 둔다.
   - 기본값은 64 / 32 / 64 / 64MHz로 예상된다.
   - 아래 계산은 모두 이 값 기준이다. 값이 다르면 숫자를 다시 계산한다.
3. LED를 깜빡인다: `HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); HAL_Delay(500);`
4. printf를 연결한다. `main.c`에 아래를 추가하고, 1초마다 `tick n`을 출력한다.
   ```c
   int __io_putchar(int ch) {
       HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
       return ch;
   }
   ```
5. 버튼을 붙인다.
   - PC13을 EXTI(하강 에지)로 설정한다. B1은 누르면 Low가 된다.
   - `HAL_GPIO_EXTI_Callback`에서는 플래그만 세운다.
   - main에서 플래그를 보고 깜빡임 주기를 500ms ↔ 100ms로 바꾼다.
6. 로직 분석기로 본다.
   - CH0 → PA5. D13 소켓에 M/M 점퍼를 꽂아 연결한다.
   - CH1 → PA2 (CN10-35 모포 핀).
   - PulseView 샘플레이트 1MHz 이상, UART 디코더 115200 8N1로 설정한다.

**완료 기준**
- [ ] 로직 분석기로 잰 LED 주기가 500ms ±1% 안에 든다
- [ ] UART 디코더에 `tick` 문자열이 보인다
- [ ] 버튼으로 깜빡임 모드가 바뀐다
- [ ] GitHub에 `nucleo_motor_ecu` 프로젝트를 올리고 `day01` 태그를 단다

**막히면**
- 보드가 인식되지 않으면 CubeIDE의 Help → ST-LINK 업그레이드를 실행한다.
- printf에서 float가 안 나오면 정수로 출력하거나 프로젝트 설정의 "Use float with printf"를 켠다.
- D1 핀에서는 UART가 안 보인다. Nucleo는 기본 설정에서 PA2/PA3가 ST-LINK로만 연결되어 있으므로 CN10-35에서 잰다.

---

## Day 2. 타이머 인터럽트와 레지스터 직접 제어

**목표:** HAL_Delay 없이 타이머로 주기를 만들고, HAL이 내부에서 하는 일을 레지스터 수준에서 확인한다.

1. **TIM2를 1kHz 인터럽트로 설정한다.** PSC 63, ARR 999로 하면 64MHz / 64 / 1000 = 1kHz가 된다.
2. **간단한 스케줄러를 만든다.**
   - `HAL_TIM_PeriodElapsedCallback`에서 ms 카운터를 올린다.
   - 10ms·100ms·1000ms마다 플래그를 세우고, main loop가 플래그를 보고 작업을 실행한다.
   - 실차 ECU의 주기 태스크 구조와 같은 모양이다.
3. **PB0(A3)을 디버그 타이밍 핀으로 쓴다.**
   - 10ms 작업을 시작할 때 High, 끝날 때 Low로 만든다.
   - 로직 분석기로 주기, 지터, 실행 시간을 잰다. 이 핀은 Day 10까지 계속 쓴다.
4. **LD2를 HAL 없이 레지스터로 깜빡인다.** RM0008의 7장(RCC)과 9장(GPIO)을 보면서 작성한다.
   ```c
   RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;      // GPIOA 클럭 켜기
   GPIOA->CRL &= ~(0xFU << 20);             // PA5 설정 비트(23:20) 지우기
   GPIOA->CRL |=  (0x2U << 20);             // MODE=10(2MHz 출력), CNF=00(push-pull)
   GPIOA->BSRR = (1U << 5);                 // 켜기
   GPIOA->BRR  = (1U << 5);                 // 끄기
   ```
5. **HAL 소스와 비교한다.** `HAL_GPIO_TogglePin` 소스를 열어 4번의 레지스터 코드와 같은 레지스터를 쓰는지 확인한다.

**완료 기준**
- [ ] PB0으로 잰 10ms 주기가 ±1% 안에 든다
- [ ] 10ms 작업의 실행 시간을 측정해 기록했다
- [ ] 레지스터 버전 LED가 동작한다

**면접 포인트:** "F1 시리즈는 CRL/CRH 레지스터로 핀 모드를 정합니다. BSRR은 set/reset을 원자적으로 해서 read-modify-write 경쟁이 생기지 않습니다."

---

## Day 3. ADC + 포텐셔미터

> **가변저항이 아직 없으면** `docs/pot_substitute.md`의 **S-3**(내장 온도센서 + VREFINT, 3채널 스캔)으로 진행한다. PA0까지 3채널로 설정해 두면 가변저항이 왔을 때 설정을 바꿀 필요가 없다. 같은 날 **S-1 UART 명령**도 만들어 둔다. Day 4부터 쓴다.

**배선:** 포텐셔미터 VCC → **3V3**, GND → GND, OUT → PA0 (A0)

1. ADC1 IN0을 Continuous 모드와 DMA Circular(Half Word)로 설정한다. Sampling time은 55.5 cycles로 한다.
   - ADC 클럭은 14MHz 이하여야 한다. CubeMX가 64MHz / 6 = 10.67MHz로 맞춰 주는지 확인한다.
2. 시작 순서: `HAL_ADCEx_Calibration_Start(&hadc1)`를 먼저 호출하고(F1 전용), 그다음 `HAL_ADC_Start_DMA()`를 호출한다.
3. 10ms 작업에서 16샘플 이동평균을 낸다. 0~4095 값을 0~100%로 바꿔 100ms마다 출력한다.
4. 노이즈를 잰다. 손대지 않은 상태에서 raw 값의 (최댓값 − 최솟값)을 필터 전과 후로 각각 기록한다.

**완료 기준**
- [ ] 끝에서 끝까지 돌렸을 때 0~4095 근처까지 나온다
- [ ] 필터 전후 흔들림 수치를 기록했다

**막히면:** 값이 4095에 고정되면 포텐셔미터 VCC를 5V에 연결했는지 확인하고, 그렇다면 즉시 뺀다. OUT 핀 위치도 확인한다.

---

## Day 4. PWM + TB6612로 모터 돌리기 (납땜 필요)

**배선**

| TB6612 핀 | 연결 |
|---|---|
| VM | 배터리 + (6V) |
| VCC | 3V3 |
| GND (전부) | 공통 GND (배터리 −, 보드 GND) |
| PWMA | PA6 (D12) |
| AIN1 | PA8 (D7) |
| AIN2 | PA9 (D8) |
| STBY | PB5 (D4) |
| AO1 / AO2 | 모터 + / − |
| PWMB, BIN1, BIN2 | 비워 둔다 (내부 풀다운) |

1. **TIM3 CH1을 PWM으로 설정한다.**
   - PSC 0, ARR 3199로 하면 64MHz / 3200 = 20kHz가 된다. 사람 귀에 들리지 않는 주파수다.
   - 듀티는 CCR = 듀티 × 3200으로 정한다.
2. **`motor.c`를 작성한다.**
   - `motor_set(int16_t duty_permille)`: 부호로 방향을 정하고, 0이면 정지한다.
   - `motor_enable(bool)`: STBY 핀을 제어한다.
3. TB6612 진리표:

   | AIN1 | AIN2 | 동작 |
   |---|---|---|
   | H | L | 정회전 |
   | L | H | 역회전 |
   | H | H | 단락 브레이크 |
   | L | L | 정지 (프리런) |

   STBY가 Low이면 입력과 관계없이 대기 상태다.
4. **포텐셔미터로 속도를 조절한다(개루프).** 가변저항이 없으면 UART `d300`·`d-300` 명령(S-1)이나 버튼 단계 입력(S-2)을 쓴다. 버튼은 방향 전환에 쓴다. 전환할 때는 듀티를 0으로 내린 뒤 반대로 돌린다. 급반전은 하지 않는다.
5. **최소 듀티(데드밴드)를 잰다.** 모터가 처음 돌기 시작하는 듀티를 기록한다.

**로직 분석기:** PA6에서 PWM 주파수가 20kHz인지, 듀티 비율이 맞는지 확인한다.

**완료 기준**
- [ ] 포텐셔미터로 속도가 조절되고, 버튼으로 방향이 바뀐다
- [ ] 최소 듀티 값을 기록했다

**막히면**
- 모터가 안 돌면 STBY가 High인지, VM에 배터리가 연결됐는지, GND가 공통인지 확인한다.
- 보드가 리셋되면 모터 전원을 보드의 5V에서 끌어왔는지 확인하고 분리한다.

---

## Day 5. 엔코더(TIM4 엔코더 모드)와 RPM 측정

**배선:** 엔코더 VCC → **5V**, GND → GND, A상 → PB6 (D10), B상 → PB7 (CN7-21, M/F 점퍼)

> FIT0450 핀 순서는 1 모터+, 2 모터−, 3 A상, 4 B상, 5 GND, 6 VCC다. 다만 DFRobot 위키에 "순서가 실제와 다를 수 있다"는 안내가 있으니 모터 기판의 실크 인쇄로 확인한다.

1. **TIM4를 엔코더 모드로 설정한다.**
   - Combined Channels = Encoder Mode
   - Encoder Mode TI1 and TI2 (4체배)
   - Counter Period 65535
   - Input Filter 6~10 (노이즈 대비)
   - 설정 후 `HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL)`을 호출한다.
2. **한 바퀴당 카운트(CPR)를 잰다.**
   - 손으로 바퀴를 정확히 10바퀴 돌리고 카운트를 10으로 나눈다.
   - 사양은 출력축 1바퀴당 960펄스(A상)다. 4체배라면 3840이 나와야 한다.
3. **10ms 작업에서 RPM을 계산한다.**
   ```c
   int16_t delta = (int16_t)(cnt - prev);   // 오버플로를 int16 캐스팅으로 처리
   // RPM = delta * 60 / (CPR * 0.01)  →  CPR 3840이면 RPM = delta * 1.5625
   ```
4. **방향 부호를 맞춘다.** 듀티가 +인데 RPM이 −로 나오면 A·B 선을 바꾸거나 TIM4 입력 극성을 반전한다.
5. **듀티-RPM 표를 만든다.** 듀티를 10단계로 바꾸며 정상상태 RPM을 적는다. Day 6 튜닝의 기준이 된다.

**로직 분석기:** A상과 B상의 90° 위상차를 보고, 회전 방향이 바뀌면 앞서는 상도 바뀌는지 확인한다.

**완료 기준**
- [ ] CPR 측정값을 사양(3840)과 비교했다
- [ ] 듀티-RPM 표를 만들었다
- [ ] 6V 무부하 최고 RPM을 사양(160RPM)과 비교했다

**면접 포인트:** "엔코더를 TIM3(PA6/PA7)이 아니라 TIM4(PB6/PB7)에 연결했습니다. 엔코더 전원이 4.5V 이상이라 출력이 5V인데, PA6/PA7은 5V tolerant 핀이 아니기 때문입니다."

---

## Day 6. PI 속도 제어 (핵심)

1. **`pi.c`를 작성한다.**
   - 구조체: `{kp, ki, integ, out_min, out_max}`
   - 함수: `pi_step(target, measured, dt)`
   - Cortex-M3에는 FPU가 없어 float를 소프트웨어로 계산한다. 100Hz에서는 문제없지만 면접 대비로 알아 둔다.
2. **Anti-windup을 넣는다.** 출력이 포화되면 적분을 멈춘다(조건부 적분).
3. **제어 루프는 TIM2 ISR 안에서 10ms마다 실행한다.** 엔코더 읽기 → PI 계산 → PWM 갱신 순서다.
   - main loop에 두면 Day 7의 LCD 갱신(I2C 블로킹)이 제어 주기를 밀어낸다.
   - PB0으로 실행 시간을 잰다.
4. **목표 RPM은 포텐셔미터로 0~120RPM 범위에서 정한다.** 배터리 전압이 떨어지므로 최고 RPM(160)보다 여유를 둔다. 가변저항이 없으면 UART `r80` 명령(S-1)을 쓴다. 스텝 응답 시험은 가변저항이 있어도 UART로 하는 것이 재현성이 좋다.
5. **튜닝 순서**
   1. Ki = 0으로 두고 Kp를 올려 진동이 시작되는 값을 찾는다.
   2. Kp를 그 값의 절반으로 둔다.
   3. Ki를 조금씩 올려 정상상태 오차를 없앤다.
6. **로그를 남긴다.**
   - main loop에서 10ms마다 `t,target,rpm,duty` CSV를 출력한다.
   - 터미널 로그를 저장해 Python이나 엑셀로 그래프를 그린다.
7. **스텝 응답을 잰다.** 목표를 0 → 80RPM으로 바꾸고 상승시간, 오버슈트, 정상상태 오차를 표로 정리한다. 손가락으로 살짝 부하를 걸었을 때 속도가 복원되는지도 확인한다.

**완료 기준**
- [ ] 스텝 응답 그래프 1장과 수치 표가 있다
- [ ] 부하를 걸었을 때 목표 속도로 복원된다

**막히면**
- 진동하면 Kp를 낮춘다.
- 목표 속도에 도달하지 못하면 배터리 전압을 확인하고 목표를 낮춘다.
- 저속에서 덜컹거리면 데드밴드 보상(최소 듀티 더하기)을 넣는다. 선택 사항이다.

---

## Day 7. I2C: INA219 전류 센서 + LCD

**배선**
- **I2C1**
  - PB8 (D15) = SCL, PB9 (D14) = SDA, 100kHz
  - CubeMX에서 PB8·PB9 핀을 직접 클릭해 I2C1_SCL·SDA로 지정한다. 기본 핀 PB6/PB7은 엔코더가 쓰고 있다.
- **LCD:** VCC 5V, GND, SDA, SCL
  - LCD 백팩의 풀업이 버스를 5V로 끌어올리기 때문에 FT 핀(PB8·PB9)을 쓴다.
- **INA219:** VCC 3V3, GND, SDA, SCL
- **전류 경로:** 배터리 + → INA219 VIN+ → VIN− → TB6612 VM

1. **I2C 스캔을 한다.**
   - 주소 1~127에 `HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10)`을 호출해 응답하는 주소를 출력한다.
   - 보통 LCD는 0x27 또는 0x3F, INA219는 0x40이다.
2. **INA219를 읽는다.** 레지스터는 16비트 빅엔디언(MSB 먼저)이다. 캘리브레이션 레지스터는 쓰지 않아도 된다.
   - 0x01(Shunt Voltage, LSB 10µV)을 읽는다. 션트 저항이 0.1Ω이면(모듈 저항에 `R100` 각인) 전류(mA) = raw × 0.1이다.
   - 0x02(Bus Voltage)를 읽는다. (raw >> 3) × 4mV가 배터리 전압이다.
   - 읽기는 main loop에서 20ms마다 한다.
3. **LCD 드라이버를 직접 작성한다(PCF8574, 4비트 모드).**
   - 보통 비트 매핑은 P0=RS, P1=RW, P2=EN, P3=백라이트, P4~P7=D4~D7이다.
   - 초기화 순서: 50ms 대기 → 0x3 세 번 → 0x2(4비트 전환) → 0x28 → 0x08 → 0x01(2ms 대기) → 0x06 → 0x0C
   - EN을 High → Low로 펄스하면 데이터가 래치된다.
4. **LCD에 상태를 표시한다.** main loop에서 200ms마다 갱신한다.
   - 1행: `T:080 R:079`
   - 2행: `I:0123mA V:5.9`
5. **전류를 측정해 기록한다.**
   - 무부하 전류와, 손으로 살짝 부하를 걸었을 때의 전류를 적는다. Day 10 과전류 임계값의 근거가 된다.
   - 사양상 정지 전류는 최대 2.8A이고, TB6612는 채널당 연속 1.2A까지다. 바퀴를 손으로 완전히 세우는 것은 1~2초 안으로 끝낸다.

**로직 분석기:** SCL과 SDA를 연결하고 I2C 디코더(샘플레이트 4MHz)로 주소와 ACK/NACK를 본다.

**선택 실험:** 제어 루프를 main loop로 옮긴 뒤 LCD 갱신 중 PB0 지터를 재고, ISR에서 실행할 때와 비교한다. 제어를 ISR에서 돌리는 이유를 데이터로 보여 줄 수 있다.

**막히면**
- LCD에 네모만 보이면 백팩 뒷면의 파란 가변저항(명암)을 돌린다.
- 스캔 결과가 0개면 SDA와 SCL이 바뀌었는지 확인한다.
- HAL_BUSY가 계속 나오면 먼저 전원을 껐다 켠다. 그래도 안 되면 ES096 2.8.7(I2C BUSY 플래그 고착) 대응법을 적용한다. SCL·SDA를 GPIO로 토글한 뒤 I2C를 SWRST로 리셋하는 방법이다.

---

## Day 8. CAN ①: 루프백 + 소형 보드 준비

### A. Nucleo 단독 루프백 (트랜시버 없이)

1. **CAN을 켜고 비트 타이밍을 맞춘다.**
   - 핀: PA11 = RX, PA12 = TX
   - Mode = **Loopback** (Silent Loopback이 아니다. Silent에서는 TX 핀이 계속 High로 고정돼 로직 분석기로 볼 수 없다)
   - 500kbps 설정 (APB1 32MHz 기준): Prescaler 4, Time Quanta in Bit Segment 1 = 13, Bit Segment 2 = 2, SJW = 1
   - 이렇게 하면 16tq, 샘플 포인트 87.5%가 된다. CubeMX의 Baud Rate 표시가 500000인지 확인한다.
2. **필터를 반드시 설정한다.** 안 하면 아무것도 수신되지 않는다.
   - FilterBank 0, IdMask 모드, 32bit, ID = 0, Mask = 0 (전부 수신), FIFO0, Activation = Enable
3. **송수신 코드를 작성한다.**
   - `HAL_CAN_Start()`를 호출한 뒤 `HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING)`을 호출한다.
   - 100ms마다 ID 0x123, DLC 8로 송신한다. byte0에는 카운터를 넣는다.
   - `HAL_CAN_RxFifo0MsgPendingCallback`에서 `HAL_CAN_GetRxMessage`로 받아 카운터를 출력하고 LED를 토글한다.
4. **로직 분석기로 본다.** PA12(CAN_TX)에 CAN 디코더를 건다. 500kbps, 샘플레이트 8MHz 이상으로 설정하고 ID·DLC·데이터·CRC 필드를 확인한다.

### B. 소형 보드(블루필) 준비

1. **Nucleo의 ST-LINK로 소형 보드를 프로그래밍한다.**
   - Nucleo의 **CN2 점퍼 2개를 뺀다.**
   - CN4(SWD) 핀을 연결한다:

     | CN4 핀 | 신호 | 소형 보드 |
     |---|---|---|
     | 1 | VDD_TARGET (3.3V 감지용) | 3.3 |
     | 2 | SWCLK | SWCLK (DCLK) |
     | 3 | GND | GND |
     | 4 | SWDIO | SWDIO (DIO) |

   - Nucleo에 다시 프로그램을 올릴 때는 CN2 점퍼를 다시 꽂는다.
2. **소형 보드 전원은 하나만 쓴다.** 자체 USB와 Nucleo 3V3 중 하나만 연결한다. 둘 다 연결하면 안 된다.
3. **새 프로젝트를 만든다.**
   - MCU Selector에서 `STM32F103C8Tx`를 고른다.
   - **SYS → Debug: Serial Wire**로 설정한다. 이걸 빼먹으면 두 번째 업로드부터 연결이 막힌다.
   - RCC HSE = Crystal, 72MHz
   - PC13 LED를 깜빡인다. 이 LED는 Low에서 켜진다.
4. **연결이 거부되면**
   - BOOT0 점퍼를 1로 놓고 Connect Under Reset으로 시도한다.
   - 그래도 안 되면 칩 각인을 확인한다. 호환 칩이라면 STM32CubeProgrammer로 시도해 본다.
   - 끝까지 안 되면 **Plan B**로 간다. Day 9 로직(카운터·체크섬·타임아웃)을 Nucleo 루프백 모드에서 스스로 보내고 받는 방식으로 검증한다.

**완료 기준**
- [ ] 루프백으로 보낸 카운터와 받은 카운터가 일치한다
- [ ] CAN 프레임 디코딩 캡처가 있다
- [ ] 소형 보드 LED가 깜빡인다

---

## Day 9. CAN ②: 2노드 통신 + E2E 보호 + 타임아웃 (핵심)

**배선**
- **각 보드와 자기 SN65HVD230 연결:** 트랜시버 3V3, GND를 연결하고 MCU CAN_TX → CTX, CRX → MCU CAN_RX로 연결한다.
  - Nucleo: PA12 → CTX, CRX → PA11
  - 소형 보드: PB9 → CTX, CRX → PB8. CubeMX에서 CAN 핀을 PB8/PB9로 리맵한다. PA11/PA12는 USB 핀이라 피한다.
- **두 트랜시버 연결:** CANH ↔ CANH, CANL ↔ CANL (두 선을 꼬아서). GND도 공통으로 묶는다.
- **종단저항:** 모듈마다 120Ω이 달려 있는지 확인한다. 멀티미터가 있으면 전원을 끈 상태에서 CANH–CANL 사이를 잰다. 약 60Ω이면 정상이다.
- **포텐셔미터는 소형 보드 PA0으로 옮긴다**(VCC 3.3V). 이제 소형 보드가 명령 ECU다. 가변저항이 없으면 고정 테스트 시퀀스(S-4: 0→60→120→60RPM, 3초 간격)를 보낸다.
- **양쪽 모두 Mode를 Normal로 바꾼다.**
- **소형 보드 비트 타이밍:** 72MHz면 APB1이 36MHz다. Prescaler 4, BS1 15, BS2 2로 하면 18tq, 샘플 포인트 88.9%가 된다. Baud Rate가 500000인지 확인한다.

**메시지 설계**

| ID | 방향 | 주기 | 내용 |
|---|---|---|---|
| 0x100 CMD | 소형 보드 → Nucleo | 10ms | B0–1 목표 RPM (uint16, 리틀엔디언) · B2 alive counter (0~15 순환) · B3 체크섬 = (B0 ^ B1 ^ B2) ^ 0xFF |
| 0x200 STATUS | Nucleo → 소형 보드 | 20ms | B0–1 실제 RPM (int16) · B2–3 전류 mA (uint16) · B4 상태 (0 STOP, 1 RUN, 2 FAULT) · B5 고장 코드 비트 |

> 체크섬 끝의 `^ 0xFF`는 전부 0인 프레임이 통과하지 못하게 하려는 것이다.

**Nucleo(모터 ECU) 수신 로직**
- 체크섬이 틀린 프레임은 버린다.
- alive counter가 이전 값 +1이 아니면 오류로 센다.
- 유효 프레임을 받은 마지막 시각을 기록한다. **100ms(10주기) 넘게 없으면 TIMEOUT 고장**으로 판단하고, STBY를 Low로, 듀티를 0으로 만든다(안전 상태).
- 오류가 3회 연속이면 FAULT로 간다.
- **복귀 조건:** 유효 프레임이 다시 들어오고 목표 RPM이 0일 때만 RUN으로 돌아간다. 통신이 살아나자마자 갑자기 재가동하는 것을 막는다.

**소형 보드(명령 ECU)**
- 포텐셔미터 값(또는 S-4 시퀀스)을 목표 RPM으로 바꿔 10ms마다 0x100을 보낸다.
- 0x200을 받아 상태가 FAULT면 LED를 빠르게 깜빡인다.

**Nucleo LCD:** 목표 RPM, 실제 RPM, 전류, 상태를 표시한다.

**로직 분석기:** Nucleo 쪽 CRX에 CAN 디코더를 건다. 0x100과 0x200의 주기(10ms, 20ms)와 ACK 슬롯을 확인한다.

**완료 기준**
- [ ] 소형 보드 포텐셔미터를 돌리면 Nucleo 모터 속도가 따라간다
- [ ] CAN 선을 하나 뽑으면 모터가 멈춘다 (정확한 측정은 Day 10)

**막히면**
- `HAL_CAN_Start`에서 타임아웃이 나는 경우: CAN_RX가 recessive(High)여야 시작된다. 트랜시버 전원과 CRX 배선을 확인한다.
- 송신은 되는데 ACK 에러가 나는 경우: 두 보드의 비트레이트가 다르거나, 상대가 Normal 모드가 아니거나, CANH·CANL이 바뀌었을 수 있다.
- 수신 콜백이 오지 않는 경우: 필터 설정이나 알림 활성화를 빠뜨렸는지 확인한다.

---

## Day 10. 고장 주입 테스트 + 포트폴리오 정리 (핵심)

### 1. 워치독(IWDG)

- LSI(약 40kHz), Prescaler 32, Reload 125로 하면 타임아웃이 약 100ms다.
- main loop에서 `HAL_IWDG_Refresh()`를 호출한다.
- 부팅할 때 `RCC->CSR`의 IWDGRSTF 비트를 확인해 "reset by watchdog"을 출력한 뒤 `__HAL_RCC_CLEAR_RESET_FLAGS()`를 호출한다.

### 2. 고장 감지 추가

| 고장 | 감지 조건 | 코드 |
|---|---|---|
| 과전류 | INA219 전류가 임계값을 넘는 상태가 200ms 지속. 임계값은 Day 7 측정값으로 정한다 (예: 정상 최대 전류 × 2) | FAULT_OVERCURRENT |
| 엔코더 고장 | 듀티가 40%를 넘는데 300ms 동안 카운트 변화량이 10 미만 | FAULT_ENCODER |
| 통신 두절 | 유효 CMD 프레임이 100ms 동안 없음 (Day 9) | FAULT_CAN_TIMEOUT |

### 3. 고장 주입 테스트 (결과 칸을 채운다)

| ID | 주입 방법 | 기대 결과 | 측정 방법 | 결과 |
|---|---|---|---|---|
| FI-01 | CANH 선 분리 | 100ms 이내 STBY Low | 로직 분석기: 마지막 CAN 프레임부터 STBY 하강까지 간격 | |
| FI-02 | 소형 보드 리셋 버튼 | 100ms 이내 STBY Low | 위와 같음 | |
| FI-03 | 체크섬 오염: 소형 보드 PB12를 GND에 연결하면 일부러 틀린 체크섬을 보내도록 코드 추가 | 3회 연속 오류 후 FAULT | UART 로그 | |
| FI-04 | 바퀴를 손으로 잡아 정지 (1~2초 이내) | 과전류 FAULT, 모터 정지 | UART 로그 + LCD | |
| FI-05 | 엔코더 VCC 선 분리 | 엔코더 FAULT, 모터 정지 | UART 로그 | |
| FI-06 | 테스트 빌드에서 버튼을 누르면 `while(1)` 진입 | IWDG 리셋, 재부팅 후 원인 출력 | UART 로그 | |
| FI-07 | 고장 후 목표 RPM이 0이 아닌 상태에서 통신 복구 | 재가동하지 않음. 목표를 0으로 내린 뒤 RUN 복귀 | LCD + 로그 | |

### 4. 요구사항 문서 (`docs/requirements.md`)

각 요구사항에 테스트 ID를 연결한다. 부트캠프에서 한 추적성 작업을 그대로 다시 하는 것이다.

| ID | 요구사항 | 검증 |
|---|---|---|
| SR-01 | 모터 ECU는 목표 RPM을 정상상태 오차 ±N RPM 안에서 추종해야 한다 (N은 Day 6 측정 후 확정) | Day 6 스텝 응답 |
| SR-02 | 유효한 CMD 프레임을 100ms 동안 받지 못하면 모터를 정지해야 한다 | FI-01, FI-02 |
| SR-03 | 체크섬이나 카운터 오류 프레임은 무시하고, 3회 연속이면 FAULT로 전환해야 한다 | FI-03 |
| SR-04 | 과전류가 200ms 지속되면 모터를 정지해야 한다 | FI-04 |
| SR-05 | 엔코더 신호가 없으면 모터를 정지해야 한다 | FI-05 |
| SR-06 | 소프트웨어가 멈추면 워치독으로 리셋해야 한다 | FI-06 |
| SR-07 | 고장 후 자동 재가동을 금지하고, 목표 0을 확인한 뒤에만 복귀해야 한다 | FI-07 |

### 5. README와 GitHub

- **README에 넣을 것**
  - 시스템 구성도
  - 핀 맵, CAN 메시지 표
  - 스텝 응답 그래프
  - 고장 주입 결과표
  - 로직 분석기 캡처 3장 (PWM, I2C, CAN)
  - 알려진 한계
- **저장소 구조**
  ```
  stm32-motor-ecu/
  ├── CLAUDE.md             # Claude용 프로젝트 지침
  ├── README.md
  ├── nucleo_motor_ecu/     # 모터 ECU (NUCLEO-F103RB)
  ├── bluepill_cmd_ecu/     # 명령 ECU (소형 보드)
  └── docs/
      ├── progress.md       # 진행 기록, 측정값
      ├── lab_guide.md      # 이 문서
      ├── pot_substitute.md # 가변저항 대체 방법, 도착 후 할 일
      ├── requirements.md
      ├── test_report.md
      └── captures/         # 로직 분석기 캡처, 그래프
  ```
- 마지막에 태그 `v1.0`을 단다.

---

## 부록 A. 면접에서 나올 만한 질문 (실습하면서 답을 채운다)

- PWM을 왜 20kHz로 했나요? (가청 대역)
- 엔코더를 왜 PB6/PB7에 연결했나요? (5V tolerant)
- Anti-windup이 없으면 어떤 문제가 생기나요?
- CAN 비트 타이밍에서 샘플 포인트는 왜 87.5% 근처로 잡나요?
- alive counter와 체크섬은 각각 어떤 고장을 잡나요? (프레임 반복·누락 vs 데이터 오염)
- 통신이 복구됐는데 왜 바로 재가동하지 않나요?
- 제어 루프를 왜 ISR에서 돌렸나요? (Day 7 지터 측정 데이터로 답한다)
- 워치독 타임아웃은 어떻게 정했나요?

## 부록 B. 이력서 한 줄 (완성한 뒤에만 쓴다)

> STM32 모터 제어 ECU 미니 프로젝트: 엔코더 기반 PI 속도 제어, CAN 2노드 통신(alive counter·체크섬, 100ms 타임아웃 시 안전 상태 전환), 요구사항 7개와 고장 주입 테스트 7건으로 검증

수치(정상상태 오차, 정지까지 걸린 시간 등)는 실제 측정값으로 바꿔 넣는다. 하지 않은 항목은 지운다.

## 부록 C. 참고 자료

- **STM32F103 레퍼런스 매뉴얼 (RM0008):** 레지스터, 타이머, bxCAN, I2C
  https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
- **STM32F103RB 데이터시트:** 핀 기능표, FT 핀 여부
  https://www.st.com/resource/en/datasheet/stm32f103rb.pdf
- **Nucleo-64 사용자 매뉴얼 (UM1724):** CN7·CN10 핀 배치, CN2·CN4(ST-LINK)
  https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf
- **ES096 에라타:** I2C BUSY 문제 등
  https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-medium-density-device-limitations-stmicroelectronics.pdf
- **FIT0450 엔코더 모터 (DFRobot 위키)**
  https://wiki.dfrobot.com/fit0450/
- **TB6612FNG**
  https://www.sparkfun.com/datasheets/Robotics/TB6612FNG.pdf
- **INA219**
  https://www.ti.com/lit/ds/symlink/ina219.pdf
- **SN65HVD230**
  https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf
- **PCF8574 (LCD 백팩)**
  https://www.ti.com/lit/ds/symlink/pcf8574.pdf
- **PulseView 다운로드**
  https://sigrok.org/wiki/Downloads
