/*
 * SimpleST7789.h - ST7789 display driver inheriting from Adafruit_GFX
 *
 * Requires: Adafruit GFX Library
 * Install via Arduino Library Manager: "Adafruit GFX Library"
 *
 * Provides:
 *   - Fast low-level SPI functions (fillRect, fillScreen, pushPixels)
 *   - Full Adafruit_GFX drawing API (text, shapes, fonts, etc.)
 *   - All drawing is automatically watchdog-safe (ESP32)
 */

#ifndef ST7789_SIMPLE_H
#define ST7789_SIMPLE_H

#include <Arduino.h>
#include <SPI.h>
#include <esp_task_wdt.h>
#include <Adafruit_GFX.h>

class SimpleST7789 : public Adafruit_GFX {
private:
  int8_t cs_pin, dc_pin, rst_pin;

  void cmd(uint8_t c) {
    digitalWrite(dc_pin, LOW);
    digitalWrite(cs_pin, LOW);
    SPI.write(c);
    digitalWrite(cs_pin, HIGH);
    delayMicroseconds(10);
    esp_task_wdt_reset();
  }

  void data(uint8_t d) {
    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);
    SPI.write(d);
    digitalWrite(cs_pin, HIGH);
    delayMicroseconds(10);
    esp_task_wdt_reset();
  }

  void data16(uint16_t d) {
    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);
    SPI.write(d >> 8);
    SPI.write(d & 0xFF);
    digitalWrite(cs_pin, HIGH);
    delayMicroseconds(10);
    esp_task_wdt_reset();
  }

public:
  // Constructor: passes 240x320 to Adafruit_GFX
  SimpleST7789(int8_t cs, int8_t dc, int8_t rst)
    : Adafruit_GFX(240, 320), cs_pin(cs), dc_pin(dc), rst_pin(rst) {}

  // ---------- Display initialisation (unchanged) ----------
  void init() {
    Serial.println("SimpleST7789::init() starting...");
    Serial.flush();
    esp_task_wdt_reset();

    if (cs_pin >= 0) {
      pinMode(cs_pin, OUTPUT);
      digitalWrite(cs_pin, HIGH);
      Serial.printf("  CS pin %d configured\n", cs_pin);
    }
    if (dc_pin >= 0) {
      pinMode(dc_pin, OUTPUT);
      digitalWrite(dc_pin, HIGH);
      Serial.printf("  DC pin %d configured\n", dc_pin);
    }

    Serial.flush();
    esp_task_wdt_reset();

    if (rst_pin >= 0) {
      Serial.printf("  Resetting via pin %d\n", rst_pin);
      pinMode(rst_pin, OUTPUT);
      digitalWrite(rst_pin, LOW);
      delay(100);
      esp_task_wdt_reset();
      digitalWrite(rst_pin, HIGH);
      delay(100);
      Serial.println("  Hardware reset complete");
    } else {
      Serial.println("  No hardware reset pin, using software reset");
      delay(100);
    }

    Serial.flush();
    esp_task_wdt_reset();

    Serial.println("  Sending ST7789 commands...");
    cmd(0x01);  // Software reset
    delay(150);
    esp_task_wdt_reset();
    cmd(0x11);  // Sleep out
    delay(100);
    esp_task_wdt_reset();
    cmd(0x3A);  // Colour mode
    data(0x55); // 16-bit colour
    esp_task_wdt_reset();
    cmd(0x36);  // MADCTL
    data(0x00); // Default orientation
    esp_task_wdt_reset();
    cmd(0x29);  // Display on
    delay(50);
    esp_task_wdt_reset();

    Serial.println("SimpleST7789::init() complete");
    Serial.flush();
  }

  // ---------- Required by Adafruit_GFX: draw a single pixel ----------
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    // Adafruit_GFX already clips, but a check doesn't hurt
    if (x < 0 || x >= _width || y < 0 || y >= _height) return;

    setAddrWindow(x, y, x, y);
    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);
    SPI.write(color >> 8);
    SPI.write(color & 0xFF);
    digitalWrite(cs_pin, HIGH);
    // No watchdog reset needed for a single pixel
  }

  // ---------- Fast overrides (Adafruit_GFX calls these internally) ----------
  void setRotation(uint8_t r) override {
    rotation = r & 3;
    switch (rotation) {
      case 0:
        _width  = 240;
        _height = 320;
        cmd(0x36); data(0x08);  // BGR order, normal orientation
        break;
      case 1:
        _width  = 320;
        _height = 240;
        cmd(0x36); data(0x68);  // BGR + rotate 90°
        break;
      case 2:
        _width  = 240;
        _height = 320;
        cmd(0x36); data(0xC8);  // BGR + 180°
        break;
      case 3:
        _width  = 320;
        _height = 240;
        cmd(0x36); data(0xA8);  // BGR + 270°
        break;
    }
    setAddrWindow(0, 0, _width-1, _height-1);
  }
  
  void fillScreen(uint16_t color) override {
    fillRect(0, 0, _width, _height, color);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    // Clipping
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width)  w = _width  - x;
    if (y + h > _height) h = _height - y;
    if (w <= 0 || h <= 0) return;

    cmd(0x2A); // Column address
    data16(x);
    data16(x + w - 1);
    cmd(0x2B); // Row address
    data16(y);
    data16(y + h - 1);
    cmd(0x2C); // Memory write

    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);

    uint32_t size = (uint32_t)w * h;
    for (uint32_t i = 0; i < size; i++) {
      SPI.write(color >> 8);
      SPI.write(color & 0xFF);
      if ((i & 0xFF) == 0) esp_task_wdt_reset();  // feed watchdog
    }
    digitalWrite(cs_pin, HIGH);
    esp_task_wdt_reset();
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    fillRect(x, y, w, 1, color);
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    fillRect(x, y, 1, h, color);
  }

  // ---------- Additional utility functions (unchanged) ----------

  // Set the address window for raw pixel writes
  void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    cmd(0x2A);
    data16(x0);
    data16(x1);
    cmd(0x2B);
    data16(y0);
    data16(y1);
    cmd(0x2C);
  }

  // Push an array of RGB565 pixels to the current address window
  void pushPixels(const uint16_t *colors, uint32_t len) {
    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);
    for (uint32_t i = 0; i < len; i++) {
      SPI.write(colors[i] >> 8);
      SPI.write(colors[i] & 0xFF);
      if ((i & 0xFF) == 0) esp_task_wdt_reset();
    }
    digitalWrite(cs_pin, HIGH);
  }
};

#endif