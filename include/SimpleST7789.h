// Simple ST7789 Display Driver
// Replaces TFT_eSPI to work reliably with our hardware

#ifndef ST7789_SIMPLE_H
#define ST7789_SIMPLE_H

#include <Arduino.h>
#include <SPI.h>
#include <esp_task_wdt.h>

class SimpleST7789 {
private:
  uint16_t width, height;
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
  SimpleST7789(int8_t cs, int8_t dc, int8_t rst) 
    : cs_pin(cs), dc_pin(dc), rst_pin(rst), width(240), height(280) {}
  
  void init() {
    Serial.println("SimpleST7789::init() starting...");
    Serial.flush();
    esp_task_wdt_reset();
    
    // Setup only valid pins
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
    
    // Reset only if pin is valid
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
    
    cmd(0x3A);  // Color mode
    data(0x55);  // 16-bit color
    esp_task_wdt_reset();
    
    cmd(0x36);  // MADCTL
    data(0x00);  // Default orientation
    esp_task_wdt_reset();
    
    cmd(0x29);  // Display on
    delay(50);
    esp_task_wdt_reset();
    
    Serial.println("SimpleST7789::init() complete");
    Serial.flush();
  }
  
  void fillScreen(uint16_t color) {
    fillRect(0, 0, width, height, color);
  }
  
  void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    cmd(0x2A);  // Column
    data16(x);
    data16(x + w - 1);
    esp_task_wdt_reset();
    
    cmd(0x2B);  // Row
    data16(y);
    data16(y + h - 1);
    esp_task_wdt_reset();
    
    cmd(0x2C);  // Write
    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);
    
    uint32_t size = (uint32_t)w * h;
    for (uint32_t i = 0; i < size; i++) {
      SPI.write(color >> 8);
      SPI.write(color & 0xFF);
      // Feed watchdog every 256 pixels
      if ((i & 0xFF) == 0) {
        esp_task_wdt_reset();
      }
    }
    digitalWrite(cs_pin, HIGH);
    esp_task_wdt_reset();
  }
  
  uint16_t getWidth() { return width; }
  uint16_t getHeight() { return height; }
  // Set address window for subsequent pixel writes
  void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    cmd(0x2A);
    data16(x0);
    data16(x1);
    cmd(0x2B);
    data16(y0);
    data16(y1);
    cmd(0x2C);
  }

  // Push an array of 16-bit RGB565 pixels to current address window
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
