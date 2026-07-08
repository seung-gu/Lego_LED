# STM32U375 WPT 영상재생 보드 — 설계 노트

- **날짜:** 2026-06-10 / **발주(PCB+부품):** 2026-06-11
- 현 보드(L412) 후속 리비전. WPT 급전, 외장 flash에서 MJPEG 읽어 소형 컬러 LCD 재생 + 터치.
- 관련: [INDUCTOR_CALC.md](INDUCTOR_CALC.md), [../l412/POWER_ANALYSIS.md](../l412/POWER_ANALYSIS.md)

> ⚠️ **확정 MCU는 STM32U375CGU6Q** (이전 후보 U385 QFN48은 SMPS가 없어 U375로 변경 — §2).

---

## 0. 확정 부품
| 구분 | 부품 | 비고 |
|---|---|---|
| MCU | **STM32U375CGU6Q** | **UFQFPN48(QFN48)**, Cortex-M33, 내장 SMPS, TSC. ST/Digikey 구매(손납땜) |
| 메인 전원 | **TPS630251** | buck-boost, **고정 2.9V**, 입력 2.3~5.5V |
| Flash | **P25Q64SH-SSH-IT** | Puya 64Mbit, 2.3~3.6V, OCTOSPI quad |
| LCD | **NT042BH-H16-01** | 0.42인치 8핀 컬러 |
| 인덕터 | CVH160808-1R0M(메인) / CVH160808-2R2M(SMPS) | Bourns 0603 shielded |

---

## 0.5 부품 선정 이유 (rationale)

### MCU — STM32U375CGU6Q
- **요구 조건:** 내장 SMPS + OCTOSPI(flash quad) + TSC(터치) + 추가 SPI(LCD)를 **동시에**, 7×7mm 이내로.
- 이 4가지를 다 만족하는 건 **U375 하나뿐**이었음 (예: U385 QFN48은 SMPS 없음).
- **패키지 변천:** 처음 **WLCSP52** 고려 → **4층 PCB 필요**(비용↑) → **UFQFPN48(7×7mm, 2층 가능)** 으로 최종 확정.

### 메인 컨버터 — TPS630251
- **요구:** wide 입력 buck-boost(이상적으론 1.8~5.5V) + **효율 90%↑**.
- TPS630251 = 효율이 매우 우수(**최소 90%↑**, 후보 중 최상). 입력은 **2.3~5.5V** — 1.8이 아닌 2.3부터가 아쉽지만, **WPT 환경에선 2.3V 미만이면 어차피 전류도 부족**해 그 이하를 찾는 의미가 없었음.
- **출력 2.9V 고정:** 3.3V보다 **저전력 구동**을 위해 2.9V를 선호했는데 딱 일치.
- → **2.9V 출력 + 고효율**, 이 두 가지가 결정타.

### Flash — P25Q64SH (Puya, 64Mbit / 8MB)
- **저전력 + quad SPI**(OCTOSPI 파이프라이닝) 구동이 핵심 이유.
- 단점: 8MB 용량 한계, 굽기(업로드)가 까다로움.
- **극복:** `ch341a`로 **SFDP 자동인식**되어 구울 수 있고, 8MB여도 **영상 길이는 충분**.

### LCD — 0.42" 8핀 컬러 (AliExpress)
- 기존 **16핀 → 8핀** 교체로 공간을 대폭 확보.
- 핀 간격 **0.65mm → 0.5mm** 로 바뀌며 뒷면 공간이 생겨, **flash 칩을 bottom 층으로** 내릴 수 있었음.
- ※ 48핀 MCU는 32핀 대비 **SMPS + 다수 VCC핀** 때문에 디커플/벌크 캡이 더 필요했고 **TSC 센서도 2개**나 달렸지만 — 위 공간 극복 덕에 **1.5×1.5cm PCB에 전부 수용**.

---

## 1. 전력 분석 (L412 실측 교훈)
- 14mA @32MHz; **LCD모듈 ~5.3mA, MCU+flash ~8.7mA**
- **WPT 거리 = 전류(전력)가 결정** — 전압 임계 아님 (백라이트 40/100% 거리 무관, 부스트 달면 정전력부하라 붕괴)
- **SMPS는 코어(Vcore)만** 줄임 — I/O·flash·LCD 무관
- 코어가 **busy-wait로 안 자서** 연속 active 전류가 큰 몫 → DMA+sleep로 전송중 재우면 일부 절감(코어 일부라 효과 제한)
- **SPI I/O 전력 = C·V²·f** (MCU 교체해도 거의 안 줆)
- 클럭/SPI 낮추기 = 약한 레버(~0.7mA/8MHz). 전압(Range1→2)이 큰 레버(~1.4mA)지만 26MHz 이하라 영상 느려져 못 씀
- flash: P25Q64 active read ~1.5~2.5mA(16MHz 싱글), standby 20µA. **Puya가 Macronix MX25R보다 active read 효율 좋음**(연속재생 앱이라 Puya 맞음)
- → **U375+SMPS 예상: 14 → ~11mA** (코어만 줆, 극적이지 않음). 4mA OLED보드는 sleep위주라 못 따라감(워크로드 차이)

---

## 2. MCU STM32U375CGU6Q
- **UFQFPN48 (QFN48, 0.5mm 피치, 둘레핀)** → **2층 라우팅 가능** (WLCSP 아님 → via-in-pad 불필요, JLCPCB 2층 + via 0.15로 OK)
- **U375 vs U385 (QFN48 한정):** U375=크립토 없음 + **SMPS 있음**, U385=크립토 있음 + **SMPS 없음**(크립토가 SMPS 핀 점유 → LDO-only). → SMPS 쓰려면 **U375 필수**
- **KGU6(32핀)은 핀 부족** (OCTOSPI quad가 6핀 먹어서) → 48핀 채택
- 손납땜 가능(QFN48). **LCSC 재고 무관** — ST/Digikey에서 칩만 구매
- **TSC 있음** (정전용량 다채널)
- ※ CubeMX에서 SMPS를 **"internal"로 설정하면 부품명에 Q**가 붙음(U375CGU6**Q**). external/bypass 선택지도 있으나 내장 SMPS 쓸 거라 internal

---

## 3. SMPS (코어 전원) — 회로도 확정
- **Config: Direct SMPS Supply** (SMPS on, LDO off = 최고효율)
- **VDD11 핀 2개**(46번, 23번) → **각 2.2µF**(= 2×2.2µF, ST AN6011 권장: 2핀이면 2개로 분할)
- **회로:**
  - `VDDSMPS(21) ← VCC` + 입력 디커플 **C15(0.1µF) + C16(10µF) → GND**
  - `VLXSMPS(20) ─ [L11 2.2µH] ─ VDD11`
  - `VDD11 ─ [C171 2.2µF + C172 2.2µF] → GND`
  - `VSSSMPS(22) → GND`
- ⚠️ **VLXSMPS는 스위치 노드** → L11로만, **VCC 직결 금지**. VDD11도 SMPS 출력 → 인덕터+캡에만.
- **SMPS↔LDO 소프트웨어 전환 가능**(PWR 레지스터, on-the-fly) → **한 보드로 SMPS vs LDO A/B 측정**
- **레이아웃:** C171은 46번 핀 옆, C172는 23번 핀 옆(각 VDD11 핀 근처). VLXSMPS↔VDD11 루프 짧게, shielded 인덕터(L11)

---

## 4. 메인 컨버터 TPS630251
- **고정 2.9V** (PWM 정확도 2.871~2.929 / PFM ~2.938) — ※ 3.0V 아님
- **입력 2.3~5.5V** — ※ 2.5V는 "2A 출력" 조건이지 min 아님
- 회로(데이터시트 Table 1·2): **L1=1µH, Cin=10µF, Cout=47µF 1개**(6.3V/0603/X5R = typical application)
- 출력캡 규정: **min 20µF effective**(8.3 Recommended Operating Conditions; 주석=ceramic DC바이어스 디레이팅 감안한 nominal 선정) + **상한 없음**(§10.2.2.3.2 "no upper limit")
  - ⚠️ **데이터시트엔 출력캡 "ESR" 스펙·언급이 전혀 없음** — 출력은 용량(µF)으로만 규정하고 "ceramic" 권장. (전해 회피 근거는 "세라믹권장+리플" 일반론이지 DS 문구 아님)
  - Softstart(§9.3.4): *"큰 출력캡이 달려도 inrush 최소화하며 부드럽게 기동"* 명시 → 큰 캡도 기동 자체는 받쳐줌 (WPT가 1ms ramp를 못 받치는 건 별개)
  - → **typical=47µF 세라믹**(6.3V/0603/X5R)이 검증된 선택. 예전 평활용 220µF는 불필요하게 크고 세라믹 아님 → 제거
- FB→VOUT(고정버전), EN→VIN, **PFM/PWM(볼 B4)→GND**(PFM=저부하 효율), VINA→VIN
- **VINA** = 제어부 공급. **251엔 별도 디커플 캡 불필요**(0.1µF 권장은 형제칩 TPS63020/31 얘기지 251 데이터시트엔 없음)
- 볼아웃 주의: **B4=PFM/PWM, C1~C3=PGND**
- 인덕터: 1µH → **CVH160808-1R0M** (계산: INDUCTOR_CALC.md)

---

## 5. Startup 시퀀싱 (BOR) / BOOT0
- **요구:** MCU는 buck-boost 출력이 안정된 뒤 켜져야 함 (ramp 중 full 부하 걸리면 약한 WPT가 regulation 못 잡고 stall)
- **BOR이 전원-업 게이트 역할** — VDD가 BOR 문턱 넘을 때까지 칩 리셋 유지 (단순 브라운아웃 감지가 아님)
- **BOR ~2.8V** 두면 2.8V까지 기다렸다 시동 = 원하는 동작
  - 단 출력 최악 2.871V와 마진 빠듯 → 안 켜지면 **2.5~2.6V로 낮춤**
- **BOR은 옵션바이트**(CubeProgrammer/SW, reflash 가능) → 칩 와서 튜닝. PCB 무관
- **모터보팅 방지:** 출력캡(47µF급, soft-start와 함께)이 부하스텝 dip 받쳐줌
- **BOOT0(PH3):** **GND 직결 OK** (메인 flash 부팅 고정). 또는 옵션바이트 **nSWBOOT0=0**으로 핀 무시. SWD로 플래싱하니 시스템 부트로더 진입 불필요 → 직결이 제일 간단

---

## 6. 핀맵 (회로도 확정 — U375CGU6Q QFN48)
| 기능 | 핀 | 비고 |
|---|---|---|
| LCD SPI2 SCLK | **PB13** | |
| LCD SPI2 MOSI | **PB15** | |
| LCD SPI2 CS | **PA0** | GPIO로 제어 (PB14에서 이동) |
| LCD DC | **PA8** | |
| LCD RESET | **PA9** | |
| 백라이트 LED-A PWM | **PA10 (TIM1_CH3)** | |
| flash OCTOSPI NCS / CLK | PA2 / PA3 | |
| flash IO0 / IO1 / IO2 / IO3 | PB1 / PB0 / PA7 / PA6 | |
| TSC 샘플링 | **PB6** | Cs = CBUTTON 22nF → GND |
| TSC 전극1(TOUCH1) / 전극2(TOUCH2) | **PB4 / PB5** | 각 Rs 10k(R11/R12) |
| SWD SWDIO / SWCLK | PA13 / PA14 | |
| BOOT0 | PH3 | GND 직결 |
| 상태 LED | **PA4** | R13 3.3k + LED1 (~0.4mA, PC13에서 이동) |

- **LCD는 SPI1→SPI2로 이동**(PB13/14/15), **백라이트는 PA11→PA10(TIM1_CH3)**, **TSC는 PB6(샘플)/PB4(TOUCH1)/PB5(TOUCH2)**로 정리됨
- **SPI2: Transmit Only Master**(MISO 제거), NSS·RDY disable
- **OCTOSPI: quad** — MISO·DQS·NCLK 불필요(IO 양방향)
- **미사용 핀(펌웨어 Analog 처리 — §13):** PC14, PC15, PH0, PH1, PA1, PA5, PA11, PA12, PA15, PB3, PB7, PB8, **PB14**(LCD CS→PA0), **PC13**(상태 LED→PA4)
- OSC핀(PC14/15, PH0/1): 내부클럭 쓰니 **미연결**(Analog)

---

## 7. Flash 연결 (P25Q64SH 8핀)
`1 CS#→NCS + 풀업 10k→VCC / 2 SO→IO1 / 3 WP#→IO2 / 4 GND / 5 SI→IO0 / 6 SCLK→CLK / 7 HOLD#/RST#→IO3 / 8 VCC+100nF`
- **쿼드라 WP#(3)·HOLD#(7)을 IO2/IO3로 직결** (내부 weak 풀업 있음)
- **CS#만 외부풀업**(내부 없음). 나머지 IO·CLK 직결
- 펌웨어서 **QE(Quad Enable) 비트 set**

---

## 8. LCD 연결 (NT042BH 8핀)
`1 LEDA→백라이트(PA10 PWM) / 2 GND / 3 RESET→PA9(+R 풀업 권장) / 4 RS→DC(PA8) / 5 SDA→MOSI(PB15) / 6 SCL→SCK(PB13) / 7 VDD→VCC+100nF / 8 CS→PA0`
- **헷갈림 주의: RS=DC, SDA=MOSI, SCL=SCK**
- 백라이트 LEDA: 음극 내부 GND(고측 구동). 저헤드룸(3.0V/Vf~2.8)이라 핀 직접 구동도 자기제한; 안전하게 전류제한 저항 권장

---

## 9. TSC 터치
- **샘플링 = PB6** (Cs ~10~22nF→GND; 회로도는 22nF=CBUTTON)
- **전극 = PB4(TOUCH1)/PB5(TOUCH2)** (각 직렬 Rs 10k = R11/R12 → 패드)
- 샘플링·전극은 **같은 TSC 그룹**(PB4/5/6 동일 그룹)이어야 함 ✓
- **핀끼리 외부 연결 없음** — 칩 내부 charge-transfer 스위치가 전극↔샘플링 연결
- 패드 = PCB 구리(자기 정전용량 Cx)가 센서, 별도 캡 없음
- 동작 중 한 그룹 내 전극은 순차 측정(버튼 2개엔 충분), **blit과 시분할**

---

## 10. 디커플링 / 전원핀 (회로도 확정)
- **VDD (48, 36, 25):** 각 **100nF**(C13/C11/C14) + **벌크 4.7µF**(C12, 48번 클러스터에)
- **VDD11 (46, 23):** 각 **2.2µF**(C171/C172) → GND (§3)
- **VDDA (9):** → VCC, **0.1µF + 1µF**(C18/C19) → GND. **VSSA (8) → GND**
- **VBAT (1):** → VCC (float 금지; ※ 이 패키지엔 별도 VDDUSB 핀 없음)
- **VDDSMPS (21):** → VCC, **10µF + 0.1µF**(C16/C15) (§3)
- **NRST (7):** **100nF → GND**(C10). 내부 풀업 있어 외부 풀업 불필요(풀다운 금지)
- **BOOT0 (PH3):** GND 직결 (§5)
- 캡 정격: 3.0V 레일엔 **6.3V/X5R/0402 OK**(DC바이어스 디레이팅 감안). SMPS 큰 캡(10µF)은 0603/0805·10V 고려

---

## 11. 레이아웃 주의
- **UFQFPN48 0.5mm 둘레핀 → 2층 라우팅 가능** (via-in-pad 불필요, WLCSP 대비 비용↓)
- **JLCPCB 드릴:** PTH(via) 최소 **0.15mm**, 0.05mm 증분(0.15→0.20→0.25→0.30…) → **0.2mm 유효**. 0.3mm 미만은 "small hole"(추가공정 가능, 제작은 됨). **이 보드는 via 0.2mm 사용**(과거 보드서 0.2mm 제작 실적 있음)
- 전원/GND 핀은 **plane으로** 떨굼
- **SMPS:** L11(VLXSMPS↔VDD11) 루프 짧게, shielded. C171은 46번·C172는 23번 핀 옆
- **TSC:** PB4/PB5 전극 트레이스 짧게, 메인 buck-boost 인덕터·SPI 노이즈에서 **격리**, blit 중 터치 스캔 안 함(시분할)
  - ※ LCD가 SPI2(PB13~15)로 빠져 MOSI가 TSC핀 사이를 안 지남. 단 PB6(샘플)·PB4/PB5(전극)을 SPI2(PB13/15) 트레이스에서 떨어뜨릴 것

---

## 11.5 전류 실측 (2026-07-08, 영상재생 중)
| 보드 | 3.3V | 2.9V |
|---|---|---|
| L412 (기존) | 17mA | 8~9mA |
| **U375 (본 보드)** | **9mA** | **4mA** |
| OLED+ATtiny (참고) | 4mA | 1mA |

- 조건: 24MHz(부스터 off)+SMPS+ICACHE+미사용핀 Analog+백라이트 PWM 40%+14.6fps DMA 재생+TSC 터치.
- **U375 = L412 대비 반토막** (17→9, 8.5→4).
- **전압 민감도의 범인 = 백라이트 LED**: 모든 보드가 3.3→2.9V에서 전류 급감. 디지털 로직은 V 비례(-12% 예상)인데 반토막인 이유는 LED Vf(~2.8V) 대비 여유전압이 0.5V→0.1V로 줄며 백라이트 전류가 ~5배 감소하기 때문. → **TPS630251 고정 2.9V 선택(§0.5)이 실측으로 검증됨.**

## 12. 펌웨어 TODO
- [ ] 클럭: 내부 osc(크리스탈 없음). L412 비교는 32MHz, 영상용 더 높게 가능
- [ ] **SMPS on/off 빌드 2개로 A/B 측정** (SMPS 절감 실측)
- [ ] flash **QE 비트 set** (quad)
- [ ] **BOR_LEV 옵션바이트** (2.8 시도 → 안 켜지면 2.5~2.6 낮춤)
- [ ] **DMA+sleep** 검토 (blit 중 코어 재우기)
- [ ] TSC PB6(샘플)/PB4(TOUCH1)/PB5(TOUCH2) 설정, blit과 **시분할**
- [ ] 백라이트 PA10 TIM1_CH3 PWM (L412 BRIGHTNESS 이식)

---

## 13. 펌웨어 절전 (bare metal, CubeMX 미사용)

### ★ 안 쓰는 핀 = 전부 Analog 모드 (무비용 절전, 잊지 말 것)
- **왜:** floating 상태로 **디지털 입력 모드**면 입력 버퍼가 중간 전압에서 떨려 **누설 전류**를 먹음. **Analog 모드**는 입력 슈미트 트리거 + 풀업/풀다운을 다 꺼서 누설을 0에 가깝게 만듦. (저전력 영상보드라 이득 분명)
- **bare metal 방법:** 해당 핀의 `GPIOx->MODER` 비트필드를 `0b11`(Analog)로 세팅. ← CubeMX "Set all free pins as Analog"가 내부적으로 하는 게 정확히 이것.
- **⚠️ 리셋 기본값에 의존 금지** — 시리즈/핀마다 reset 후 상태가 다름(디버그핀 PA13/14/15·PB3는 AF로 시작). **명시적으로 MODER 써줄 것.**
- **대상 미사용 핀(현 설계):** PC14, PC15, PH0, PH1, PA1, PA5, PA11, PA12, PA15, PB3, PB7, PB8, PB14, **PC13**(상태 LED가 PA4로 이동)
- **제외(건드리지 말 것):** SWD = PA13/PA14, 그리고 쓰는 페리 핀(SPI2 · OCTOSPI · TSC PB4/5/6 · 백라이트 PA10 · 상태 LED PA4 · LCD GPIO PA0/8/9 등)

### 안 쓰는 GPIO 포트는 클럭도 아예 안 켬
- 포트 전체가 미사용이면 `RCC->AHB2ENR`에서 그 포트 클럭을 **안 켜면** 전력 0.
- 단 한 핀이라도 쓰는 포트는 클럭을 켜야 하므로, 그 포트 안의 **미사용 핀만 Analog** 처리.

### 기타 (기존 분석과 연결)
- 안 쓰는 페리(USB, 미사용 TIM/UART 등) 클럭 enable 안 함 — `RCC` enable 비트 자체를 안 세팅
- DMA + sleep(WFI)로 blit 중 코어 재우기 (§1, §12 — 효과는 코어 일부라 제한적)
- SMPS on (Direct SMPS Supply) = 코어 효율 공급 (§3)
