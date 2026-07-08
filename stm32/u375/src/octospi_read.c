/*
 * STM32U375 OCTOSPI quad(1-4-4) memory-mapped read 테스트
 * flash: P25Q64SH (Puya 8MB), 배선 NCS=PA2 CLK=PA3 IO0=PB1 IO1=PB0 IO2=PA7 IO3=PA6 (전부 AF10)
 *
 * 목표: OCTOSPI를 memory-mapped 모드로 설정 → 0x90000000 에서 flash가 보이면
 *       SWD로 0x90000000 읽어 MJPL 매직(리틀엔디안 0x4C504A4D) 확인.
 *
 * ⚠️ 첫 버전 — 실측 디버그 예상값(조정 후보): DCYC(dummy), DCR2 PRESCALER, MTYP, AF10(U375 DS).
 * 모든 레지스터/비트는 CMSIS stm32u375xx.h 원본에서 검증. U3엔 OCTOSPIM(IO Manager) 없음(직접 AF 연결).
 */
#include <stdint.h>

/* ---- RCC ---- */
#define RCC_BASE      0x40030C00UL
#define RCC_AHB2ENR1  (*(volatile uint32_t *)(RCC_BASE + 0x08CUL))  /* GPIOxEN */
#define RCC_AHB2ENR2  (*(volatile uint32_t *)(RCC_BASE + 0x090UL))  /* OCTOSPI1EN=bit4 */

/* ---- GPIO ---- */
typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2];
} GPIO_t;
#define GPIOA ((GPIO_t *)0x42020000UL)
#define GPIOB ((GPIO_t *)0x42020400UL)

/* ---- OCTOSPI1 (XSPI) @ 0x420D1400 ---- */
#define OB     0x420D1400UL
#define O_CR   (*(volatile uint32_t *)(OB + 0x000))
#define O_DCR1 (*(volatile uint32_t *)(OB + 0x008))
#define O_DCR2 (*(volatile uint32_t *)(OB + 0x00C))
#define O_SR   (*(volatile uint32_t *)(OB + 0x020))
#define O_FCR  (*(volatile uint32_t *)(OB + 0x024))
#define O_DLR  (*(volatile uint32_t *)(OB + 0x040))
#define O_AR   (*(volatile uint32_t *)(OB + 0x048))
#define O_DR   (*(volatile uint32_t *)(OB + 0x050))
#define O_CCR  (*(volatile uint32_t *)(OB + 0x100))
#define O_TCR  (*(volatile uint32_t *)(OB + 0x108))
#define O_IR   (*(volatile uint32_t *)(OB + 0x110))
#define O_ABR  (*(volatile uint32_t *)(OB + 0x120))

#define SR_TCF   (1u << 1)
#define SR_BUSY  (1u << 5)

/* mode 코드: 1=1line, 3=4line(quad) */
static void af10(GPIO_t *g, int pin)
{
    g->MODER   = (g->MODER   & ~(3u << (pin * 2))) | (2u << (pin * 2)); /* AF mode */
    g->OSPEEDR |=  (3u << (pin * 2));                                   /* very high speed */
    int r = pin >> 3, s = (pin & 7) * 4;
    g->AFR[r]  = (g->AFR[r] & ~(0xFu << s)) | (10u << s);               /* AF10 */
}

static void tcf_wait(void) { while (!(O_SR & SR_TCF)) {} O_FCR = SR_TCF; }
static void busy_wait(void) { while (O_SR & SR_BUSY) {} }

/* indirect write: instruction (+ optional data bytes) — 주소 없음 */
static void flash_cmd(uint8_t inst, const uint8_t *data, int len)
{
    busy_wait();
    O_CR = 1u;                                    /* EN, FMODE=00(indirect write) */
    if (len) O_DLR = (uint32_t)(len - 1);
    O_CCR = (1u << 0) | (len ? (1u << 24) : 0);   /* IMODE=1line, DMODE=1line if data */
    O_IR  = inst;                                 /* 주소 없으니 즉시 트리거 */
    for (int i = 0; i < len; i++)
        *(volatile uint8_t *)&O_DR = data[i];
    tcf_wait();
    busy_wait();
}

int main(void)
{
    /* 1) 클럭: GPIOA,B + OCTOSPI1 */
    RCC_AHB2ENR1 |= (1u << 0) | (1u << 1);    /* GPIOAEN, GPIOBEN */
    RCC_AHB2ENR2 |= (1u << 4);                /* OCTOSPI1EN */

    /* 2) 핀 AF10: PA2,3,6,7 / PB0,1 */
    af10(GPIOA, 2); af10(GPIOA, 3); af10(GPIOA, 6); af10(GPIOA, 7);
    af10(GPIOB, 0); af10(GPIOB, 1);

    /* 3) OCTOSPI 기본 설정 */
    O_CR   = 0;                    /* disable */
    O_DCR1 = (22u << 16);          /* DEVSIZE=22 → 2^23=8MB, MTYP=0(standard) */
    O_DCR2 = 3u;                   /* PRESCALER=3 → kernel/4 (느리게 시작) */

    /* 4) flash QE(Quad Enable=SR2 bit1) 켜기 */
    flash_cmd(0x06, 0, 0);                     /* WREN */
    uint8_t qe = 0x02;
    flash_cmd(0x31, &qe, 1);                   /* WRSR2 = 0x02 (QE=1) */
    for (volatile int i = 0; i < 400000; i++) {} /* QE는 non-volatile write, program time 대기 */

    /* 5) memory-mapped 1-4-4 read (0xEB) 설정 */
    O_CR  = 0;                     /* reconfig 위해 disable */
    O_ABR = 0x00;                  /* mode/alternate byte = 0 (continuous read off) */
    O_TCR = 4u;                    /* DCYC=4 (0xEB: mode 2cyc + dummy 4 = 6) — 실측 조정 후보 */
    O_CCR = (1u << 0)              /* IMODE  = 1line   (instruction) */
          | (3u << 8)              /* ADMODE = 4line   (address)     */
          | (2u << 12)             /* ADSIZE = 24-bit                */
          | (3u << 16)             /* ABMODE = 4line   (mode byte)   */
          | (0u << 20)             /* ABSIZE = 1 byte                */
          | (3u << 24);            /* DMODE  = 4line   (data)        */
    O_IR  = 0xEB;                  /* Fast Read Quad I/O */
    O_CR  = (3u << 28) | 1u;       /* FMODE=11(memory-mapped) + EN */

    /* 6) 이제 0x90000000 = flash. 확인용으로 첫 워드/두번째 워드를 전역에 복사.
     *    (SWD로 이 변수들 또는 0x90000000 직접 읽으면 됨) */
    volatile uint32_t magic0 = *(volatile uint32_t *)0x90000000; /* 기대: 0x4C504A4D "MJPL" */
    volatile uint32_t magic1 = *(volatile uint32_t *)0x90000004;
    (void)magic0; (void)magic1;

    while (1) { __asm__ volatile ("nop"); }
}
