#include <TFT_eSPI.h>
#include <SPI.h>

// --- UI & COLOR DEFINITIONS ---
#define BG_COLOR        0x10A6 // A very dark blue-grey
#define TEXT_COLOR      TFT_WHITE
#define ACCENT_COLOR    0x06FF // A bright cyan
#define ACCENT_ALT      TFT_ORANGE
#define DANGER_COLOR    TFT_RED
#define SAFE_COLOR      TFT_GREEN
#define LABEL_COLOR     TFT_LIGHTGREY

// --- GLOBAL OBJECTS & VARIABLES ---
TFT_eSPI tft = TFT_eSPI(); // The main screen object
TFT_eSprite spr = TFT_eSprite(&tft); // A single sprite for all flicker-free drawing

enum Screen {
  SCREEN_CLOCK,
  SCREEN_HEALTH
};
Screen currentScreen = SCREEN_CLOCK;
bool needsRedraw = true;

// Time variables
uint8_t hh = 0, mm = 0, ss = 0;
uint8_t omm = 99; // Old minute, to detect when the minute changes

// Touch debounce variables
uint32_t lastTouchTime = 0;
const uint32_t touchDebounce = 500; // 500ms between touches

// Update timer
uint32_t lastUpdateTime = 0;

// --- FORWARD DECLARATIONS ---
static uint8_t conv2d(const char* p);
void drawClockScreen();
void drawHealthScreen();
void handleTouch();
void drawTouchButton(const char* label);

// --- PLACEHOLDER SENSOR FUNCTIONS ---
float getTemperature() { return random(280, 320) / 10.0; }
float getHumidity() { return random(400, 550) / 10.0; }
bool isFallDetected() { return (random(100) > 95); }

// =======================================================================================
//   SETUP
// =======================================================================================
void setup(void) {
 
  Serial.begin(115200);

  hh = conv2d(__TIME__);
  mm = conv2d(__TIME__ + 3);
  ss = conv2d(__TIME__ + 6);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(BG_COLOR);

  // ** FIX: Use 4-bit color depth to save RAM **
  spr.setColorDepth(4);
  // Add a check to see if sprite creation was successful
  if (!spr.createSprite(tft.width(), tft.height())) {
    Serial.println("FATAL: Sprite creation failed! Not enough RAM.");
    while(1); // Halt execution
  }

  needsRedraw = true;
}

// =======================================================================================
//   MAIN LOOP
// =======================================================================================
void loop() {
  handleTouch();

  // Only update the display periodically to be more efficient
  if (millis() - lastUpdateTime > 1000) { // Update every second
    lastUpdateTime = millis();
    needsRedraw = true; // Flag that an update is needed
  }

  if (needsRedraw) {
    spr.fillSprite(BG_COLOR);

    switch (currentScreen) {
      case SCREEN_CLOCK:
        drawClockScreen();
        break;
      case SCREEN_HEALTH:
        drawHealthScreen();
        break;
    }
    
    spr.pushSprite(0, 0);
    needsRedraw = false; // Reset the flag until the next update
  }
  
  delay(10); // Small delay to prevent system overload
}

// =======================================================================================
//   SCREEN DRAWING FUNCTIONS
// =======================================================================================

void drawClockScreen() {
  // Update time logic
  ss++;
  if (ss > 59) {
    ss = 0;
    mm++;
    if (mm > 59) {
      mm = 0;
      hh++;
      if (hh > 23) hh = 0;
    }
  }

  // Only redraw the main time if the minute has changed, or for the first draw
  if (mm != omm) {
    omm = mm; // Update the 'old minute'
  }
  
  // --- Time ---
  char time_str[6];
  sprintf(time_str, "%02d:%02d", hh, mm);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TEXT_COLOR, BG_COLOR);
  spr.drawString(time_str, tft.width() / 2, tft.height() / 2 - 20, 8);

  // --- Date ---
  spr.setTextColor(LABEL_COLOR, BG_COLOR);
  spr.drawString(__DATE__, tft.width() / 2, tft.height() / 2 + 50, 4);

  // --- Button ---
  drawTouchButton("Health >");
}

void drawHealthScreen() {
  // --- Title ---
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(ACCENT_COLOR, BG_COLOR);
  spr.drawString("SYSTEM STATUS", tft.width() / 2, 10, 4);

  // --- Temperature ---
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(LABEL_COLOR, BG_COLOR);
  spr.drawString("Temperature", 20, 60, 2);
  char tempStr[8];
  dtostrf(getTemperature(), 4, 1, tempStr);
  strcat(tempStr, " C");
  spr.setTextColor(TEXT_COLOR, BG_COLOR);
  spr.drawString(tempStr, 20, 80, 4);

  // --- Humidity ---
  spr.setTextColor(LABEL_COLOR, BG_COLOR);
  spr.drawString("Humidity", 20, 130, 2);
  char humStr[8];
  dtostrf(getHumidity(), 4, 1, humStr);
  strcat(humStr, " %");
  spr.setTextColor(TEXT_COLOR, BG_COLOR);
  spr.drawString(humStr, 20, 150, 4);

  // --- Accelerometer Status ---
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(LABEL_COLOR, BG_COLOR);
  spr.drawString("Activity", tft.width() - 20, 60, 2);
  if (isFallDetected()) {
    spr.fillRoundRect(tft.width() - 130, 80, 110, 40, 5, DANGER_COLOR);
    spr.setTextColor(TEXT_COLOR);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("FALL!", tft.width() - 75, 100, 4);
  } else {
    spr.fillRoundRect(tft.width() - 130, 80, 110, 40, 5, SAFE_COLOR);
    spr.setTextColor(TEXT_COLOR);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Safe", tft.width() - 75, 100, 4);
  }

  // --- Button ---
  drawTouchButton("< Clock");
}


// =======================================================================================
//   HELPER AND UTILITY FUNCTIONS
// =======================================================================================

void handleTouch() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y) && (millis() - lastTouchTime > touchDebounce)) {
    lastTouchTime = millis();

    if (y > tft.height() - 60) {
      if (currentScreen == SCREEN_CLOCK) {
        currentScreen = SCREEN_HEALTH;
      } else {
        currentScreen = SCREEN_CLOCK;
      }
      needsRedraw = true; // Flag that we need to redraw everything
      omm = 99; // Force redraw of time when switching back to clock
    }
  }
}

void drawTouchButton(const char* label) {
  spr.fillRoundRect(40, tft.height() - 50, tft.width() - 80, 40, 5, ACCENT_COLOR);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(BG_COLOR);
  spr.drawString(label, tft.width() / 2, tft.height() - 30, 2);
}

static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9') v = *p - '0';
  return 10 * v + *++p - '0';
}
