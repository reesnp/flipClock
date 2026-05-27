// Minimal SPI test - compile with: pio run -e esp32 --target upload
// This tests if the display responds to basic SPI commands

#include <Arduino.h>
#include <SPI.h>

#define TFT_CS    5
#define TFT_DC    19
#define TFT_RST   -1
#define TFT_MOSI  23
#define TFT_SCLK  18

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== SPI Display Test ===");
  
  // Setup pins
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_CS, HIGH);  // CS high (not selected)
  digitalWrite(TFT_DC, HIGH);
  
  Serial.println("GPIO pins configured");
  Serial.flush();
  delay(100);
  
  // Initialize SPI
  Serial.println("Initializing SPI...");
  Serial.flush();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(1000000);  // 1MHz - very slow
  Serial.println("SPI initialized at 1MHz");
  Serial.flush();
  delay(100);
  
  // Test write
  Serial.println("Testing SPI write...");
  Serial.flush();
  digitalWrite(TFT_CS, LOW);  // Select
  SPI.write(0xAA);  // Send 0xAA
  SPI.write(0x55);  // Send 0x55
  digitalWrite(TFT_CS, HIGH);  // Deselect
  Serial.println("SPI write test complete");
  Serial.flush();
  delay(100);
  
  Serial.println("=== Test Complete - If you see this, SPI works ===");
}

void loop() {
  delay(1000);
  Serial.println("SPI test running...");
}
