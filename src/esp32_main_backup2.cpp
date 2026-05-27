#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <SPI.h>
#include "SimpleST7789.h"

// Hardware pins - AI Thinker Audio Kit
// #define LCD_BL -1       
// #define LCD_CS 5        
// #define LCD_DC 19       
// #define LCD_RST -1      
#define PA_ENABLE_GPIO 21
#define TFT_BACKLIGHT_GPIO 22
//200279376222
// Objects
SimpleST7789 tft(5, 19, -1);  // CS=5, DC=19, RST=-1
WebServer server(80);
volatile uint32_t a2dp_bytes_last_interval = 0;
volatile uint32_t a2dp_bytes_total = 0;
volatile uint32_t a2dp_sample_rate = 0;
bool a2dp_connected = false;
volatile int16_t a2dp_sample_min = 0;
volatile int16_t a2dp_sample_max = 0;
volatile bool a2dp_sample_seen = false;

// Latest time received from Uno (parsed from "TIME:HH:MM:SS")
int uno_hour = -1;
int uno_minute = -1;
int uno_second = -1;
bool uno_connected = false;  // Set true when we receive first TIME message

// Timezone and display settings
long timezoneOffset = -18000;  // Offset in seconds (default: EST = -5 hours = -18000 seconds)
bool use24HourFormat = true;   // true = 24-hour, false = 12-hour AM/PM

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 1000;
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 5000;  // Update display every 5 seconds instead of 1
const int pwmChannel = 0;
int backlightValue = 255;
bool bluetoothEnabled = true;
String currentTrack = "No Track";
bool ntpSynced = false;
unsigned long lastNtpSync = 0;
const unsigned long ntpSyncInterval = 3600000;  // Sync every hour (3600000 ms)
unsigned long lastReadySent = 0;
const unsigned long readyRetryInterval = 2000;  // resend READY every 2s until Uno responds
int readyAttempts = 0;
const int readyAttemptsMax = 5;  // stop retrying after 5 attempts

// Debug log buffer
#define DEBUG_LOG_SIZE 20
String debugLog[DEBUG_LOG_SIZE];
int debugLogIndex = 0;

void addDebugLog(String msg) {
  debugLog[debugLogIndex] = msg;
  debugLogIndex = (debugLogIndex + 1) % DEBUG_LOG_SIZE;
}

void handleRoot();
void handleSettings();
void handleBluetooth();
void handleConnectUno();
void handleSetTime();
void handleTimeSettings();
void avrc_metadata_callback(uint8_t id, const uint8_t *text);
void avrc_connection_callback(bool connected);
void sample_rate_callback(uint16_t rate);
void stream_reader_callback(const uint8_t *data, uint32_t len);
void volume_change_callback(int volume);
void updateDisplay();
void syncUnoTimeFromNtp();

// Hardware Serial1 for Uno communication (TX=GPIO1, RX=GPIO3)
#define UNO_TX_PIN 1
#define UNO_RX_PIN 3
#define UNO_BAUD 115200

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== Flip Clock Starting ===");
  Serial.printf("Free heap: %lu bytes\n", ESP.getFreeHeap());
  Serial.flush();

  Serial1.begin(UNO_BAUD, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);
  Serial.printf("Serial1 initialized (Uno comm: TX=GPIO%d, RX=GPIO%d)\n", UNO_TX_PIN, UNO_RX_PIN);
  Serial1.println("READY");
  lastReadySent = millis();
  readyAttempts = 1;

  Serial.println("--- ESP32 WiFi startup ---");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("FlipClock-ESP32");
  Serial.println("WiFi.begin() -> GetOffMyLAN");
  WiFi.begin("GetOffMyLAN", "hazmaywal10");
  Serial.print("Connecting to WiFi");
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500);
    wifiTimeout++;
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());

    // Start NTP (will sync in background)
    configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // UTC
    Serial.println("NTP client started (will sync in background)");
  } else {
    Serial.print("WiFi connection failed, status=");
    Serial.println(WiFi.status());
    Serial.println("Starting fallback AP: FlipClock-AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("FlipClock-AP");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  pinMode(PA_ENABLE_GPIO, OUTPUT);
  digitalWrite(PA_ENABLE_GPIO, HIGH);
  Serial.println("Power Amplifier enabled (GPIO21)");

  // Setup display backlight (simple HIGH, no PWM initially to test)
  pinMode(TFT_BACKLIGHT_GPIO, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_GPIO, HIGH);  // Turn backlight ON
  Serial.println("Display backlight enabled (GPIO22, HIGH)");
  Serial.flush();
  delay(100);
  
  Serial.printf("Free heap before TFT init: %lu bytes\n", ESP.getFreeHeap());
  Serial.flush();

  // Initialize SPI
  Serial.println("Initializing SPI...");
  SPI.begin(18, -1, 23, 5);  // SCLK, MISO, MOSI, CS
  SPI.setFrequency(1000000);  // 1MHz - ultra slow for stability
  Serial.println("SPI initialized");
  Serial.flush();

  // Initialize display
  Serial.println("Initializing display...");
  Serial.flush();
  tft.init();
  Serial.println("Display initialized successfully!");
  Serial.printf("TFT dimensions: %dx%d\n", tft.getWidth(), tft.getHeight());
  Serial.flush();

  // Quick visible display test: red, green, blue, then clear
  tft.fillScreen(0xF800); // Red
  delay(300);
  tft.fillScreen(0x07E0); // Green
  delay(300);
  tft.fillScreen(0x001F); // Blue
  delay(300);
  tft.fillScreen(0x0000); // Black
  Serial.println("Display test colors shown");

  Serial.println("Rotation set to 0 (portrait)");

  // Initialize AudioKit (ES8388 codec) using proper library
  // TEMPORARILY DISABLED - focus on display first
  // auto cfg = kit.defaultConfig(KitOutput);  // Output only for Bluetooth speaker
  // cfg.sd_active = false; // No SD card
  // kit.begin(cfg);
  Serial.println("AudioKit skipped for testing");

  // Start Bluetooth A2DP (it will use the I2S driver that AudioKit set up)
  // TEMPORARILY DISABLED - focus on display first
  // a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  // a2dp_sink.set_avrc_connection_state_callback(avrc_connection_callback);
  // a2dp_sink.set_sample_rate_callback(sample_rate_callback);
  // a2dp_sink.set_on_volumechange(volume_change_callback);
  // a2dp_sink.set_stream_reader(stream_reader_callback, false);  // false = don't init I2S, AudioKit owns it
  // a2dp_sink.start("FlipClock Speaker");
  Serial.println("Bluetooth A2DP skipped for testing");

  server.on("/", handleRoot);
  server.on("/set", handleSettings);
  server.on("/bluetooth", handleBluetooth);
  server.on("/connectuno", handleConnectUno);
  server.on("/settime", handleSetTime);
  server.on("/timesettings", handleTimeSettings);
  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width'></head><body>";
  html += "<h1>Flip Clock</h1>";
  
  // Time display
  html += "<h2>Current Time</h2>";
  if (uno_hour >= 0 && uno_minute >= 0 && uno_second >= 0) {
    char timeStr[16];
    sprintf(timeStr, "%02d:%02d:%02d", uno_hour, uno_minute, uno_second);
    html += "<p style='font-size:24px;'>" + String(timeStr) + "</p>";
  } else {
    html += "<p>Waiting for time...</p>";
  }
  
  // NTP sync button
  html += "<button onclick=\"fetch('/settime').then(()=>location.reload())\">Sync Time from NTP</button>";
  html += "<p style='font-size:12px;'>Auto-syncs every hour</p><br>";
  
  // Time Settings
  html += "<h2>Time Settings</h2>";
  html += "<form method='POST' action='/timesettings'>";
  html += "<label>Timezone Offset (hours): <input type='number' name='offset' value='" + String(timezoneOffset / 3600) + "' min='-12' max='14' step='0.5'></label><br>";
  html += "<label><input type='radio' name='format' value='24'" + String(use24HourFormat ? " checked" : "") + "> 24-hour</label>";
  html += "<label><input type='radio' name='format' value='12'" + String(!use24HourFormat ? " checked" : "") + "> 12-hour (AM/PM)</label><br>";
  html += "<button type='submit'>Save Time Settings</button>";
  html += "</form><br>";
  
  // Stepper Motor Testing
  html += "<h2>Stepper Motor Test</h2>";
  html += "<button onclick=\"fetch('/set?flip=FLIP1').then(()=>location.reload())\">Test Stepper 1</button> ";

  // html += "<button onclick=\"fetch('/set?flip=FLIP2').then(()=>location.reload())\">Test Stepper 2</button> ";
  // html += "<button onclick=\"fetch('/set?flip=FLIP3').then(()=>location.reload())\">Test Stepper 3</button> ";
  html += "<button onclick=\"fetch('/set?flip=FLIP').then(()=>location.reload())\">Test All Steppers</button><br><br>";
  
  // Stepper Motor Controls
  html += "<h2>Stepper Motor Controls</h2>";
  html += "<form method='POST' action='/set'>";
  html += "<h3>Stepper 1</h3>";
  html += "<label>Speed (RPM): <input type='number' name='speed1' min='1' max='15' value='6'></label> ";
  html += "<label>Move (steps): <input type='number' name='move1' value='0' step='100'></label><br>";
  /*
  html += "<h3>Stepper 2</h3>";
  html += "<label>Speed (RPM): <input type='number' name='speed2' min='1' max='15' value='6'></label> ";
  html += "<label>Move (steps): <input type='number' name='move2' value='0' step='100'></label><br>";
  html += "<h3>Stepper 3</h3>";
  html += "<label>Speed (RPM): <input type='number' name='speed3' min='1' max='15' value='6'></label> ";
  html += "<label>Move (steps): <input type='number' name='move3' value='0' step='100'></label><br>";
  */
  html += "<button type='submit'>Apply Motor Settings</button>";
  html += "</form><br>";
  
  // Bluetooth status
  html += "<h2>Bluetooth Speaker</h2>";
  // DISABLED - a2dp_sink not available
  // html += "<p>Status: " + String(a2dp_sink.is_connected() ? "Connected" : "Disconnected") + "</p>";
  html += "<p>Status: DISABLED</p>";
  if (currentTrack != "No Track") html += "<p>Track: " + currentTrack + "</p>";
  html += "<form method='POST' action='/bluetooth'>";
  html += "<button name='action' value='toggle'>" + String(bluetoothEnabled ? "Disable" : "Enable") + "</button>";
  html += "</form>";
  
  // Debug log
  html += "<h2>Debug Log</h2>";
  html += "<p>Uno Connected: " + String(uno_connected ? "YES" : "NO") + "</p>";
  html += "<p>WiFi: " + WiFi.localIP().toString() + "</p>";
  html += "<p>NTP Synced: " + String(ntpSynced ? "YES" : "NO") + "</p>";
  html += "<form method='POST' action='/connectuno' style='margin-bottom:10px;'><button type='submit'>Connect to Uno</button></form>";
  html += "<div style='background:#f0f0f0;padding:10px;font-family:monospace;font-size:12px;max-height:300px;overflow-y:scroll;'>";
  for (int i = 0; i < DEBUG_LOG_SIZE; i++) {
    int idx = (debugLogIndex + i) % DEBUG_LOG_SIZE;
    if (debugLog[idx].length() > 0) {
      html += debugLog[idx] + "<br>";
    }
  }
  html += "</div>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSettings() {
  if (server.hasArg("flip")) {
    String cmd = server.arg("flip");
    if (cmd.length() > 0) {
      Serial1.println(cmd);  // Send FLIP command via Serial
    }
  }
  // Handle speed settings
  if (server.hasArg("speed1")) {
    Serial1.println("SPEED1:" + server.arg("speed1"));
  }
  if (server.hasArg("speed2")) {
    Serial1.println("SPEED2:" + server.arg("speed2"));
  }
  if (server.hasArg("speed3")) {
    Serial1.println("SPEED3:" + server.arg("speed3"));
  }
  // Handle move commands
  if (server.hasArg("move1")) {
    int steps = server.arg("move1").toInt();
    if (steps != 0) {
      Serial1.println("MOVE1:" + server.arg("move1"));
    }
  }
  if (server.hasArg("move2")) {
    int steps = server.arg("move2").toInt();
    if (steps != 0) {
      Serial1.println("MOVE2:" + server.arg("move2"));
    }
  }
  if (server.hasArg("move3")) {
    int steps = server.arg("move3").toInt();
    if (steps != 0) {
      Serial1.println("MOVE3:" + server.arg("move3"));
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleBluetooth() {
  if (server.hasArg("action")) {
    // DISABLED - a2dp_sink not available
    // if (bluetoothEnabled) {
    //   a2dp_sink.end();
    //   bluetoothEnabled = false;
    // } else {
    //   a2dp_sink.start("FlipClock Speaker");
    //   bluetoothEnabled = true;
    // }
    bluetoothEnabled = !bluetoothEnabled;
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleConnectUno() {
  // Manual trigger to try reconnecting to Uno
  uno_connected = false;
  readyAttempts = 0;
  lastReadySent = 0;
  Serial1.println("READY");
  readyAttempts = 1;
  lastReadySent = millis();
  addDebugLog("TX: READY (manual connect)");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetTime() {
  syncUnoTimeFromNtp();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleTimeSettings() {
  if (server.hasArg("offset")) {
    float offsetHours = server.arg("offset").toFloat();
    timezoneOffset = (long)(offsetHours * 3600);
  }
  if (server.hasArg("format")) {
    use24HourFormat = (server.arg("format") == "24");
  }
  // Re-sync time with new timezone
  if (uno_connected) {
    syncUnoTimeFromNtp();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  if (id == 0x1) {
    currentTrack = String((char*)text);
    Serial.print("Now playing: ");
    Serial.println(currentTrack);
  }
}

void avrc_connection_callback(bool connected) {
  a2dp_connected = connected;
  Serial.printf("A2DP AVRC %s\n", connected ? "connected" : "disconnected");
}

void sample_rate_callback(uint16_t rate) {
  a2dp_sample_rate = rate;
  Serial.printf("A2DP sample rate: %u Hz\n", rate);
}

void volume_change_callback(int volume) {
  // DISABLED - kit not available
  // A2DP volume is 0-127, AudioKit expects 0-100
  // int audiokit_vol = map(volume, 0, 127, 0, 100);
  // kit.setVolume(audiokit_vol);
  // Serial.printf("Volume changed: BT=%d AudioKit=%d\n", volume, audiokit_vol);
  Serial.printf("Volume changed: %d (audiokit disabled)\n", volume);
}

void stream_reader_callback(const uint8_t *data, uint32_t len) {
  a2dp_bytes_total += len;
  a2dp_bytes_last_interval += len;

  // DISABLED - kit not available
  // Write audio data to AudioKit instead of letting A2DP control I2S
  // kit.write(data, len);

  // Capture amplitude window for debug (16-bit little-endian stereo)
  if (len >= 2) {
    const int16_t *s = (const int16_t *)data;
    size_t samples = len / 2; // count 16-bit samples (L/R interleaved)
    int16_t local_min = a2dp_sample_min;
    int16_t local_max = a2dp_sample_max;
    if (!a2dp_sample_seen) {
      local_min = 32767;
      local_max = -32768;
    }
    for (size_t i = 0; i < samples; i++) {
      int16_t v = s[i];
      if (v < local_min) local_min = v;
      if (v > local_max) local_max = v;
    }
    a2dp_sample_min = local_min;
    a2dp_sample_max = local_max;
    a2dp_sample_seen = true;
  }
}

void loop() {
  server.handleClient();

  // Check for responses from Uno (e.g., "TIME:HH:MM:SS")
  if (Serial1.available()) {
    String response = Serial1.readStringUntil('\n');
    response.trim();
    addDebugLog("RX: " + response);
    if (response.startsWith("TIME:")) {
      if (!uno_connected) {
        addDebugLog("*** UNO CONNECTED ***");
        uno_connected = true;  // Mark Uno as connected
      }
      // Parse TIME:HH:MM:SS and sanity-check ranges
      int h, m, s;
      if (sscanf(response.c_str(), "TIME:%d:%d:%d", &h, &m, &s) == 3 &&
          h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60) {
        uno_hour = h;
        uno_minute = m;
        uno_second = s;
      } else {
        addDebugLog("WARN: Invalid time " + response);
      }
    }
  }

  // Periodic NTP sync (initial sync + every hour)
  if (WiFi.status() == WL_CONNECTED && uno_connected) {
    if (!ntpSynced || (millis() - lastNtpSync >= ntpSyncInterval)) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 2000)) {  // 2 second timeout for NTP sync
        syncUnoTimeFromNtp();
        ntpSynced = true;
        lastNtpSync = millis();
        Serial.println("Automatic NTP sync completed");
      }
    }
  }

  // Retry READY a limited number of times until Uno replies
  if (!uno_connected && readyAttempts < readyAttemptsMax && millis() - lastReadySent >= readyRetryInterval) {
    Serial1.println("READY");
    lastReadySent = millis();
    readyAttempts++;
    addDebugLog("TX: READY (retry " + String(readyAttempts) + "/" + String(readyAttemptsMax) + ")");
  }

  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();
    static unsigned long startTime = millis();
    unsigned long elapsed = (millis() - startTime) / 1000;
    int minute = (elapsed / 60) % 60;
    
    static int lastMinute = -1;
    if (minute != lastMinute) {
      lastMinute = minute;
      // Serial1.println("FLIP1");  // Send to Uno via Serial1 - DISABLED
    }
    
    a2dp_bytes_last_interval = 0;
    a2dp_sample_seen = false;
  }

  // Update display less frequently (every 5 seconds)
  if (millis() - lastDisplayUpdate >= displayUpdateInterval) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}

void syncUnoTimeFromNtp() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    // Apply timezone offset
    time_t rawTime = mktime(&timeinfo) + timezoneOffset;
    struct tm *adjustedTime = localtime(&rawTime);
    
    char buf[40];
    snprintf(buf, sizeof(buf), "SETTIME:%04d-%02d-%02d %02d:%02d:%02d",
             adjustedTime->tm_year + 1900,
             adjustedTime->tm_mon + 1,
             adjustedTime->tm_mday,
             adjustedTime->tm_hour,
             adjustedTime->tm_min,
             adjustedTime->tm_sec);
    Serial1.println(buf);
    addDebugLog("TX: " + String(buf));
  } else {
    addDebugLog("ERROR: NTP sync failed");
  }
}

void updateDisplay() {
  Serial.println("=== Display Update ===");
  Serial.printf("Display dimensions: %dx%d\n", tft.getWidth(), tft.getHeight());
  
  // Simple visible pattern to confirm the display is alive
  tft.fillScreen(0x0000);  // Black background
  uint16_t w = tft.getWidth() / 3;
  tft.fillRect(0, 0, w, tft.getHeight(), 0xF800);      // Red
  tft.fillRect(w, 0, w, tft.getHeight(), 0x07E0);      // Green
  tft.fillRect(2 * w, 0, tft.getWidth() - 2 * w, tft.getHeight(), 0x001F);  // Blue
  
  Serial.println("Display updated with test pattern");
}
