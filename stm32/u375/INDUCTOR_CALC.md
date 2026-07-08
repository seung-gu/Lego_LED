# 인덕터 선정 / 리플·피크 전류 계산 (STM32U375 보드)

- **날짜:** 2026-06-10
- **대상:** 메인 buck-boost(TPS630251) + MCU 내부 SMPS(STM32U375)
- 스위칭 컨버터 인덕터의 리플/피크 전류로 **정격(Isat)** 정하는 법. 어느 컨버터든 동일 적용.

---

## 1. 핵심 공식

| 구하는 것 | 공식 |
|---|---|
| **리플 전류** | ΔI_L = V_L · D / (f · L) |
| **평균 전류** | 벅: I_avg = Iout / 부스트: I_avg = Vout·Iout/(η·Vin) |
| **피크 전류** | **I_PEAK = I_avg + ΔI_L/2** |
| **필요 정격** | **Isat ≥ 1.2 × I_PEAK** (20% 마진) |

> 직관: ΔI_L은 `V = L·di/dt`에서 → "인덕터에 걸린 전압 × 켜진 시간 ÷ L"

## 2. 벅/부스트 파라미터 (V_L, D)

| 모드 | 조건 | V_L (켜진 동안) | D |
|---|---|---|---|
| **벅** | Vin > Vout | Vin − Vout | Vout/Vin |
| **부스트** | Vin < Vout | Vin | (Vout−Vin)/Vout |

## 3. 순서
1. **입력 범위 양끝**(min Vin·max Vin)에서 각각 계산
2. 각 조건 I_PEAK 구해서 **제일 큰 값** 선택
3. **×1.2** → Isat 요구치

## 4. ⚠️ CCM / PFM 주의
- 위 공식 = **CCM(연속 도통)** 가정
- light-load면 실제론 **PFM(펄스 스킵, 절전모드)** → 진짜 피크는 컨버터 **PFM 임계**(보통 CCM 계산보다 작음)
- 즉 **CCM I_PEAK는 상한 참고값.** 여유 두고 Isat 선정.

---

## 5. 적용 결과

### 5.1 메인 3.0V buck-boost — TPS630251
조건: Vout=2.9V, Vin=2.3~5.5V, Iout≈14mA, f=2.5MHz, L=1µH, η=0.9

| 조건 | 모드 | D | ΔI_L | I_avg | **I_PEAK** |
|---|---|---|---|---|---|
| Vin=2.3 | 부스트 | 0.21 | 190mA | 20mA | 115mA |
| **Vin=5.5** | **벅** | 0.53 | 548mA | 14mA | **288mA ← worst** |

- worst = 고입력(벅) 쪽 (리플이 큼). CCM 288mA → ×1.2 = 346mA
- 단 light-load라 실제 PFM → 여유 봐서 **Isat ~0.5~1A**
- **선정: Bourns CVH160808-1R0M** (1µH / 0.95A / DCR 0.2Ω / 0603 / shielded)

### 5.2 MCU 내부 SMPS — STM32U375 (VLXSMPS→VDD11/Vcore)
- ST **AN6011** 권장: **2.2µH ceramic coil** (코어 전류 작아 정격 빡세지 않음 → Table 9에 "ceramic coil"만 명시)
- **선정: Bourns CVH160808-2R2M** (2.2µH / 0.75A / DCR 0.3Ω / 0603 / shielded)

---

## 6. 참고
- **ΔI_L ∝ 1/L** → L 키우면 리플↓ (단 컨버터 권장 L 범위 지킬 것; TPS630251은 0.6~1µH)
- **캡은 출력 "전압" 리플**을 잡지, **인덕터 "전류" 리플**은 안 줄임 (별개 — 인덕터 정격은 전류 리플로 봄)
- **DCR 낮을수록 효율↑** — 특히 메인 buck-boost는 보드 전체 전류를 나르므로 저DCR 중요
- shielded(메탈합금/molded) 권장 — TSC 터치 노이즈 민감 (메인 buck-boost), MCU SMPS는 저전류라 덜 민감
- Table 3(TPS630251)의 4~7A 인덕터는 **풀출력(2A) 예시**지 light-load 필수 아님 — 위 식으로 직접 계산해 정함
