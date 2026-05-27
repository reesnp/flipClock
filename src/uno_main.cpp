#include <Arduino.h>
#include <Wire.h>
#include <Stepper.h>
#include <DS3231.h>
#include <SoftwareSerial.h>

// ── RTC ────────────────────────────────────────────────────────────────────
DS3231 rtc;
const uint8_t RTC_ADDR = 0x68;
bool rtc_ok = false;

// ── SoftwareSerial for ESP32 ───────────────────────────────────────────────
// Reduced to 9600 — SoftwareSerial is unreliable above ~38400 on Uno.
// Match UNO_BAUD on ESP32 side to 9600 if using this.
// Pins 6/7 chosen to avoid conflict if steppers 2/3 are ever re-enabled
// (they would use pins 6-13). Use 8/9 if you need 6/7 for something else.
// *** UPDATE ESP32 UNO_BAUD to match ***
#define ESP_RX_PIN 10   // Uno RX ← ESP32 TX (GPIO1)
#define ESP_TX_PIN 11   // Uno TX → ESP32 RX (GPIO3)
#define ESP_BAUD   9600
SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);

// ── Stepper ────────────────────────────────────────────────────────────────
#define STEPS 2048
Stepper stepper1(STEPS, 2, 3, 4, 5);
int  stepper1_speed    = 6;
long stepper1_position = 0;

// ── Stepper non-blocking state ─────────────────────────────────────────────
// stepper.step() blocks for seconds. We break it into small chunks
// so serial and RTC updates keep running during a flip.
volatile long  stepper1_stepsRemaining = 0;
const int      STEPS_PER_CHUNK        = 16;  // steps per loop iteration

// ── Timing ────────────────────────────────────────────────────────────────
unsigned long lastRtcUpdate = 0;
const unsigned long RTC_UPDATE_INTERVAL = 5000;
bool espConnected = false;

// ── Non-blocking serial buffer ────────────────────────────────────────────
String espSerialBuffer = "";

// ── Forward declarations ───────────────────────────────────────────────────
void handleCommand(const String &cmd);
void sendTimeToESP32();
bool rtcClockHalted();
bool clearRtcCH();

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(ESP_BAUD);
  delay(1000);
  Serial.println("\n=== Flip Clock Uno Starting ===");

  // SoftwareSerial for ESP32
  espSerial.begin(ESP_BAUD);
  Serial.print("SoftwareSerial: RX=pin%d TX=pin%d @ %d baud\n");
  Serial.println(ESP_RX_PIN);
  Serial.println(ESP_TX_PIN);
  Serial.println(ESP_BAUD);

  // RTC — correct order: beginTransmission first, then check result
  Wire.begin();
  Wire.beginTransmission(RTC_ADDR);
  rtc_ok = (Wire.endTransmission() == 0);
  if (rtc_ok) {
    rtc.setClockMode(false);  // 24-hour mode
    // Clear oscillator stop flag if set
    if (rtcClockHalted()) {
      Serial.println("RTC: CH flag set, clearing...");
      clearRtcCH() ? Serial.println("RTC: CH cleared") 
                   : Serial.println("RTC: CH clear FAILED");
    }
    Serial.println("RTC OK (I2C A4/A5)");
  } else {
    Serial.println("RTC NOT FOUND at 0x68");
  }

  // Stepper
  stepper1.setSpeed(stepper1_speed);
  Serial.println("Stepper1 ready (D2-D5, 2048 steps/rev, 6 RPM)");
  Serial.println("Setup complete\n");
}

// ── Loop ───────────────────────────────────────────────────────────────────
void loop() {

  // ── Non-blocking stepper — run a small chunk each loop ────────────────
  if (stepper1_stepsRemaining != 0) {
    int chunk = STEPS_PER_CHUNK;
    if (stepper1_stepsRemaining < 0) chunk = -STEPS_PER_CHUNK;
    if (abs(stepper1_stepsRemaining) < STEPS_PER_CHUNK)
      chunk = (int)stepper1_stepsRemaining;

    stepper1.step(chunk);
    stepper1_position      += chunk;
    stepper1_stepsRemaining -= chunk;
  }

  // ── Non-blocking ESP32 serial read ────────────────────────────────────
  while (espSerial.available()) {
    char c = espSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      espSerialBuffer.trim();
      if (espSerialBuffer.length() > 0) {
        String cmd = espSerialBuffer;
        espSerialBuffer = "";

        if (cmd == "READY") {
          espConnected = true;
          Serial.println("ESP32 connected (READY received)");
          sendTimeToESP32();
        } else {
          handleCommand(cmd);
        }
      }
    } else {
      espSerialBuffer += c;
      if (espSerialBuffer.length() > 128)
        espSerialBuffer.remove(0, espSerialBuffer.length() - 128);
    }
  }

  // ── Periodic time send ─────────────────────────────────────────────────
  if (millis() - lastRtcUpdate >= RTC_UPDATE_INTERVAL) {
    lastRtcUpdate = millis();
    if (espConnected) sendTimeToESP32();
  }
}

// ── Command handler ────────────────────────────────────────────────────────
void handleCommand(const String &cmd) {
  Serial.println("CMD: " + cmd);

  if (cmd == "FLIP1") {
    // Queue steps — non-blocking, executed in loop chunks
    stepper1_stepsRemaining = STEPS;
    Serial.println("FLIP1 queued");

  } else if (cmd == "FLIP") {
    stepper1_stepsRemaining = STEPS;
    Serial.println("FLIP all queued (stepper1)");

  } else if (cmd.startsWith("SPEED1:")) {
    stepper1_speed = constrain(cmd.substring(7).toInt(), 1, 15);
    stepper1.setSpeed(stepper1_speed);
    Serial.println("Speed1: " + String(stepper1_speed));

  } else if (cmd.startsWith("MOVE1:")) {
    int steps = cmd.substring(6).toInt();
    stepper1_stepsRemaining = steps;  // queued, non-blocking
    Serial.println("MOVE1 queued: " + String(steps) + " steps");

  } else if (cmd.startsWith("BL:")) {
    Serial.println("BL: " + cmd.substring(3));  // future use

  } else if (cmd == "TIME") {
    sendTimeToESP32();

  } else if (cmd.startsWith("SETTIME:")) {
    int year, month, day, hour, minute, second;
    if (sscanf(cmd.c_str(), "SETTIME:%d-%d-%d %d:%d:%d",
               &year, &month, &day, &hour, &minute, &second) == 6) {
      if (!rtc_ok) {
        Serial.println("TIME SET FAIL: no RTC");
      } else if (year  >= 2000 && year  < 2100
              && month >= 1    && month <= 12
              && day   >= 1    && day   <= 31
              && hour  >= 0    && hour  <  24
              && minute >= 0   && minute < 60
              && second >= 0   && second < 60) {
        rtc.setYear(year - 2000);
        rtc.setMonth(month);
        rtc.setDate(day);
        rtc.setHour(hour);
        rtc.setMinute(minute);
        rtc.setSecond(second);
        Serial.println("TIME SET OK");
        sendTimeToESP32();
      } else {
        Serial.println("TIME SET FAIL: range error");
      }
    } else {
      Serial.println("TIME SET FAIL: parse error");
    }

  } else {
    Serial.println("Unknown cmd: " + cmd);
  }
}

// ── Send time to ESP32 ─────────────────────────────────────────────────────
void sendTimeToESP32() {
  if (!rtc_ok) {
    espSerial.println("RTC_NOT_FOUND");
    return;
  }

  // Check oscillator stop flag — RTC may have lost power
  if (!rtc.oscillatorCheck()) {
    Serial.println("RTC: oscillator stopped");
    espSerial.println("RTC_OSC_STOP");
    return;
  }

  // Check clock-halt bit
  if (rtcClockHalted()) {
    Serial.println("RTC: CH bit set");
    clearRtcCH();
    delay(20);
  }

  // Retry a few times in case of I2C noise
  const int MAX_ATTEMPTS = 3;
  uint8_t hour = 0, minute = 0, second = 0;
  bool valid = false;
  for (int i = 0; i < MAX_ATTEMPTS && !valid; i++) {
    bool h12 = false, PM = false;
    hour   = rtc.getHour(h12, PM);
    minute = rtc.getMinute();
    second = rtc.getSecond();
    if (hour < 24 && minute < 60 && second < 60) valid = true;
    else delay(10);
  }

  if (!valid) {
    Serial.print("TIME_INVALID (");
  Serial.print(hour);
  Serial.print(",");
  Serial.print(minute);
  Serial.print(",");
  Serial.print(second);
  Serial.println(")");
    espSerial.println("TIME_INVALID");
    return;
  }

  // Send TIME:HH:MM:SS
  char buf[16];
  snprintf(buf, sizeof(buf), "TIME:%02d:%02d:%02d", hour, minute, second);
  espSerial.println(buf);
  Serial.println("Sent: " + String(buf));
}

// ── RTC CH bit helpers ─────────────────────────────────────────────────────
bool rtcClockHalted() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return true;
  Wire.requestFrom(RTC_ADDR, (uint8_t)1);
  if (Wire.available() < 1) return true;
  return (Wire.read() & 0x80);
}

bool clearRtcCH() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  Wire.requestFrom(RTC_ADDR, (uint8_t)1);
  if (Wire.available() < 1) return false;
  uint8_t raw = Wire.read() & 0x7F;
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  Wire.write(raw);
  return (Wire.endTransmission() == 0);
}