#include "display_manager.h"
#include "SimpleST7789.h"
#include <WebServer.h>
#include <time.h>
#include <SPIFFS.h>
#include <FS.h>

extern SimpleST7789 tft;
extern WebServer    server;

// ── Internal state ─────────────────────────────────────────────────────────
static String lastUpdatePayload = "";

// Cache last drawn calendar so we only redraw when date changes
static int lastDrawnDay   = -1;
static int lastDrawnMonth = -1;
static int lastDrawnYear  = -1;

// ── Forward declarations ───────────────────────────────────────────────────
static void drawDigit(int x, int y, int w, int h, uint16_t color, int digit);
static void drawSeparator(int x, int y, int w, int h, uint16_t color);

// ── Init ───────────────────────────────────────────────────────────────────
void displayInit() {
  tft.fillScreen(0x0000);
  lastDrawnDay   = -1;
  lastDrawnMonth = -1;
  lastDrawnYear  = -1;
}

// ── 7-segment digit ────────────────────────────────────────────────────────
static void drawDigit(int x, int y, int w, int h, uint16_t color, int digit) {
  if (digit < 0 || digit > 9) return;
  int segW = max(1, w / 6);
  int segH = max(1, h / 6);
  int half = h / 2;

  // A — top
  if (digit==0||digit==2||digit==3||digit==5||digit==6||digit==7||digit==8||digit==9)
    tft.fillRect(x+segW, y, w-segW*2, segH, color);
  // B — top right
  if (digit==0||digit==1||digit==2||digit==3||digit==4||digit==7||digit==8||digit==9)
    tft.fillRect(x+w-segW, y+segH, segW, half-segH, color);
  // C — bottom right
  if (digit==0||digit==1||digit==3||digit==4||digit==5||digit==6||digit==7||digit==8||digit==9)
    tft.fillRect(x+w-segW, y+half, segW, half-segH, color);
  // D — bottom
  if (digit==0||digit==2||digit==3||digit==5||digit==6||digit==8||digit==9)
    tft.fillRect(x+segW, y+h-segH, w-segW*2, segH, color);
  // E — bottom left
  if (digit==0||digit==2||digit==6||digit==8)
    tft.fillRect(x, y+half, segW, half-segH, color);
  // F — top left
  if (digit==0||digit==4||digit==5||digit==6||digit==8||digit==9)
    tft.fillRect(x, y+segH, segW, half-segH, color);
  // G — middle
  if (digit==2||digit==3||digit==4||digit==5||digit==6||digit==8||digit==9)
    tft.fillRect(x+segW, y+half-segH/2, w-segW*2, segH, color);
}

static void drawSeparator(int x, int y, int w, int h, uint16_t color) {
  int hh = h / 4;
  tft.fillRect(x, y+h/2-hh/2, w, hh, color);
}

// ── Calendar ───────────────────────────────────────────────────────────────
// Only redraws when the day changes — safe to call frequently
void displayShowCalendar() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 0)) return;

  int day = timeinfo.tm_mday;
  // ONLY proceed if the day actually changed
  if (day == lastDrawnDay) return; 

  lastDrawnDay = day;
  tft.fillScreen(0x0000);

  int year  = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon  + 1;

  // Skip full redraw if date hasn't changed
  if (day == lastDrawnDay && month == lastDrawnMonth && year == lastDrawnYear) {
    return;
  }
  lastDrawnDay   = day;
  lastDrawnMonth = month;
  lastDrawnYear  = year;

  // Clear screen once
  tft.fillScreen(0x0000);

  uint16_t fg      = 0xFFFF;
  uint16_t headerC = 0x07E0;  // green header bar
  uint16_t todayC  = 0xF800;  // red highlight for today

  int dispW = tft.getWidth();
  int dispH = tft.getHeight();

  // Header bar
  tft.fillRect(0, 0, dispW, 28, headerC);

  // Draw month/year in header using small digits
  // e.g. "12 2024" — two digits month, space, four digits year
  int hDigW = 10;
  int hDigH = 18;
  int hx = 4;
  int hy = 5;
  drawDigit(hx,                 hy, hDigW, hDigH, 0x0000, month/10);
  drawDigit(hx + hDigW + 2,    hy, hDigW, hDigH, 0x0000, month%10);
  drawSeparator(hx + hDigW*2 + 6, hy, 6, hDigH, 0x0000);
  drawDigit(hx + hDigW*2 + 16,       hy, hDigW, hDigH, 0x0000, (year/1000)%10);
  drawDigit(hx + hDigW*3 + 18,       hy, hDigW, hDigH, 0x0000, (year/100)%10);
  drawDigit(hx + hDigW*4 + 20,       hy, hDigW, hDigH, 0x0000, (year/10)%10);
  drawDigit(hx + hDigW*5 + 22,       hy, hDigW, hDigH, 0x0000, year%10);

  // Grid
  int cellW = dispW / 7;
  int cellH = (dispH - 28) / 6;

  // First weekday of month
  struct tm first = {};
  first.tm_year = timeinfo.tm_year;
  first.tm_mon  = timeinfo.tm_mon;
  first.tm_mday = 1;
  mktime(&first);
  int startWeekday = first.tm_wday;

  // Days in month
  int daysInMonth;
  if (month==1||month==3||month==5||month==7||month==8||month==10||month==12)
    daysInMonth = 31;
  else if (month==4||month==6||month==9||month==11)
    daysInMonth = 30;
  else {
    bool leap = (year%4==0 && (year%100!=0 || year%400==0));
    daysInMonth = leap ? 29 : 28;
  }

  int d = 1;
  for (int r = 0; r < 6 && d <= daysInMonth; r++) {
    for (int c = 0; c < 7 && d <= daysInMonth; c++) {
      int cellIndex = r*7 + c;
      if (cellIndex < startWeekday) continue;

      int cx = c * cellW;
      int cy = 28 + r * cellH;

      // Highlight today
      if (d == day) {
        tft.fillRect(cx+1, cy+1, cellW-2, cellH-2, todayC);
      }

      // Draw day number using two small digits
      int dw = cellW / 3;
      int dh = cellH / 2;
      int sx = cx + 2;
      int sy = cy + 3;
      uint16_t digitColor = (d == day) ? 0xFFFF : fg;

      if (d >= 10) {
        drawDigit(sx,      sy, dw, dh, digitColor, d/10);
        drawDigit(sx+dw+1, sy, dw, dh, digitColor, d%10);
      } else {
        drawDigit(sx + dw/2, sy, dw, dh, digitColor, d);
      }
      d++;
    }
  }
}

// ── Date (YYYY-MM-DD) ──────────────────────────────────────────────────────
void displayShowDate() {
  static int lastMin = -1;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return;

  // ONLY redraw if the minute has changed!
  if (timeinfo.tm_min == lastMin) return;
  lastMin = timeinfo.tm_min;

  tft.fillScreen(0x0000);

  // Non-blocking — if no time yet, show placeholder
  if (!getLocalTime(&timeinfo, 0)) {
    tft.fillScreen(0x0000);
    tft.fillRect(10, 10, tft.getWidth()-20, 80, 0x07E0);
    return;
  }

  int year  = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon  + 1;
  int day   = timeinfo.tm_mday;

  tft.fillScreen(0x0000);
  uint16_t fg = 0xFFFF;

  int startX = 8;
  int startY = 20;
  int digitW = 24;
  int digitH = 48;

  // Year — 4 digits
  drawDigit(startX + 0*(digitW+2), startY, digitW, digitH, fg, (year/1000)%10);
  drawDigit(startX + 1*(digitW+2), startY, digitW, digitH, fg, (year/100)%10);
  drawDigit(startX + 2*(digitW+2), startY, digitW, digitH, fg, (year/10)%10);
  drawDigit(startX + 3*(digitW+2), startY, digitW, digitH, fg, year%10);

  int sepX = startX + 4*(digitW+2) + 4;
  drawSeparator(sepX, startY, 8, digitH, fg);

  // Month — 2 digits
  int mx = sepX + 14;
  drawDigit(mx,          startY, digitW, digitH, fg, month/10);
  drawDigit(mx+digitW+2, startY, digitW, digitH, fg, month%10);

  int sep2X = mx + 2*(digitW+2) + 4;
  drawSeparator(sep2X, startY, 8, digitH, fg);

  // Day — 2 digits
  int dx = sep2X + 14;
  drawDigit(dx,          startY, digitW, digitH, fg, day/10);
  drawDigit(dx+digitW+2, startY, digitW, digitH, fg, day%10);
}

// ── Album art ──────────────────────────────────────────────────────────────
// SimpleST7789 doesn't support TJpg_Decoder's pixel push API.
// Show a placeholder with track info instead.
void displayShowAlbum(const String &path) {
  tft.fillScreen(0x0000);

  // Header bar
  tft.fillRect(0, 0, tft.getWidth(), 30, 0xF800);

  // Placeholder album art box
  int bx = 20;
  int by = 40;
  int bw = tft.getWidth()  - 40;
  int bh = tft.getHeight() - 60;
  tft.fillRect(bx, by, bw, bh, 0x2104);  // dark grey

  // Border
  tft.fillRect(bx,         by,         bw, 2,  0x07E0);
  tft.fillRect(bx,         by+bh-2,    bw, 2,  0x07E0);
  tft.fillRect(bx,         by,         2,  bh, 0x07E0);
  tft.fillRect(bx+bw-2,   by,         2,  bh, 0x07E0);

  // Note: to render the actual JPEG you need a library compatible with
  // SimpleST7789's pixel interface. TJpg_Decoder requires setAddrWindow +
  // pushPixels which SimpleST7789 may not expose. Switch to TFT_eSPI to
  // unlock JPEG rendering.
}

// ── Web update handler ─────────────────────────────────────────────────────
void displayHandleUpdate(const String &body) {
  lastUpdatePayload = body;
  if      (body.indexOf("calendar") >= 0) { lastDrawnDay = -1; displayShowCalendar(); }
  else if (body.indexOf("album")    >= 0) {
    int p = body.indexOf('/');
    displayShowAlbum(p >= 0 ? body.substring(p) : "/album.jpg");
  } else {
    displayShowDate();
  }
}

// ── Register web handlers ──────────────────────────────────────────────────
// Note: do NOT call SPIFFS.begin() here — already done in main setup()
void displayRegisterHandlers() {
  server.on("/display/update", HTTP_POST, []() {
    // Instead of parsing the whole body into a String, check for keywords
    if (server.hasArg("plain")) {
        displayHandleUpdate(server.arg("plain"));
    }
    server.send(200, F("text/plain"), F("OK")); // Wrapped in F()
  });
}