/*
 * Project Name: Vector eInk Dashboard
 * Designed For: TRMNL 7.5" (OG) DIY Kit By Seeed Studio
 *
 * License: GPL3+
 * This project is licensed under the GNU General Public License v3.0 or later.
 * You are free to use, modify, and distribute this software under the terms
 * of the GPL, as long as you preserve the original license and credit the original
 * author. For more details, see <https://www.gnu.org/licenses/gpl-3.0.en.html>.
 *
 * Original work Copyright (C) 2026 Ameya Angadi (GPL-3.0-or-later).
 * Customized and maintained as Vector eInk Dashboard by Huy Vector.
 *
 */


#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <FFat.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "time.h"
#include "TFT_eSPI.h"
#include "icons.h"
#include "slideshow_images.h"
#include "fonts/NotoSansMedium18.h"
#include "fonts/NotoSansMedium24.h"
#include "fonts/NotoSansSemiBold32.h"
#include "fonts/NotoSansBold56.h"

// 1 = UI demo with sample data and 10-second page rotation.
// 0 = normal Vector eInk Dashboard operation.
#ifndef UI_DEMO_MODE
#define UI_DEMO_MODE 0
#endif

// =========================================================================
// ========================== CONFIGURATION ZONE ===========================
// =========================================================================

// --- Wi-Fi Credentials ---
String WIFI_SSID = "";
String WIFI_PASSWORD = "";

// --- API Keys ---
String OWM_API_KEY = "";
String CITY = "Ho Chi Minh City";
String COUNTRY_CODE = "VN";
float WEATHER_LAT = 0.0f;
float WEATHER_LON = 0.0f;
bool weatherLocationResolved = false;
String weatherError = "";

String CMC_API_KEY = "";
String CMC_SYMBOLS = "BTC,ETH,SOL,DOGE";

String ZEN_API_KEY = "";

String TODOIST_API_KEY = "";

// --- Cloud Script API Gateways ---
String STOCK_API_URL = "";

// --- NTP Time Server Configuration ---
const char* NTP_SERVER = "pool.ntp.org";
long GMT_OFFSET_SEC = 25200;
int DAYLIGHT_OFFSET_SEC = 0;
String UI_LANGUAGE = "auto";
bool LUNAR_CALENDAR_ENABLED = false;

// --- Hardware Pins ---
const int BUTTON_PREV = D4;   
const int BUTTON_NEXT = D2;   
const int BUTTON_SELECT = D1; 

// --- Battery Diagnostic Engine Constants ---
#define BATTERY_PIN 1       // GPIO1 (A0) - BAT_ADC pin reader link
#define ADC_EN_PIN 6        // GPIO6 (A5) - ADC_EN power cutoff switch gate
const float CALIBRATION_FACTOR = 0.968;

// --- Timing Intervals (in milliseconds) ---
unsigned long SCREEN_SWITCH_INTERVAL = 300000;
unsigned long DATA_FETCH_INTERVAL = 3600000;
unsigned long SLIDESHOW_IMAGE_INTERVAL = 300000;

// =========================================================================

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

const uint8_t* activeUIFont = nullptr;

void setUIFont(uint8_t size) {
#ifdef EPAPER_ENABLE
  const uint8_t* requested = NotoSansMedium18;
  if (size >= 56) requested = NotoSansBold56;
  else if (size >= 32) requested = NotoSansSemiBold32;
  else if (size >= 24) requested = NotoSansMedium24;
  if (activeUIFont != requested) {
    epaper.loadFont(requested);
    activeUIFont = requested;
  }
#endif
}

Preferences configPreferences;
WebServer configServer(80);
DNSServer dnsServer;

// --- Master Mode State Machine ---
enum SystemMode {
  MODE_PAGES = 0,
  MODE_SLIDESHOW = 1
};
SystemMode currentMode = MODE_PAGES;

// --- Screen State Machine ---
enum ScreenState {
  SCREEN_TIME_QUOTE = 0,
  // Index 1 is intentionally retained so existing saved screen masks stay compatible.
  SCREEN_REMOVED_WEATHER = 1,
  SCREEN_CRYPTO = 2,
  SCREEN_STOCK = 3, 
  SCREEN_TODO = 4, 
  SCREEN_DASHBOARD = 5
};
int currentScreen = SCREEN_DASHBOARD;
const int NUM_SCREENS = 6;
bool screenEnabled[NUM_SCREENS] = {true, false, true, true, true, true};
bool slideshowEnabled = true;

// Uploaded slideshow files live outside the application partition. Each file is
// an 800x480, 1-bit, MSB-first bitmap (exactly 48,000 bytes).
const int MAX_UPLOADED_SLIDES = 50;
const int TOTAL_SLIDESHOW_SLOTS = MAX_UPLOADED_SLIDES + NUM_SLIDESHOW_IMAGES;
bool slideshowStorageReady = false;
enum PhotoStorageBackend { PHOTO_STORAGE_NONE, PHOTO_STORAGE_FFAT, PHOTO_STORAGE_LITTLEFS };
PhotoStorageBackend photoStorageBackend = PHOTO_STORAGE_NONE;
fs::FS* photoStorage = nullptr;
File portalUploadFile;
String portalUploadPath;
String portalUploadError;
bool portalUploadSucceeded = false;
uint8_t slideshowBitmapBuffer[SLIDESHOW_IMAGE_BYTES];

// --- Button Edge Detection States ---
bool lastStatePrev = HIGH;
bool lastStateNext = HIGH;
bool lastStateSel = HIGH;
unsigned long lastButtonPress = 0;
bool needsScreenUpdate = false; 
bool needsDataRefresh = false;  
uint8_t clockRefreshCount = 0;
int rapidPressCount = 0;
unsigned long rapidPressWindowStart = 0;
bool portalCountdownActive = false;
unsigned long portalCountdownStart = 0;
int lastCountdownSecond = -1;

// --- Global Tracking Variables ---
int lastMinute = -1;
int lastDashboardDay = -1;
unsigned long lastScreenSwitch = 0;
unsigned long lastDataFetch = 0;
unsigned long lastSlideshowSwitch = 0;
int currentSlideshowImage = 0;
struct tm demoTimeInfo = {};

struct LunarDate {
  int day;
  int month;
  int year;
  bool leap;
};

// --- Global Data Store: Weather & Quotes ---
String currentTemp = "--";
String feelsLikeTemp = "--"; 
String currentCondition = "--";
String weatherIconCode = "01d";
String currentHumidity = "--";
String currentWind = "--";
String currentPressure = "--";
String sunriseTime = "--:-- AM";
String sunsetTime = "--:-- PM";

String quoteText = "Loading daily inspiration...";
String quoteAuthor = "";

// --- Global Data Store: Crypto ---
String cryptoData[4] = { "BTC: --", "ETH: --", "SOL: --", "DOGE: --" };

// --- Global Data Store: Stocks ---
struct StockData {
  String symbol;
  String name;
  String price;
  String change;
};
StockData activeStocks[5];
int stockCount = 0;

// --- Global Data Store: Tasks ---
String activeTasks[5] = {"", "", "", "", ""};
int taskCount = 0;

// --- Layout Definitions ---
const int CENTER_X = 400;
const int CENTER_Y = 240;

// =========================================================================
// ==================== CORE USER DEFINED FUNCTIONS ========================
// =========================================================================

/* Evaluates tactile inputs, triggers chording windows, and processes navigation state switches. */
void handleButtons();
void loadConfiguration();
void saveConfiguration();
void startConfigPortal();
bool connectWiFiForData(unsigned long timeoutMs = 20000);
void turnOffWiFi();
void lowPowerIdle();
bool syncClockFromNtp(unsigned long timeoutMs = 10000);
bool syncClockFromHttpDate(const String& httpDate);
void restoreFallbackClock();
void updatePortalCountdown();
void drawPortalCountdown(int secondsLeft);
int findEnabledScreen(int from, int direction);
int findSlideshowImageLinear(int from, int direction);
void advanceContent(int direction);
bool slideshowImageValid(int index);
int embeddedSlideshowImageCount();
int uploadedSlideshowImageCount();
int totalSlideshowImageCount();
int findSlideshowImage(int from, int direction);
String uploadedSlidePath(int slot);
bool mountPhotoStorage(bool formatIfNeeded);
size_t photoStorageTotalBytes();
size_t photoStorageUsedBytes();
int photoStorageCapacity();
bool loadUploadedSlide(int slot);
String slideshowFilesJson();
void handleSlideshowUpload();
String urlEncode(const String& value);
bool resolveWeatherLocation(WiFiClientSecure& client, HTTPClient& http);
const char* effectiveLanguageCode();
const char* calendarDayLabel(int weekday);
String localizedDate(const struct tm& timeinfo);
String localizedMonthYear(const struct tm& timeinfo);
double lunarTimeZoneForCountry();
LunarDate solarToLunar(int day, int month, int year, double timeZone);
void buildLunarFooter(const LunarDate& lunar, String& firstLine, String& secondLine);

/* Orchestrates sequentially structured download streams from all paired web services. */
void fetchAllData();

/* Drives structural screen clearing routines and redirects render flows to active templates. */
void switchScreen(int screenIndex, bool cleanCycle = true);

/* Captures multiple high-speed analog samples to extract the precise real-world battery voltage. */
float readBatteryVoltage();

/* Translates raw analog lithium cell potentials into a standard 0 to 100 percent load reading. */
int calculateBatteryPercentage(float voltage);
void drawBatteryStatus(bool leftAligned);

/* Renders the time and quote page containing the layout timepiece and weather summary. */
void drawTimeQuoteScreen(struct tm& timeinfo);

/* Blits the size-scaled clock text strings centered at top boundaries. */
void drawTimeQuoteClock(struct tm& timeinfo);

/* Draws vertical cryptocurrency market rows tracking relative changes inside rounded plates. */
void drawCryptoScreen();

/* Builds horizontal corporate market tickers displaying symbols, company listings, and active valuation trends. */
void drawStockScreen();

/* Generates side-by-side components tracking active user tasks and monthly calendar grids. */
void drawTodoScreen();

/* Renders clock/weather cards on the left and a full monthly calendar on the right. */
void drawDashboardScreen(struct tm& timeinfo);
void drawDashboardClock(struct tm& timeinfo);
void updateDashboardClockPartial(struct tm& timeinfo);

/* Blits full-resolution 800x480 landscape background artwork files onto the display buffer. */
void drawSlideshowScreen(struct tm& timeinfo);

/* Draws floating high-contrast digital time slices in corners of active pictures. */
void drawSlideshowClock(struct tm& timeinfo);

/* Selects and balances vector alignments for incoming weather icon strings. */
void drawWeatherIcon(int x, int y, String iconCode);

/* Analyzes custom strings, wrapping lines tightly according to defined character widths. */
void drawWrappedStringCenter(String text, int yPos, int maxCharsPerLine);

void drawBootScreen() {
#ifdef EPAPER_ENABLE
  epaper.drawBitmap(0, 0, epd_bitmap_Untitled_2, 800, 480, TFT_WHITE, TFT_BLACK);
  epaper.update();
  delay(1800);
#endif
}

bool mountPhotoStorage(bool formatIfNeeded) {
  FFat.end();
  LittleFS.end();
  photoStorage = nullptr;
  photoStorageBackend = PHOTO_STORAGE_NONE;

  if (FFat.begin(false)) {
    photoStorage = &FFat;
    photoStorageBackend = PHOTO_STORAGE_FFAT;
  } else if (LittleFS.begin(false)) {
    photoStorage = &LittleFS;
    photoStorageBackend = PHOTO_STORAGE_LITTLEFS;
  } else if (formatIfNeeded) {
    FFat.end();
    if (FFat.begin(true)) {
      photoStorage = &FFat;
      photoStorageBackend = PHOTO_STORAGE_FFAT;
    } else {
      FFat.end();
      if (LittleFS.begin(true)) {
        photoStorage = &LittleFS;
        photoStorageBackend = PHOTO_STORAGE_LITTLEFS;
      }
    }
  }

  slideshowStorageReady = photoStorage != nullptr;
  if (slideshowStorageReady) {
    Serial.printf("[Photos] Mounted %s\n",
                  photoStorageBackend == PHOTO_STORAGE_FFAT ? "FATFS" : "LittleFS");
  }
  return slideshowStorageReady;
}

size_t photoStorageTotalBytes() {
  if (photoStorageBackend == PHOTO_STORAGE_FFAT) return FFat.totalBytes();
  if (photoStorageBackend == PHOTO_STORAGE_LITTLEFS) return LittleFS.totalBytes();
  return 0;
}

size_t photoStorageUsedBytes() {
  if (photoStorageBackend == PHOTO_STORAGE_FFAT) return FFat.usedBytes();
  if (photoStorageBackend == PHOTO_STORAGE_LITTLEFS) return LittleFS.usedBytes();
  return 0;
}

int photoStorageCapacity() {
  size_t total = photoStorageTotalBytes();
  size_t reserve = 64UL * 1024UL;
  if (total <= reserve) return 0;
  return min(MAX_UPLOADED_SLIDES, (int)((total - reserve) / SLIDESHOW_IMAGE_BYTES));
}

bool slideshowImageValid(int index) {
  if (index >= 0 && index < MAX_UPLOADED_SLIDES) {
    if (!slideshowStorageReady || !photoStorage) return false;
    File file = photoStorage->open(uploadedSlidePath(index), FILE_READ);
    if (!file) return false;
    bool valid = file.size() == SLIDESHOW_IMAGE_BYTES;
    file.close();
    return valid;
  }
  int embeddedIndex = index - MAX_UPLOADED_SLIDES;
  return embeddedIndex >= 0 && embeddedIndex < NUM_SLIDESHOW_IMAGES &&
         slideshow_image_sizes[embeddedIndex] >= SLIDESHOW_IMAGE_BYTES;
}

int embeddedSlideshowImageCount() {
  int count = 0;
  for (int i = 0; i < NUM_SLIDESHOW_IMAGES; i++) {
    if (slideshow_image_sizes[i] >= SLIDESHOW_IMAGE_BYTES) count++;
  }
  return count;
}

String uploadedSlidePath(int slot) {
  char path[20];
  snprintf(path, sizeof(path), "/slide%02d.bin", slot);
  return String(path);
}

int uploadedSlideshowImageCount() {
  if (!slideshowStorageReady) return 0;
  int count = 0;
  for (int i = 0; i < MAX_UPLOADED_SLIDES; i++) {
    if (slideshowImageValid(i)) count++;
  }
  return count;
}

int totalSlideshowImageCount() {
  return uploadedSlideshowImageCount() + embeddedSlideshowImageCount();
}

bool loadUploadedSlide(int slot) {
  if (!slideshowStorageReady || !photoStorage || slot < 0 || slot >= MAX_UPLOADED_SLIDES) return false;
  File file = photoStorage->open(uploadedSlidePath(slot), FILE_READ);
  if (!file || file.size() != SLIDESHOW_IMAGE_BYTES) {
    if (file) file.close();
    return false;
  }
  size_t bytesRead = file.read(slideshowBitmapBuffer, SLIDESHOW_IMAGE_BYTES);
  file.close();
  return bytesRead == SLIDESHOW_IMAGE_BYTES;
}

int findSlideshowImage(int from, int direction) {
  for (int step = 1; step <= TOTAL_SLIDESHOW_SLOTS; step++) {
    int candidate = (from + direction * step + TOTAL_SLIDESHOW_SLOTS * 2) % TOTAL_SLIDESHOW_SLOTS;
    if (slideshowImageValid(candidate)) return candidate;
  }
  return -1;
}

int findSlideshowImageLinear(int from, int direction) {
  for (int candidate = from + direction;
       candidate >= 0 && candidate < TOTAL_SLIDESHOW_SLOTS;
       candidate += direction) {
    if (slideshowImageValid(candidate)) return candidate;
  }
  return -1;
}

String slideshowFilesJson() {
  String json = F("{\"ready\":");
  json += slideshowStorageReady ? F("true") : F("false");
  json += F(",\"count\":");
  json += String(uploadedSlideshowImageCount());
  json += F(",\"max\":");
  json += String(photoStorageCapacity());
  json += F(",\"totalBytes\":");
  json += slideshowStorageReady ? String(photoStorageTotalBytes()) : F("0");
  json += F(",\"usedBytes\":");
  json += slideshowStorageReady ? String(photoStorageUsedBytes()) : F("0");
  json += F(",\"files\":[");
  bool first = true;
  if (slideshowStorageReady) {
    for (int i = 0; i < photoStorageCapacity(); i++) {
      if (!slideshowImageValid(i)) continue;
      if (!first) json += ',';
      first = false;
      json += F("{\"slot\":");
      json += String(i);
      json += F(",\"name\":\"Photo ");
      json += String(i + 1);
      json += F("\"}");
    }
  }
  json += F("]}");
  return json;
}

void handleSlideshowUpload() {
  HTTPUpload& upload = configServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    portalUploadError = "";
    portalUploadSucceeded = false;
    portalUploadPath = "";
    if (!slideshowStorageReady) {
      portalUploadError = "Photo storage is not mounted.";
      return;
    }
    int freeSlot = -1;
    for (int i = 0; i < photoStorageCapacity(); i++) {
      if (!photoStorage->exists(uploadedSlidePath(i))) {
        freeSlot = i;
        break;
      }
    }
    if (freeSlot < 0) {
      portalUploadError = "Photo storage is full.";
      return;
    }
    portalUploadPath = uploadedSlidePath(freeSlot);
    portalUploadFile = photoStorage->open(portalUploadPath, FILE_WRITE);
    if (!portalUploadFile) portalUploadError = "Could not create the photo file.";
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (portalUploadError.length() || !portalUploadFile) return;
    if (portalUploadFile.size() + upload.currentSize > SLIDESHOW_IMAGE_BYTES) {
      portalUploadError = "Converted image is larger than 48,000 bytes.";
      portalUploadFile.close();
      photoStorage->remove(portalUploadPath);
      return;
    }
    if (portalUploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      portalUploadError = "Could not write the complete photo.";
      portalUploadFile.close();
      photoStorage->remove(portalUploadPath);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (portalUploadFile) portalUploadFile.close();
    if (!portalUploadError.length()) {
      File completed = photoStorage->open(portalUploadPath, FILE_READ);
      size_t finalSize = completed ? completed.size() : 0;
      if (completed) completed.close();
      if (finalSize == SLIDESHOW_IMAGE_BYTES) portalUploadSucceeded = true;
      else {
        portalUploadError = "Converted image must be exactly 48,000 bytes.";
        photoStorage->remove(portalUploadPath);
      }
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (portalUploadFile) portalUploadFile.close();
    if (portalUploadPath.length() && photoStorage) photoStorage->remove(portalUploadPath);
    portalUploadError = "Upload was cancelled.";
  }
}

String htmlEscape(const String& value) {
  String out;
  out.reserve(value.length() + 16);
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '\"') out += F("&quot;");
    else out += c;
  }
  return out;
}

void loadConfiguration() {
  configPreferences.begin("vector_cfg", true);
  WIFI_SSID = configPreferences.getString("ssid", "");
  WIFI_PASSWORD = configPreferences.getString("wifiPass", "");
  OWM_API_KEY = configPreferences.getString("owmKey", "");
  CITY = configPreferences.getString("city", "Ho Chi Minh City");
  COUNTRY_CODE = configPreferences.getString("country", "VN");
  String savedLocationKey = configPreferences.getString("locationKey", "");
  WEATHER_LAT = configPreferences.getFloat("weatherLat", 0.0f);
  WEATHER_LON = configPreferences.getFloat("weatherLon", 0.0f);
  weatherLocationResolved = savedLocationKey == CITY + "|" + COUNTRY_CODE;
  CMC_API_KEY = configPreferences.getString("cmcKey", "");
  CMC_SYMBOLS = configPreferences.getString("cmcSymbols", "BTC,ETH,SOL,DOGE");
  ZEN_API_KEY = configPreferences.getString("zenKey", "");
  TODOIST_API_KEY = configPreferences.getString("todoKey", "");
  STOCK_API_URL = configPreferences.getString("stockUrl", "");
  GMT_OFFSET_SEC = configPreferences.getLong("gmt", 25200);
  UI_LANGUAGE = configPreferences.getString("language", "auto");
  LUNAR_CALENDAR_ENABLED = configPreferences.getBool("lunarCalendar", false);
  SCREEN_SWITCH_INTERVAL = configPreferences.getULong("screenMs", 300000);
  DATA_FETCH_INTERVAL = constrain(configPreferences.getULong("fetchMs", 3600000),
                                  3600000UL, 86400000UL);
  SLIDESHOW_IMAGE_INTERVAL = configPreferences.getULong("slideMs", 300000);
  uint8_t mask = configPreferences.getUChar("screens", 0x3F);
  for (int i = 0; i < NUM_SCREENS; i++) screenEnabled[i] = mask & (1 << i);
  screenEnabled[SCREEN_REMOVED_WEATHER] = false;
  bool hasVisibleScreen = false;
  for (int i = 0; i < NUM_SCREENS; i++) hasVisibleScreen |= screenEnabled[i];
  if (!hasVisibleScreen) screenEnabled[SCREEN_DASHBOARD] = true;
  slideshowEnabled = configPreferences.getBool("slideshow", true);
  int firstSlideshowImage = findSlideshowImage(TOTAL_SLIDESHOW_SLOTS - 1, 1);
  if (firstSlideshowImage < 0) slideshowEnabled = false;
  else currentSlideshowImage = firstSlideshowImage;
  configPreferences.end();
}

void saveConfiguration() {
  uint8_t mask = 0;
  for (int i = 0; i < NUM_SCREENS; i++) {
    screenEnabled[i] = i != SCREEN_REMOVED_WEATHER && configServer.hasArg("screen" + String(i));
    if (screenEnabled[i]) mask |= (1 << i);
  }
  if (mask == 0) {
    mask = (1 << SCREEN_DASHBOARD);
    screenEnabled[SCREEN_DASHBOARD] = true;
  }

  WIFI_SSID = configServer.arg("ssid");
  WIFI_PASSWORD = configServer.arg("wifiPass");
  OWM_API_KEY = configServer.arg("owmKey");
  CITY = configServer.arg("city");
  COUNTRY_CODE = configServer.arg("country");
  if (COUNTRY_CODE == "__other__") COUNTRY_CODE = configServer.arg("countryCustom");
  CITY.trim();
  COUNTRY_CODE.trim();
  COUNTRY_CODE.toUpperCase();
  weatherLocationResolved = false;
  CMC_API_KEY = configServer.arg("cmcKey");
  CMC_SYMBOLS = configServer.arg("cmcSymbols");
  ZEN_API_KEY = configServer.arg("zenKey");
  TODOIST_API_KEY = configServer.arg("todoKey");
  STOCK_API_URL = configServer.arg("stockUrl");
  DAYLIGHT_OFFSET_SEC = 0;
  GMT_OFFSET_SEC = configServer.arg("timezone").toInt() * 3600L;
  UI_LANGUAGE = configServer.arg("language");
  if (UI_LANGUAGE.length() == 0) UI_LANGUAGE = "auto";
  LUNAR_CALENDAR_ENABLED = configServer.hasArg("lunarCalendar");
  SCREEN_SWITCH_INTERVAL = max(1L, configServer.arg("screenMin").toInt()) * 60000UL;
  DATA_FETCH_INTERVAL = constrain(configServer.arg("fetchHours").toInt(), 1, 24) * 3600000UL;
  SLIDESHOW_IMAGE_INTERVAL = max(1L, configServer.arg("slideMin").toInt()) * 60000UL;
  slideshowEnabled = configServer.hasArg("slideshow") && totalSlideshowImageCount() > 0;

  configPreferences.begin("vector_cfg", false);
  configPreferences.putString("ssid", WIFI_SSID);
  configPreferences.putString("wifiPass", WIFI_PASSWORD);
  configPreferences.putString("owmKey", OWM_API_KEY);
  configPreferences.putString("city", CITY);
  configPreferences.putString("country", COUNTRY_CODE);
  configPreferences.putString("locationKey", "");
  configPreferences.putString("cmcKey", CMC_API_KEY);
  configPreferences.putString("cmcSymbols", CMC_SYMBOLS);
  configPreferences.putString("zenKey", ZEN_API_KEY);
  configPreferences.putString("todoKey", TODOIST_API_KEY);
  configPreferences.putString("stockUrl", STOCK_API_URL);
  configPreferences.putLong("gmt", GMT_OFFSET_SEC);
  configPreferences.putString("language", UI_LANGUAGE);
  configPreferences.putBool("lunarCalendar", LUNAR_CALENDAR_ENABLED);
  configPreferences.putULong("screenMs", SCREEN_SWITCH_INTERVAL);
  configPreferences.putULong("fetchMs", DATA_FETCH_INTERVAL);
  configPreferences.putULong("slideMs", SLIDESHOW_IMAGE_INTERVAL);
  configPreferences.putUChar("screens", mask);
  configPreferences.putBool("slideshow", slideshowEnabled);
  configPreferences.end();
}

String checked(bool value) { return value ? " checked" : ""; }

const char* uiLabel(const char* en, const char* vi, const char* es, const char* fr, const char* de) {
  String language = effectiveLanguageCode();
  if (language == "vi") return vi;
  if (language == "es") return es;
  if (language == "fr") return fr;
  if (language == "de") return de;
  return en;
}

const char* effectiveLanguageCode() {
  if (UI_LANGUAGE != "auto") return UI_LANGUAGE.c_str();
  if (COUNTRY_CODE == "VN") return "vi";
  if (COUNTRY_CODE == "ES" || COUNTRY_CODE == "MX" || COUNTRY_CODE == "AR" ||
      COUNTRY_CODE == "CL" || COUNTRY_CODE == "CO" || COUNTRY_CODE == "PE") return "es";
  if (COUNTRY_CODE == "FR" || COUNTRY_CODE == "BE") return "fr";
  if (COUNTRY_CODE == "DE" || COUNTRY_CODE == "AT" || COUNTRY_CODE == "CH") return "de";
  return "en";
}

const char* calendarDayLabel(int weekday) {
  static const char* en[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  static const char* vi[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
  static const char* es[] = {"Do", "Lu", "Ma", "Mi", "Ju", "Vi", "Sa"};
  static const char* fr[] = {"Di", "Lu", "Ma", "Me", "Je", "Ve", "Sa"};
  static const char* de[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  String language = effectiveLanguageCode();
  if (language == "vi") return vi[weekday];
  if (language == "es") return es[weekday];
  if (language == "fr") return fr[weekday];
  if (language == "de") return de[weekday];
  return en[weekday];
}

String localizedMonthYear(const struct tm& timeinfo) {
  static const char* en[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
  static const char* es[] = {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
  static const char* fr[] = {"Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", "Août", "Septembre", "Octobre", "Novembre", "Décembre"};
  static const char* de[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};
  String language = effectiveLanguageCode();
  int year = timeinfo.tm_year + 1900;
  if (language == "vi") return "Tháng " + String(timeinfo.tm_mon + 1) + " " + String(year);
  if (language == "es") return String(es[timeinfo.tm_mon]) + " " + year;
  if (language == "fr") return String(fr[timeinfo.tm_mon]) + " " + year;
  if (language == "de") return String(de[timeinfo.tm_mon]) + " " + year;
  return String(en[timeinfo.tm_mon]) + " " + year;
}

String localizedDate(const struct tm& timeinfo) {
  static const char* enDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  static const char* viDays[] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};
  static const char* esDays[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
  static const char* frDays[] = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"};
  static const char* deDays[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
  String language = effectiveLanguageCode();
  const char* weekday = enDays[timeinfo.tm_wday];
  if (language == "vi") weekday = viDays[timeinfo.tm_wday];
  else if (language == "es") weekday = esDays[timeinfo.tm_wday];
  else if (language == "fr") weekday = frDays[timeinfo.tm_wday];
  else if (language == "de") weekday = deDays[timeinfo.tm_wday];
  if (language == "vi") return String(weekday) + ", " + timeinfo.tm_mday + "/" + (timeinfo.tm_mon + 1) + "/" + (timeinfo.tm_year + 1900);
  return String(weekday) + ", " + timeinfo.tm_mday + " " + localizedMonthYear(timeinfo);
}

String buildPortalPage() {
  String page;
  page.reserve(24000);
  page += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>Vector eInk Setup</title><style>body{font-family:system-ui;margin:auto;max-width:760px;padding:20px;background:#111827;color:#f9fafb}fieldset{border:1px solid #374151;border-radius:12px;margin:16px 0;padding:16px}legend{color:#fbbf24;font-weight:700}label{display:block;margin:10px 0 4px}input,select{box-sizing:border-box;width:100%;padding:10px;border-radius:8px;border:1px solid #4b5563;background:#0b1220;color:white}.check{display:flex;gap:8px;align-items:center}.check input{width:auto}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.muted,small{color:#9ca3af}.photo{display:flex;justify-content:space-between;align-items:center;border-top:1px solid #374151;padding:9px 0}.photo button{width:auto;padding:7px 12px;background:#ef4444;color:white}.preview{display:none;width:100%;height:auto;margin:12px 0;border:1px solid #4b5563;background:white}@media(max-width:600px){.grid{grid-template-columns:1fr}}button{width:100%;padding:14px;border:0;border-radius:10px;background:#fbbf24;font-weight:800;cursor:pointer}button:disabled{opacity:.5;cursor:not-allowed}</style></head><body><h1>Vector eInk Setup</h1><form method='post' action='/save'>");
  page += F("<fieldset><legend>Wi-Fi</legend><label>Wi-Fi name (SSID)</label><input name='ssid' list='networks' required value='"); page += htmlEscape(WIFI_SSID); page += F("'><datalist id='networks'>");
  int count = WiFi.scanNetworks();
  for (int i = 0; i < count; i++) { page += F("<option value='"); page += htmlEscape(WiFi.SSID(i)); page += F("'>"); }
  page += F("</datalist><label>Wi-Fi password</label><input type='password' name='wifiPass' value='"); page += htmlEscape(WIFI_PASSWORD); page += F("'></fieldset>");
  page += F("<fieldset><legend>Weather</legend><label>OpenWeather API key</label><input name='owmKey' value='"); page += htmlEscape(OWM_API_KEY); page += F("'><div class='grid'><div><label>Country</label><select id='country' name='country' onchange='updateCountry(true)'>");
  const char* countryCodes[] = {"VN","US","GB","CA","AU","NZ","JP","KR","CN","TW","HK","SG","TH","MY","ID","PH","IN","PK","BD","LK","NP","AE","SA","QA","TR","IL","FR","DE","ES","IT","PT","NL","BE","CH","AT","PL","CZ","HU","RO","GR","SE","NO","DK","FI","IE","RU","UA","BR","AR","CL","CO","PE","MX","ZA","EG","MA","NG","KE","GH"};
  const char* countryNames[] = {"Vietnam","United States","United Kingdom","Canada","Australia","New Zealand","Japan","South Korea","China","Taiwan","Hong Kong","Singapore","Thailand","Malaysia","Indonesia","Philippines","India","Pakistan","Bangladesh","Sri Lanka","Nepal","United Arab Emirates","Saudi Arabia","Qatar","Türkiye","Israel","France","Germany","Spain","Italy","Portugal","Netherlands","Belgium","Switzerland","Austria","Poland","Czechia","Hungary","Romania","Greece","Sweden","Norway","Denmark","Finland","Ireland","Russia","Ukraine","Brazil","Argentina","Chile","Colombia","Peru","Mexico","South Africa","Egypt","Morocco","Nigeria","Kenya","Ghana"};
  const int countryCount = sizeof(countryCodes) / sizeof(countryCodes[0]);
  bool listedCountry = false;
  for (int i = 0; i < countryCount; i++) {
    page += F("<option value='"); page += countryCodes[i]; page += "'";
    if (COUNTRY_CODE == countryCodes[i]) { page += F(" selected"); listedCountry = true; }
    page += ">"; page += countryNames[i]; page += " ("; page += countryCodes[i]; page += F(")</option>");
  }
  page += F("<option value='__other__'"); if (!listedCountry) page += F(" selected"); page += F(">Other country / enter code manually</option></select><input id='countryCustom' name='countryCustom' maxlength='2' pattern='[A-Za-z]{2}' value='");
  if (!listedCountry) page += htmlEscape(COUNTRY_CODE);
  page += F("' placeholder='Two-letter ISO code, e.g. NO' style='display:none;margin-top:8px' oninput=\"this.value=this.value.toUpperCase();updateCities(true)\"></div><div><label>City</label><select id='cityPreset' onchange='chooseCity()'></select><input id='city' name='city' required value='"); page += htmlEscape(CITY); page += F("' placeholder='Enter a city, district, or area' style='display:none;margin-top:8px'></div></div>");
  page += F("<small>The list contains common countries only. For another country, select ‘Other country’ and enter its two-letter ISO 3166-1 code in uppercase (for example: NO, IS, UY). Choose a suggested city or ‘Other city’ to enter any location recognized by OpenWeather.</small>");
  page += F("<script>const cityMap={"
            "VN:['Hanoi','Ho Chi Minh City','Da Nang','Hai Phong','Can Tho','Hue','Nha Trang','Da Lat','Vung Tau','Quy Nhon','Buon Ma Thuot','Bien Hoa'],"
            "US:['New York','Los Angeles','Chicago','Houston','Phoenix','Philadelphia','San Antonio','San Diego','Dallas','San Francisco','Washington'],"
            "GB:['London','Manchester','Birmingham','Liverpool','Edinburgh','Glasgow','Bristol'],"
            "CA:['Toronto','Montreal','Vancouver','Calgary','Ottawa','Edmonton','Quebec City'],"
            "AU:['Sydney','Melbourne','Brisbane','Perth','Adelaide','Canberra','Gold Coast'],"
            "NZ:['Auckland','Wellington','Christchurch','Hamilton','Dunedin'],"
            "JP:['Tokyo','Osaka','Kyoto','Yokohama','Nagoya','Sapporo','Fukuoka'],"
            "KR:['Seoul','Busan','Incheon','Daegu','Daejeon','Gwangju'],"
            "CN:['Beijing','Shanghai','Guangzhou','Shenzhen','Chengdu','Hangzhou','Wuhan'],"
            "TW:['Taipei','Kaohsiung','Taichung','Tainan','Hsinchu'],"
            "HK:['Hong Kong','Kowloon','Tsuen Wan','Sha Tin'],"
            "SG:['Singapore'],"
            "TH:['Bangkok','Chiang Mai','Phuket','Pattaya','Khon Kaen'],"
            "MY:['Kuala Lumpur','George Town','Johor Bahru','Kota Kinabalu','Kuching'],"
            "ID:['Jakarta','Surabaya','Bandung','Medan','Denpasar','Yogyakarta'],"
            "PH:['Manila','Quezon City','Cebu City','Davao City','Baguio'],"
            "IN:['New Delhi','Mumbai','Bengaluru','Chennai','Kolkata','Hyderabad','Pune'],"
            "PK:['Islamabad','Karachi','Lahore','Rawalpindi','Peshawar'],"
            "BD:['Dhaka','Chattogram','Khulna','Rajshahi','Sylhet'],"
            "LK:['Colombo','Kandy','Galle','Jaffna','Negombo'],"
            "NP:['Kathmandu','Pokhara','Lalitpur','Biratnagar'],"
            "AE:['Dubai','Abu Dhabi','Sharjah','Ajman'],"
            "SA:['Riyadh','Jeddah','Mecca','Medina','Dammam'],"
            "QA:['Doha','Al Rayyan','Al Wakrah'],"
            "TR:['Istanbul','Ankara','Izmir','Bursa','Antalya'],"
            "IL:['Jerusalem','Tel Aviv','Haifa','Beersheba'],"
            "FR:['Paris','Marseille','Lyon','Toulouse','Nice','Bordeaux'],"
            "DE:['Berlin','Hamburg','Munich','Cologne','Frankfurt','Stuttgart'],"
            "ES:['Madrid','Barcelona','Valencia','Seville','Bilbao','Malaga'],"
            "IT:['Rome','Milan','Naples','Turin','Florence','Venice'],"
            "PT:['Lisbon','Porto','Braga','Coimbra','Faro'],"
            "NL:['Amsterdam','Rotterdam','The Hague','Utrecht','Eindhoven'],"
            "BE:['Brussels','Antwerp','Ghent','Bruges','Liege'],"
            "CH:['Zurich','Geneva','Basel','Bern','Lausanne'],"
            "AT:['Vienna','Graz','Salzburg','Innsbruck','Linz'],"
            "PL:['Warsaw','Krakow','Wroclaw','Gdansk','Poznan'],"
            "CZ:['Prague','Brno','Ostrava','Plzen'],"
            "HU:['Budapest','Debrecen','Szeged','Pecs'],"
            "RO:['Bucharest','Cluj-Napoca','Timisoara','Iasi','Brasov'],"
            "GR:['Athens','Thessaloniki','Patras','Heraklion'],"
            "SE:['Stockholm','Gothenburg','Malmo','Uppsala'],"
            "NO:['Oslo','Bergen','Trondheim','Stavanger'],"
            "DK:['Copenhagen','Aarhus','Odense','Aalborg'],"
            "FI:['Helsinki','Espoo','Tampere','Turku'],"
            "IE:['Dublin','Cork','Galway','Limerick'],"
            "RU:['Moscow','Saint Petersburg','Novosibirsk','Yekaterinburg','Kazan'],"
            "UA:['Kyiv','Lviv','Odesa','Kharkiv','Dnipro'],"
            "BR:['Sao Paulo','Rio de Janeiro','Brasilia','Salvador','Fortaleza'],"
            "AR:['Buenos Aires','Cordoba','Rosario','Mendoza'],"
            "CL:['Santiago','Valparaiso','Concepcion','Antofagasta'],"
            "CO:['Bogota','Medellin','Cali','Cartagena','Barranquilla'],"
            "PE:['Lima','Arequipa','Cusco','Trujillo'],"
            "MX:['Mexico City','Guadalajara','Monterrey','Puebla','Tijuana'],"
            "ZA:['Johannesburg','Cape Town','Durban','Pretoria','Gqeberha'],"
            "EG:['Cairo','Alexandria','Giza','Luxor','Aswan'],"
            "MA:['Casablanca','Marrakech','Rabat','Fes','Tangier'],"
            "NG:['Lagos','Abuja','Kano','Ibadan','Port Harcourt'],"
            "KE:['Nairobi','Mombasa','Kisumu','Nakuru'],"
            "GH:['Accra','Kumasi','Tamale','Takoradi']};"
            "const lunarRegions={VN:'Vietnamese lunar calendar · UTC+7',CN:'Chinese lunar calendar · UTC+8',TW:'Traditional Chinese lunar calendar · UTC+8',HK:'Traditional Chinese lunar calendar · UTC+8',SG:'Lunar calendar · UTC+8',MY:'Malaysian lunar display · UTC+8',KR:'Korean lunar calendar · UTC+9',JP:'Japanese lunisolar display · UTC+9'};"
            "const savedCity=document.getElementById('city').value;"
            "function selectedCountryCode(){const p=document.getElementById('country'),c=document.getElementById('countryCustom');return p.value==='__other__'?c.value.trim().toUpperCase():p.value;}"
            "function updateCountry(reset){const p=document.getElementById('country'),c=document.getElementById('countryCustom'),manual=p.value==='__other__';c.style.display=manual?'block':'none';c.required=manual;updateCities(reset);}"
            "function chooseCity(){const p=document.getElementById('cityPreset'),i=document.getElementById('city'),custom=p.value==='__other__';i.style.display=custom?'block':'none';i.readOnly=!custom;if(!custom)i.value=p.value;}"
            "function updateLunarHelp(){const e=document.getElementById('lunarHelp'),box=document.getElementById('lunarCalendar');if(!e||!box)return;const code=selectedCountryCode(),supported=Boolean(lunarRegions[code]);e.textContent=lunarRegions[code]||'A regional lunar calendar is not available for this country; the standard calendar will remain unchanged.';box.disabled=!supported;if(!supported)box.checked=false;}"
            "function updateCities(reset){const code=selectedCountryCode(),p=document.getElementById('cityPreset'),i=document.getElementById('city'),cities=cityMap[code]||[];p.innerHTML='';cities.forEach(v=>{const o=document.createElement('option');o.value=v;o.textContent=v;p.appendChild(o);});const other=document.createElement('option');other.value='__other__';other.textContent='Other city…';p.appendChild(other);if(reset){p.value=cities.length?cities[0]:'__other__';if(cities.length)i.value=cities[0];else i.value='';}else{const match=cities.find(v=>v.toLowerCase()===savedCity.toLowerCase());p.value=match||'__other__';}chooseCity();updateLunarHelp();}"
            "updateCountry(false);</script></fieldset>");
  page += F("<fieldset><legend>Services</legend><label>CoinMarketCap API key</label><input name='cmcKey' value='"); page += htmlEscape(CMC_API_KEY); page += F("'><label>Crypto symbols</label><input name='cmcSymbols' value='"); page += htmlEscape(CMC_SYMBOLS); page += F("'><label>ZenQuotes API key</label><input name='zenKey' value='"); page += htmlEscape(ZEN_API_KEY); page += F("'><small>Optional: Quote of the Day works on the free endpoint without a key. Enter a key only for higher limits or Premium features.</small><label>Todoist token</label><input name='todoKey' value='"); page += htmlEscape(TODOIST_API_KEY); page += F("'><label>Google Apps Script Stock URL</label><input name='stockUrl' value='"); page += htmlEscape(STOCK_API_URL); page += F("'></fieldset>");
  page += F("<fieldset><legend>Time & language</legend><label>Time zone</label><select name='timezone'>");
  int currentTimezone = GMT_OFFSET_SEC / 3600;
  for (int timezone = -12; timezone <= 14; timezone++) {
    page += F("<option value='"); page += String(timezone); page += "'";
    if (timezone == currentTimezone) page += F(" selected");
    page += F(">UTC");
    if (timezone >= 0) page += "+";
    page += String(timezone); page += F("</option>");
  }
  page += F("</select><label>Dashboard language</label><select name='language'>");
  const char* languageCodes[] = {"auto", "vi", "en", "es", "fr", "de"};
  const char* languageNames[] = {"Auto (from weather country)", "Vietnamese", "English", "Spanish", "French", "German"};
  for (int i = 0; i < 6; i++) {
    page += F("<option value='"); page += languageCodes[i]; page += "'";
    if (UI_LANGUAGE == languageCodes[i]) page += F(" selected");
    page += ">"; page += languageNames[i]; page += F("</option>");
  }
  page += F("</select><small>Auto currently maps Vietnam to Vietnamese; Spanish-speaking countries to Spanish; France/Belgium to French; Germany/Austria/Switzerland to German; all other countries use English.</small></fieldset>");
  page += F("<fieldset><legend>Calendar type</legend><label class='check'><input type='checkbox' checked disabled>Gregorian calendar</label><label class='check'><input id='lunarCalendar' type='checkbox' name='lunarCalendar'");
  page += checked(LUNAR_CALENDAR_ENABLED);
  page += F(">Show regional lunar date</label><small id='lunarHelp'></small></fieldset><script>updateLunarHelp();</script>");
  page += F("<fieldset><legend>Display pages</legend>");
  const char* names[] = {"Time & Quote", nullptr, "Cryptocurrency", "Stocks", "Tasks & Calendar", "Dashboard"};
  for (int i = 0; i < NUM_SCREENS; i++) {
    if (i == SCREEN_REMOVED_WEATHER) continue;
    page += F("<label class='check'><input type='checkbox' name='screen"); page += String(i); page += "'" + checked(screenEnabled[i]) + ">" + names[i] + "</label>";
  }
  int embeddedSlides = embeddedSlideshowImageCount();
  int uploadedSlides = uploadedSlideshowImageCount();
  int allSlides = embeddedSlides + uploadedSlides;
  page += F("<label class='check'><input id='slideshowToggle' type='checkbox'");
  if (allSlides > 0) page += F(" name='slideshow'"); else page += F(" disabled");
  page += checked(slideshowEnabled); page += F(">Include photos in the page rotation</label><small>Embedded images detected: "); page += String(embeddedSlides);
  page += F(". Uploaded photos detected: "); page += String(uploadedSlides);
  page += F(". Uploaded photos are stored in the filesystem partition instead of the firmware.</small></fieldset>");
  page += F("<fieldset><legend>Intervals</legend><div class='grid'><div><label>Change page (minutes)</label><input type='number' min='1' name='screenMin' value='"); page += String(SCREEN_SWITCH_INTERVAL / 60000UL); page += F("'></div><div><label>Fetch API data</label><select name='fetchHours'>");
  int fetchHours = constrain((int)(DATA_FETCH_INTERVAL / 3600000UL), 1, 24);
  for (int hour = 1; hour <= 24; hour++) {
    page += F("<option value='"); page += String(hour); page += "'";
    if (hour == fetchHours) page += F(" selected");
    page += ">"; page += String(hour); page += (hour == 1 ? F(" hour") : F(" hours")); page += F("</option>");
  }
  page += F("</select><small>Wi-Fi stays off between fetches. Press the Select/Update button to fetch immediately.</small></div><div><label>Change photo (minutes)</label><input type='number' min='1' name='slideMin' value='"); page += String(SLIDESHOW_IMAGE_INTERVAL / 60000UL); page += F("'></div></div></fieldset><button type='submit'>SAVE & REBOOT</button></form>");
  page += F("<fieldset><legend>Slideshow photos</legend><p class='muted'>Select JPG or PNG files. Your browser crops each photo to 800×480, applies Floyd–Steinberg black-and-white dithering, and uploads only the 48 KB e-ink bitmap. Capacity depends on the selected filesystem partition.</p><input id='photoFiles' type='file' accept='image/jpeg,image/png' multiple><canvas id='photoPreview' class='preview' width='800' height='480'></canvas><button id='uploadPhotos' type='button' onclick='uploadSelectedPhotos()'>CONVERT & UPLOAD</button><p id='photoStatus' class='muted'></p><div id='photoList'></div>");
  if (!slideshowStorageReady) page += F("<p><b>Photo storage is not mounted.</b> Use the format button once on a new FATFS or LittleFS partition. Formatting deletes every uploaded photo.</p><button type='button' onclick='formatPhotoStorage()'>FORMAT PHOTO STORAGE</button>");
  page += F("</fieldset><script>const $=id=>document.getElementById(id);async function photoInfo(){try{const r=await fetch('/photos');const d=await r.json();const mb=n=>(n/1048576).toFixed(1),toggle=$('slideshowToggle');$('photoStatus').textContent=d.ready?`${d.count}/${d.max} photos · ${mb(d.usedBytes)} MB used of ${mb(d.totalBytes)} MB`:'Photo storage is unavailable.';$('photoList').innerHTML=d.files.map(f=>`<div class='photo'><span>${f.name}</span><button type='button' onclick='deletePhoto(${f.slot})'>DELETE</button></div>`).join('');if(toggle&&d.count>0){toggle.disabled=false;toggle.name='slideshow'}return d}catch(e){$('photoStatus').textContent='Could not read photo storage.'}}async function deletePhoto(slot){if(!confirm('Delete this photo?'))return;await fetch('/photo/delete?slot='+slot,{method:'POST'});await photoInfo()}async function formatPhotoStorage(){if(!confirm('Format photo storage? All uploaded photos will be deleted.'))return;const r=await fetch('/photos/format',{method:'POST'});alert(await r.text());location.reload()}function convertPhoto(file){return new Promise((resolve,reject)=>{const img=new Image(),url=URL.createObjectURL(file);img.onload=()=>{const c=$('photoPreview'),x=c.getContext('2d',{willReadFrequently:true}),scale=Math.max(800/img.width,480/img.height),w=img.width*scale,h=img.height*scale;x.fillStyle='white';x.fillRect(0,0,800,480);x.drawImage(img,(800-w)/2,(480-h)/2,w,h);URL.revokeObjectURL(url);const data=x.getImageData(0,0,800,480),gray=new Float32Array(800*480);for(let i=0;i<gray.length;i++)gray[i]=.299*data.data[i*4]+.587*data.data[i*4+1]+.114*data.data[i*4+2];const out=new Uint8Array(48000);for(let y=0;y<480;y++)for(let xx=0;xx<800;xx++){const i=y*800+xx,old=gray[i],v=old>=128?255:0,e=old-v;if(v)out[i>>3]|=128>>(i&7);if(xx<799)gray[i+1]+=e*7/16;if(y<479){if(xx)gray[i+799]+=e*3/16;gray[i+800]+=e*5/16;if(xx<799)gray[i+801]+=e/16}}c.style.display='block';x.putImageData((()=>{const p=x.createImageData(800,480);for(let i=0;i<gray.length;i++){const v=(out[i>>3]&(128>>(i&7)))?255:0;p.data[i*4]=p.data[i*4+1]=p.data[i*4+2]=v;p.data[i*4+3]=255}return p})(),0,0);resolve(out)},img.onerror=()=>reject(new Error('Cannot open '+file.name));img.src=url})}async function uploadSelectedPhotos(){const files=[...$('photoFiles').files],btn=$('uploadPhotos');if(!files.length){$('photoStatus').textContent='Choose at least one JPG or PNG file.';return}btn.disabled=true;try{for(let i=0;i<files.length;i++){$('photoStatus').textContent=`Converting ${i+1}/${files.length}: ${files[i].name}`;const bytes=await convertPhoto(files[i]),form=new FormData();form.append('photo',new Blob([bytes],{type:'application/octet-stream'}),files[i].name+'.bin');const r=await fetch('/photo/upload',{method:'POST',body:form}),msg=await r.text();if(!r.ok)throw new Error(msg)}$('photoFiles').value='';await photoInfo();$('photoStatus').textContent+=' · Upload complete. Include photos in the page rotation above, then save settings.'}catch(e){$('photoStatus').textContent='Upload failed: '+e.message}finally{btn.disabled=false}}photoInfo();</script></body></html>");
  return page;
}

bool connectWiFiForData(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (WIFI_SSID.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  // Keep modem sleep disabled only during the short data/NTP session.
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < timeoutMs) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected for data: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("[WiFi] Data connection timed out");
  turnOffWiFi();
  return false;
}

void turnOffWiFi() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  Serial.println("[WiFi] Radio off");
}

void restoreFallbackClock() {
  configPreferences.begin("vector_cfg", true);
  uint64_t savedEpoch = configPreferences.getULong64("lastEpoch", 0);
  configPreferences.end();

  time_t fallbackEpoch = (time_t)savedEpoch;
  if (fallbackEpoch < 1700000000) {
    const char* monthNames = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char monthText[4] = {};
    int day = 1, year = 2026, hour = 0, minute = 0, second = 0;
    sscanf(__DATE__, "%3s %d %d", monthText, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
    const char* monthAt = strstr(monthNames, monthText);
    struct tm buildTime = {};
    buildTime.tm_year = year - 1900;
    buildTime.tm_mon = monthAt ? (monthAt - monthNames) / 3 : 0;
    buildTime.tm_mday = day;
    buildTime.tm_hour = hour;
    buildTime.tm_min = minute;
    buildTime.tm_sec = second;
    buildTime.tm_isdst = -1;
    fallbackEpoch = mktime(&buildTime);
  }

  struct timeval fallback = {fallbackEpoch, 0};
  settimeofday(&fallback, nullptr);
  Serial.printf("[Time] Using fallback epoch: %lld\n", (long long)fallbackEpoch);
}

bool syncClockFromNtp(unsigned long timeoutMs) {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC,
             NTP_SERVER, "time.google.com", "time.cloudflare.com");
  unsigned long started = millis();
  struct tm timeinfo;
  while (millis() - started < timeoutMs) {
    if (getLocalTime(&timeinfo, 1000) && timeinfo.tm_year + 1900 >= 2024) {
      time_t now = time(nullptr);
      configPreferences.begin("vector_cfg", false);
      configPreferences.putULong64("lastEpoch", (uint64_t)now);
      configPreferences.end();
      Serial.printf("[Time] NTP synchronized: %lld\n", (long long)now);
      return true;
    }
    Serial.print("*");
  }
  Serial.println("\n[Time] NTP timeout; continuing with fallback clock");
  restoreFallbackClock();
  return false;
}

bool syncClockFromHttpDate(const String& httpDate) {
  if (httpDate.length() < 20) return false;
  struct tm utcTime = {};
  char* parsedEnd = strptime(httpDate.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &utcTime);
  if (!parsedEnd) return false;
  int year = utcTime.tm_year + 1900;
  unsigned month = utcTime.tm_mon + 1;
  unsigned day = utcTime.tm_mday;
  year -= month <= 2;
  int era = (year >= 0 ? year : year - 399) / 400;
  unsigned yearOfEra = year - era * 400;
  int adjustedMonth = (int)month + (month > 2 ? -3 : 9);
  unsigned dayOfYear = (153 * adjustedMonth + 2) / 5 + day - 1;
  unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  int64_t daysSinceEpoch = (int64_t)era * 146097 + dayOfEra - 719468;
  time_t serverEpoch = (time_t)(daysSinceEpoch * 86400LL + utcTime.tm_hour * 3600L +
                                utcTime.tm_min * 60L + utcTime.tm_sec);
  if (serverEpoch < 1700000000) return false;

  struct timeval serverClock = {serverEpoch, 0};
  settimeofday(&serverClock, nullptr);
  configPreferences.begin("vector_cfg", false);
  configPreferences.putULong64("lastEpoch", (uint64_t)serverEpoch);
  configPreferences.end();
  Serial.printf("[Time] Synchronized from HTTPS Date: %lld\n", (long long)serverEpoch);
  return true;
}

void lowPowerIdle() {
  // Light sleep retains RAM, millis()/RTC time and the display framebuffer.
  // A 250 ms safety timer keeps page/minute scheduling accurate; any button
  // wakes the MCU immediately because the buttons are active-low.
  esp_sleep_enable_timer_wakeup(250000ULL);
  esp_light_sleep_start();
}

void startConfigPortal() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Vector-eInk-Setup", "vector75");
  dnsServer.start(53, "*", WiFi.softAPIP());
  bool saved = false;

  configServer.on("/", HTTP_GET, []() {
    configServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    configServer.sendHeader("Pragma", "no-cache");
    configServer.send(200, "text/html; charset=utf-8", buildPortalPage());
  });
  configServer.on("/save", HTTP_POST, [&saved]() {
    saveConfiguration();
    configServer.sendHeader("Cache-Control", "no-store");
    configServer.send(200, "text/html; charset=utf-8", "<meta charset='utf-8'><h2>Settings saved. Vector eInk Dashboard is rebooting…</h2>");
    saved = true;
  });
  configServer.on("/photos", HTTP_GET, []() {
    configServer.sendHeader("Cache-Control", "no-store");
    configServer.send(200, "application/json", slideshowFilesJson());
  });
  configServer.on("/photo/upload", HTTP_POST, []() {
    if (portalUploadSucceeded) {
      configServer.send(200, "text/plain", "Photo uploaded.");
    } else {
      String message = portalUploadError.length() ? portalUploadError : "Upload failed.";
      configServer.send(400, "text/plain", message);
    }
    portalUploadSucceeded = false;
    portalUploadError = "";
  }, handleSlideshowUpload);
  configServer.on("/photo/delete", HTTP_POST, []() {
    int slot = configServer.arg("slot").toInt();
    if (!slideshowStorageReady || slot < 0 || slot >= MAX_UPLOADED_SLIDES) {
      configServer.send(400, "text/plain", "Invalid photo slot.");
      return;
    }
    bool removed = photoStorage && photoStorage->remove(uploadedSlidePath(slot));
    if (removed && currentSlideshowImage == slot) {
      currentSlideshowImage = findSlideshowImage(slot, 1);
      if (currentSlideshowImage < 0) {
        slideshowEnabled = false;
        currentMode = MODE_PAGES;
        currentScreen = screenEnabled[SCREEN_DASHBOARD]
                          ? SCREEN_DASHBOARD
                          : findEnabledScreen(SCREEN_DASHBOARD, 1);
      }
    }
    configServer.send(removed ? 200 : 404, "text/plain", removed ? "Photo deleted." : "Photo not found.");
  });
  configServer.on("/photos/format", HTTP_POST, []() {
    if (portalUploadFile) portalUploadFile.close();
    slideshowStorageReady = mountPhotoStorage(true);
    configServer.send(slideshowStorageReady ? 200 : 500, "text/plain",
                      slideshowStorageReady ? "Photo storage formatted." : "Could not format photo storage.");
  });
  configServer.onNotFound([]() {
    configServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    configServer.send(302, "text/plain", "");
  });
  configServer.begin();

#ifdef EPAPER_ENABLE
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextDatum(MC_DATUM);
  setUIFont(32);
  epaper.drawString("Vector eInk Setup", CENTER_X, 120);
  setUIFont(24);
  epaper.drawString("Wi-Fi: Vector-eInk-Setup", CENTER_X, 220);
  epaper.drawString("Password: vector75", CENTER_X, 270);
  epaper.drawString("Open: 192.168.4.1", CENTER_X, 320);
  epaper.update();
#endif

  while (!saved) {
    dnsServer.processNextRequest();
    configServer.handleClient();
    delay(2);
  }
  delay(1200);
  ESP.restart();
}

int findEnabledScreen(int from, int direction) {
  for (int step = 1; step <= NUM_SCREENS; step++) {
    int candidate = (from + direction * step + NUM_SCREENS * 2) % NUM_SCREENS;
    if (screenEnabled[candidate]) return candidate;
  }
  return SCREEN_DASHBOARD;
}

void advanceContent(int direction) {
  // Unified order: Dashboard -> Time & Quote -> Crypto -> Stocks -> Tasks -> photos.
  const int pageOrder[] = {
    SCREEN_DASHBOARD, SCREEN_TIME_QUOTE, SCREEN_CRYPTO, SCREEN_STOCK, SCREEN_TODO
  };
  const int pageCount = sizeof(pageOrder) / sizeof(pageOrder[0]);

  if (currentMode == MODE_SLIDESHOW) {
    int adjacentImage = findSlideshowImageLinear(currentSlideshowImage, direction);
    if (slideshowEnabled && adjacentImage >= 0) {
      currentSlideshowImage = adjacentImage;
      lastSlideshowSwitch = millis();
      needsScreenUpdate = true;
      return;
    }

    currentMode = MODE_PAGES;
    if (direction > 0) {
      for (int i = 0; i < pageCount; i++) {
        if (screenEnabled[pageOrder[i]]) {
          currentScreen = pageOrder[i];
          break;
        }
      }
    } else {
      for (int i = pageCount - 1; i >= 0; i--) {
        if (screenEnabled[pageOrder[i]]) {
          currentScreen = pageOrder[i];
          break;
        }
      }
    }
    lastScreenSwitch = millis();
    needsScreenUpdate = true;
    return;
  }

  int currentPosition = 0;
  for (int i = 0; i < pageCount; i++) {
    if (pageOrder[i] == currentScreen) {
      currentPosition = i;
      break;
    }
  }

  if (direction > 0) {
    for (int i = currentPosition + 1; i < pageCount; i++) {
      if (screenEnabled[pageOrder[i]]) {
        currentScreen = pageOrder[i];
        lastScreenSwitch = millis();
        needsScreenUpdate = true;
        return;
      }
    }
    if (slideshowEnabled) {
      int firstImage = findSlideshowImage(-1, 1);
      if (firstImage >= 0) {
        currentMode = MODE_SLIDESHOW;
        currentSlideshowImage = firstImage;
        lastSlideshowSwitch = millis();
        needsScreenUpdate = true;
        return;
      }
    }
    for (int i = 0; i <= currentPosition; i++) {
      if (screenEnabled[pageOrder[i]]) {
        currentScreen = pageOrder[i];
        lastScreenSwitch = millis();
        needsScreenUpdate = true;
        return;
      }
    }
  } else {
    for (int i = currentPosition - 1; i >= 0; i--) {
      if (screenEnabled[pageOrder[i]]) {
        currentScreen = pageOrder[i];
        lastScreenSwitch = millis();
        needsScreenUpdate = true;
        return;
      }
    }
    if (slideshowEnabled) {
      int lastImage = findSlideshowImage(0, -1);
      // findSlideshowImage wraps; start at slot zero and request the previous valid slot.
      if (lastImage >= 0) {
        currentMode = MODE_SLIDESHOW;
        currentSlideshowImage = lastImage;
        lastSlideshowSwitch = millis();
        needsScreenUpdate = true;
        return;
      }
    }
    for (int i = pageCount - 1; i >= currentPosition; i--) {
      if (screenEnabled[pageOrder[i]]) {
        currentScreen = pageOrder[i];
        lastScreenSwitch = millis();
        needsScreenUpdate = true;
        return;
      }
    }
  }
}

void drawPortalCountdown(int secondsLeft) {
#ifdef EPAPER_ENABLE
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextDatum(MC_DATUM);
  setUIFont(32);
  epaper.drawString("Wi-Fi setup in", CENTER_X, 150);
  setUIFont(60);
  epaper.drawString(String(secondsLeft), CENTER_X, 260);
  setUIFont(18);
  epaper.drawString("Press any button to cancel", CENTER_X, 390);
  epaper.update();
#endif
}

void updatePortalCountdown() {
  if (!portalCountdownActive) return;
  unsigned long elapsed = millis() - portalCountdownStart;
  int secondsLeft = 5 - (elapsed / 1000);
  if (secondsLeft != lastCountdownSecond && secondsLeft > 0) {
    lastCountdownSecond = secondsLeft;
#ifdef EPAPER_ENABLE
    epaper.fillRect(320, 205, 160, 110, TFT_WHITE);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.setTextDatum(MC_DATUM);
    setUIFont(60);
    epaper.drawString(String(secondsLeft), CENTER_X, 260);
    epaper.updataPartial(320, 205, 160, 110);
#endif
  }
  if (elapsed >= 5000) {
    portalCountdownActive = false;
    startConfigPortal();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("[Boot] ESP32 reset reason: %d\n", (int)esp_reset_reason());
  slideshowStorageReady = mountPhotoStorage(false);
  if (!slideshowStorageReady) Serial.println("[Photos] No compatible photo storage partition is mounted");
  loadConfiguration();
  
  // Initialize Hardware Pins
  pinMode(BUTTON_PREV, INPUT_PULLUP);
  pinMode(BUTTON_NEXT, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  gpio_wakeup_enable((gpio_num_t)BUTTON_PREV, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_NEXT, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_SELECT, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  
  pinMode(ADC_EN_PIN, OUTPUT);
  digitalWrite(ADC_EN_PIN, LOW); 
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

#ifdef EPAPER_ENABLE
  epaper.begin();
  drawBootScreen();

#if UI_DEMO_MODE
  // Du lieu mau de xem nhanh toan bo giao dien ma khong can Internet.
  for (int i = 0; i < NUM_SCREENS; i++) screenEnabled[i] = true;
  screenEnabled[SCREEN_REMOVED_WEATHER] = false;
  slideshowEnabled = false;
  SCREEN_SWITCH_INTERVAL = 10000;

  currentTemp = "31";
  feelsLikeTemp = "34";
  currentCondition = "MÂY RẢI RÁC";
  weatherIconCode = "02d";
  currentHumidity = "72%";
  currentWind = "2.8 m/s";
  currentPressure = "1008 hPa";
  sunriseTime = "05:42 AM";
  sunsetTime = "06:17 PM";
  CMC_API_KEY = "DEMO";
  STOCK_API_URL = "DEMO";
  quoteText = "Mỗi ngày tiến một bước nhỏ sẽ tạo nên kết quả lớn.";
  quoteAuthor = "- Bản thử giao diện";
  cryptoData[0] = "BTC: $118420 (+2.4%)";
  cryptoData[1] = "ETH: $3760 (+1.8%)";
  cryptoData[2] = "SOL: $194 (-0.7%)";
  cryptoData[3] = "DOGE: $0.24 (+3.1%)";

  stockCount = 5;
  activeStocks[0] = {"AAPL", "Apple Inc.", "214.40", "+1.2%"};
  activeStocks[1] = {"NVDA", "NVIDIA", "172.15", "+2.8%"};
  activeStocks[2] = {"TSLA", "Tesla", "328.90", "-0.6%"};
  activeStocks[3] = {"MSFT", "Microsoft", "506.30", "+0.4%"};
  activeStocks[4] = {"VNM", "Vinamilk", "61.50", "+0.9%"};

  taskCount = 5;
  activeTasks[0] = "Kiểm tra bố cục tổng thể";
  activeTasks[1] = "Chọn phông chữ tiếng Việt";
  activeTasks[2] = "Chỉnh màn hình thời tiết";
  activeTasks[3] = "Thử ba phím điều khiển";
  activeTasks[4] = "Lên kế hoạch phiên bản mới";
  demoTimeInfo.tm_year = 126;  // 2026
  demoTimeInfo.tm_mon = 6;     // July
  demoTimeInfo.tm_mday = 21;
  demoTimeInfo.tm_wday = 2;
  demoTimeInfo.tm_hour = 9;
  demoTimeInfo.tm_min = 30;

  currentScreen = SCREEN_DASHBOARD;
  lastScreenSwitch = millis();
  switchScreen(currentScreen);
  return;
#else

  if (WIFI_SSID.length() == 0) startConfigPortal();

  epaper.fillScreen(TFT_WHITE);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextDatum(MC_DATUM);
  setUIFont(32);
  epaper.drawString("Connecting to WiFi...", CENTER_X, CENTER_Y);
  epaper.update();

  if (!connectWiFiForData()) startConfigPortal();

  // Never block boot forever when the router/firewall does not allow NTP.
  syncClockFromNtp();

  epaper.fillScreen(TFT_WHITE);
  epaper.drawString("Fetching Live Data...", CENTER_X, CENTER_Y);
  epaper.update();

  fetchAllData();

  lastScreenSwitch = millis();
  lastSlideshowSwitch = millis();
  currentScreen = screenEnabled[SCREEN_DASHBOARD] ? SCREEN_DASHBOARD : findEnabledScreen(SCREEN_DASHBOARD, 1);
  switchScreen(currentScreen);
#endif
#endif
}

void loop() {
#if UI_DEMO_MODE
  handleButtons();
  updatePortalCountdown();
  if (portalCountdownActive) {
    delay(10);
    return;
  }

  if (rapidPressCount > 0 && millis() - rapidPressWindowStart > 2500) {
    rapidPressCount = 0;
  }

  if (needsScreenUpdate && rapidPressCount == 0) {
    switchScreen(currentScreen);
    needsScreenUpdate = false;
  }

  unsigned long demoNow = millis();
  if (demoNow - lastScreenSwitch >= SCREEN_SWITCH_INTERVAL) {
    lastScreenSwitch = demoNow;
    currentScreen = findEnabledScreen(currentScreen, 1);
    switchScreen(currentScreen);
  }

  static unsigned long lastDemoMinute = 0;
  if (demoNow - lastDemoMinute >= 60000) {
    lastDemoMinute = demoNow;
    demoTimeInfo.tm_min++;
    if (demoTimeInfo.tm_min >= 60) {
      demoTimeInfo.tm_min = 0;
      demoTimeInfo.tm_hour = (demoTimeInfo.tm_hour + 1) % 24;
    }
  }
  delay(10);
  return;
#else
  handleButtons();
  updatePortalCountdown();
  if (portalCountdownActive) {
    delay(10);
    return;
  }

#ifdef EPAPER_ENABLE
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    delay(500);
    return;
  }

  if (rapidPressCount > 0 && millis() - rapidPressWindowStart > 2500) {
    rapidPressCount = 0;
  }

  // --- ASYNC EXECUTION BLOCKS ---
  if (needsDataRefresh) {
      epaper.fillScreen(TFT_WHITE);
      epaper.setTextColor(TFT_BLACK, TFT_WHITE);
      epaper.setTextDatum(MC_DATUM);
      setUIFont(24);
      epaper.drawString("Updating...", CENTER_X, CENTER_Y);
      epaper.update();

      // Select/Update always performs one complete on-demand data refresh.
      fetchAllData();
      
      lastDataFetch = millis(); 
      needsDataRefresh = false;
      needsScreenUpdate = true;
  }

  if (needsScreenUpdate && rapidPressCount == 0) {
      if (currentMode == MODE_PAGES) {
          switchScreen(currentScreen, true);
      } else {
          // A page/photo change gets a complete black/white cleaning cycle.
          epaper.fillScreen(TFT_BLACK);
          epaper.update();
          epaper.fillScreen(TFT_WHITE);
          epaper.update();
          drawSlideshowScreen(timeinfo);
          epaper.update();
          clockRefreshCount = 0;
      }
      needsScreenUpdate = false;
  }

  // --- STANDARD BACKGROUND TASKS ---
  unsigned long currentMillis = millis();

  // Pages and photos share one continuous rotation. Pages use the page interval;
  // uploaded photos use the slideshow-image interval.
  if (currentMode == MODE_SLIDESHOW) {
    if (!slideshowEnabled || currentMillis - lastSlideshowSwitch >= SLIDESHOW_IMAGE_INTERVAL) {
      advanceContent(1);
    }
  } else if (currentMillis - lastScreenSwitch >= SCREEN_SWITCH_INTERVAL) {
    advanceContent(1);
  }

  if (currentMillis - lastDataFetch >= DATA_FETCH_INTERVAL) {
    fetchAllData();
  }

  if (timeinfo.tm_min != lastMinute) {
    lastMinute = timeinfo.tm_min;

    if (!needsScreenUpdate) {
      bool clockWasUpdated = false;
      if (currentMode == MODE_SLIDESHOW) {
        int cx = 630, cy = 400, cw = 160, ch = 70;
        epaper.fillRect(cx, cy, cw, ch, TFT_BLACK);
        epaper.updataPartial(cx, cy, cw, ch);
        delay(20);
        epaper.fillRect(cx, cy, cw, ch, TFT_WHITE);
        epaper.updataPartial(cx, cy, cw, ch);
        delay(20);
        drawSlideshowClock(timeinfo);
        epaper.updataPartial(cx, cy, cw, ch);
        clockWasUpdated = true;
      } 
      else if (currentScreen == SCREEN_TIME_QUOTE) {
        int clockWipeX = 100, clockWipeY = 30, wipeW = 600, wipeH = 140; 
        epaper.fillRect(clockWipeX, clockWipeY, wipeW, wipeH, TFT_BLACK);
        epaper.updataPartial(clockWipeX, clockWipeY, wipeW, wipeH);
        delay(20);
        epaper.fillRect(clockWipeX, clockWipeY, wipeW, wipeH, TFT_WHITE);
        epaper.updataPartial(clockWipeX, clockWipeY, wipeW, wipeH);
        delay(20);
        drawTimeQuoteClock(timeinfo);
        epaper.updataPartial(clockWipeX, clockWipeY, wipeW, wipeH);
        clockWasUpdated = true;
      } else if (currentScreen == SCREEN_DASHBOARD) {
        updateDashboardClockPartial(timeinfo);
        clockWasUpdated = !needsScreenUpdate;
      }

      // Three local clock-cleaning cycles can weaken unchanged pixels on the
      // UC8179. Restore the complete current image once, without another
      // black/white cleaning cycle; page/data changes perform that separately.
      if (clockWasUpdated) clockRefreshCount++;
      if (clockWasUpdated && clockRefreshCount >= 3 && !needsScreenUpdate) {
        if (currentMode == MODE_PAGES) {
          switchScreen(currentScreen, false);
        } else {
          epaper.fillScreen(TFT_WHITE);
          drawSlideshowScreen(timeinfo);
          epaper.update();
        }
        clockRefreshCount = 0;
      }
    }
  }

#endif

  lowPowerIdle();
#endif
}

// =========================================================================
// ========================== HARDWARE CONTROLS ============================
// =========================================================================

void handleButtons() {
  bool statePrev = digitalRead(BUTTON_PREV);
  bool stateNext = digitalRead(BUTTON_NEXT);
  bool stateSel  = digitalRead(BUTTON_SELECT);
  bool anyPressEdge = (lastStatePrev == HIGH && statePrev == LOW) ||
                      (lastStateNext == HIGH && stateNext == LOW) ||
                      (lastStateSel == HIGH && stateSel == LOW);

  if (portalCountdownActive && anyPressEdge && millis() - lastButtonPress > 200) {
    portalCountdownActive = false;
    rapidPressCount = 0;
    lastButtonPress = millis();
    needsScreenUpdate = true;
    lastStatePrev = statePrev;
    lastStateNext = stateNext;
    lastStateSel = stateSel;
    return;
  }

  // Debounce block
  if (millis() - lastButtonPress > 200) { 
    
    // Check if ANY button just got pressed (Edge Detection)
    if ((lastStatePrev == HIGH && statePrev == LOW) || 
        (lastStateNext == HIGH && stateNext == LOW) || 
        (lastStateSel == HIGH && stateSel == LOW)) {
        
        // Wait 80ms to give the user time to press the second button for a chord
        delay(80);
        
        // Re-read states to check if multiple buttons are currently held down
        statePrev = digitalRead(BUTTON_PREV);
        stateNext = digitalRead(BUTTON_NEXT);
        stateSel  = digitalRead(BUTTON_SELECT);

        // Five quick presses on any button arm the Wi-Fi setup countdown.
        if (rapidPressCount == 0 || millis() - rapidPressWindowStart > 2500) {
          rapidPressCount = 1;
          rapidPressWindowStart = millis();
        } else {
          rapidPressCount++;
        }
        if (rapidPressCount >= 5) {
          rapidPressCount = 0;
          portalCountdownActive = true;
          lastCountdownSecond = 5;
          lastButtonPress = millis();
          drawPortalCountdown(5);
          portalCountdownStart = millis();
          lastStatePrev = statePrev;
          lastStateNext = stateNext;
          lastStateSel = stateSel;
          return;
        }

        // Pages and uploaded photos use the same Previous/Next navigation cycle.
        if (currentMode == MODE_PAGES) {
          if (statePrev == LOW) {
            advanceContent(-1);
            lastButtonPress = millis();
          } else if (stateNext == LOW) {
            advanceContent(1);
            lastButtonPress = millis();
          } else if (stateSel == LOW) {
            needsDataRefresh = true; 
            lastButtonPress = millis();
          }
        } 
        else if (currentMode == MODE_SLIDESHOW) {
          if (statePrev == LOW) {
            advanceContent(-1);
            lastButtonPress = millis();
          } else if (stateNext == LOW) {
            advanceContent(1);
            lastButtonPress = millis();
          } else if (stateSel == LOW) {
            needsDataRefresh = true;
            lastButtonPress = millis();
          }
        }
    }
  }

  // Preserve edge state
  lastStatePrev = statePrev;
  lastStateNext = stateNext;
  lastStateSel = stateSel;
}

// =========================================================================
// ========================== BATTERY LOGIC ================================
// =========================================================================

float readBatteryVoltage() {
  digitalWrite(ADC_EN_PIN, HIGH);
  delay(10);
  
  long sum = 0;
  for(int i = 0; i < 30; i++) {
    sum += analogRead(BATTERY_PIN);
    delayMicroseconds(100);
  }
  digitalWrite(ADC_EN_PIN, LOW);
  
  float adc_avg = sum / 30.0;
  float voltage = (adc_avg / 4095.0) * 3.6 * 2.0 * CALIBRATION_FACTOR;
  return voltage;
}

int calculateBatteryPercentage(float voltage) {
  // Map standard 1S LiPo curve (4.2V Max, 3.2V Min)
  int percentage = (voltage - 3.2) * 100.0 / (4.2 - 3.2);
  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;
  return percentage;
}

void drawBatteryStatus(bool leftAligned) {
#ifdef EPAPER_ENABLE
  int percentage = calculateBatteryPercentage(readBatteryVoltage());
  const int x = leftAligned ? 10 : 698;
  const int y = 444;
  const int w = 92;
  const int h = 28;

  epaper.fillRoundRect(x, y, w, h, 7, TFT_WHITE);
  epaper.drawRoundRect(x, y, w, h, 7, TFT_BLACK);
  const unsigned char* icon = percentage > 50 ? icon_battery : icon_battery_half;
  epaper.drawBitmap(x + 7, y + 6, icon, 32, 16, TFT_WHITE, TFT_BLACK);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextDatum(MR_DATUM);
  setUIFont(18);
  epaper.drawString(String(percentage) + "%", x + w - 7, y + h / 2);
#endif
}

// East Asian lunisolar conversion based on the astronomical new-moon method.
// The selected weather country chooses the regional reference time zone.
static const double LUNAR_PI = 3.14159265358979323846;

long lunarJulianDay(int day, int month, int year) {
  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  long jd = day + (153L * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 - 32045;
  if (jd < 2299161) jd = day + (153L * m + 2) / 5 + 365L * y + y / 4 - 32083;
  return jd;
}

double lunarNewMoon(int k) {
  double time = k / 1236.85;
  double time2 = time * time;
  double time3 = time2 * time;
  double radians = LUNAR_PI / 180.0;
  double julian = 2415020.75933 + 29.53058868 * k + 0.0001178 * time2 - 0.000000155 * time3;
  julian += 0.00033 * sin((166.56 + 132.87 * time - 0.009173 * time2) * radians);
  double sunMean = 359.2242 + 29.10535608 * k - 0.0000333 * time2 - 0.00000347 * time3;
  double moonMean = 306.0253 + 385.81691806 * k + 0.0107306 * time2 + 0.00001236 * time3;
  double moonLatitude = 21.2964 + 390.67050646 * k - 0.0016528 * time2 - 0.00000239 * time3;
  double correction = (0.1734 - 0.000393 * time) * sin(sunMean * radians)
                    + 0.0021 * sin(2 * sunMean * radians)
                    - 0.4068 * sin(moonMean * radians)
                    + 0.0161 * sin(2 * moonMean * radians)
                    - 0.0004 * sin(3 * moonMean * radians)
                    + 0.0104 * sin(2 * moonLatitude * radians)
                    - 0.0051 * sin((sunMean + moonMean) * radians)
                    - 0.0074 * sin((sunMean - moonMean) * radians)
                    + 0.0004 * sin((2 * moonLatitude + sunMean) * radians)
                    - 0.0004 * sin((2 * moonLatitude - sunMean) * radians)
                    - 0.0006 * sin((2 * moonLatitude + moonMean) * radians)
                    + 0.0010 * sin((2 * moonLatitude - moonMean) * radians)
                    + 0.0005 * sin((2 * moonMean + sunMean) * radians);
  double deltaTime;
  if (time < -11) {
    deltaTime = 0.001 + 0.000839 * time + 0.0002261 * time2
              - 0.00000845 * time3 - 0.000000081 * time * time3;
  } else {
    deltaTime = -0.000278 + 0.000265 * time + 0.000262 * time2;
  }
  return julian + correction - deltaTime;
}

int lunarNewMoonDay(int k, double timeZone) {
  return (int)floor(lunarNewMoon(k) + 0.5 + timeZone / 24.0);
}

double lunarSunLongitude(double julianDay) {
  double time = (julianDay - 2451545.0) / 36525.0;
  double time2 = time * time;
  double radians = LUNAR_PI / 180.0;
  double meanAnomaly = 357.52910 + 35999.05030 * time - 0.0001559 * time2 - 0.00000048 * time * time2;
  double meanLongitude = 280.46645 + 36000.76983 * time + 0.0003032 * time2;
  double correction = (1.914600 - 0.004817 * time - 0.000014 * time2) * sin(radians * meanAnomaly)
                    + (0.019993 - 0.000101 * time) * sin(2 * radians * meanAnomaly)
                    + 0.000290 * sin(3 * radians * meanAnomaly);
  double longitude = (meanLongitude + correction) * radians;
  longitude -= 2 * LUNAR_PI * floor(longitude / (2 * LUNAR_PI));
  return longitude;
}

int lunarSunLongitudeSector(int dayNumber, double timeZone) {
  return (int)floor(lunarSunLongitude(dayNumber - 0.5 - timeZone / 24.0) / LUNAR_PI * 6);
}

int lunarMonthEleven(int year, double timeZone) {
  long offset = lunarJulianDay(31, 12, year) - 2415021;
  int k = (int)floor(offset / 29.530588853);
  int newMoon = lunarNewMoonDay(k, timeZone);
  if (lunarSunLongitudeSector(newMoon, timeZone) >= 9) {
    newMoon = lunarNewMoonDay(k - 1, timeZone);
  }
  return newMoon;
}

int lunarLeapMonthOffset(int monthEleven, double timeZone) {
  int k = (int)floor(0.5 + (monthEleven - 2415021.076998695) / 29.530588853);
  int lastSector = -1;
  int index = 1;
  int sector = lunarSunLongitudeSector(lunarNewMoonDay(k + index, timeZone), timeZone);
  do {
    lastSector = sector;
    index++;
    sector = lunarSunLongitudeSector(lunarNewMoonDay(k + index, timeZone), timeZone);
  } while (sector != lastSector && index < 14);
  return index - 1;
}

LunarDate solarToLunar(int day, int month, int year, double timeZone) {
  int dayNumber = lunarJulianDay(day, month, year);
  int k = (int)floor((dayNumber - 2415021.076998695) / 29.530588853);
  int monthStart = lunarNewMoonDay(k + 1, timeZone);
  if (monthStart > dayNumber) monthStart = lunarNewMoonDay(k, timeZone);

  int monthElevenA = lunarMonthEleven(year, timeZone);
  int monthElevenB = monthElevenA;
  int lunarYear;
  if (monthElevenA >= monthStart) {
    lunarYear = year;
    monthElevenA = lunarMonthEleven(year - 1, timeZone);
  } else {
    lunarYear = year + 1;
    monthElevenB = lunarMonthEleven(year + 1, timeZone);
  }

  int lunarDay = dayNumber - monthStart + 1;
  int monthDifference = (monthStart - monthElevenA) / 29;
  int lunarMonth = monthDifference + 11;
  bool lunarLeap = false;
  if (monthElevenB - monthElevenA > 365) {
    int leapDifference = lunarLeapMonthOffset(monthElevenA, timeZone);
    if (monthDifference >= leapDifference) {
      lunarMonth = monthDifference + 10;
      lunarLeap = monthDifference == leapDifference;
    }
  }
  if (lunarMonth > 12) lunarMonth -= 12;
  if (lunarMonth >= 11 && monthDifference < 4) lunarYear--;
  return {lunarDay, lunarMonth, lunarYear, lunarLeap};
}

double lunarTimeZoneForCountry() {
  if (COUNTRY_CODE == "VN") return 7.0;
  if (COUNTRY_CODE == "CN" || COUNTRY_CODE == "TW" || COUNTRY_CODE == "HK" ||
      COUNTRY_CODE == "SG" || COUNTRY_CODE == "MY") return 8.0;
  if (COUNTRY_CODE == "KR" || COUNTRY_CODE == "JP") return 9.0;
  return 0.0;
}

void buildLunarFooter(const LunarDate& lunar, String& firstLine, String& secondLine) {
  int stemIndex = (lunar.year + 6) % 10;
  int branchIndex = (lunar.year + 8) % 12;

  if (COUNTRY_CODE == "VN") {
    const char* stems[] = {"Giáp", "Ất", "Bính", "Đinh", "Mậu", "Kỷ", "Canh", "Tân", "Nhâm", "Quý"};
    const char* branches[] = {"Tý", "Sửu", "Dần", "Mão", "Thìn", "Tỵ", "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"};
    firstLine = lunar.day <= 10 ? "Âm lịch: ngày mùng " : "Âm lịch: ngày ";
    firstLine += String(lunar.day);
    secondLine = "Tháng " + String(lunar.month);
    if (lunar.leap) secondLine += " nhuận";
    secondLine += " Năm " + String(lunar.year) + " (" + stems[stemIndex] + " " + branches[branchIndex] + ")";
    return;
  }

  if (COUNTRY_CODE == "CN" || COUNTRY_CODE == "TW" || COUNTRY_CODE == "HK") {
    const char* stems[] = {"Jia", "Yi", "Bing", "Ding", "Wu", "Ji", "Geng", "Xin", "Ren", "Gui"};
    const char* branches[] = {"Zi", "Chou", "Yin", "Mao", "Chen", "Si", "Wu", "Wei", "Shen", "You", "Xu", "Hai"};
    firstLine = "Nong li: ri " + String(lunar.day);
    secondLine = String(lunar.year) + " nian ";
    if (lunar.leap) secondLine += "run ";
    secondLine += String(lunar.month) + " yue (" + stems[stemIndex] + " " + branches[branchIndex] + ")";
    return;
  }

  if (COUNTRY_CODE == "KR") {
    const char* stems[] = {"Gap", "Eul", "Byeong", "Jeong", "Mu", "Gi", "Gyeong", "Sin", "Im", "Gye"};
    const char* branches[] = {"Ja", "Chuk", "In", "Myo", "Jin", "Sa", "O", "Mi", "Sin", "Yu", "Sul", "Hae"};
    firstLine = "Eumnyeok: " + String(lunar.day) + "-il";
    secondLine = String(lunar.year) + "-nyeon ";
    if (lunar.leap) secondLine += "yun ";
    secondLine += String(lunar.month) + "-wol (" + stems[stemIndex] + "-" + branches[branchIndex] + ")";
    return;
  }

  if (COUNTRY_CODE == "JP") {
    const char* stems[] = {"Kinoe", "Kinoto", "Hinoe", "Hinoto", "Tsuchinoe", "Tsuchinoto", "Kanoe", "Kanoto", "Mizunoe", "Mizunoto"};
    const char* branches[] = {"Ne", "Ushi", "Tora", "U", "Tatsu", "Mi", "Uma", "Hitsuji", "Saru", "Tori", "Inu", "I"};
    firstLine = "Kyureki: " + String(lunar.day) + "-nichi";
    secondLine = String(lunar.year) + "-nen ";
    if (lunar.leap) secondLine += "uru ";
    secondLine += String(lunar.month) + "-gatsu (" + stems[stemIndex] + "-" + branches[branchIndex] + ")";
    return;
  }

  const char* elements[] = {"Wood", "Wood", "Fire", "Fire", "Earth", "Earth", "Metal", "Metal", "Water", "Water"};
  const char* animals[] = {"Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake", "Horse", "Goat", "Monkey", "Rooster", "Dog", "Pig"};
  if (COUNTRY_CODE == "MY") {
    firstLine = "Kalendar lunar: hari " + String(lunar.day);
    secondLine = lunar.leap ? "Bulan lompat " : "Bulan ";
    secondLine += String(lunar.month) + " Tahun " + String(lunar.year) + " (" + elements[stemIndex] + " " + animals[branchIndex] + ")";
  } else {
    firstLine = "Lunar: day " + String(lunar.day);
    secondLine = lunar.leap ? "Leap month " : "Month ";
    secondLine += String(lunar.month) + " Year " + String(lunar.year) + " (" + elements[stemIndex] + " " + animals[branchIndex] + ")";
  }
}

// =========================================================================
// ======================== API DATA INGESTION =============================
// =========================================================================

String urlEncode(const String& value) {
  const char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length() * 3);
  for (size_t i = 0; i < value.length(); i++) {
    uint8_t c = (uint8_t)value[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += (char)c;
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

bool resolveWeatherLocation(WiFiClientSecure& client, HTTPClient& http) {
  if (weatherLocationResolved) return true;
  if (OWM_API_KEY.length() == 0 || CITY.length() == 0 || COUNTRY_CODE.length() != 2) {
    weatherError = "CHECK WEATHER SETUP";
    return false;
  }

  String query = urlEncode(CITY + "," + COUNTRY_CODE);
  String url = "https://api.openweathermap.org/geo/1.0/direct?q=" + query +
               "&limit=1&appid=" + OWM_API_KEY;
  if (!http.begin(client, url)) {
    weatherError = "GEOCODING URL ERROR";
    return false;
  }

  int status = http.GET();
  String body = http.getString();
  http.end();
  Serial.printf("[Weather] Geocoding HTTP %d\n", status);

  if (status != 200) {
    weatherError = "GEOCODING API " + String(status);
    Serial.println(body);
    return false;
  }

  JsonDocument doc;
  DeserializationError jsonError = deserializeJson(doc, body);
  if (jsonError || !doc.is<JsonArray>() || doc.size() == 0) {
    weatherError = "CITY NOT FOUND";
    return false;
  }

  WEATHER_LAT = doc[0]["lat"] | 0.0f;
  WEATHER_LON = doc[0]["lon"] | 0.0f;
  const char* localizedName = doc[0]["local_names"][effectiveLanguageCode()];
  const char* canonicalName = doc[0]["name"];
  if (localizedName && localizedName[0]) CITY = localizedName;
  else if (canonicalName && canonicalName[0]) CITY = canonicalName;
  weatherLocationResolved = true;
  weatherError = "";

  configPreferences.begin("vector_cfg", false);
  configPreferences.putString("city", CITY);
  configPreferences.putString("locationKey", CITY + "|" + COUNTRY_CODE);
  configPreferences.putFloat("weatherLat", WEATHER_LAT);
  configPreferences.putFloat("weatherLon", WEATHER_LON);
  configPreferences.end();

  Serial.printf("[Weather] %s, %s -> %.5f, %.5f\n", CITY.c_str(), COUNTRY_CODE.c_str(), WEATHER_LAT, WEATHER_LON);
  return true;
}

void fetchAllData() {
  lastDataFetch = millis();
  if (!connectWiFiForData()) {
    Serial.println("[Data] Keeping cached values because Wi-Fi is unavailable");
    return;
  }

  // Re-sync the RTC while the radio is awake. Timekeeping continues locally
  // after Wi-Fi is turned off.
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC,
             NTP_SERVER, "time.google.com", "time.cloudflare.com");
  
  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  
  HTTPClient http;

  // 1. Resolve the selected city through OpenWeather Geocoding, then use lat/lon.
  if (resolveWeatherLocation(secureClient, http)) {
    String weatherUrl = "https://api.openweathermap.org/data/2.5/weather?lat=" + String(WEATHER_LAT, 5) +
                        "&lon=" + String(WEATHER_LON, 5) + "&appid=" + OWM_API_KEY +
                        "&units=metric&lang=" + String(effectiveLanguageCode());
    http.begin(secureClient, weatherUrl);
    const char* timeHeaderKeys[] = {"Date"};
    http.collectHeaders(timeHeaderKeys, 1);
    int weatherStatus = http.GET();
    String weatherBody = http.getString();
    String weatherServerDate = http.header("Date");
    Serial.printf("[Weather] Current weather HTTP %d\n", weatherStatus);
    if (weatherStatus > 0) syncClockFromHttpDate(weatherServerDate);
    if (weatherStatus == 200) {
    JsonDocument doc;
    deserializeJson(doc, weatherBody);
    currentTemp = String((int)round((float)doc["main"]["temp"]));
    feelsLikeTemp = String((int)round((float)doc["main"]["feels_like"])); 
    currentCondition = String((const char*)doc["weather"][0]["description"]);
    currentCondition.toUpperCase();
    weatherIconCode = String((const char*)doc["weather"][0]["icon"]);
    currentHumidity = String((int)doc["main"]["humidity"]) + "%";
    currentPressure = String((int)doc["main"]["pressure"]) + " hPa";
    currentWind = String((float)doc["wind"]["speed"], 1) + " m/s";

    long sr = doc["sys"]["sunrise"]; long ss = doc["sys"]["sunset"]; struct tm *ti; time_t epoch; char buf[16];
    epoch = sr; ti = localtime(&epoch); strftime(buf, sizeof(buf), "%I:%M %p", ti); sunriseTime = String(buf);
    epoch = ss; ti = localtime(&epoch); strftime(buf, sizeof(buf), "%I:%M %p", ti); sunsetTime = String(buf);
      weatherError = "";
    } else {
      weatherError = "WEATHER API " + String(weatherStatus);
      currentCondition = weatherError;
      Serial.println(weatherBody);
    }
    http.end();
  } else {
    currentCondition = weatherError;
  }

  // 2. Crypto
  if (CMC_API_KEY.length()) {
    http.begin(secureClient, "https://pro-api.coinmarketcap.com/v1/cryptocurrency/quotes/latest?symbol=" + String(CMC_SYMBOLS));
    http.addHeader("X-CMC_PRO_API_KEY", CMC_API_KEY);
    http.addHeader("Accept", "application/json");
    if (http.GET() == 200) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      const char* coins[] = { "BTC", "ETH", "SOL", "DOGE" };
      for (int i = 0; i < 4; i++) {
        if (doc["data"][coins[i]]) {
          float price = doc["data"][coins[i]]["quote"]["USD"]["price"];
          float change = doc["data"][coins[i]]["quote"]["USD"]["percent_change_24h"];
          String priceStr = (price < 1.0) ? String(price, 3) : String((int)round(price));
          cryptoData[i] = String(coins[i]) + ": $" + priceStr + " (" + String(change, 1) + "%)";
        }
      }
    }
    http.end();
  }

  // 3. Quotes
  http.begin(secureClient, "https://zenquotes.io/api/today?key=" + String(ZEN_API_KEY));
  if (http.GET() == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    quoteText = String((const char*)doc[0]["q"]);
    quoteAuthor = "- " + String((const char*)doc[0]["a"]);
  }
  http.end(); // Clean up socket connection for the next domain

  // 4. Todoist
  if (TODOIST_API_KEY.length()) {
    http.begin(secureClient, "https://api.todoist.com/api/v1/tasks?filter=due%20today");
    http.addHeader("Authorization", "Bearer " + String(TODOIST_API_KEY));
    if (http.GET() == 200) {
      DynamicJsonDocument doc(16384);
      if (!deserializeJson(doc, http.getString())) {
        taskCount = 0;
        JsonArray array = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["results"].as<JsonArray>();
        for (JsonVariant task : array) {
          if (taskCount >= 5) break;
          activeTasks[taskCount++] = String((const char*)task["content"]);
        }
      }
    }
    http.end();
  }

  // 5. Stocks (Requires strict redirect config changes)
  if (STOCK_API_URL.length()) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
    http.begin(secureClient, STOCK_API_URL);
    if (http.GET() == 200) {
      DynamicJsonDocument doc(4096);
      if (!deserializeJson(doc, http.getString())) {
        stockCount = 0;
        for (JsonVariant stock : doc["stocks"].as<JsonArray>()) {
          if (stockCount >= 5) break;
          activeStocks[stockCount].symbol = String((const char*)stock["symbol"]);
          activeStocks[stockCount].name = String((const char*)stock["name"]);
          activeStocks[stockCount].price = String((const char*)stock["price"]);
          activeStocks[stockCount].change = String((const char*)stock["change"]);
          stockCount++;
        }
      }
    }
    http.end();
  }
  time_t cachedEpoch = time(nullptr);
  if (cachedEpoch >= 1700000000) {
    configPreferences.begin("vector_cfg", false);
    configPreferences.putULong64("lastEpoch", (uint64_t)cachedEpoch);
    configPreferences.end();
  }
  turnOffWiFi();
}

// =========================================================================
// ======================== UI RENDERING LOGIC =============================
// =========================================================================

void switchScreen(int screenIndex, bool cleanCycle) {
#ifdef EPAPER_ENABLE
  if (cleanCycle) {
    epaper.fillScreen(TFT_BLACK);
    epaper.update();
    epaper.fillScreen(TFT_WHITE);
    epaper.update();
  }

  epaper.fillScreen(TFT_WHITE);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextDatum(MC_DATUM);

  struct tm timeinfo;
#if UI_DEMO_MODE
  timeinfo = demoTimeInfo;
#else
  getLocalTime(&timeinfo);
#endif

  if (screenIndex == SCREEN_TIME_QUOTE) {
    drawTimeQuoteScreen(timeinfo);
  } else if (screenIndex == SCREEN_CRYPTO) {
    drawCryptoScreen();
  } else if (screenIndex == SCREEN_STOCK) {
    drawStockScreen();
  } else if (screenIndex == SCREEN_TODO) {
    drawTodoScreen();
  } else if (screenIndex == SCREEN_DASHBOARD) {
    drawDashboardScreen(timeinfo);
  }

  drawBatteryStatus(false);

  epaper.update();  
  if (cleanCycle) clockRefreshCount = 0;
#endif
}

// --- Photo Slideshow Mode Rendering ---
void drawSlideshowScreen(struct tm& timeinfo) {
#ifdef EPAPER_ENABLE
  if (!slideshowImageValid(currentSlideshowImage)) {
    epaper.setTextDatum(MC_DATUM);
    setUIFont(24);
    epaper.drawString("No slideshow photos available", CENTER_X, CENTER_Y);
    return;
  }
  if (currentSlideshowImage < MAX_UPLOADED_SLIDES) {
    if (!loadUploadedSlide(currentSlideshowImage)) {
      epaper.setTextDatum(MC_DATUM);
      setUIFont(24);
      epaper.drawString("Could not read uploaded photo", CENTER_X, CENTER_Y);
      return;
    }
    epaper.drawBitmap(0, 0, slideshowBitmapBuffer, 800, 480, TFT_WHITE, TFT_BLACK);
  } else {
    int embeddedIndex = currentSlideshowImage - MAX_UPLOADED_SLIDES;
    epaper.drawBitmap(0, 0, slideshow_images[embeddedIndex], 800, 480, TFT_WHITE, TFT_BLACK);
  }
  drawSlideshowClock(timeinfo);
  drawBatteryStatus(true);
#endif
}

void drawSlideshowClock(struct tm& timeinfo) {
#ifdef EPAPER_ENABLE
  int cx = 630, cy = 400, cw = 160, ch = 70;
  
  epaper.fillRect(cx, cy, cw, ch, TFT_WHITE);
  epaper.drawRoundRect(cx, cy, cw, ch, 8, TFT_BLACK);
  
  char timeStr[12];
  strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo);
  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
  
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextDatum(MC_DATUM);
  
  setUIFont(24);
  epaper.drawString(timeStr, cx + cw / 2, cy + 25);
  
  epaper.drawLine(cx + 15, cy + 42, cx + cw - 15, cy + 42, TFT_BLACK);
  
  setUIFont(18);
  epaper.drawString(dateStr, cx + cw / 2, cy + 55);
#endif
}

// --- Time & Quote Screen Layout ---
void drawTimeQuoteScreen(struct tm& timeinfo) {
#ifdef EPAPER_ENABLE
  drawTimeQuoteClock(timeinfo);

  int tempCharWidth = 24; 
  int tempW = currentTemp.length() * tempCharWidth;
  int iconW = 64;
  int gap1 = 15; 
  int gap2 = 12; 
  int charCW = 24; 
  
  int totalGrpW = iconW + gap1 + tempW + gap2 + charCW;
  int startGrpX = CENTER_X - (totalGrpW / 2);

  int iconX = startGrpX + (iconW / 2); 
  int iconY = 210; 
  drawWeatherIcon(iconX, iconY, weatherIconCode);

  epaper.setTextDatum(BL_DATUM); 
  setUIFont(32);
  int textX = startGrpX + iconW + gap1;
  int alignBottomY = iconY + 28; 
  epaper.drawString(currentTemp.c_str(), textX, alignBottomY);

  int degreeX = textX + tempW + 6;
  epaper.drawCircle(degreeX, alignBottomY - 32, 3, TFT_BLACK);
  epaper.drawCircle(degreeX, alignBottomY - 32, 2, TFT_BLACK);
  
  epaper.drawString("C", degreeX + 8, alignBottomY);

  epaper.drawLine(200, 280, 600, 280, TFT_BLACK);
  
  epaper.setTextDatum(MC_DATUM);
  setUIFont(24);
  drawWrappedStringCenter(quoteText, 330, 36); // Wrap length tightened

  setUIFont(18);
  epaper.drawString(quoteAuthor, CENTER_X, 430);

#endif
}

void drawTimeQuoteClock(struct tm& timeinfo) {
#ifdef EPAPER_ENABLE
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextDatum(MC_DATUM);

  char timeStr[12];
  strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo); 
  String dateText = localizedDate(timeinfo);

  setUIFont(24);
  epaper.drawString(dateText, CENTER_X, 60);

  setUIFont(60);
  epaper.drawString(timeStr, CENTER_X, 130);
#endif
}

// --- Crypto Screen Layout ---
void drawCryptoScreen() {
#ifdef EPAPER_ENABLE
  epaper.setTextDatum(MC_DATUM);
  setUIFont(32);
  epaper.drawString(uiLabel("Crypto Markets", "Thị trường Crypto", "Mercado Crypto", "Marché Crypto", "Krypto-Markt"), CENTER_X, 40);

  if (CMC_API_KEY.length() == 0) {
    setUIFont(24);
    epaper.drawString("API Key Required", CENTER_X, CENTER_Y);
    return;
  }

  int startY = 120;
  for (int i = 0; i < 4; i++) {
    int yCenter = startY + (i * 85);
    epaper.drawRoundRect(75, yCenter - 35, 650, 70, 15, TFT_BLACK);
    epaper.setTextDatum(MC_DATUM);
    setUIFont(32);
    epaper.drawString(cryptoData[i], CENTER_X, yCenter);
  }

  epaper.drawRoundRect(CENTER_X - 310, 445, 620, 25, 8, TFT_BLACK);
  epaper.setTextDatum(MC_DATUM);
  setUIFont(18);
  epaper.drawString("Data is updated from API and may not be the latest", CENTER_X, 458);

#endif
}

// --- Stock Market Screen Layout ---
void drawStockScreen() {
#ifdef EPAPER_ENABLE
  epaper.setTextDatum(MC_DATUM);
  setUIFont(32);
  epaper.drawString(uiLabel("Stock Market", "Thị trường cổ phiếu", "Mercado bursátil", "Marché boursier", "Aktienmarkt"), CENTER_X, 40);

  if (STOCK_API_URL.length() == 0) {
    setUIFont(24);
    epaper.drawString("API Link Required", CENTER_X, CENTER_Y);
    return;
  }

  int startY = 85;
  int boxH = 60;
  int boxW = 760; 
  int startX = CENTER_X - (boxW / 2); 

  for (int i = 0; i < 5; i++) {
    int yPos = startY + (i * (boxH + 12));
    epaper.drawRoundRect(startX, yPos, boxW, boxH, 10, TFT_BLACK);
    
    if (i < stockCount) {
      epaper.setTextDatum(ML_DATUM);
      setUIFont(18); 
      epaper.drawString(activeStocks[i].symbol, startX + 20, yPos + 30);

      String shortName = activeStocks[i].name;
      if (shortName.length() > 20) {
        shortName = shortName.substring(0, 18) + "..";
      }
      epaper.drawString(shortName, startX + 220, yPos + 30);

      epaper.setTextDatum(MR_DATUM);
      epaper.drawString(activeStocks[i].price, startX + boxW - 140, yPos + 30);
      
      // Calculate dynamic 16x16 vector math relative to price string length
      int priceStrLen = activeStocks[i].price.length() * 12; 
      int rupeeX = startX + boxW - 140 - priceStrLen - 20;
      epaper.drawBitmap(rupeeX, yPos + 22, icon_rupee, 16, 16, TFT_WHITE, TFT_BLACK);

      epaper.drawString(activeStocks[i].change, startX + boxW - 20, yPos + 30);
    } else {
      epaper.setTextDatum(MC_DATUM);
      setUIFont(18);
      epaper.drawString("Loading...", CENTER_X, yPos + 30);
    }
  }

  epaper.drawRoundRect(CENTER_X - 310, 445, 620, 25, 8, TFT_BLACK);
  epaper.setTextDatum(MC_DATUM);
  setUIFont(18);
  epaper.drawString("Data is updated from API and may not be the latest", CENTER_X, 458);

#endif
}

void drawDashboardClock(struct tm& timeinfo) {
#ifdef EPAPER_ENABLE
  char timeText[12];
  strftime(timeText, sizeof(timeText), "%I:%M %p", &timeinfo);

  epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  epaper.setTextDatum(MC_DATUM);
  setUIFont(56);
  epaper.drawString(timeText, 205, 95);
  lastDashboardDay = timeinfo.tm_yday;
#endif
}

void updateDashboardClockPartial(struct tm& timeinfo) {
#ifdef EPAPER_ENABLE
  if (timeinfo.tm_yday != lastDashboardDay) {
    // The calendar and lunar footer also change at midnight.
    needsScreenUpdate = true;
    return;
  }

  // Clean and redraw only the clock band. It stays below the location label
  // and ends at y=126, leaving a two-pixel gap above the divider at y=128.
  const int x = 42;
  const int y = 58;
  const int w = 326;
  const int h = 68;

  // Clean the clock window aggressively to prevent old digits from ghosting.
  epaper.fillRect(x, y, w, h, TFT_BLACK);
  epaper.updataPartial(x, y, w, h);
  delay(20);

  epaper.fillRect(x, y, w, h, TFT_WHITE);
  epaper.updataPartial(x, y, w, h);
  delay(20);

  drawDashboardClock(timeinfo);
  epaper.updataPartial(x, y, w, h);
#endif
}

// --- Vector Dashboard: Clock + Weather + Forecast + Calendar ---
void drawDashboardScreen(struct tm& timeinfo) {
#ifdef EPAPER_ENABLE
  const int leftX = 20;
  const int leftW = 370;
  const int rightX = 410;
  const int rightW = 370;

  // Top-left clock and current weather card.
  epaper.drawRoundRect(leftX, 20, leftW, 205, 14, TFT_BLACK);
  String weatherLocation = CITY + " - " + COUNTRY_CODE;

  epaper.setTextDatum(MC_DATUM);
  setUIFont(18);
  epaper.drawString(weatherLocation, leftX + leftW / 2, 45);
  drawDashboardClock(timeinfo);
  epaper.drawLine(leftX + 22, 128, leftX + leftW - 22, 128, TFT_BLACK);

  drawWeatherIcon(leftX + 72, 169, weatherIconCode);
  setUIFont(32);
  epaper.setTextDatum(ML_DATUM);
  epaper.drawString(currentTemp, leftX + 125, 165);
  int tempWidth = epaper.textWidth(currentTemp);
  int degreeX = leftX + 125 + tempWidth + 7;
  epaper.drawCircle(degreeX, 151, 3, TFT_BLACK);
  epaper.drawString("C", degreeX + 8, 165);
  setUIFont(18);
  epaper.setTextDatum(MC_DATUM);
  epaper.drawString(currentCondition, leftX + 265, 203);

  // Bottom-left forecast/details card, based on the existing weather screen.
  epaper.drawRoundRect(leftX, 240, leftW, 220, 14, TFT_BLACK);
  setUIFont(24);
  epaper.drawString(uiLabel("FORECAST", "DỰ BÁO", "PRONÓSTICO", "PRÉVISIONS", "WETTER"), leftX + leftW / 2, 270);
  epaper.drawLine(leftX + 22, 292, leftX + leftW - 22, 292, TFT_BLACK);

  const char* labels[] = {
    uiLabel("Humidity", "Độ ẩm", "Humedad", "Humidité", "Feuchte"),
    uiLabel("Wind", "Gió", "Viento", "Vent", "Wind"),
    uiLabel("Pressure", "Áp suất", "Presión", "Pression", "Luftdruck"),
    uiLabel("Sunrise", "Bình minh", "Amanecer", "Lever", "Aufgang"),
    uiLabel("Sunset", "Hoàng hôn", "Atardecer", "Coucher", "Untergang")
  };
  String values[] = {currentHumidity, currentWind, currentPressure, sunriseTime, sunsetTime};
  setUIFont(18);
  for (int i = 0; i < 5; i++) {
    int y = 320 + i * 31;
    epaper.setTextDatum(ML_DATUM);
    epaper.drawString(labels[i], leftX + 24, y);
    epaper.setTextDatum(MR_DATUM);
    epaper.drawString(values[i], leftX + leftW - 24, y);
  }

  // Right-side monthly calendar.
  epaper.drawRoundRect(rightX, 20, rightW, 440, 14, TFT_BLACK);
  String monthYearText = localizedMonthYear(timeinfo);
  double lunarTimeZone = lunarTimeZoneForCountry();
  bool showLunar = LUNAR_CALENDAR_ENABLED && lunarTimeZone != 0.0;
  epaper.setTextDatum(MC_DATUM);
  setUIFont(32);
  epaper.drawString(monthYearText, rightX + rightW / 2, 52);
  epaper.drawLine(rightX + 20, 78, rightX + rightW - 20, 78, TFT_BLACK);

  int currentDay = timeinfo.tm_mday;
  int currentMonth = timeinfo.tm_mon;
  int currentYear = timeinfo.tm_year + 1900;
  int currentWday = timeinfo.tm_wday;
  int daysInMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (currentMonth == 1 && ((currentYear % 4 == 0 && currentYear % 100 != 0) || (currentYear % 400 == 0))) {
    daysInMonths[1] = 29;
  }

  int firstDayOfWeek = (currentWday - (currentDay - 1) % 7 + 7) % 7;
  const int gridX = rightX + 20;
  const int cellW = 47;
  const int cellH = showLunar ? 39 : 50;
  setUIFont(18);
  for (int col = 0; col < 7; col++) {
    epaper.drawString(calendarDayLabel(col), gridX + col * cellW + cellW / 2, 108);
  }

  int dayCounter = 1;
  for (int row = 0; row < 6; row++) {
    for (int col = 0; col < 7; col++) {
      if ((row == 0 && col < firstDayOfWeek) || dayCounter > daysInMonths[currentMonth]) continue;
      int cx = gridX + col * cellW + cellW / 2;
      int cy = 153 + row * cellH;
      if (dayCounter == currentDay) {
        epaper.fillRoundRect(cx - 18, cy - 18, 36, 36, 6, TFT_BLACK);
        epaper.setTextColor(TFT_WHITE, TFT_BLACK);
        epaper.drawString(String(dayCounter), cx, cy);
        epaper.setTextColor(TFT_BLACK, TFT_WHITE);
      } else {
        epaper.drawString(String(dayCounter), cx, cy);
      }
      dayCounter++;
    }
  }

  if (showLunar) {
    LunarDate lunarToday = solarToLunar(currentDay, currentMonth + 1, currentYear, lunarTimeZone);
    String lunarFirstLine;
    String lunarSecondLine;
    buildLunarFooter(lunarToday, lunarFirstLine, lunarSecondLine);
    Serial.printf("[Lunar] %s %.0f: %02d/%02d/%04d%s\n",
                  COUNTRY_CODE.c_str(), lunarTimeZone, lunarToday.day, lunarToday.month,
                  lunarToday.year, lunarToday.leap ? " leap" : "");
    Serial.println("[Lunar] " + lunarFirstLine);
    Serial.println("[Lunar] " + lunarSecondLine);
    epaper.drawLine(rightX + 35, 377, rightX + rightW - 35, 377, TFT_BLACK);
    setUIFont(18);
    epaper.setTextDatum(MC_DATUM);
    epaper.drawString(lunarFirstLine, rightX + rightW / 2, 405);
    epaper.drawString(lunarSecondLine, rightX + rightW / 2, 433);
  }
#endif
}

// --- Task & Calendar Screen Layout ---
void drawTodoScreen() {
#ifdef EPAPER_ENABLE
  struct tm timeinfo;
#if UI_DEMO_MODE
  timeinfo = demoTimeInfo;
#else
  getLocalTime(&timeinfo);
#endif

  int boxW = 370;
  int boxH = 440;
  int leftX = 20;
  int rightX = 410;
  int topY = 20;

  epaper.drawRoundRect(leftX, topY, boxW, boxH, 15, TFT_BLACK);
  epaper.setTextDatum(MC_DATUM);
  setUIFont(32);
  epaper.drawString(uiLabel("Today's Tasks", "Việc hôm nay", "Tareas de hoy", "Tâches du jour", "Heutige Aufgaben"), leftX + boxW / 2, topY + 35);
  epaper.drawLine(leftX + 20, topY + 65, leftX + boxW - 20, topY + 65, TFT_BLACK);

  if (taskCount == 0) {
    setUIFont(24);
    epaper.drawString(uiLabel("No tasks", "Không có công việc", "Sin tareas", "Aucune tâche", "Keine Aufgaben"), leftX + boxW / 2, topY + 220);
  } else {
    epaper.setTextDatum(ML_DATUM);
    int itemY = topY + 110;
    for (int i = 0; i < taskCount; i++) {
      setUIFont(18); 
      String taskStr = activeTasks[i];
      int maxChars = 24; 
      int currentY = itemY;
      if (taskStr.length() > maxChars) currentY -= 12; 

      String currentLine = "";
      while (taskStr.length() > 0) {
        int spaceIdx = taskStr.indexOf(' ');
        String word = (spaceIdx == -1) ? taskStr : taskStr.substring(0, spaceIdx);
        if (currentLine.length() + word.length() > maxChars) {
          epaper.drawString(currentLine, leftX + 60, currentY);
          currentLine = word + " ";
          currentY += 24; 
        } else {
          currentLine += word + " ";
        }
        if (spaceIdx == -1) break;
        taskStr = taskStr.substring(spaceIdx + 1);
      }
      if (currentLine.length() > 0) epaper.drawString(currentLine, leftX + 60, currentY);
      
      epaper.drawCircle(leftX + 35, itemY, 8, TFT_BLACK);
      epaper.drawCircle(leftX + 35, itemY, 7, TFT_BLACK);
      itemY += 65;
    }
  }

  epaper.drawRoundRect(rightX, topY, boxW, boxH, 15, TFT_BLACK);
  epaper.setTextDatum(MC_DATUM);
  setUIFont(32);
  
  String monthYearText = localizedMonthYear(timeinfo);
  epaper.drawString(monthYearText, rightX + boxW / 2, topY + 35);
  epaper.drawLine(rightX + 20, topY + 65, rightX + boxW - 20, topY + 65, TFT_BLACK);

  int currentDay = timeinfo.tm_mday;
  int currentMonth = timeinfo.tm_mon; 
  int currentYear = timeinfo.tm_year + 1900;
  int currentWday = timeinfo.tm_wday; 

  int daysInMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (currentMonth == 1 && ((currentYear % 4 == 0 && currentYear % 100 != 0) || (currentYear % 400 == 0))) {
    daysInMonths[1] = 29; 
  }
  
  int totalDays = daysInMonths[currentMonth];
  int firstDayOfWeek = (currentWday - (currentDay - 1) % 7 + 7) % 7;
  int startGridX = rightX + 18; 
  int startGridY = topY + 110;
  int cellW = 48;
  int cellH = 48;

  setUIFont(24);
  for (int i = 0; i < 7; i++) {
    epaper.drawString(calendarDayLabel(i), startGridX + (i * cellW) + (cellW / 2), startGridY);
  }

  startGridY += 45;
  int dayCounter = 1;
  for (int row = 0; row < 6; row++) {
    for (int col = 0; col < 7; col++) {
      if ((row == 0 && col < firstDayOfWeek) || dayCounter > totalDays) continue; 
      int cx = startGridX + (col * cellW) + (cellW / 2);
      int cy = startGridY + (row * cellH);

      if (dayCounter == currentDay) {
        epaper.fillRoundRect(cx - 20, cy - 20, 40, 40, 6, TFT_BLACK);
        epaper.setTextColor(TFT_WHITE, TFT_BLACK);
        epaper.drawString(String(dayCounter), cx, cy);
        epaper.setTextColor(TFT_BLACK, TFT_WHITE); 
      } else {
        epaper.drawString(String(dayCounter), cx, cy);
      }
      dayCounter++;
    }
  }
#endif
}

// --- UI Helper Functions ---
void drawWrappedStringCenter(String text, int startY, int maxCharsPerLine) {
#ifdef EPAPER_ENABLE
  int currentY = startY;
  String currentLine = "";

  int spaceIdx = 0;
  while (text.length() > 0) {
    spaceIdx = text.indexOf(' ');
    String word = (spaceIdx == -1) ? text : text.substring(0, spaceIdx);

    if (currentLine.length() + word.length() > maxCharsPerLine) {
      currentLine.trim(); 
      epaper.drawString(currentLine, CENTER_X, currentY);
      currentLine = word + " ";
      currentY += 40;
    } else {
      currentLine += word + " ";
    }

    if (spaceIdx == -1) break;
    text = text.substring(spaceIdx + 1);
  }
  if (currentLine.length() > 0) {
    currentLine.trim(); 
    epaper.drawString(currentLine, CENTER_X, currentY);
  }
#endif
}

void drawWeatherIcon(int x, int y, String iconCode) {
#ifdef EPAPER_ENABLE
  const unsigned char* iconArray = icon_default; 
  if (iconCode == "01d" || iconCode == "01n") iconArray = icon_01;
  else if (iconCode == "02d" || iconCode == "02n") iconArray = icon_02;
  else if (iconCode == "03d" || iconCode == "03n" || iconCode == "04d" || iconCode == "04n") iconArray = icon_03;
  else if (iconCode == "09d" || iconCode == "09n" || iconCode == "10d" || iconCode == "10n") iconArray = icon_09;
  else if (iconCode == "11d" || iconCode == "11n") iconArray = icon_11;
  else if (iconCode == "13d" || iconCode == "13n") iconArray = icon_13;
  else if (iconCode == "50d" || iconCode == "50n") iconArray = icon_50;
  epaper.drawBitmap(x - 32, y - 24, iconArray, 64, 64, TFT_WHITE, TFT_BLACK);
#endif
}
