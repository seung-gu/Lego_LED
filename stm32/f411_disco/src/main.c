#include "stm32f4xx_hal.h"
#include <math.h>
#include <string.h>
#include "tjpgd.h"

// 🦆 ST7735 0.42" 96x54 패널 핀 매핑 (SPI2)
//   SCK  = PB13
//   MOSI = PB15
//   CS   = PB12
//   DC   = PB1
//   RES  = PB0

// 🦆 MX25L12833F 외장 플래시 핀 매핑 (SPI1)
//   SCK  = PA5
//   MISO = PA6
//   MOSI = PA7
//   CS   = PA4

#define LCD_W  96
#define LCD_H  54

// 작은 0.42" 패널은 보통 화면 중앙 영역만 쓰므로 offset이 필요해요.
// 안 보이면 이 값들을 먼저 의심하세요! (보통 24/0 또는 0/24 근방)
#define COL_OFFSET 16
#define ROW_OFFSET 0

#define CS_LOW()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)
#define CS_HIGH()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)
#define DC_CMD()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,  GPIO_PIN_RESET)
#define DC_DAT()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,  GPIO_PIN_SET)
#define RST_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  GPIO_PIN_RESET)
#define RST_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  GPIO_PIN_SET)

#define FLASH_CS_LOW()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define FLASH_CS_HIGH() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

SPI_HandleTypeDef hspi1;   // 외장 플래시
SPI_HandleTypeDef hspi2;   // LCD
DMA_HandleTypeDef hdma_spi2_tx;   // LCD DMA
TIM_HandleTypeDef htim4;          // 백라이트 PWM (PB6)

// 이중 버퍼링: 한 쪽은 디코딩, 다른 쪽은 DMA로 LCD 전송
static uint16_t frame_a[LCD_W * LCD_H];
static uint16_t frame_b[LCD_W * LCD_H];
static volatile uint16_t *back_buf  = frame_a;   // 현재 디코딩 대상
static volatile uint16_t *front_buf = frame_b;   // DMA 전송 중
static volatile int dma_busy = 0;

// 터치 센서: 짧게 = 다음 영상, 길게(500ms+) = 밝기 -20
#define LONG_PRESS_MS 1000   // 1초 이내 = 다음 영상, 1초 이상 = 밝기 조절
static volatile uint32_t touch_press_tick = 0;  // 누른 시점
static volatile int short_touch = 0;            // 짧게 떼면 1
static volatile int long_touch  = 0;            // 길게 누르면 1
static volatile int skip_video  = 0;
static int brightness = 100;

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void SPI2_Init(void);
static void LCD_Reset(void);
static void LCD_WriteCmd(uint8_t cmd);
static void LCD_WriteData(uint8_t data);
static void LCD_Init(void);
static void LCD_FillColor(uint16_t color);
static void LCD_DrawColorBars(void);
static void LCD_DrawMandelbrot(void);
static void LCD_DrawRoseFlower(void);
static void LCD_DrawBytes(const uint8_t *bytes, int n);
static void SPI1_Init(void);
static void DMA_Init(void);
static void Backlight_Init(void);
static void MX25_ReadID(uint8_t rx[4]);
static void MX25_Read(uint32_t addr, uint8_t *buf, uint32_t len);
static void PlayFirstFrame(void);
void Error_Handler(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    SPI2_Init();
    SPI1_Init();
    DMA_Init();
    Backlight_Init();

    LCD_Reset();
    LCD_Init();

    // 🦆 단계별 LED 진단
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);  // LD4: 항상 ON (살아있음)

    // 칩 리셋: RSTEN(0x66) + RST(0x99) — CH341A 후 모드 정리
    uint8_t rsten = 0x66, rst = 0x99;
    FLASH_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &rsten, 1, HAL_MAX_DELAY);
    FLASH_CS_HIGH();
    HAL_Delay(1);
    FLASH_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &rst, 1, HAL_MAX_DELAY);
    FLASH_CS_HIGH();
    HAL_Delay(50);  // 리셋 완료 대기

    uint8_t raw[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    MX25_ReadID(raw);

    // LD3 (PD13): SPI에서 0xFF 아닌 뭔가 받았으면 ON
    int got_something = (raw[0]!=0xFF) || (raw[1]!=0xFF) || (raw[2]!=0xFF) || (raw[3]!=0xFF);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, got_something ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // LD5 (PD14): 0xC2 (Macronix) 발견하면 ON
    int got_macronix = (raw[0]==0xC2) || (raw[1]==0xC2) || (raw[2]==0xC2) || (raw[3]==0xC2);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, got_macronix ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // LD6 (PD15): flash 'M' 읽으면 ON
    uint8_t magic[8] = {0};
    MX25_Read(0, magic, 8);
    int got_m = 0;
    for (int i = 0; i < 8; i++) if (magic[i]=='M') got_m = 1;
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, got_m ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // 🦆 첫 번째 JPEG 프레임 디코딩 후 LCD 출력
    LCD_FillColor(0xFFE0);  // 진행 중 표시: 노란 화면
    PlayFirstFrame();

    while (1)
    {
        // LED 진단 결과 유지 — 토글 없음
        HAL_Delay(100);
    }
}

// ───────── LCD 함수들 ─────────

static void LCD_Reset(void)
{
    RST_HIGH(); HAL_Delay(5);
    RST_LOW();  HAL_Delay(20);
    RST_HIGH(); HAL_Delay(150);
}

static void LCD_WriteCmd(uint8_t cmd)
{
    DC_CMD();
    CS_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);
    CS_HIGH();
}

static void LCD_WriteData(uint8_t data)
{
    DC_DAT();
    CS_LOW();
    HAL_SPI_Transmit(&hspi2, &data, 1, HAL_MAX_DELAY);
    CS_HIGH();
}

static void LCD_Init(void)
{
    LCD_WriteCmd(0x01);       // SWRESET
    HAL_Delay(150);
    LCD_WriteCmd(0x11);       // SLPOUT (sleep out)
    HAL_Delay(150);

    LCD_WriteCmd(0x3A);       // COLMOD
    LCD_WriteData(0x05);      // 16-bit/pixel (RGB565)

    LCD_WriteCmd(0x36);       // MADCTL
    LCD_WriteData(0x08);      // BGR 순서

    LCD_WriteCmd(0x20);       // INVOFF

    LCD_WriteCmd(0x29);       // DISPON (display on)
    HAL_Delay(100);
}

static void LCD_FillColor(uint16_t color)
{
    uint8_t hi = color >> 8, lo = color & 0xFF;

    // CASET (열 범위)
    LCD_WriteCmd(0x2A);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET + LCD_W - 1);

    // RASET (행 범위)
    LCD_WriteCmd(0x2B);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET + LCD_H - 1);

    // RAMWR (픽셀 데이터 시작)
    LCD_WriteCmd(0x2C);
    DC_DAT();
    CS_LOW();
    for (uint32_t i = 0; i < (uint32_t)LCD_W * LCD_H; i++)
    {
        HAL_SPI_Transmit(&hspi2, &hi, 1, HAL_MAX_DELAY);
        HAL_SPI_Transmit(&hspi2, &lo, 1, HAL_MAX_DELAY);
    }
    CS_HIGH();
}

// 🦆 SMPTE 컬러바: 흰, 노랑, 시안, 초록, 마젠타, 빨강, 파랑, 검정 (왼→오)
static void LCD_DrawColorBars(void)
{
    static const uint16_t bars[8] = {
        0xFFFF,  // 흰
        0xFFE0,  // 노랑 (R+G)
        0x07FF,  // 시안 (G+B)
        0x07E0,  // 초록
        0xF81F,  // 마젠타 (R+B)
        0xF800,  // 빨강
        0x001F,  // 파랑
        0x0000,  // 검정
    };

    // 전체 화면 윈도우 설정
    LCD_WriteCmd(0x2A);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET + LCD_W - 1);

    LCD_WriteCmd(0x2B);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET + LCD_H - 1);

    LCD_WriteCmd(0x2C);  // RAMWR
    DC_DAT();
    CS_LOW();
    for (uint16_t y = 0; y < LCD_H; y++)
    {
        for (uint16_t x = 0; x < LCD_W; x++)
        {
            uint16_t c = bars[x * 8 / LCD_W];   // 8등분
            uint8_t hi = c >> 8, lo = c & 0xFF;
            HAL_SPI_Transmit(&hspi2, &hi, 1, HAL_MAX_DELAY);
            HAL_SPI_Transmit(&hspi2, &lo, 1, HAL_MAX_DELAY);
        }
    }
    CS_HIGH();
}

// 🦆 만델브로트 색상 매핑 — 반복 횟수에 따라 무지개 색
static uint16_t mandelbrot_color(int iter, int max_iter)
{
    if (iter >= max_iter) return 0x0000;  // 집합 내부는 검정

    // HSV→RGB 흉내: 반복 횟수를 hue로
    int t = (iter * 6 * 32) / max_iter;     // 0~191
    int region = t / 32;
    int frac   = (t % 32) * 8;              // 0~255 within region

    uint8_t r=0, g=0, b=0;
    switch (region) {
        case 0: r=255;       g=frac;       b=0;          break;  // 빨→노랑
        case 1: r=255-frac;  g=255;        b=0;          break;  // 노랑→초록
        case 2: r=0;         g=255;        b=frac;       break;  // 초록→시안
        case 3: r=0;         g=255-frac;   b=255;        break;  // 시안→파랑
        case 4: r=frac;      g=0;          b=255;        break;  // 파랑→마젠타
        default:r=255;       g=0;          b=255-frac;   break;  // 마젠타→빨
    }
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static void LCD_DrawMandelbrot(void)
{
    LCD_WriteCmd(0x2A);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET + LCD_W - 1);

    LCD_WriteCmd(0x2B);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET + LCD_H - 1);

    LCD_WriteCmd(0x2C);
    DC_DAT();
    CS_LOW();

    const int MAX_ITER = 64;
    for (int py = 0; py < LCD_H; py++) {
        float ci = -1.1f + 2.2f * py / LCD_H;
        for (int px = 0; px < LCD_W; px++) {
            float cr = -2.2f + 3.0f * px / LCD_W;
            float zr = 0.0f, zi = 0.0f;
            int iter = 0;
            while (zr*zr + zi*zi < 4.0f && iter < MAX_ITER) {
                float tmp = zr*zr - zi*zi + cr;
                zi = 2.0f*zr*zi + ci;
                zr = tmp;
                iter++;
            }
            uint16_t c = mandelbrot_color(iter, MAX_ITER);
            uint8_t hi = c >> 8, lo = c & 0xFF;
            HAL_SPI_Transmit(&hspi2, &hi, 1, HAL_MAX_DELAY);
            HAL_SPI_Transmit(&hspi2, &lo, 1, HAL_MAX_DELAY);
        }
    }
    CS_HIGH();
}

// 🦆 hue(0~360) → RGB565 무지개 변환
static uint16_t hue_to_rgb565(int hue)
{
    hue = ((hue % 360) + 360) % 360;
    int region = hue / 60;
    int frac   = (hue % 60) * 255 / 60;

    uint8_t r=0, g=0, b=0;
    switch (region) {
        case 0: r=255;       g=frac;       b=0;          break;
        case 1: r=255-frac;  g=255;        b=0;          break;
        case 2: r=0;         g=255;        b=frac;       break;
        case 3: r=0;         g=255-frac;   b=255;        break;
        case 4: r=frac;      g=0;          b=255;        break;
        default:r=255;       g=0;          b=255-frac;   break;
    }
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// 🦆 장미 곡선: r = cos(k*θ) — k=5면 5장 꽃잎
static void LCD_DrawRoseFlower(void)
{
    LCD_WriteCmd(0x2A);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET + LCD_W - 1);

    LCD_WriteCmd(0x2B);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET + LCD_H - 1);

    LCD_WriteCmd(0x2C);
    DC_DAT();
    CS_LOW();

    const float cx = LCD_W / 2.0f;
    const float cy = LCD_H / 2.0f;
    const float scale = (LCD_H / 2.0f) - 1.0f;
    const float k_petals = 5.0f;

    for (int py = 0; py < LCD_H; py++) {
        for (int px = 0; px < LCD_W; px++) {
            float dx = (px - cx) / scale;
            float dy = (py - cy) / scale;
            float r = sqrtf(dx*dx + dy*dy);
            float theta = atan2f(dy, dx);

            float r_rose = fabsf(cosf(k_petals * theta));

            uint16_t color;
            if (r <= r_rose) {
                // 꽃잎 내부: 각도로 hue 매핑
                int hue = (int)(theta * 180.0f / 3.14159265f) + 180;
                color = hue_to_rgb565(hue);
            } else {
                color = 0x0000;  // 배경 검정
            }

            uint8_t hi = color >> 8, lo = color & 0xFF;
            HAL_SPI_Transmit(&hspi2, &hi, 1, HAL_MAX_DELAY);
            HAL_SPI_Transmit(&hspi2, &lo, 1, HAL_MAX_DELAY);
        }
    }
    CS_HIGH();
}

// 🦆 N개 바이트를 화면에 세로 막대로 표시 (진단용)
//   각 막대 위쪽=8비트 그레이스케일, 아래쪽 가는줄=정답이면 초록/오답이면 빨강
//   막대 색: 흰색이면 0xFF, 검정이면 0x00, 회색조로 중간값
static void LCD_DrawBytes(const uint8_t *bytes, int n)
{
    // 정답: JEDEC 3 + 매직 4
    static const uint8_t expected[7] = {
        0xC2, 0x20, 0x18,        // JEDEC
        0x4D, 0x4A, 0x50, 0x4C   // 'M' 'J' 'P' 'L'
    };

    LCD_WriteCmd(0x2A);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET);
    LCD_WriteData(0); LCD_WriteData(COL_OFFSET + LCD_W - 1);

    LCD_WriteCmd(0x2B);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET);
    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET + LCD_H - 1);

    LCD_WriteCmd(0x2C);
    DC_DAT();
    CS_LOW();

    int bar_w = LCD_W / n;  // 막대 너비
    int status_h = 6;       // 하단 상태 표시줄 높이

    for (int y = 0; y < LCD_H; y++) {
        for (int x = 0; x < LCD_W; x++) {
            int idx = x / bar_w;
            if (idx >= n) idx = n - 1;

            uint16_t color;
            if (y >= LCD_H - status_h) {
                // 하단: 정답 여부 (초록=일치, 마젠타=불일치)
                color = (bytes[idx] == expected[idx]) ? 0x07E0 : 0xF81F;
            } else {
                // 상단: 바이트 값을 hue로 매핑 (0~255 → 0~359°)
                int hue = bytes[idx] * 360 / 256;
                color = hue_to_rgb565(hue);
            }
            uint8_t hi = color >> 8, lo = color & 0xFF;
            HAL_SPI_Transmit(&hspi2, &hi, 1, HAL_MAX_DELAY);
            HAL_SPI_Transmit(&hspi2, &lo, 1, HAL_MAX_DELAY);
        }
    }
    CS_HIGH();
}

// ───────── MJPEG 재생 ─────────
//
// 우리 pack 포맷:
//   [0..3]   매직 'MJPL'
//   [4..7]   파일 개수 N (LE uint32)
//   [8..]    엔트리 N개 × 8바이트 (offset:4 + size:4, LE)
//   [...]    실제 MJPEG 데이터 연속
//
// MJPEG = JPEG 프레임들이 연속된 형태. 각 프레임은 0xFFD8(SOI)로 시작, 0xFFD9(EOI)로 끝.

typedef struct {
    uint32_t flash_addr;   // 현재 읽기 위치
    uint32_t end_addr;     // 이 비디오의 끝
} JpegStream;

// TJpgDec의 입력 콜백 — JPEG 바이트를 가져오거나 건너뛰기
static size_t jd_input(JDEC *jd, uint8_t *buf, size_t nb)
{
    JpegStream *s = (JpegStream *)jd->device;
    if (s->flash_addr + nb > s->end_addr) nb = s->end_addr - s->flash_addr;
    if (buf) {
        MX25_Read(s->flash_addr, buf, nb);
    }
    // buf==NULL이면 그냥 건너뛰기 (주소만 전진)
    s->flash_addr += nb;
    return nb;
}

// TJpgDec의 출력 콜백 — 디코딩된 RGB565 픽셀 블록을 back_buf에 누적
//   (LCD 직접 출력 안 함, 나중에 DMA로 한꺼번에)
//   바이트 스왑해서 저장 (ST7735는 빅엔디안 받음)
static int jd_output(JDEC *jd, void *bitmap, JRECT *rect)
{
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
            back_buf[dy * LCD_W + dx] = (p >> 8) | (p << 8);   // 바이트 스왑
        }
    }
    return 1;
}

static void PlayFirstFrame(void)
{
    // TOC 헤더 읽기: [0..3]='MJPL', [4..7]=count
    uint8_t hdr[8];
    MX25_Read(0, hdr, 8);
    uint32_t n_videos = hdr[4] | (hdr[5]<<8) | (hdr[6]<<16) | (hdr[7]<<24);
    if (n_videos == 0 || n_videos > 100) n_videos = 1;  // 안전장치

    static uint8_t work[32000];   // 큰 JPEG 처리용

    while (1) {
        // 13개 비디오 순차 재생
        for (uint32_t v = 0; v < n_videos; v++) {
            // TOC 엔트리 읽기: 각 8바이트 (offset + size), 8바이트 헤더 뒤부터
            uint8_t entry[8];
            MX25_Read(8 + v * 8, entry, 8);
            uint32_t v_off  = entry[0] | (entry[1]<<8) | (entry[2]<<16) | (entry[3]<<24);
            uint32_t v_size = entry[4] | (entry[5]<<8) | (entry[6]<<16) | (entry[7]<<24);
            uint32_t v_end  = v_off + v_size;

            JpegStream stream = { .flash_addr = v_off, .end_addr = v_end };

            // 이중 버퍼 재생 + 터치 처리
            skip_video = 0;
            while (stream.flash_addr < stream.end_addr - 2 && !skip_video) {
                // 짧은 터치(<1초)에서 떼면 → 다음 영상
                if (short_touch) {
                    short_touch = 0;
                    skip_video = 1;
                    break;
                }
                // 길게 누름(≥1초) → 밝기 -20 (한 번만 트리거)
                if (touch_press_tick && !long_touch &&
                    (HAL_GetTick() - touch_press_tick) >= LONG_PRESS_MS) {
                    long_touch = 1;
                    if (brightness > 20) brightness -= 20;
                    else brightness = 100;
                    TIM4->CCR1 = brightness;
                }
                if (!touch_press_tick) long_touch = 0;  // 손 떼면 리셋
                // 1) back_buf에 디코딩 (CPU)
                JDEC jd;
                uint32_t saved_addr = stream.flash_addr;
                JRESULT res = jd_prepare(&jd, jd_input, work, sizeof(work), &stream);
                if (res == JDR_OK) res = jd_decomp(&jd, jd_output, 0);

                if (res != JDR_OK) {
                    // 디코딩 실패: 표시 안 함, 다음 SOI 찾으러 1바이트 전진
                    stream.flash_addr = saved_addr + 1;
                } else {
                    // 디코딩 성공: 이전 DMA 완료 대기 → 스왑 → 새 DMA 시작
                    while (dma_busy) { __WFI(); }
                    volatile uint16_t *tmp = front_buf;
                    front_buf = back_buf;
                    back_buf  = tmp;

                    LCD_WriteCmd(0x2A);
                    LCD_WriteData(0); LCD_WriteData(COL_OFFSET);
                    LCD_WriteData(0); LCD_WriteData(COL_OFFSET + LCD_W - 1);
                    LCD_WriteCmd(0x2B);
                    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET);
                    LCD_WriteData(0); LCD_WriteData(ROW_OFFSET + LCD_H - 1);
                    LCD_WriteCmd(0x2C);
                    DC_DAT();
                    CS_LOW();
                    dma_busy = 1;
                    HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)front_buf, LCD_W * LCD_H * 2);
                }

                // 5) 다음 SOI 스캔 (CPU는 DMA와 병렬로 동작)
                {
                    static uint8_t chunk[64];
                    int found = 0;
                    while (!found && stream.flash_addr < stream.end_addr - 1) {
                        uint32_t to_read = stream.end_addr - stream.flash_addr;
                        if (to_read > sizeof(chunk)) to_read = sizeof(chunk);
                        MX25_Read(stream.flash_addr, chunk, to_read);
                        for (uint32_t i = 0; i + 1 < to_read; i++) {
                            if (chunk[i] == 0xFF && chunk[i+1] == 0xD8) {
                                stream.flash_addr += i;
                                found = 1;
                                break;
                            }
                        }
                        if (!found) stream.flash_addr += to_read - 1;
                    }
                }
            }
        }
    }
}

// ───────── DMA + 인터럽트 ─────────

static void DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_spi2_tx.Instance                 = DMA1_Stream4;
    hdma_spi2_tx.Init.Channel             = DMA_CHANNEL_0;
    hdma_spi2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_spi2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_spi2_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_spi2_tx.Init.Mode                = DMA_NORMAL;
    hdma_spi2_tx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_spi2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi2_tx) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(&hspi2, hdmatx, hdma_spi2_tx);

    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    HAL_NVIC_SetPriority(SPI2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(SPI2_IRQn);
}

void DMA1_Stream4_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_spi2_tx); }
void SPI2_IRQHandler(void)         { HAL_SPI_IRQHandler(&hspi2); }
void EXTI2_IRQHandler(void)        { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2); }

// 🦆 백라이트 PWM (PB6 = TIM4 CH1, 1kHz)
static void Backlight_Init(void)
{
    __HAL_RCC_TIM4_CLK_ENABLE();

    // GPIO: PB6 alternate function (AF2 = TIM4)
    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOB, &g);

    // TIM4: 32MHz / 320 / 100 = 1kHz, 100단계 듀티
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 320 - 1;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 100 - 1;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim4);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 50;          // 시작값 50% (0~100)
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim4, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
}

// 터치 센서 콜백 — 디바운싱 적용 (50ms 이내 재 트리거 무시)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != GPIO_PIN_2) return;
    static uint32_t last_edge = 0;
    uint32_t now = HAL_GetTick();
    if (now - last_edge < 50) return;
    last_edge = now;

    // TTP223 Active HIGH: 터치 시 HIGH, 뗐을 때 LOW
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_SET) {
        // 누름 (HIGH) - 이미 눌린 상태면 무시
        if (touch_press_tick == 0) touch_press_tick = now;
    } else {
        // 뗌 (LOW)
        if (touch_press_tick) {
            uint32_t held = now - touch_press_tick;
            if (held < LONG_PRESS_MS) short_touch = 1;   // 짧게 = 밝기
            touch_press_tick = 0;
        }
    }
}

// DMA 전송 완료 콜백 — SPI2 끝나면 자동 호출
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2) {
        CS_HIGH();    // LCD CS 해제
        dma_busy = 0;
    }
}

// ───────── 초기화 ─────────

static void SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;       // SPI mode 0
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;  // 50/2 = 25MHz
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

// 🦆 MX25 JEDEC ID 읽기 (raw 응답 4바이트 그대로 반환 — 디버깅용)
static void MX25_ReadID(uint8_t rx[4])
{
    uint8_t tx[4] = {0x9F, 0xFF, 0xFF, 0xFF};
    FLASH_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 4, HAL_MAX_DELAY);
    FLASH_CS_HIGH();
}

// 🦆 MX25 READ (0x03) — 임의 주소에서 len 바이트 읽기
//   이 셋업에선 데이터가 1바이트 시프트되어, cmd 4바이트 중 마지막 응답이 데이터 첫 바이트.
//   그래서 cmd 3바이트만 보내고 1바이트 시프트 보상 후 데이터 수신.
static void MX25_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    // cmd 4바이트 + len 데이터 한 번에 전송/수신 (시프트 없음)
    uint32_t total = 4 + len;
    static uint8_t tx_buf[8200];   // 충분히 크게
    static uint8_t rx_buf[8200];
    if (total > sizeof(tx_buf)) total = sizeof(tx_buf);

    tx_buf[0] = 0x03;
    tx_buf[1] = (uint8_t)(addr >> 16);
    tx_buf[2] = (uint8_t)(addr >> 8);
    tx_buf[3] = (uint8_t)(addr & 0xFF);
    for (uint32_t i = 4; i < total; i++) tx_buf[i] = 0xFF;

    FLASH_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, total, HAL_MAX_DELAY);
    FLASH_CS_HIGH();

    // 리셋 후 정상 정렬: 데이터는 rx_buf[4]부터 (cmd 4바이트 후)
    for (uint32_t i = 0; i < len && (4 + i) < total; i++) {
        buf[i] = rx_buf[4 + i];
    }
}

static void SPI2_Init(void)
{
    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;  // 50/2 = 25MHz
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    if (HAL_SPI_Init(&hspi2) != HAL_OK) Error_Handler();
}

static void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    // SPI2 (LCD): PB13(SCK), PB15(MOSI) — Alternate Function
    g.Pin       = GPIO_PIN_13 | GPIO_PIN_15;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &g);

    // LCD 보조핀: PB12(CS), PB1(DC), PB0(RES) — 일반 출력
    g.Pin       = GPIO_PIN_12 | GPIO_PIN_1 | GPIO_PIN_0;
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = 0;
    HAL_GPIO_Init(GPIOB, &g);

    CS_HIGH();   // LCD 초기 상태 비활성

    // SPI1 (외장 플래시): PA5(SCK), PA6(MISO), PA7(MOSI) — AF5
    g.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    // 플래시 CS: PA4 — 일반 출력
    g.Pin       = GPIO_PIN_4;
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = 0;
    HAL_GPIO_Init(GPIOA, &g);

    FLASH_CS_HIGH();   // 플래시 초기 상태 비활성

    // 터치 센서(TTP223 Active HIGH): PE2 input, 풀다운, both edges EXTI
    __HAL_RCC_GPIOE_CLK_ENABLE();
    g.Pin   = GPIO_PIN_2;
    g.Mode  = GPIO_MODE_IT_RISING_FALLING;
    g.Pull  = GPIO_PULLDOWN;       // 무신호 시 LOW 유지
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &g);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);

    // 보드 LED 4개 (PD12~PD15) — 진단 표시용
    g.Pin   = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &g);
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    // HSI 16MHz → PLL → 32MHz
    osc.OscillatorType       = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState             = RCC_HSI_ON;
    osc.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState         = RCC_PLL_ON;
    osc.PLL.PLLSource        = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM             = 16;
    osc.PLL.PLLN             = 128;
    osc.PLL.PLLP             = RCC_PLLP_DIV4;  // 128/4 = 32MHz
    osc.PLL.PLLQ             = 4;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;     // PCLK1 = 32MHz
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     // PCLK2 = 32MHz
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    while (1) { }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}
