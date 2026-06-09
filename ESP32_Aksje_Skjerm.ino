/*
  ESP32 AKSJESKJERM - CLEAN MARKET DASHBOARD
  Skjerm: ST7789V 2.0" TFT 240x320
  Landscape: 320x240

  RIKTIG PINSETT:
  TFT CS   -> GPIO5
  TFT DC   -> GPIO19
  TFT RST  -> GPIO22
  TFT SDA  -> GPIO23
  TFT SCL  -> GPIO18
  VCC      -> 3V3
  GND      -> GND

  Biblioteker:
  - Adafruit GFX Library
  - Adafruit ST7735 and ST7789 Library
  - ArduinoJson
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <time.h>
#include <math.h>

// =====================================================
// WIFI - FYLL INN FØR OPPLASTING
// =====================================================
const char WIFI_SSID[] = "DITT_WIFI_NAVN";
const char WIFI_PASSWORD[] = "DITT_WIFI_PASSORD";

// =====================================================
// TFT ST7789V
// =====================================================
#define TFT_CS    5
#define TFT_DC    19
#define TFT_RST   22
#define TFT_MOSI  23
#define TFT_SCLK  18

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// =====================================================
// SKJERM
// =====================================================
#define SCREEN_W 320
#define SCREEN_H 240

// =====================================================
// FARGER
// =====================================================
#define C_BLACK   ST77XX_BLACK
#define C_WHITE   ST77XX_WHITE
#define C_RED     ST77XX_RED
#define C_GREEN   ST77XX_GREEN
#define C_BLUE    ST77XX_BLUE
#define C_YELLOW  ST77XX_YELLOW
#define C_CYAN    ST77XX_CYAN

uint16_t C_BG        = 0x0008;
uint16_t C_BG_GRID   = 0x0841;
uint16_t C_HEADER    = 0x0000;
uint16_t C_CARD      = 0x18C3;
uint16_t C_CARD_2    = 0x2104;
uint16_t C_LINE      = 0x39E7;
uint16_t C_MUTED     = 0x9CF3;
uint16_t C_POS       = 0x07E0;
uint16_t C_NEG       = 0xF800;
uint16_t C_WARN      = 0xFD20;
uint16_t C_ACCENT    = 0x05FF;
uint16_t C_TICKER_BG = 0x0000;

// =====================================================
// INTERVALLER
// =====================================================
const unsigned long MAIN_SCREEN_INTERVAL_MS = 7000UL;
const unsigned long QUOTE_REFRESH_MS        = 60000UL;
const unsigned long FETCH_GAP_MS            = 1200UL;
const unsigned long WIFI_RETRY_MS           = 15000UL;
const unsigned long WIFI_TIMEOUT_MS         = 25000UL;
const unsigned long TICKER_SPEED_MS         = 35UL;
const unsigned long CLOCK_REFRESH_MS        = 30000UL;

#define HISTORY_POINTS 24

// =====================================================
// INSTRUMENTER
// =====================================================
struct Instrument {
  const char* name;
  const char* shortName;
  const char* symbol;
  const char* urlSymbol;

  float price;
  float previousClose;
  float change;
  float percent;

  String currency;
  time_t marketTime;

  float history[HISTORY_POINTS];
  uint8_t historyCount;

  bool valid;
  String error;
};

Instrument instruments[] = {
  {"Kongsberg",       "Kongsberg", "KOG.OL",   "KOG.OL",   NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"Equinor",         "Equinor",   "EQNR.OL",  "EQNR.OL",  NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"DNB Bank",        "DNB",       "DNB.OL",   "DNB.OL",   NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"Aker Solutions",  "Aker Sol.", "AKSO.OL",  "AKSO.OL",  NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"Aker BP",         "Aker BP",   "AKRBP.OL", "AKRBP.OL", NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"Mowi",            "Mowi",      "MOWI.OL",  "MOWI.OL",  NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"Scatec",          "Scatec",    "SCATC.OL", "SCATC.OL", NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"Brent Oil",       "Brent",     "BZ=F",     "BZ%3DF",   NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"Gold",            "Gold",      "GC=F",     "GC%3DF",   NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"},
  {"S&P 500",         "S&P 500",   "^GSPC",    "%5EGSPC",  NAN, NAN, NAN, NAN, "", 0, {0}, 0, false, "Starter"}
};

const size_t INSTRUMENT_COUNT = sizeof(instruments) / sizeof(instruments[0]);

// =====================================================
// GLOBAL STATUS
// =====================================================
size_t currentInstrument = 0;
size_t fetchInstrument = 0;

bool fetchCycleActive = false;
bool screenDirty = true;
bool wifiWasConnected = false;

unsigned long mainScreenChangedAt = 0;
unsigned long lastFetchAt = 0;
unsigned long nextRefreshAt = 0;
unsigned long lastWifiAttemptAt = 0;
unsigned long lastTickerMoveAt = 0;
unsigned long lastClockRefreshAt = 0;

int tickerX = SCREEN_W;
String tickerText = "";

// =====================================================
// TEKST / FORMAT
// =====================================================
void setText(uint16_t color, uint8_t size) {
  tft.setFont(NULL);
  tft.setTextColor(color);
  tft.setTextSize(size);
  tft.setTextWrap(false);
}

int textWidth(const String &text, uint8_t size) {
  return text.length() * 6 * size;
}

void drawTextCentered(const String &text, int16_t centerX, int16_t y, uint8_t size, uint16_t color) {
  setText(color, size);

  int w = textWidth(text, size);
  int x = centerX - w / 2;
  if (x < 0) x = 0;

  tft.setCursor(x, y);
  tft.print(text);
}

void drawTextRight(const String &text, int16_t rightX, int16_t y, uint8_t size, uint16_t color) {
  setText(color, size);

  int w = textWidth(text, size);
  int x = rightX - w;
  if (x < 0) x = 0;

  tft.setCursor(x, y);
  tft.print(text);
}

String formatPrice(float value) {
  if (isnan(value)) return "--";

  if (fabs(value) >= 1000.0f) return String(value, (unsigned int)0);
  if (fabs(value) >= 100.0f)  return String(value, (unsigned int)1);
  return String(value, (unsigned int)2);
}

String formatSigned(float value, uint8_t decimals) {
  if (isnan(value)) return "--";

  String s = "";
  if (value > 0.0f) s += "+";
  s += String(value, (unsigned int)decimals);
  return s;
}

String formatPercent(float value) {
  if (isnan(value)) return "--%";

  String s = "";
  if (value > 0.0f) s += "+";
  s += String(value, (unsigned int)2);
  s += "%";
  return s;
}

String formatMarketTime(time_t timestamp) {
  if (timestamp <= 0) return "--";

  struct tm timeinfo;
  localtime_r(&timestamp, &timeinfo);

  char buffer[24];
  snprintf(
    buffer,
    sizeof(buffer),
    "%02d.%02d %02d:%02d",
    timeinfo.tm_mday,
    timeinfo.tm_mon + 1,
    timeinfo.tm_hour,
    timeinfo.tm_min
  );

  return String(buffer);
}

String localClock() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 50)) {
    return "--:--";
  }

  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  return String(buffer);
}

uint16_t directionColor(float change) {
  if (isnan(change)) return C_MUTED;
  if (change > 0.0f) return C_POS;
  if (change < 0.0f) return C_NEG;
  return C_MUTED;
}

// =====================================================
// DESIGN
// =====================================================
void drawBackground() {
  tft.fillScreen(C_BG);

  for (int x = 0; x < SCREEN_W; x += 16) {
    tft.drawFastVLine(x, 32, 184, C_BG_GRID);
  }

  for (int y = 32; y < 218; y += 16) {
    tft.drawFastHLine(0, y, SCREEN_W, C_BG_GRID);
  }
}

void drawCard(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fill) {
  tft.fillRoundRect(x, y, w, h, 8, fill);
  tft.drawRoundRect(x, y, w, h, 8, C_LINE);
}

void drawHeader() {
  tft.fillRect(0, 0, 320, 28, C_HEADER);
  tft.drawFastHLine(0, 28, 320, C_LINE);

  setText(C_WHITE, 2);
  tft.setCursor(8, 6);
  tft.print("MARKET");

  setText(C_MUTED, 1);
  tft.setCursor(82, 6);
  tft.print("Yahoo Finance");

  if (WiFi.status() == WL_CONNECTED) {
    setText(C_POS, 1);
    tft.setCursor(226, 6);
    tft.print("Wi-Fi OK");
  } else {
    setText(C_NEG, 1);
    tft.setCursor(214, 6);
    tft.print("Wi-Fi FEIL");
  }

  setText(C_MUTED, 1);
  tft.setCursor(276, 18);
  tft.print(localClock());
}

void drawMoveBar(uint16_t color) {
  tft.fillRect(0, 29, 320, 4, color);
}

void drawArrow(int16_t x, int16_t y, float change) {
  uint16_t c = directionColor(change);

  if (isnan(change) || change == 0.0f) {
    tft.fillRoundRect(x, y + 26, 58, 12, 6, c);
    return;
  }

  if (change > 0.0f) {
    tft.fillTriangle(x, y + 42, x + 30, y, x + 60, y + 42, c);
    tft.fillRoundRect(x + 21, y + 38, 18, 36, 5, c);
  } else {
    tft.fillRoundRect(x + 21, y, 18, 36, 5, c);
    tft.fillTriangle(x, y + 32, x + 30, y + 74, x + 60, y + 32, c);
  }
}

void drawSparkline(int16_t x, int16_t y, int16_t w, int16_t h, Instrument &item, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, C_BG);
  tft.drawRoundRect(x, y, w, h, 5, C_LINE);

  if (item.historyCount < 2) {
    setText(C_MUTED, 1);
    tft.setCursor(x + 8, y + 10);
    tft.print("Ingen graf");
    return;
  }

  float minVal = item.history[0];
  float maxVal = item.history[0];

  for (uint8_t i = 1; i < item.historyCount; i++) {
    if (item.history[i] < minVal) minVal = item.history[i];
    if (item.history[i] > maxVal) maxVal = item.history[i];
  }

  if (maxVal == minVal) {
    maxVal += 1.0f;
    minVal -= 1.0f;
  }

  for (int i = 1; i < item.historyCount; i++) {
    int x1 = x + 5 + ((i - 1) * (w - 10)) / (item.historyCount - 1);
    int x2 = x + 5 + (i * (w - 10)) / (item.historyCount - 1);

    int y1 = y + h - 5 - ((item.history[i - 1] - minVal) * (h - 10)) / (maxVal - minVal);
    int y2 = y + h - 5 - ((item.history[i] - minVal) * (h - 10)) / (maxVal - minVal);

    tft.drawLine(x1, y1, x2, y2, color);
    tft.drawLine(x1, y1 + 1, x2, y2 + 1, color);
  }
}

void drawRefreshBar() {
  if (nextRefreshAt <= millis()) return;

  unsigned long remaining = nextRefreshAt - millis();
  unsigned long elapsed = QUOTE_REFRESH_MS - remaining;

  if (elapsed > QUOTE_REFRESH_MS) elapsed = QUOTE_REFRESH_MS;

  int barX = 8;
  int barY = 214;
  int barW = 304;
  int barH = 3;

  tft.fillRoundRect(barX, barY, barW, barH, 2, C_LINE);

  int progress = (elapsed * barW) / QUOTE_REFRESH_MS;
  if (progress < 0) progress = 0;
  if (progress > barW) progress = barW;

  tft.fillRoundRect(barX, barY, progress, barH, 2, C_ACCENT);
}

// =====================================================
// TICKER
// =====================================================
void buildTickerText() {
  tickerText = "   ";

  for (size_t i = 0; i < INSTRUMENT_COUNT; i++) {
    Instrument &item = instruments[i];

    tickerText += item.symbol;
    tickerText += " ";

    if (item.valid) {
      tickerText += formatPrice(item.price);
      tickerText += " ";
      tickerText += item.currency;
      tickerText += " ";

      if (item.change > 0.0f) {
        tickerText += "^ ";
      } else if (item.change < 0.0f) {
        tickerText += "v ";
      } else {
        tickerText += "- ";
      }

      tickerText += formatPercent(item.percent);
    } else {
      tickerText += "--";
    }

    tickerText += "     ";
  }

  tickerText += "   ";
}

void drawTicker() {
  tft.fillRect(0, 218, SCREEN_W, 22, C_TICKER_BG);

  setText(C_WHITE, 1);
  tft.setCursor(tickerX, 226);
  tft.print(tickerText);

  int tickerWidth = textWidth(tickerText, 1);

  tickerX -= 2;

  if (tickerX < -tickerWidth) {
    tickerX = SCREEN_W;
  }

  tft.drawFastHLine(0, 217, SCREEN_W, C_LINE);
}

// =====================================================
// HOVEDDISPLAY
// =====================================================
void drawInstrument() {
  Instrument &item = instruments[currentInstrument];

  drawBackground();
  drawHeader();

  uint16_t moveColor = directionColor(item.change);
  drawMoveBar(moveColor);

  // Symbolkort
  drawCard(8, 38, 304, 42, C_CARD);

  setText(C_ACCENT, 3);
  tft.setCursor(18, 48);
  tft.print(item.symbol);

  setText(C_WHITE, 2);
  tft.setCursor(172, 52);
  tft.print(item.shortName);

  setText(C_MUTED, 1);
  tft.setCursor(280, 42);
  tft.print(currentInstrument + 1);
  tft.print("/");
  tft.print(INSTRUMENT_COUNT);

  // Hovedkort
  drawCard(8, 86, 146, 82, C_CARD_2);

  // Sidekort
  drawCard(162, 86, 150, 82, C_CARD_2);

  // Grafkort
  drawCard(8, 172, 304, 38, C_CARD);

  if (WiFi.status() != WL_CONNECTED) {
    drawTextCentered("INGEN", 81, 106, 3, C_NEG);
    drawTextCentered("WI-FI", 81, 134, 3, C_NEG);
    drawTextCentered("Prover igjen", 237, 120, 1, C_WARN);
    drawRefreshBar();
    screenDirty = false;
    return;
  }

  if (!item.valid) {
    drawTextCentered("HENTER", 81, 106, 3, C_WARN);
    drawTextCentered("DATA", 81, 134, 3, C_WARN);
    drawTextCentered(item.error.substring(0, 18), 237, 120, 1, C_NEG);
    drawRefreshBar();
    screenDirty = false;
    return;
  }

  // Pris
  String priceText = formatPrice(item.price);
  uint8_t priceSize = 4;

  if (priceText.length() <= 4) {
    priceSize = 5;
  }

  if (priceText.length() >= 6) {
    priceSize = 3;
  }

  drawTextCentered(priceText, 81, 98, priceSize, C_WHITE);

  setText(C_MUTED, 2);
  tft.setCursor(112, 137);
  tft.print(item.currency);

  // Prosent og endring
  String pctText = formatPercent(item.percent);
  String changeText = formatSigned(item.change, 2);

  tft.fillRoundRect(20, 143, 66, 18, 9, moveColor);
  drawTextCentered(pctText, 53, 148, 1, C_BLACK);

  drawTextRight(changeText, 146, 147, 1, moveColor);

  // Pil
  drawArrow(176, 92, item.change);

  setText(C_MUTED, 1);
  tft.setCursor(246, 96);
  tft.print("FORRIGE");

  setText(C_WHITE, 2);
  drawTextRight(formatPrice(item.previousClose), 304, 114, 2, C_WHITE);

  setText(C_MUTED, 1);
  tft.setCursor(246, 141);
  tft.print("ENDRING");

  drawTextRight(changeText, 304, 153, 1, moveColor);

  // Graf
  setText(C_MUTED, 1);
  tft.setCursor(18, 179);
  tft.print("TREND 1M");

  drawSparkline(82, 177, 218, 24, item, moveColor);

  setText(C_MUTED, 1);
  tft.setCursor(18, 203);
  tft.print("Sist: ");
  tft.print(formatMarketTime(item.marketTime));

  drawRefreshBar();

  screenDirty = false;
}

// =====================================================
// OPPSTART
// =====================================================
void drawBootScreen(String title, String line1, String line2, uint16_t color) {
  drawBackground();

  drawTextCentered("MARKET", 160, 40, 3, C_WHITE);
  drawTextCentered(title, 160, 88, 2, color);
  drawTextCentered(line1, 160, 122, 1, C_MUTED);
  drawTextCentered(line2, 160, 142, 1, C_MUTED);

  setText(C_MUTED, 1);
  tft.setCursor(10, 210);
  tft.print("ST7789V 320x240  CS5 DC19 RST22");
}

void drawScreenTest() {
  tft.fillScreen(C_RED);
  delay(150);

  tft.fillScreen(C_GREEN);
  delay(150);

  tft.fillScreen(C_BLUE);
  delay(150);

  tft.fillScreen(C_BLACK);
  delay(120);

  drawBootScreen("Skjerm OK", "Starter system", "", C_POS);
  delay(600);
}

// =====================================================
// WIFI
// =====================================================
bool connectWifi() {
  Serial.println();
  Serial.print("Kobler til Wi-Fi: ");
  Serial.println(WIFI_SSID);

  drawBootScreen("Kobler til Wi-Fi", WIFI_SSID, "", C_WARN);

  WiFi.disconnect(true, true);
  delay(600);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_TIMEOUT_MS) {
    delay(500);

    Serial.print(".");

    String line = "Tid: ";
    line += String((millis() - startAttempt) / 1000);
    line += " sek";

    drawBootScreen("Kobler til Wi-Fi", line, "", C_WARN);
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi tilkoblet");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());

    drawBootScreen("Wi-Fi OK", WiFi.localIP().toString(), "RSSI: " + String(WiFi.RSSI()), C_POS);
    delay(1200);

    wifiWasConnected = true;
    screenDirty = true;
    nextRefreshAt = 0;

    return true;
  }

  Serial.println("Wi-Fi feilet");

  drawBootScreen("Wi-Fi feilet", "Sjekk passord / 2.4 GHz", "", C_NEG);
  delay(1500);

  wifiWasConnected = false;
  WiFi.disconnect(true, true);

  screenDirty = true;
  return false;
}

void serviceWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.println("Wi-Fi OK igjen");
      screenDirty = true;
      nextRefreshAt = 0;
    }

    return;
  }

  wifiWasConnected = false;

  if (millis() - lastWifiAttemptAt >= WIFI_RETRY_MS) {
    lastWifiAttemptAt = millis();
    connectWifi();
  }
}

// =====================================================
// HENTE KURS
// =====================================================
bool fetchQuote(Instrument &item) {
  if (WiFi.status() != WL_CONNECTED) {
    item.error = "Ingen Wi-Fi";
    item.valid = false;
    return false;
  }

  String url = "https://query1.finance.yahoo.com/v8/finance/chart/";
  url += item.urlSymbol;
  url += "?interval=1d&range=1mo";

  Serial.print("Henter ");
  Serial.print(item.symbol);
  Serial.print(": ");
  Serial.println(url);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);

  if (!http.begin(client, url)) {
    item.error = "HTTP start feil";
    item.valid = false;
    return false;
  }

  http.addHeader("User-Agent", "Mozilla/5.0 ESP32 Stock Screen");

  int status = http.GET();

  if (status != HTTP_CODE_OK) {
    item.error = "HTTP " + String(status);
    item.valid = false;

    Serial.print(item.symbol);
    Serial.print(": HTTP-feil ");
    Serial.println(status);

    http.end();
    return false;
  }

  DynamicJsonDocument document(32768);

  DeserializationError jsonError = deserializeJson(document, http.getStream());

  http.end();

  if (jsonError) {
    item.error = "JSON-feil";
    item.valid = false;

    Serial.print(item.symbol);
    Serial.print(": JSON-feil ");
    Serial.println(jsonError.c_str());

    return false;
  }

  JsonObject meta = document["chart"]["result"][0]["meta"];

  if (meta.isNull() || meta["regularMarketPrice"].isNull()) {
    item.error = "Ingen kurs";
    item.valid = false;
    return false;
  }

  float price = meta["regularMarketPrice"].as<float>();
  float previous = NAN;

  if (!meta["chartPreviousClose"].isNull()) {
    previous = meta["chartPreviousClose"].as<float>();
  } else if (!meta["previousClose"].isNull()) {
    previous = meta["previousClose"].as<float>();
  }

  item.price = price;
  item.previousClose = previous;
  item.change = isnan(previous) ? NAN : price - previous;
  item.percent = isnan(previous) || previous == 0.0f ? NAN : (item.change / previous) * 100.0f;
  item.currency = meta["currency"] | "";
  item.marketTime = meta["regularMarketTime"] | 0;

  item.historyCount = 0;

  JsonArray closes = document["chart"]["result"][0]["indicators"]["quote"][0]["close"].as<JsonArray>();

  if (!closes.isNull()) {
    int total = closes.size();
    int start = total - HISTORY_POINTS;
    if (start < 0) start = 0;

    for (int i = start; i < total && item.historyCount < HISTORY_POINTS; i++) {
      if (!closes[i].isNull()) {
        item.history[item.historyCount] = closes[i].as<float>();
        item.historyCount++;
      }
    }
  }

  item.valid = true;
  item.error = "";

  Serial.print(item.symbol);
  Serial.print(" = ");
  Serial.print(item.price, 4);
  Serial.print(" ");
  Serial.print(item.currency);
  Serial.print(" | ");
  Serial.print(item.percent, 2);
  Serial.print("% | Historikk: ");
  Serial.println(item.historyCount);

  return true;
}

void startFetchCycle() {
  if (WiFi.status() != WL_CONNECTED) return;

  fetchCycleActive = true;
  fetchInstrument = 0;
  lastFetchAt = millis() - FETCH_GAP_MS;

  Serial.println();
  Serial.println("Starter kursoppdatering");
}

void serviceFetchCycle() {
  if (!fetchCycleActive) return;
  if (millis() - lastFetchAt < FETCH_GAP_MS) return;

  lastFetchAt = millis();

  size_t index = fetchInstrument;

  tft.fillRect(0, 218, SCREEN_W, 22, C_TICKER_BG);
  setText(C_MUTED, 1);
  tft.setCursor(8, 226);
  tft.print("Henter kurs: ");
  tft.print(instruments[index].symbol);

  bool updated = fetchQuote(instruments[index]);

  if (updated) {
    buildTickerText();

    if (index == currentInstrument) {
      screenDirty = true;
    }
  }

  fetchInstrument++;

  if (fetchInstrument >= INSTRUMENT_COUNT) {
    fetchCycleActive = false;
    nextRefreshAt = millis() + QUOTE_REFRESH_MS;

    buildTickerText();
    tickerX = SCREEN_W;

    Serial.println("Kursoppdatering ferdig");
    screenDirty = true;
  }
}

// =====================================================
// SETUP / LOOP
// =====================================================
void setupDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.init(240, 320);
  tft.setRotation(1);
  tft.invertDisplay(true);
  tft.setSPISpeed(40000000);

  drawScreenTest();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Starter ESP32 aksjeskjerm - clean market dashboard");

  setupDisplay();

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");

  buildTickerText();

  connectWifi();

  mainScreenChangedAt = millis();
  lastWifiAttemptAt = millis();
  nextRefreshAt = 0;
  screenDirty = true;
  tickerX = SCREEN_W;

  drawInstrument();
}

void loop() {
  serviceWifi();

  if (WiFi.status() == WL_CONNECTED && !fetchCycleActive && (long)(millis() - nextRefreshAt) >= 0) {
    startFetchCycle();
  }

  serviceFetchCycle();

  if (millis() - mainScreenChangedAt >= MAIN_SCREEN_INTERVAL_MS) {
    mainScreenChangedAt = millis();

    currentInstrument++;
    if (currentInstrument >= INSTRUMENT_COUNT) {
      currentInstrument = 0;
    }

    screenDirty = true;
  }

  if (millis() - lastClockRefreshAt >= CLOCK_REFRESH_MS) {
    lastClockRefreshAt = millis();
    screenDirty = true;
  }

  if (screenDirty) {
    drawInstrument();
  }

  if (!fetchCycleActive && millis() - lastTickerMoveAt >= TICKER_SPEED_MS) {
    lastTickerMoveAt = millis();
    drawTicker();
  }

  delay(5);
}
