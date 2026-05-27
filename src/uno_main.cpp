#include <Arduino.h>
#include <Wire.h>
#include <Stepper.h>
#include <DS3231.h>
#include <SoftwareSerial.h>

// I2C RTC
DS3231 rtc;

// I2C address for DS3231
const uint8_t RTC_ADDR = 0x68;
bool rtc_ok = false;

// Software Serial for ESP32 communication (RX on pin 10, TX on pin 11)
// UNO RX (pin 10) receives from ESP32 TX (GPIO1)
// UNO TX (pin 11) sends to ESP32 RX (GPIO3)
SoftwareSerial espSerial(10, 11);  // RX=10, TX=11

#define STEPS 2048  // Steps per revolution for 28BYJ-48

// Stepper pin definitions (ULN2003 driver)
Stepper stepper1(STEPS, 2, 3, 4, 5);      // Motor 1: D2-D5
// Stepper stepper2(STEPS, 6, 7, 8, 9);      // Motor 2: D6-D9
// Stepper stepper3(STEPS, 10, 11, 12, 13);  // Motor 3: D10-D13

// Stepper speed settings (RPM)
int stepper1_speed = 6;
// int stepper2_speed = 6;
// int stepper3_speed = 6;

// Current positions (steps from start)
long stepper1_position = 0;
// long stepper2_position = 0;
// long stepper3_position = 0;

unsigned long lastRtcUpdate = 0;
const unsigned long RTC_UPDATE_INTERVAL = 5000;  // Send time every 5 seconds
bool espConnected = false;

// Forward declarations
void handleCommand(String cmd);
void sendTimeToESP32();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Flip Clock Uno Starting ===");
  
  // Initialize Software Serial for ESP32 communication
  espSerial.begin(9600);
  Serial.println("SoftwareSerial initialized (ESP32 comm: RX=pin10, TX=pin11)");

  // Initialize I2C for RTC
  Wire.begin();
  rtc_ok = Wire.endTransmission() == 0;  // quick bus sanity after beginTransmission below
  Wire.beginTransmission(RTC_ADDR);
  rtc_ok = (Wire.endTransmission() == 0);
  if (rtc_ok) {
    rtc.setClockMode(false);  // 24-hour mode
    Serial.println("RTC initialized (I2C A4/A5)");
  } else {
    Serial.println("RTC NOT FOUND at 0x68 (check SDA=A4, SCL=A5, VCC, GND)");
  }

  // Initialize steppers
  stepper1.setSpeed(stepper1_speed);
  // stepper2.setSpeed(stepper2_speed);
  // stepper3.setSpeed(stepper3_speed);
  Serial.println("Stepper1 initialized (D2-D5, ULN2003, 2048 steps/rev, 6 RPM)");
  // Serial.println("Stepper2 initialized (D6-D9, ULN2003, 2048 steps/rev, 6 RPM)");
  // Serial.println("Stepper3 initialized (D10-D13, ULN2003, 2048 steps/rev, 6 RPM)");

  Serial.println("Setup complete - waiting for ESP32 commands\n");
}

void loop() {
  // Check for commands from ESP32 via SoftwareSerial
  if (espSerial.available()) {
    String cmd = espSerial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "READY") {
      espConnected = true;
      Serial.println("UNO: READY received from ESP32");
      sendTimeToESP32();  // send immediately on connect
      return;
    }
    handleCommand(cmd);
  }

  // Send RTC time periodically to ESP32
  if (millis() - lastRtcUpdate >= RTC_UPDATE_INTERVAL) {
    lastRtcUpdate = millis();
    sendTimeToESP32();
  }
}

void handleCommand(String cmd) {
  if (cmd == "FLIP1") {
    Serial.println("FLIP1 command received - rotating stepper 1");
    stepper1.step(STEPS);  // Full rotation
    stepper1_position += STEPS;
    Serial.println("Flip 1 complete");
  } /*else if (cmd == "FLIP2") {
    Serial.println("FLIP2 command received - rotating stepper 2");
    stepper2.step(STEPS);  // Full rotation
    stepper2_position += STEPS;
    Serial.println("Flip 2 complete");
  } else if (cmd == "FLIP3") {
    Serial.println("FLIP3 command received - rotating stepper 3");
    stepper3.step(STEPS);  // Full rotation
    stepper3_position += STEPS;
    Serial.println("Flip 3 complete");
  }*/ else if (cmd == "FLIP") {
    // Flip all (just stepper1 for now)
    Serial.println("FLIP all command received - rotating stepper 1");
    stepper1.step(STEPS);
    // stepper2.step(STEPS);
    // stepper3.step(STEPS);
    stepper1_position += STEPS;
    // stepper2_position += STEPS;
    // stepper3_position += STEPS;
    Serial.println("Flip all complete");
  } else if (cmd.startsWith("SPEED1:")) {
    stepper1_speed = cmd.substring(7).toInt();
    if (stepper1_speed < 1) stepper1_speed = 1;
    if (stepper1_speed > 15) stepper1_speed = 15;
    stepper1.setSpeed(stepper1_speed);
    Serial.print("Stepper 1 speed set to ");
    Serial.println(stepper1_speed);
  } /*else if (cmd.startsWith("SPEED2:")) {
    stepper2_speed = cmd.substring(7).toInt();
    if (stepper2_speed < 1) stepper2_speed = 1;
    if (stepper2_speed > 15) stepper2_speed = 15;
    stepper2.setSpeed(stepper2_speed);
    Serial.print("Stepper 2 speed set to ");
    Serial.println(stepper2_speed);
  } else if (cmd.startsWith("SPEED3:")) {
    stepper3_speed = cmd.substring(7).toInt();
    if (stepper3_speed < 1) stepper3_speed = 1;
    if (stepper3_speed > 15) stepper3_speed = 15;
    stepper3.setSpeed(stepper3_speed);
    Serial.print("Stepper 3 speed set to ");
    Serial.println(stepper3_speed);
  }*/ else if (cmd.startsWith("MOVE1:")) {
    int steps = cmd.substring(6).toInt();
    Serial.print("Moving stepper 1 by ");
    Serial.print(steps);
    Serial.println(" steps");
    stepper1.step(steps);
    stepper1_position += steps;
    Serial.println("Move 1 complete");
  } /*else if (cmd.startsWith("MOVE2:")) {
    int steps = cmd.substring(6).toInt();
    Serial.print("Moving stepper 2 by ");
    Serial.print(steps);
    Serial.println(" steps");
    stepper2.step(steps);
    stepper2_position += steps;
    Serial.println("Move 2 complete");
  } else if (cmd.startsWith("MOVE3:")) {
    int steps = cmd.substring(6).toInt();
    Serial.print("Moving stepper 3 by ");
    Serial.print(steps);
    Serial.println(" steps");
    stepper3.step(steps);
    stepper3_position += steps;
    Serial.println("Move 3 complete");
  }*/ else if (cmd.startsWith("BL:")) {
    // Brightness control command (future use)
    int brightness = cmd.substring(3).toInt();
    Serial.print("Brightness: ");
    Serial.println(brightness);
  } else if (cmd == "TIME") {
    // Send time on demand
    sendTimeToESP32();
  } else if (cmd.startsWith("SETTIME:")) {
    // Expect format: SETTIME:YYYY-MM-DD HH:MM:SS
    int year, month, day, hour, minute, second;
    if (sscanf(cmd.c_str(), "SETTIME:%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
      if (!rtc_ok) {
        Serial.println("TIME SET FAIL (rtc missing)");
      } else if (year >= 2000 && year < 2100 && month >= 1 && month <= 12 && day >= 1 && day <= 31 && hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60) {
        rtc.setYear(year - 2000);
        rtc.setMonth(month);
        rtc.setDate(day);
        rtc.setHour(hour);
        rtc.setMinute(minute);
        rtc.setSecond(second);
        Serial.println("TIME SET OK");
        sendTimeToESP32();  // Echo updated time back to ESP32
      } else {
        Serial.println("TIME SET FAIL (range)");
      }
    } else {
      Serial.println("TIME SET FAIL (parse)");
    }
  } else {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
  }
}

void sendTimeToESP32() {
  if (!rtc_ok) {
    Serial.println("RTC_NOT_FOUND");
    espSerial.println("RTC_NOT_FOUND");
    return;
  }
  // Read time from RTC
  bool h12 = false, PM_time = false;
  uint8_t hour = rtc.getHour(h12, PM_time);
  uint8_t minute = rtc.getMinute();
  uint8_t second = rtc.getSecond();

  // Validate the read; if the RTC is not wired/responding, DS3231 lib can return garbage (e.g., 0xFF -> 165)
  if (hour >= 24 || minute >= 60 || second >= 60) {
    Serial.println("TIME_INVALID");
    espSerial.println("TIME_INVALID");
    return;
  }

  // Send to ESP32 in format: "TIME:HH:MM:SS"
  espSerial.print("TIME:");
  if (hour < 10) espSerial.print("0");
  espSerial.print(hour);
  espSerial.print(":");
  if (minute < 10) espSerial.print("0");
  espSerial.print(minute);
  espSerial.print(":");
  if (second < 10) espSerial.print("0");
  espSerial.println(second);
  
  // Also print to USB serial for debugging
  Serial.print("Sent to ESP32: TIME:");
  if (hour < 10) Serial.print("0");
  Serial.print(hour);
  Serial.print(":");
  if (minute < 10) Serial.print("0");
  Serial.print(minute);
  Serial.print(":");
  if (second < 10) Serial.print("0");
  Serial.println(second);
}