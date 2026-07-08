// 🦆 STM32L412KBU6 - "핀 놓기(hi-Z)" 펌웨어
// 목적: 외장 플래시와 공유하는 SPI 핀을 MCU가 놓게 해서,
//       CH341A가 보드 플래시를 프로그래밍할 때 충돌(contention)을 없앤다.
// 놓는 핀: PB3(SCK)/PB4(MISO)/PB5(MOSI)/PC14(CS) → 전부 아날로그 입력(완전 하이임피던스)
// 주의: PB3=JTDO, PB4=NJTRST 라 리셋 기본값이 JTAG임. 아날로그로 덮어써야 진짜 풀려남.
//       SWD(PA13/14)는 안 건드리므로 ST-Link 디버깅은 계속 살아있음.

#include "stm32l4xx_hal.h"

int main(void)
{
    HAL_Init();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_ANALOG;   // 완전 하이임피던스 (드라이브/풀 전부 분리)
    g.Pull = GPIO_NOPULL;

    // 플래시 SCK/MISO/MOSI — PB3(JTDO)/PB4(NJTRST)의 JTAG 기본값도 여기서 덮어써짐
    g.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOB, &g);

    // 플래시 CS — PC14
    g.Pin = GPIO_PIN_14;
    HAL_GPIO_Init(GPIOC, &g);

    // 이제 MCU는 플래시 버스를 완전히 놓았다. 가만히 잠만 잔다.
    while (1) {
        __WFI();
    }
}
