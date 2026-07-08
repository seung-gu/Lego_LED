// 🦆 STM32L412KBU6 커스텀 보드 - 외장 플래시(MX25L12833F)에서 MJPEG 읽어 LCD 재생
//
// 핀맵 (UFQFPN32 칩 핀번호 → 포트):
//   [LCD - ST7735 96x54, SPI1 @ PA5/PA7]
//     led-a(백라이트 PWM)=핀6 PA0(TIM2_CH1)   dc=핀7 PA1   reset=핀8 PA2   cs=핀9 PA3
//     sclk=핀11 PA5(AF5)        mosi=핀13 PA7(AF5)
//   [Flash - MX25L12833F 16MB, SPI1 @ PB3/PB4/PB5]
//     cs=핀2 PC14 (★검증됨: PC13 아님!)   sclk=핀26 PB3(AF5)
//     miso=핀27 PB4(AF5)        mosi=핀28 PB5(AF5)
//
// 🦆 LCD와 플래시가 같은 SPI1을 다른 핀으로 공유 → spi_route_lcd()/spi_route_flash()로 전환.
//
// 📌 터치센서 보류: 현 보드는 PA9/PA10/PA11에 패드를 연결했으나 이 핀들은 TSC 미지원.
//    L412 UFQFPN32의 TSC 핀은 PA15(G3_IO1), PB4~PB7(G2_IO1~4)뿐. 단, PB4/PB5는 플래시가 점유.
//    → 나중에 회로를 TSC 제공 핀으로 수정한 뒤 터치 기능 추가 예정.

#include "stm32l4xx_hal.h"
#include <string.h>
#include "tjpgd.h"

#define LCD_W  96
#define LCD_H  54
#define COL_OFFSET 16
#define ROW_OFFSET 0
#define BRIGHTNESS 40            // 백라이트 고정 밝기 (0~100)

#define DC_CMD()       HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET)
#define DC_DAT()       HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET)
#define RST_LOW()      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET)
#define RST_HIGH()     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET)
#define LCD_CS_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET)
#define FLASH_CS_LOW()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_RESET)
#define FLASH_CS_HIGH() HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_SET)

volatile uint32_t heartbeat = 0;
SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim2;   // 백라이트 PWM (PA0 = TIM2_CH1)

static uint16_t frame[LCD_W * LCD_H];   // 단일 프레임 버퍼 (바이트스왑 저장)

void SysTick_Handler(void) { HAL_IncTick(); }
static void Error_Handler(void) { while (1) {} }

// ───────── 클럭: MSI 32MHz ─────────
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    osc.MSIState            = RCC_MSI_ON;
    osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    osc.MSIClockRange       = RCC_MSIRANGE_10;   // 32MHz
    osc.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_MSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) Error_Handler();
}

// ───────── SPI1 핀 라우팅 (LCD ↔ 플래시) ─────────
static void spi_route_lcd(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH; g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);
}
static void spi_route_flash(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH; g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &g);
}

// ───────── 백라이트 PWM (PA0 = TIM2_CH1, ~1kHz, 고정 밝기) ─────────
static void Backlight_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_0;
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW; g.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance           = TIM2;
    htim2.Init.Prescaler     = 320 - 1;
    htim2.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim2.Init.Period        = 100 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim2);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = BRIGHTNESS;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

// ───────── GPIO/SPI 초기화 ─────────
static void GPIO_SPI_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;   // DC/RST/CS
    g.Mode  = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_14; HAL_GPIO_Init(GPIOC, &g);    // 플래시 CS

    LCD_CS_HIGH();
    FLASH_CS_HIGH();

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;  // 16MHz
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    HAL_SPI_Init(&hspi1);

    spi_route_lcd();
}

// ───────── LCD ─────────
static void LCD_WriteCmd(uint8_t cmd)
{
    DC_CMD(); LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}
static void LCD_WriteData(uint8_t data)
{
    DC_DAT(); LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}
static void LCD_Reset(void)
{
    RST_HIGH(); HAL_Delay(5);
    RST_LOW();  HAL_Delay(20);
    RST_HIGH(); HAL_Delay(150);
}
static void LCD_Init(void)
{
    LCD_WriteCmd(0x01); HAL_Delay(150);
    LCD_WriteCmd(0x11); HAL_Delay(150);
    LCD_WriteCmd(0x3A); LCD_WriteData(0x05);
    LCD_WriteCmd(0x36); LCD_WriteData(0x08);
    LCD_WriteCmd(0x20);
    LCD_WriteCmd(0x29); HAL_Delay(100);
}
static void LCD_SetWindow(void)
{
    LCD_WriteCmd(0x2A);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET + LCD_W - 1);
    LCD_WriteCmd(0x2B);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET + LCD_H - 1);
}
static void LCD_FillColor(uint16_t color)
{
    uint8_t hi = color >> 8, lo = color & 0xFF;
    LCD_SetWindow();
    LCD_WriteCmd(0x2C);
    DC_DAT(); LCD_CS_LOW();
    for (uint32_t i = 0; i < (uint32_t)LCD_W * LCD_H; i++) {
        HAL_SPI_Transmit(&hspi1, &hi, 1, HAL_MAX_DELAY);
        HAL_SPI_Transmit(&hspi1, &lo, 1, HAL_MAX_DELAY);
    }
    LCD_CS_HIGH();
}
static void LCD_Blit(void)
{
    LCD_SetWindow();
    LCD_WriteCmd(0x2C);
    DC_DAT(); LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)frame, LCD_W * LCD_H * 2, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

// ───────── MX25 외장 플래시 (사용 전 spi_route_flash() 필요) ─────────
static void MX25_Reset(void)
{
    uint8_t rsten = 0x66, rst = 0x99;
    FLASH_CS_LOW();  HAL_SPI_Transmit(&hspi1, &rsten, 1, HAL_MAX_DELAY); FLASH_CS_HIGH();
    HAL_Delay(1);
    FLASH_CS_LOW();  HAL_SPI_Transmit(&hspi1, &rst,   1, HAL_MAX_DELAY); FLASH_CS_HIGH();
    HAL_Delay(50);
}
static void MX25_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    static uint8_t tx_buf[4 + 512];
    static uint8_t rx_buf[4 + 512];
    while (len) {
        uint32_t chunk = (len > 512) ? 512 : len;
        uint32_t total = 4 + chunk;
        tx_buf[0] = 0x03;
        tx_buf[1] = (uint8_t)(addr >> 16);
        tx_buf[2] = (uint8_t)(addr >> 8);
        tx_buf[3] = (uint8_t)(addr & 0xFF);
        for (uint32_t i = 4; i < total; i++) tx_buf[i] = 0xFF;
        FLASH_CS_LOW();
        HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, total, HAL_MAX_DELAY);
        FLASH_CS_HIGH();
        for (uint32_t i = 0; i < chunk; i++) buf[i] = rx_buf[4 + i];
        addr += chunk; buf += chunk; len -= chunk;
    }
}

// ───────── MJPEG 디코딩 ─────────
typedef struct { uint32_t flash_addr, end_addr; } JpegStream;

static size_t jd_input(JDEC *jd, uint8_t *buf, size_t nb)
{
    JpegStream *s = (JpegStream *)jd->device;
    if (s->flash_addr + nb > s->end_addr) nb = s->end_addr - s->flash_addr;
    if (buf) MX25_Read(s->flash_addr, buf, nb);
    s->flash_addr += nb;
    return nb;
}
static int jd_output(JDEC *jd, void *bitmap, JRECT *rect)
{
    (void)jd;
    uint16_t *pixels = (uint16_t *)bitmap;
    int w = rect->right - rect->left + 1;
    int h = rect->bottom - rect->top + 1;
    for (int y = 0; y < h; y++) {
        int dy = rect->top + y;
        if (dy >= LCD_H) break;
        for (int x = 0; x < w; x++) {
            int dx = rect->left + x;
            if (dx >= LCD_W) break;
            uint16_t p = pixels[y * w + x];
            frame[dy * LCD_W + dx] = (p >> 8) | (p << 8);
        }
    }
    return 1;
}
static uint8_t jd_work[4096];

static void PlayVideos(void)
{
    spi_route_flash();
    uint8_t hdr[8];
    MX25_Read(0, hdr, 8);
    uint32_t n = hdr[4] | (hdr[5]<<8) | (hdr[6]<<16) | (hdr[7]<<24);
    if (n == 0 || n > 100) n = 1;

    int v = 0;
    while (1) {
        uint8_t entry[8];
        MX25_Read(8 + v * 8, entry, 8);
        uint32_t v_off  = entry[0] | (entry[1]<<8) | (entry[2]<<16) | (entry[3]<<24);
        uint32_t v_size = entry[4] | (entry[5]<<8) | (entry[6]<<16) | (entry[7]<<24);
        JpegStream stream = { .flash_addr = v_off, .end_addr = v_off + v_size };

        while (stream.flash_addr < stream.end_addr - 2) {
            JDEC jd;
            uint32_t saved = stream.flash_addr;
            JRESULT res = jd_prepare(&jd, jd_input, jd_work, sizeof(jd_work), &stream);
            if (res == JDR_OK) res = jd_decomp(&jd, jd_output, 0);

            if (res != JDR_OK) {
                stream.flash_addr = saved + 1;
            } else {
                spi_route_lcd();
                LCD_Blit();
                spi_route_flash();
                heartbeat++;
            }

            static uint8_t chunk[64];
            int found = 0;
            while (!found && stream.flash_addr < stream.end_addr - 1) {
                uint32_t to_read = stream.end_addr - stream.flash_addr;
                if (to_read > sizeof(chunk)) to_read = sizeof(chunk);
                MX25_Read(stream.flash_addr, chunk, to_read);
                for (uint32_t i = 0; i + 1 < to_read; i++) {
                    if (chunk[i] == 0xFF && chunk[i+1] == 0xD8) {
                        stream.flash_addr += i; found = 1; break;
                    }
                }
                if (!found) stream.flash_addr += to_read - 1;
            }
        }
        v = (v + 1) % n;   // 다음 영상으로 순환
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_SPI_Init();
    Backlight_Init();

    LCD_Reset();
    LCD_Init();
    LCD_FillColor(0xFFE0);   // 노랑: 부팅

    spi_route_flash();
    MX25_Reset();
    uint8_t magic[8] = {0};
    MX25_Read(0, magic, 8);
    int ok = (magic[0]=='M' && magic[1]=='J' && magic[2]=='P' && magic[3]=='L');

    spi_route_lcd();
    if (!ok) {
        LCD_FillColor(0xF800);   // 빨강: 플래시 매직 실패
        while (1) { heartbeat++; HAL_Delay(200); }
    }

    PlayVideos();
    return 0;
}
