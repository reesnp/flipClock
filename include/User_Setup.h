#ifndef USER_SETUP_H
#define USER_SETUP_H

// ── Driver ───────────────────────────────────────────────────────────────────
#define ST7789_DRIVER
#define TFT_RGB_ORDER TFT_BGR   // Waveshare 1.69" needs BGR order
#define TFT_INVERSION_ON        // Required for correct colors on ST7789V2

// ── Dimensions ───────────────────────────────────────────────────────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 280

// ── Pins ─────────────────────────────────────────────────────────────────────
#define TFT_CS    5
#define TFT_DC    19
#define TFT_RST   -1   // Only valid if RST pin is tied to 3.3V on your board
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_MISO  -1

// ── SPI Speed ────────────────────────────────────────────────────────────────
// Start conservative — increase to 40MHz once display is confirmed working
#define SPI_FREQUENCY        10000000   // 10MHz
#define SPI_READ_FREQUENCY    5000000
#define SPI_TOUCH_FREQUENCY   2500000

// ── Fonts ────────────────────────────────────────────────────────────────────
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7   // 7-segment style — perfect for a clock display
#define LOAD_FONT8

#endif