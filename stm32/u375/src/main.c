/*
 * STM32U375CGU6Q - bare metal 최소 예제
 * PA4 상태 LED(§6: R13 3.3k + LED1)를 껐다 켰다 (blink)
 *
 * 레지스터 주소는 ST 공식 CMSIS 헤더(stm32u375xx.h) 원본에서 검증됨.
 *   - non-secure alias 사용 (TrustZone 비활성 기본 부팅)
 *   - PERIPH_BASE      = 0x40000000
 *   - AHB1PERIPH_BASE  = 0x40020000  (PERIPH + 0x00020000)
 *   - AHB2PERIPH_BASE  = 0x42020000  (PERIPH + 0x02020000)
 *   - RCC_BASE         = 0x40030C00  (AHB1PERIPH + 0x00010C00)
 *   - GPIOA_BASE       = 0x42020000  (AHB2PERIPH + 0)
 */

#include <stdint.h>

/* ---------------- RCC ---------------- */
#define RCC_BASE              0x40030C00UL
#define RCC_AHB2ENR1          (*(volatile uint32_t *)(RCC_BASE + 0x08CUL))
#define RCC_AHB2ENR1_GPIOAEN  (1UL << 0)          /* IO port A clock enable */

/* ---------------- GPIO ---------------- */
typedef struct {
    volatile uint32_t MODER;    /* 0x00 mode           */
    volatile uint32_t OTYPER;   /* 0x04 output type    */
    volatile uint32_t OSPEEDR;  /* 0x08 output speed   */
    volatile uint32_t PUPDR;    /* 0x0C pull-up/down   */
    volatile uint32_t IDR;      /* 0x10 input data     */
    volatile uint32_t ODR;      /* 0x14 output data    */
    volatile uint32_t BSRR;     /* 0x18 bit set/reset  */
} GPIO_TypeDef;

#define GPIOA   ((GPIO_TypeDef *)0x42020000UL)

#define LED_PIN  4              /* PA4 */

/* 대략적인 busy-wait 딜레이 (리셋 후 기본 내부 클럭 기준, 눈에 보이는 속도면 충분) */
static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    /* 1) GPIOA 포트 클럭 enable */
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;

    /* 2) PA4 를 general-purpose output 으로 (MODER: 2bit/pin, 01=output)
     *    리셋 기본값에 의존하지 않고 명시적으로 clear 후 set (§13 방침) */
    GPIOA->MODER &= ~(3UL << (LED_PIN * 2));
    GPIOA->MODER |=  (1UL << (LED_PIN * 2));
    /* push-pull(OTYPER=0)·기본 속도·풀 없음 = 리셋 기본값 그대로 사용 */

    /* 3) 깜빡이기 */
    while (1) {
        GPIOA->BSRR = (1UL << LED_PIN);         /* PA4 = 1 (set,   하위 16bit) */
        delay(800000);
        GPIOA->BSRR = (1UL << (LED_PIN + 16));  /* PA4 = 0 (reset, 상위 16bit) */
        delay(800000);
    }
}
