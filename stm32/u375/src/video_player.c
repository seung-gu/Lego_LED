/*
 * STM32U375 bare metal 영상 재생기
 *   flash(P25Q64) --OCTOSPI quad mmap 0x90000000--> MJPEG 프레임
 *   --tjpgd 디코드--> RGB565 --SPI2--> LCD(ST7735 96x54, NT042BH)
 *
 * 핀맵(DESIGN_NOTES §6/§8):
 *   LCD: CS=PA0 DC=PA8 RST=PA9 백라이트=PA10 / SPI2 SCK=PB13(AF5) MOSI=PB15(AF5)
 *   flash OCTOSPI: NCS=PA2 CLK=PA3 IO0=PB1 IO1=PB0 IO2=PA7 IO3=PA6 (AF10)
 * 모든 레지스터/비트는 CMSIS stm32u375xx.h 원본 검증.
 * ⚠️ 첫 통합 — 실측 디버그 후보: SPI2 SR TXP/EOT 비트, LCD 오프셋, OCTOSPI dummy, AF번호.
 */
#include <stdint.h>
#include <string.h>
#include "tjpgd.h"

#define LCD_W       96
#define LCD_H       54
#define COL_OFFSET  16
#define ROW_OFFSET  0

typedef volatile uint32_t vu32;

/* ================= RCC ================= */
#define RCC_B         0x40030C00UL
#define RCC_CR        (*(vu32 *)(RCC_B + 0x000))
#define RCC_ICSCR1    (*(vu32 *)(RCC_B + 0x008))
#define RCC_CFGR4     (*(vu32 *)(RCC_B + 0x028))
#define RCC_AHB2ENR1  (*(vu32 *)(RCC_B + 0x08C))
#define RCC_AHB2ENR2  (*(vu32 *)(RCC_B + 0x090))
#define RCC_APB1ENR1  (*(vu32 *)(RCC_B + 0x09C))
#define FLASH_ACR     (*(vu32 *)(0x40022000UL + 0x00))
#define PWR_VOSR      (*(vu32 *)(0x40030800UL + 0x0C))   /* PWR=AHB1PERIPH+0x10800 */

/* ── 클럭 48MHz (MSIRC0 96MHz ÷2) ──
 * 검증(HAL stm32u3xx_hal_rcc.c): 리셋 기본 = MSIRC1÷2 = 12MHz(range5).
 * 48MHz@Range2는 flash 2WS + EPOD 부스터 필요(>24MHz). 부스터 입력 3~16MHz. */
#define RCC_AHB1ENR2  (*(vu32 *)(RCC_B + 0x094))

#define PWR_CR3       (*(vu32 *)(0x40030800UL + 0x08))
#define PWR_SVMSR     (*(vu32 *)(0x40030800UL + 0x3C))

/* 빌드 스위치: -DUSE_24MHZ 주면 24MHz(부스터 off, 저전력), 기본은 48MHz */
#ifdef USE_24MHZ
#define SYSCLK_HZ 24000000u
static void clock_setup(void)
{
    RCC_AHB1ENR2 |= (1u<<2);                         /* PWREN */
    (void)RCC_AHB1ENR2;
#ifndef USE_LDO                                      /* -DUSE_LDO: 리셋기본 LDO 유지 (A/B 비교용) */
    while (!(PWR_SVMSR & (1u<<1))) {                 /* SMPS 전환 (재시도) */
        PWR_CR3 |= (1u<<1);
    }
#endif
    FLASH_ACR = (FLASH_ACR & ~0xFu) | 1u;            /* 1WS (24MHz@Range2) */
    /* MSIS = MSIRC1(24MHz)÷1. 부스터/HSI16 불필요(≤24MHz), 96MHz RC0도 안 돌림 */
    RCC_ICSCR1 = (RCC_ICSCR1 & ~(3u<<29))
               | (1u<<31) | (1u<<23);                /* MSISSEL=RC1, MSIRGSEL, DIV=/1 */
    while (!(RCC_CR & (1u<<2))) {}                   /* MSISRDY */
}
#else
#define SYSCLK_HZ 48000000u
static void clock_setup(void)
{
    RCC_AHB1ENR2 |= (1u<<2);                         /* PWREN (VOSR 접근 전 필수!) */
    (void)RCC_AHB1ENR2;                              /* 클럭 enable 전파 대기(더미 리드) */

    /* SMPS 모드 (Direct SMPS Supply, §3): CR3 REGSEL=1 → SVMSR REGS=1 대기.
     * HAL 권장: 레인지 변경 전에 SMPS 먼저 전환 (여긴 Range2 유지) */
    /* 쓰기 직후엔 간혹 무시됨(실측: 수동 SWD론 성공) → 될 때까지 재시도 */
#ifndef USE_LDO                                      /* -DUSE_LDO: 리셋기본 LDO 유지 (A/B 비교용) */
    while (!(PWR_SVMSR & (1u<<1))) {                 /* REGS=1 (SMPS active) 까지 */
        PWR_CR3 |= (1u<<1);                          /* REGSEL = SMPS 재시도 */
    }
#endif

    FLASH_ACR = (FLASH_ACR & ~0xFu) | 2u;            /* 2WS (48MHz@Range2) */
    RCC_CR |= (1u<<11);                              /* HSI16 ON (부스터 입력용) */
    while (!(RCC_CR & (1u<<13))) {}                  /* HSIRDY */
    RCC_CFGR4 = (RCC_CFGR4 & ~(0x3u | (0xFu<<12)))
              | 0x2u | (1u<<12);                     /* BOOSTSEL=HSI16, DIV=/2 → 8MHz */
    PWR_VOSR |= (1u<<8);                             /* BOOSTEN */
    while (!(PWR_VOSR & (1u<<24))) {}                /* BOOSTRDY */
    /* MSIS = MSIRC0(96MHz)÷2 = 48MHz; MSIRGSEL=1로 ICSCR1 적용 (HAL 시퀀스) */
    RCC_ICSCR1 = (RCC_ICSCR1 & ~((1u<<31) | (3u<<29)))
               | (1u<<23) | (1u<<29);                /* MSISSEL=RC0, MSIRGSEL, DIV=/2 */
    while (!(RCC_CR & (1u<<2))) {}                   /* MSISRDY */
}
#endif

/* ================= GPIO ================= */
typedef struct { vu32 MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2]; } GPIO_t;
#define GPIOA ((GPIO_t *)0x42020000UL)
#define GPIOB ((GPIO_t *)0x42020400UL)

static void pin_out(GPIO_t *g, int p)  { g->MODER = (g->MODER & ~(3u<<(p*2))) | (1u<<(p*2)); }
static void pin_af(GPIO_t *g, int p, int af)
{
    g->MODER  = (g->MODER  & ~(3u<<(p*2))) | (2u<<(p*2));
    g->OSPEEDR |= (3u<<(p*2));
    int r = p>>3, s = (p&7)*4;
    g->AFR[r] = (g->AFR[r] & ~(0xFu<<s)) | ((uint32_t)af<<s);
}
#define SET(g,p)  ((g)->BSRR = (1u<<(p)))
#define CLR(g,p)  ((g)->BSRR = (1u<<((p)+16)))

/* LCD control pins */
#define CS_LOW()   CLR(GPIOA,0)
#define CS_HIGH()  SET(GPIOA,0)
#define DC_CMD()   CLR(GPIOA,8)
#define DC_DAT()   SET(GPIOA,8)
#define RST_LOW()  CLR(GPIOA,9)
#define RST_HIGH() SET(GPIOA,9)

/* ================= SPI2 (신형) @ 0x40003800 ================= */
#define SPI2_B    0x40003800UL
#define SPI2_CR1  (*(vu32 *)(SPI2_B + 0x00))
#define SPI2_CR2  (*(vu32 *)(SPI2_B + 0x04))
#define SPI2_CFG1 (*(vu32 *)(SPI2_B + 0x08))
#define SPI2_CFG2 (*(vu32 *)(SPI2_B + 0x0C))
#define SPI2_SR   (*(vu32 *)(SPI2_B + 0x14))
#define SPI2_IFCR (*(vu32 *)(SPI2_B + 0x18))
#define SPI2_TXDR (*(vu32 *)(SPI2_B + 0x20))
#define SPI_SPE     (1u<<0)
#define SPI_CSTART  (1u<<9)
#define SR_TXP      (1u<<1)      /* 신형 SPI SR: TXP=bit1, EOT=bit3 (실측검증) */
#define SR_EOT      (1u<<3)

/* ================= OCTOSPI @ 0x420D1400 ================= */
#define OB     0x420D1400UL
#define O_CR   (*(vu32 *)(OB+0x000))
#define O_DCR1 (*(vu32 *)(OB+0x008))
#define O_DCR2 (*(vu32 *)(OB+0x00C))
#define O_SR   (*(vu32 *)(OB+0x020))
#define O_FCR  (*(vu32 *)(OB+0x024))
#define O_DLR  (*(vu32 *)(OB+0x040))
#define O_DR   (*(vu32 *)(OB+0x050))
#define O_CCR  (*(vu32 *)(OB+0x100))
#define O_TCR  (*(vu32 *)(OB+0x108))
#define O_IR   (*(vu32 *)(OB+0x110))
#define O_ABR  (*(vu32 *)(OB+0x120))
#define OSR_TCF   (1u<<1)
#define OSR_BUSY  (1u<<5)
#define FLASH_MMAP ((volatile uint8_t *)0x90000000UL)

static uint16_t frame_buf[2][LCD_W * LCD_H];   /* 더블버퍼: DMA 전송 중 다른쪽에 디코드 */
static uint16_t *decode_buf = frame_buf[0];
static uint8_t  jd_work[4096];
volatile uint32_t heartbeat = 0;

/* ── 프레임 페이싱: SysTick 15Hz + WFI sleep ── */
#define FPS_TARGET   15   /* 24MHz 디코드 상한 ~14fps라 사실상 최대속도 재생 */
#define SYST_CSR  (*(vu32 *)0xE000E010)   /* CTRL: ENABLE0 TICKINT1 CLKSOURCE2 */
#define SYST_RVR  (*(vu32 *)0xE000E014)   /* reload */
#define SYST_CVR  (*(vu32 *)0xE000E018)   /* current */
static volatile uint32_t tick_flag = 0;
void SysTick_Handler(void) { tick_flag = 1; }   /* startup의 Default_Handler 대체 (필수!) */

static void pacing_init(void)
{
    SYST_RVR = (SYSCLK_HZ / FPS_TARGET) - 1u;   /* 24bit 내 */
    SYST_CVR = 0;
    SYST_CSR = 7u;                              /* ENABLE + TICKINT + CLKSOURCE=core */
}
static void frame_wait(void)                    /* 다음 15Hz 틱까지 WFI로 잠 */
{
    while (!tick_flag) { __asm__ volatile ("wfi"); }
    tick_flag = 0;
}

/* ---------- 백라이트: PA10 = TIM1_CH3 PWM (L412 BRIGHTNESS 이식) ---------- */
#define BRIGHTNESS 40                     /* % */
#define TIM1_B 0x40012C00UL
#define T1_CR1   (*(vu32 *)(TIM1_B+0x00))
#define T1_EGR   (*(vu32 *)(TIM1_B+0x14))
#define T1_CCMR2 (*(vu32 *)(TIM1_B+0x1C))
#define T1_CCER  (*(vu32 *)(TIM1_B+0x20))
#define T1_PSC   (*(vu32 *)(TIM1_B+0x28))
#define T1_ARR   (*(vu32 *)(TIM1_B+0x2C))
#define T1_CCR3  (*(vu32 *)(TIM1_B+0x3C))
#define T1_BDTR  (*(vu32 *)(TIM1_B+0x44))
#define RCC_APB2ENR (*(vu32 *)(RCC_B + 0x0A4))

static void backlight_init(void)
{
    RCC_APB2ENR |= (1u<<11);              /* TIM1EN */
    pin_af(GPIOA, 10, 1);                 /* PA10 = AF1(TIM1_CH3) */
    T1_PSC   = (SYSCLK_HZ/1000000u) - 1u; /* 1MHz 틱 */
    T1_ARR   = 100u - 1u;                 /* 주기 100틱 = 10kHz PWM */
    T1_CCR3  = BRIGHTNESS;                /* 듀티 40% */
    T1_CCMR2 = (6u<<4) | (1u<<3);         /* OC3M=PWM1, OC3PE */
    T1_CCER  = (1u<<8);                   /* CC3E */
    T1_BDTR  = (1u<<15);                  /* MOE (advanced 타이머 출력 게이트) */
    T1_EGR   = 1u;                        /* UG: PSC/ARR 로드 */
    T1_CR1   = (1u<<7) | 1u;              /* ARPE + CEN */
}

/* ---------- TSC 터치 (PB4=G2_IO1 전극1, PB5=G2_IO2 전극2, PB6=G2_IO3 샘플링, 전부 AF9) ---------- */
#define TSC_B 0x40024000UL
#define TSC_CR     (*(vu32 *)(TSC_B+0x00))
#define TSC_ICR    (*(vu32 *)(TSC_B+0x08))
#define TSC_ISR    (*(vu32 *)(TSC_B+0x0C))
#define TSC_IOHCR  (*(vu32 *)(TSC_B+0x10))
#define TSC_IOSCR  (*(vu32 *)(TSC_B+0x20))
#define TSC_IOCCR  (*(vu32 *)(TSC_B+0x28))
#define TSC_IOGCSR (*(vu32 *)(TSC_B+0x30))
#define TSC_IOG2CR (*(vu32 *)(TSC_B+0x38))
#define RCC_AHB1ENR1 (*(vu32 *)(RCC_B + 0x088))

static void tsc_init(void)
{
    RCC_AHB1ENR1 |= (1u<<16);                 /* TSCEN */
    pin_af(GPIOB,4,9); pin_af(GPIOB,5,9);     /* 전극: AF9 push-pull */
    pin_af(GPIOB,6,9);                        /* 샘플링: AF9 + open-drain */
    GPIOB->OTYPER |= (1u<<6);
    /* PB4는 NJTRST라 리셋 후 내부 풀업 — TSC엔 치명적(기준값 177 vs 4163 실측).
     * TSC 핀 셋 다 풀업/풀다운 명시적 해제 */
    GPIOB->PUPDR &= ~((3u<<(4*2)) | (3u<<(5*2)) | (3u<<(6*2)));
    TSC_IOHCR  = 0;                           /* 히스테리시스 off (TSC 권장) */
    TSC_IOSCR  = (1u<<6);                     /* G2_IO3 = 샘플링 캡 */
    TSC_IOGCSR = (1u<<1);                     /* G2 그룹 enable */
    /* CTPH=2cyc CTPL=2cyc, PGPSC=/32(24MHz→750kHz), MCV=16383, TSCE */
    TSC_CR = (1u<<28) | (1u<<24) | (5u<<12) | (6u<<5) | 1u;
}
static uint32_t tsc_read(int ch)              /* ch 0=PB4(IO1) 1=PB5(IO2), 값↓=터치 */
{
    TSC_IOCCR = (1u << (4+ch));               /* 전극 하나씩 (G2_IO1=bit4, IO2=bit5) */
    TSC_ICR   = 3u;                           /* EOAF/MCEF 클리어 */
    TSC_CR   |= (1u<<1);                      /* START */
    while (!(TSC_ISR & 3u)) {}
    if (TSC_ISR & 2u) return 0x3FFF;          /* max count error → no touch 취급 */
    return TSC_IOG2CR & 0x3FFFu;
}

/* ── 터치 로직: 짧은터치=영상이동, 긴터치=밝기, 터치중=PA4 LED ── */
#define LONG_FRAMES 5                         /* 채널당 스캔 5회 ≈ 0.7s (2프레임당 1스캔) */
static uint32_t tsc_base[2];
static int tsc_dur[2] = {0,0};
static volatile int vid_step = 0;
static uint32_t brightness = BRIGHTNESS;

static void touch_calibrate(void)
{
    uint32_t s0=0, s1=0;
    for (int i=0;i<16;i++){ s0 += tsc_read(0); s1 += tsc_read(1); }
    tsc_base[0] = s0/16; tsc_base[1] = s1/16;
}
static int tsc_touched[2] = {0,0};

static void touch_eval(int ch, uint32_t c)     /* 측정 완료된 채널 하나 판정 */
{
    tsc_touched[ch] = (c < tsc_base[ch] - tsc_base[ch]/8);   /* 12.5% 하락 = 터치 */
    if (tsc_touched[0] || tsc_touched[1]) SET(GPIOA,4); else CLR(GPIOA,4);

    /* ch0 = TOUCH1: 짧게=다음, 길게=밝게 / ch1 = TOUCH2: 짧게=이전, 길게=어둡게 */
    if (tsc_touched[ch]) {
        tsc_dur[ch]++;
        if (tsc_dur[ch] >= LONG_FRAMES && ((tsc_dur[ch]-LONG_FRAMES) % 2)==0) {
            if (ch==0 && brightness<100) brightness += 5;
            if (ch==1 && brightness>5)   brightness -= 5;
            T1_CCR3 = brightness;
        }
    } else {
        if (tsc_dur[ch] > 0 && tsc_dur[ch] < LONG_FRAMES)
            vid_step = (ch==0) ? +1 : -1;
        tsc_dur[ch] = 0;
    }
}

/* 비동기 터치: 측정 시작만 걸고 리턴 → 디코드/DMA 동안 TSC 하드웨어가 측정
 * (블로킹 ~34ms/frame → ~0. 채널은 프레임마다 교대, LONG_FRAMES는 채널당 스캔 기준) */
static int tsc_ch = 0, tsc_started = 0;
static void touch_poll(void)
{
    if (tsc_started && (TSC_ISR & 3u)) {       /* 이전 측정 끝났으면 결과 회수 */
        uint32_t c = (TSC_ISR & 2u) ? 0x3FFFu : (TSC_IOG2CR & 0x3FFFu);
        touch_eval(tsc_ch, c);
        tsc_ch ^= 1;
        tsc_started = 0;
    }
    if (!tsc_started) {                        /* 다음 채널 측정 시작(비동기) */
        TSC_IOCCR = (1u << (4+tsc_ch));
        TSC_ICR   = 3u;
        TSC_CR   |= (1u<<1);
        tsc_started = 1;
    }
}

/* ---------- SPI2 ---------- */
static void spi_tx(const uint8_t *d, uint32_t n)
{
    SPI2_CR2 = n;                       /* TSIZE */
    SPI2_CR1 |= SPI_SPE;
    SPI2_CR1 |= SPI_CSTART;
    for (uint32_t i = 0; i < n; i++) {
        while (!(SPI2_SR & SR_TXP)) {}
        *(volatile uint8_t *)&SPI2_TXDR = d[i];
    }
    while (!(SPI2_SR & SR_EOT)) {}
    SPI2_IFCR = 0xFFFFFFFFu;
    SPI2_CR1 &= ~SPI_SPE;
}
static void lcd_cmd(uint8_t c)  { DC_CMD(); CS_LOW(); spi_tx(&c,1); CS_HIGH(); }
static void lcd_dat(uint8_t d)  { DC_DAT(); CS_LOW(); spi_tx(&d,1); CS_HIGH(); }

static void delay(volatile uint32_t n) { while (n--) __asm__ volatile("nop"); }

/* ---------- LCD (ST7735, L412 시퀀스) ---------- */
/* delay 상수는 48MHz 기준 (기존 12MHz 값 ×4) */
static void lcd_reset(void) { RST_HIGH(); delay(320000); RST_LOW(); delay(800000); RST_HIGH(); delay(6000000); }
static void lcd_init(void)
{
    lcd_cmd(0x01); delay(6000000);
    lcd_cmd(0x11); delay(6000000);
    lcd_cmd(0x3A); lcd_dat(0x05);
    lcd_cmd(0x36); lcd_dat(0x08);
    lcd_cmd(0x20);
    lcd_cmd(0x29); delay(4000000);
}
static void lcd_window(void)
{
    lcd_cmd(0x2A); lcd_dat(0); lcd_dat(COL_OFFSET); lcd_dat(0); lcd_dat(COL_OFFSET+LCD_W-1);
    lcd_cmd(0x2B); lcd_dat(0); lcd_dat(ROW_OFFSET); lcd_dat(0); lcd_dat(ROW_OFFSET+LCD_H-1);
}
static void lcd_fill(uint16_t c)
{
    uint8_t hi=c>>8, lo=c&0xFF;
    lcd_window(); lcd_cmd(0x2C);
    DC_DAT(); CS_LOW();
    for (uint32_t i=0;i<(uint32_t)LCD_W*LCD_H;i++){ spi_tx(&hi,1); spi_tx(&lo,1); }
    CS_HIGH();
}
/* ── DMA 더블버퍼 blit (GPDMA1 CH0 → SPI2_TX req=9) ──
 * blit_start()는 즉시 리턴 → 전송 중 CPU는 다음 프레임 디코드 (파이프라이닝) */
#define DMACH0 0x40020050UL          /* GPDMA1(0x40020000) + 0x50 */
#define D_CFCR (*(vu32 *)(DMACH0+0x0C))
#define D_CSR  (*(vu32 *)(DMACH0+0x10))
#define D_CCR  (*(vu32 *)(DMACH0+0x14))
#define D_CTR1 (*(vu32 *)(DMACH0+0x40))
#define D_CTR2 (*(vu32 *)(DMACH0+0x44))
#define D_CBR1 (*(vu32 *)(DMACH0+0x48))
#define D_CSAR (*(vu32 *)(DMACH0+0x4C))
#define D_CDAR (*(vu32 *)(DMACH0+0x50))

static int blit_busy = 0;

static void blit_wait(void)          /* 이전 DMA blit 완료까지 (있다면) */
{
    if (!blit_busy) return;
    while (!(SPI2_SR & SR_EOT)) {}   /* SPI가 마지막 바이트까지 밀어냈는지 */
    SPI2_IFCR = 0xFFFFFFFFu;
    SPI2_CR1 &= ~SPI_SPE;
    SPI2_CFG1 &= ~(1u<<15);          /* TXDMAEN off (lcd_cmd 등 CPU 전송과 충돌 방지) */
    D_CFCR = 0x00007F00u;            /* DMA 플래그 클리어 */
    CS_HIGH();
    blit_busy = 0;
}
static void blit_start(uint16_t *buf)
{
    uint32_t len = LCD_W*LCD_H*2u;
    lcd_window(); lcd_cmd(0x2C);     /* 윈도우/RAMWR은 CPU로 (짧음) */
    DC_DAT(); CS_LOW();
    D_CFCR = 0x00007F00u;
    D_CTR1 = (1u<<3);                /* SINC=1, 폭 8bit(mem→periph) */
    D_CTR2 = 9u | (1u<<10);          /* REQSEL=SPI2_TX, DREQ=목적지 요청 */
    D_CBR1 = len;
    D_CSAR = (uint32_t)buf;
    D_CDAR = (uint32_t)&SPI2_TXDR;
    SPI2_CR2 = len;                  /* TSIZE */
    SPI2_CFG1 |= (1u<<15);           /* TXDMAEN */
    D_CCR = 1u;                      /* DMA EN */
    SPI2_CR1 |= SPI_SPE;
    SPI2_CR1 |= SPI_CSTART;
    blit_busy = 1;
}

/* ---------- OCTOSPI: quad mmap 초기화 (octospi_read.c 검증본) ---------- */
static void ospi_tcf(void){ while(!(O_SR&OSR_TCF)){} O_FCR=OSR_TCF; }
static void ospi_busy(void){ while(O_SR&OSR_BUSY){} }
static void flash_cmd(uint8_t inst, const uint8_t *data, int len)
{
    ospi_busy();
    O_CR = 1u;
    if (len) O_DLR = (uint32_t)(len-1);
    O_CCR = (1u<<0) | (len?(1u<<24):0);
    O_IR = inst;
    for (int i=0;i<len;i++) *(volatile uint8_t*)&O_DR = data[i];
    ospi_tcf(); ospi_busy();
}
static void octospi_init(void)
{
    O_CR = 0;
    O_DCR1 = (22u<<16);           /* 8MB */
    O_DCR2 = 0u;                  /* prescaler /1 = 커널클럭 그대로 (P25Q64 104MHz까지 여유) */
    flash_cmd(0x06,0,0);          /* WREN */
    uint8_t qe=0x02; flash_cmd(0x31,&qe,1);   /* QE=1 */
    delay(400000);
    O_CR = 0;
    O_ABR = 0;
    O_TCR = 4u;                   /* dummy */
    O_CCR = (1u<<0)|(3u<<8)|(2u<<12)|(3u<<16)|(0u<<20)|(3u<<24);
    O_IR = 0xEB;
    O_CR = (3u<<28) | 1u;         /* memory-mapped + EN */
}

/* ---------- tjpgd 콜백 (flash는 mmap memcpy) ---------- */
typedef struct { uint32_t addr, end; } Stream;
static size_t jd_in(JDEC *jd, uint8_t *buf, size_t nb)
{
    Stream *s = (Stream *)jd->device;
    if (s->addr + nb > s->end) nb = s->end - s->addr;
    if (buf) memcpy(buf, (const void *)(FLASH_MMAP + s->addr), nb);
    s->addr += nb;
    return nb;
}
static int jd_out(JDEC *jd, void *bitmap, JRECT *rect)
{
    (void)jd;
    uint16_t *px = (uint16_t *)bitmap;
    int w = rect->right-rect->left+1, h = rect->bottom-rect->top+1;
    for (int y=0;y<h;y++){ int dy=rect->top+y; if(dy>=LCD_H)break;
        for (int x=0;x<w;x++){ int dx=rect->left+x; if(dx>=LCD_W)break;
            uint16_t p=px[y*w+x]; decode_buf[dy*LCD_W+dx]=(p>>8)|(p<<8); } }
    return 1;
}

/* ---------- 재생 (mmap; L412 로직) ---------- */
static void play(void)
{
    uint32_t n = FLASH_MMAP[4]|(FLASH_MMAP[5]<<8)|(FLASH_MMAP[6]<<16)|(FLASH_MMAP[7]<<24);
    if (n==0 || n>100) n=1;
    int v=0;
    while (1) {
        const volatile uint8_t *e = FLASH_MMAP + 8 + v*8;
        uint32_t off  = e[0]|(e[1]<<8)|(e[2]<<16)|(e[3]<<24);
        uint32_t size = e[4]|(e[5]<<8)|(e[6]<<16)|(e[7]<<24);
        Stream st = { off, off+size };
        while (st.addr < st.end - 2) {
            JDEC jd; uint32_t saved = st.addr;
            JRESULT r = jd_prepare(&jd, jd_in, jd_work, sizeof(jd_work), &st);
            if (r==JDR_OK) r = jd_decomp(&jd, jd_out, 0);
            if (r!=JDR_OK) { st.addr = saved+1; }
            else {
                frame_wait();                    /* 페이싱(대기중 WFI) */
                blit_wait();                     /* 이전 프레임 DMA 완료 대기 */
                touch_poll();                    /* SPI 조용한 틈에 터치 스캔 (§9) */
                blit_start(decode_buf);          /* 이번 프레임 DMA 시작(비동기) */
                decode_buf = (decode_buf == frame_buf[0]) ? frame_buf[1] : frame_buf[0];
                heartbeat++;                     /* 다음 디코드는 DMA와 병렬 진행 */
            }
            if (vid_step) break;                        /* 터치로 영상 전환 요청 */
            /* 다음 프레임(0xFFD8) 찾기 */
            int found=0;
            while (!found && st.addr < st.end-1) {
                uint32_t a=st.addr;
                if (FLASH_MMAP[a]==0xFF && FLASH_MMAP[a+1]==0xD8) { found=1; break; }
                st.addr++;
            }
        }
        if (vid_step) { v = (v + vid_step + (int)n) % (int)n; vid_step = 0; }
        else          { v = (v+1) % n; }
    }
}

int main(void)
{
    clock_setup();                 /* 12MHz(리셋기본) → 48MHz(기본) 또는 24MHz(-DUSE_24MHZ) */
    pacing_init();                 /* SysTick 15Hz (frame_wait/WFI용) */

    /* 페리 클럭: GPIOA/B, OCTOSPI1, SPI2, GPDMA1 */
    RCC_AHB1ENR1 |= (1u<<0);       /* GPDMA1EN (blit DMA) */
    RCC_AHB2ENR1 |= (1u<<0)|(1u<<1);
    RCC_AHB2ENR2 |= (1u<<4);
    RCC_APB1ENR1 |= (1u<<14);

    /* OCTOSPI 핀 AF10 */
    pin_af(GPIOA,2,10); pin_af(GPIOA,3,10); pin_af(GPIOA,6,10); pin_af(GPIOA,7,10);
    pin_af(GPIOB,0,10); pin_af(GPIOB,1,10);
    /* SPI2 핀 AF5 */
    pin_af(GPIOB,13,5); pin_af(GPIOB,15,5);
    /* LCD 제어 */
    pin_out(GPIOA,0); pin_out(GPIOA,8); pin_out(GPIOA,9);
    CS_HIGH();
    backlight_init();           /* PA10 TIM1_CH3 PWM 40% */

    /* ── 절전 §13: 미사용 핀 전부 Analog(MODER=11) — 입력버퍼 누설 차단.
     *    사용: PA0,2,3,4(LED),6,7,8,9,10 + SWD(PA13,14) / PB0,1,4,5,6(TSC),13,15.
     *    GPIOC/H는 클럭 자체를 안 켰으므로 불필요. */
    GPIOA->MODER |= (3u<<(1*2))|(3u<<(5*2))
                  | (3u<<(11*2))|(3u<<(12*2))|(3u<<(15*2));
    GPIOB->MODER |= (3u<<(2*2))|(3u<<(3*2))
                  | (3u<<(7*2))|(3u<<(8*2))|(3u<<(9*2))
                  | (3u<<(10*2))|(3u<<(11*2))|(3u<<(12*2))|(3u<<(14*2));

    /* ── 터치 + 상태 LED ── */
    pin_out(GPIOA,4); CLR(GPIOA,4);   /* PA4 = 터치 표시 LED */
    tsc_init();
    touch_calibrate();                /* 부팅시 무터치 기준값 (16회 평균) */

    /* ── ICACHE 켜기 (0x40030400): 디코드 가속 → 같은 fps에 sleep 시간 증가 */
    {
        vu32 *icache_sr = (vu32 *)0x40030404;
        vu32 *icache_cr = (vu32 *)0x40030400;
        while (*icache_sr & 1u) {}      /* BUSYF 대기 */
        *icache_cr |= 1u;               /* EN */
    }

    /* SPI2 신형: master, SSM, 8bit, simplex TX, mode0
     * ⚠️ SSI=1을 MASTER 켜기 '전에' — SSI=0인 채 MASTER+SSM 쓰면 내부 NSS=low로
     *    MODF(mode fault) 걸려 하드웨어가 MASTER를 자동 클리어해버림(실측 확인). */
    SPI2_CR1 = (1u<<12);                            /* SSI=1 먼저 (내부 NSS high) */
#ifdef USE_24MHZ
    SPI2_CFG1 = (7u<<0) | (0u<<28);                 /* DSIZE=8bit, MBR=0(/2) → 24/2=12MHz */
#else
    SPI2_CFG1 = (7u<<0) | (1u<<28);                 /* DSIZE=8bit, MBR=1(/4) → 48/4=12MHz (ST7735 스펙내) */
#endif
    SPI2_CFG2 = (1u<<22) | (1u<<26) | (1u<<17);     /* MASTER, SSM, COMM=01(simplex TX) */

    octospi_init();

    lcd_reset(); lcd_init();
    lcd_fill(0xFFE0);           /* 노랑: 부팅 */

    int ok = (FLASH_MMAP[0]=='M'&&FLASH_MMAP[1]=='J'&&FLASH_MMAP[2]=='P'&&FLASH_MMAP[3]=='L');
    if (!ok) { lcd_fill(0xF800); while(1){ heartbeat++; delay(1000000);} }  /* 빨강 */

    play();
    return 0;
}
