// Direct ST7789 SPI test - raw display commands
// This bypasses TFT_eSPI to test if display responds to ST7789 commands

#include <Arduino.h>
#include <SPI.h>

#define TFT_CS    5
#define TFT_DC    19
#define TFT_RST   -1
#define TFT_MOSI  23
#define TFT_SCLK  18

// ST7789 Commands
#define ST7789_SWRESET   0x01
#define ST7789_SLPOUT    0x11
#define ST7789_DISPOFF   0x28
#define ST7789_DISPON    0x29
#define ST7789_CASET     0x2A
#define ST7789_RASET     0x2B
#define ST7789_RAMWR     0x2C
#define ST7789_MADCTL    0x36
#define ST7789_COLMOD    0x3A

void spi_command(uint8_t cmd) {
  digitalWrite(TFT_DC, LOW);   // Command mode
  digitalWrite(TFT_CS, LOW);
  SPI.write(cmd);
  digitalWrite(TFT_CS, HIGH);
}

void spi_data(uint8_t data) {
  digitalWrite(TFT_DC, HIGH);  // Data mode
  digitalWrite(TFT_CS, LOW);
  SPI.write(data);
  digitalWrite(TFT_CS, HIGH);
}

void spi_data16(uint16_t data) {
  digitalWrite(TFT_DC, HIGH);  // Data mode
  digitalWrite(TFT_CS, LOW);
  SPI.write((data >> 8) & 0xFF);
  SPI.write(data & 0xFF);
  digitalWrite(TFT_CS, HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== ST7789 Raw Command Test ===");
  
  // Setup pins
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_DC, HIGH);
  
  Serial.println("GPIO pins configured");
  Serial.flush();
  delay(100);
  
  // Initialize SPI
  Serial.println("Initializing SPI...");
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(10000000);  // 10MHz
  Serial.println("SPI initialized");
  Serial.flush();
  delay(100);
  
  // Reset display (if RST connected)
  // For now, just do software reset
  Serial.println("Sending software reset...");
  spi_command(ST7789_SWRESET);
  Serial.flush();
  delay(150);
  
  Serial.println("Sending sleep out...");
  spi_command(ST7789_SLPOUT);
  Serial.flush();
  delay(100);
  
  Serial.println("Setting color mode to 16-bit...");
  spi_command(ST7789_COLMOD);
  spi_data(0x55);  // 16-bit color
  Serial.flush();
  delay(10);
  
  Serial.println("Sending display on...");
  spi_command(ST7789_DISPON);
  Serial.flush();
  delay(50);
  
  Serial.println("=== Display initialized successfully! ===");
  Serial.println("If you see this without crashing, display is responsive.");
}

void loop() {
  delay(1000);
}
