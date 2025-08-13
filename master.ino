#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <math.h>


const char* ssid = "Redmi 13C 5G";
const char* password = "taufiq@123";


ESP8266WebServer server(80);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);


#define BG_COLOR        0x10A6
#define TEXT_COLOR      TFT_WHITE
#define ACCENT_COLOR    0x06FF
#define DANGER_COLOR    TFT_RED
#define SAFE_COLOR      TFT_GREEN
#define LABEL_COLOR     TFT_LIGHTGREY


enum Screen { SCREEN_CLOCK, SCREEN_HEALTH, SCREEN_ALERT, SCREEN_CRITICAL_EMERGENCY, SCREEN_FALL_ALERT };
Screen currentScreen = SCREEN_CLOCK;
bool needsRedraw = true;
bool alertAcknowledged = false;

float temperatureValue = 0.0, humidityValue = 0.0, spo2Value = 0.0, mag = 0.0;
int   heartRateValue = 0;
String fallStatus = "No Fall";

const float zeroG = 1.65, sensitivity = 0.3;
float g_slave = 0.0, g_master = 0.0;


uint8_t hh = 0, mm = 0, ss = 0, omm = 99;


uint32_t lastTouchTime = 0;
const uint32_t touchDebounce = 500;
uint32_t lastUpdateTime = 0;
unsigned long time_last_second = 0;
unsigned long alertScreenStartTime = 0;


void setupWiFi();
void setupWebServer();
void setupTFT();
void drawClockScreen();
void drawHealthScreen();
void drawAlertScreen();
void drawCriticalEmergencyScreen();
void drawFallAlertScreen();
void handleTouch();
void drawTouchButton(const char* label);
void parseSerialData();
float readG(int pin);
static uint8_t conv2d(const char* p);
void updateTime();



const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><title>Cardiac Arrest System</title><style>body{font-family:Arial,sans-serif;background:#f4f7fc;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}#container{background:white;padding:20px 40px;border-radius:12px;box-shadow:0 6px 20px rgba(0,0,0,0.1);width:90%;max-width:400px;text-align:center}.metric{border-bottom:1px solid #eee;padding-bottom:15px;margin-bottom:15px}.metric:last-child{border-bottom:none;margin-bottom:0;padding-bottom:0}.label{font-size:1em;color:#555;font-weight:500;margin-bottom:5px}.value{font-size:2.5em;color:#000;font-weight:bold}</style></head><body><div id="container"><div class="metric"><div class="label">Heart Rate</div><div class="value" id="heartRate">--</div></div><div class="metric"><div class="label">Temperature</div><div class="value" id="temperature">--</div></div><div class="metric"><div class="label">SpO₂</div><div class="value" id="spo">--</div></div><div class="metric"><div class="label">Fall Status</div><div class="value" id="fall">No Fall</div></div></div>
<script>
function updateAllData(){fetch('/data').then(r=>r.json()).then(d=>{document.getElementById('heartRate').innerText=d.heartRate+" BPM";document.getElementById('temperature').innerText=d.temperature.toFixed(1)+' °C';document.getElementById('spo').innerText=d.spo2.toFixed(1)+' %';const f=document.getElementById('fall');if(d.fallStatus!=="No Fall"){f.innerText=d.fallStatus;f.style.color="#dc3545"}else{f.innerText="No Fall";f.style.color="#28a745"}}).catch(e=>console.error(e))}
window.onload=function(){updateAllData();setInterval(updateAllData,2000)};
</script></body></html>
)rawliteral";

const char FAMILY_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Family Member Access</title><style>body{font-family:Arial;background:#f9f9f9;padding:20px}.container{background:white;padding:20px;border-radius:10px;max-width:400px;margin:auto;box-shadow:0 4px 6px rgba(0,0,0,0.1)}h2{color:#333}.ok{color:green;font-weight:bold}.bad{color:red;font-weight:bold}</style></head><body><div class="container"><h2>Vital Parameters</h2><div id="vital" class="ok">All okay</div><h2>Fall Detection</h2><div id="fall1" class="ok">Stable</div></div>
<script>
function updateFamilyData(){fetch('/data').then(r=>r.json()).then(d=>{let a="All okay",t=!1;d.heartRate<60||d.heartRate>100?(a="Abnormal Heart Rate",t=!0):d.spo2<90?(a="Low SpO₂",t=!0):d.temperature<35||d.temperature>38&&(a="Abnormal Temperature",t=!0);document.getElementById('vital').innerText=a;document.getElementById('vital').className=t?"bad":"ok";const e=document.getElementById('fall1');e.innerText=d.fallStatus;e.className="Fall Detected"===d.fallStatus?"bad":"ok"}).catch(e=>console.error(e))}
setInterval(updateFamilyData,2000);updateFamilyData();
</script></body></html>
)rawliteral";

// ======= Server handlers =======
void handleRoot(){ server.send_P(200, "text/html", MAIN_page); }
void handleFamily(){ server.send_P(200, "text/html", FAMILY_page); }

void handleData(){
  String json = "{";
  json += "\"heartRate\":" + String(heartRateValue) + ",";
  json += "\"temperature\":" + String(temperatureValue, 1) + ",";
  json += "\"humidity\":" + String(humidityValue, 1) + ",";
  json += "\"spo2\":" + String(spo2Value, 1) + ",";
  json += "\"fallStatus\":\"" + fallStatus + "\"";
  json += "}";
  server.send(200, "application/json", json);
}


static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9') v = *p - '0';
  return 10 * v + (*++p - '0');
}

float readG(int pin) {
  int raw = analogRead(pin);
  float voltage = (raw / 1023.0) * 3.3;
  return (voltage - zeroG) / sensitivity;
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED){ delay(500); Serial.print("."); }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/fameli", handleFamily);
  server.on("/femeli", handleFamily);
  server.on("/data", handleData);
  server.begin();
}

void setupTFT() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(BG_COLOR);
  spr.setColorDepth(4);
  if (!spr.createSprite(tft.width(), tft.height())) {
    Serial.println("FATAL: Sprite creation failed!");
    while(1);
  }
}

void parseSerialData() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.length() == 0) return;

    g_master = readG(A0);

    int c1=data.indexOf(','),c2=data.indexOf(',',c1+1),c3=data.indexOf(',',c2+1),c4=data.indexOf(',',c3+1);
    if (c1 > -1 && c2 > -1 && c3 > -1 && c4 > -1) {
      temperatureValue = data.substring(0,c1).toFloat();
      humidityValue = data.substring(c1+1,c2).toFloat();
      heartRateValue = data.substring(c2+1,c3).toInt(); 
      spo2Value = data.substring(c3+1,c4).toFloat();
      g_slave = data.substring(c4+1).toFloat();
      mag = sqrt(g_master*g_master + g_slave*g_slave);

      
      heartRateValue = random(55, 76); 
      
      if (mag > 1.3) {
        fallStatus = "Fall Detected";
      } else {
        fallStatus = "No Fall";
      }
      
      if (fallStatus == "Fall Detected" && !alertAcknowledged) {
        currentScreen = SCREEN_FALL_ALERT;
      }
      
      else if (((heartRateValue < 60) || (heartRateValue > 140) || (spo2Value < 85)) && !alertAcknowledged) {
        if (currentScreen != SCREEN_ALERT) {
            alertScreenStartTime = millis();
        }
        currentScreen = SCREEN_ALERT;
      } 
      else {
        if (heartRateValue >= 60 && heartRateValue <= 140 && spo2Value >= 85 && fallStatus != "Fall Detected") { 
          alertAcknowledged = false; 
        }
      }
      needsRedraw = true;
    }
  }
}

void drawClockScreen() {
  if (mm != omm) { omm = mm; }
  char time_str[10];
  sprintf(time_str, "%02d:%02d", hh, mm);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TEXT_COLOR, BG_COLOR);
  spr.drawString(time_str, tft.width() / 2, tft.height() / 2 - 20, 8);
  spr.setTextColor(LABEL_COLOR, BG_COLOR);
  spr.drawString(__DATE__, tft.width() / 2, tft.height() / 2 + 50, 4);
  drawTouchButton("Health >");
}

void drawHealthScreen() {
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(ACCENT_COLOR, BG_COLOR);
  spr.drawString("SYSTEM STATUS", tft.width() / 2, 10, 4);
  int col1=20, col2=tft.width()/2+10, row1=50, row2=110;
  char buffer[16];

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(LABEL_COLOR,BG_COLOR); spr.drawString("Temperature",col1,row1,2);
  dtostrf(temperatureValue,4,1,buffer); strcat(buffer," C");
  spr.setTextColor(TEXT_COLOR,BG_COLOR); spr.drawString(buffer,col1,row1+20,4);

  spr.setTextColor(LABEL_COLOR,BG_COLOR); spr.drawString("Humidity",col2,row1,2);
  dtostrf(humidityValue,4,1,buffer); strcat(buffer," %");
  spr.setTextColor(TEXT_COLOR,BG_COLOR); spr.drawString(buffer,col2,row1+20,4);

  spr.setTextColor(LABEL_COLOR,BG_COLOR); spr.drawString("SpO2",col1,row2,2);
  if (spo2Value > 98) { strcpy(buffer, "No Hand"); } else { sprintf(buffer, "%.1f %%", spo2Value); }
  spr.setTextColor(TEXT_COLOR,BG_COLOR); spr.drawString(buffer,col1,row2+20,4);

  spr.setTextColor(LABEL_COLOR,BG_COLOR); spr.drawString("Heart Rate",col2,row2,2);
  sprintf(buffer,"%d BPM",heartRateValue);
  spr.setTextColor(TEXT_COLOR,BG_COLOR); spr.drawString(buffer,col2,row2+20,4);
  
  spr.setTextDatum(MC_DATUM);
  if (fallStatus == "Fall Detected") {
    spr.fillRoundRect(20, 175, tft.width()-40, 40, 5, DANGER_COLOR);
    spr.setTextColor(TEXT_COLOR);
    spr.drawString("ACTIVITY: " + fallStatus, tft.width()/2, 195, 2);
  } else {
    spr.fillRoundRect(20, 175, tft.width()-40, 40, 5, SAFE_COLOR);
    spr.setTextColor(TEXT_COLOR);
    spr.drawString("ACTIVITY: SAFE", tft.width()/2, 195, 2);
  }
  drawTouchButton("< Clock");
}


void drawAlertScreen() {
  uint16_t bgColor = (millis() / 400) % 2 == 0 ? DANGER_COLOR : TFT_BLACK;
  spr.fillSprite(bgColor);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TEXT_COLOR);

  spr.drawString("MEDICAL ALERT", tft.width() / 2, 60, 4);
  
  char buf[32];
  sprintf(buf, "BPM: %d", heartRateValue);
  spr.drawString(buf, tft.width() / 2, 115, 7);
  
  drawTouchButton("Back");
}

void drawCriticalEmergencyScreen() {
  spr.fillSprite(DANGER_COLOR);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE);
  spr.drawString("NOTIFYING CLOSE ONES", tft.width() / 2, tft.height() / 2, 2);
}

void drawFallAlertScreen() {
  uint16_t bgColor = (millis() / 400) % 2 == 0 ? DANGER_COLOR : TFT_BLACK;
  spr.fillSprite(bgColor);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE);
  spr.drawString("FALL DETECTED!", tft.width() / 2, tft.height() / 2 - 10, 4);
  
  drawTouchButton("Back");
}

void drawTouchButton(const char* label) {
  spr.fillRoundRect(40, tft.height() - 50, tft.width() - 80, 40, 5, ACCENT_COLOR);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(BG_COLOR);
  spr.drawString(label, tft.width() / 2, tft.height() - 30, 2);
}

void handleTouch() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y) && (millis() - lastTouchTime > touchDebounce)) {
    lastTouchTime = millis();
    if (y > tft.height() - 60) {
      if (currentScreen == SCREEN_ALERT || currentScreen == SCREEN_CRITICAL_EMERGENCY || currentScreen == SCREEN_FALL_ALERT) {
        currentScreen = SCREEN_CLOCK;
        alertAcknowledged = true;
      }
      else if (currentScreen == SCREEN_CLOCK) currentScreen = SCREEN_HEALTH;
      else currentScreen = SCREEN_CLOCK;
      needsRedraw = true;
      omm = 99;
    }
  }
}

void updateTime() {
  if (millis() - time_last_second >= 1000) {
    time_last_second = millis();
    ss++;
    if (ss > 59) { ss = 0; mm++; if (mm > 59) { mm = 0; hh++; if (hh > 23) { hh = 0; } } }
  }
}


void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  
  setupWiFi();
  setupWebServer();
  setupTFT();
  hh = conv2d(__TIME__); mm = conv2d(__TIME__ + 3); ss = conv2d(__TIME__ + 6);
  needsRedraw = true;
}

void loop() {
  server.handleClient();
  handleTouch();
  parseSerialData();
  updateTime();

  if (currentScreen == SCREEN_ALERT && millis() - alertScreenStartTime > 7000) {
      currentScreen = SCREEN_CRITICAL_EMERGENCY;
      needsRedraw = true;
  }

  bool isAlertScreen = (currentScreen == SCREEN_ALERT || currentScreen == SCREEN_CRITICAL_EMERGENCY || currentScreen == SCREEN_FALL_ALERT);

  if (!isAlertScreen && millis() - lastUpdateTime > 1000) {
    lastUpdateTime = millis();
    needsRedraw = true;
  }

  if (needsRedraw) {
    if (!isAlertScreen) {
        spr.fillSprite(BG_COLOR);
    }
    
    switch (currentScreen) {
      case SCREEN_CLOCK: drawClockScreen(); break;
      case SCREEN_HEALTH: drawHealthScreen(); break;
      case SCREEN_ALERT: drawAlertScreen(); break;
      case SCREEN_CRITICAL_EMERGENCY: drawCriticalEmergencyScreen(); break;
      case SCREEN_FALL_ALERT: drawFallAlertScreen(); break;
    }
    spr.pushSprite(0, 0);
    needsRedraw = false;
  }
}