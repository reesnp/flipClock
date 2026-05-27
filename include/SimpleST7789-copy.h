#ifndef ST7789_SIMPLE_H
#define ST7789_SIMPLE_H

#include <Arduino.h>
#include <SPI.h>
#include <esp_task_wdt.h>

class SimpleST7789 {
private:
  uint16_t width, height;
  int8_t cs_pin, dc_pin, rst_pin;
  
  // Optimized low-level commands
  void writeCommand(uint8_t cmd) {
    digitalWrite(dc_pin, LOW);
    digitalWrite(cs_pin, LOW);
    SPI.transfer(cmd);
    digitalWrite(cs_pin, HIGH);
  }

  void writeData(uint8_t data) {
    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);
    SPI.transfer(data);
    digitalWrite(cs_pin, HIGH);
  }

  void writeData16(uint16_t data) {
    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);
    SPI.transfer16(data); // Uses hardware SPI 16-bit transfer (faster)
    digitalWrite(cs_pin, HIGH);
  }

public:
  SimpleST7789(int8_t cs, int8_t dc, int8_t rst) 
    : cs_pin(cs), dc_pin(dc), rst_pin(rst), width(240), height(280) {}
  
  void init() {
    pinMode(cs_pin, OUTPUT);
    pinMode(dc_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);
    digitalWrite(dc_pin, HIGH);

    writeCommand(0x01); // Software Reset
    delay(150);
    writeCommand(0x11); // Sleep Out
    delay(100);
    writeCommand(0x3A); writeData(0x55); // 16-bit color
    writeCommand(0x36); writeData(0x00); // MADCTL
    writeCommand(0x29); // Display On
  }
  
  void fillScreen(uint16_t color) {
    fillRect(0, 0, width, height, color);
  }
  
  void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // 1. Set Address Window
    writeCommand(0x2A); writeData16(x); writeData16(x + w - 1);
    writeCommand(0x2B); writeData16(y); writeData16(y + h - 1);
    writeCommand(0x2C); // Memory Write
    
    // 2. Optimized Loop
    digitalWrite(dc_pin, HIGH);
    digitalWrite(cs_pin, LOW);
    
    uint32_t totalPixels = (uint32_t)w * h;
    // Swap bytes for SPI big-endian order
    uint16_t colorSwapped = (color << 8) | (color >> 8); 
    
    for (uint32_t i = 0; i < totalPixels; i++) {
      SPI.transfer16(colorSwapped);
      
      // Yield every 512 pixels to allow Bluetooth/WiFi tasks to breathe
      if ((i & 511) == 0) {
        esp_task_wdt_reset();
        yield(); 
      }
    }
    digitalWrite(cs_pin, HIGH);
  }
  
  uint16_t getWidth() { return width; }
  uint16_t getHeight() { return height; }
};

#endif