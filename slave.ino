#include <DHT11.h>
#include <Wire.h>
#include "FS.h"

#include "MAX30105.h"
#include "heartRate.h"


DHT11 dht11(D4);
MAX30105 particleSensor;

int heartRate = 0;
float spo2 = 0;

int humidity = 0;
int temperature = 0;

const float zeroG = 1.65;
const float sensitivity = 0.3;

unsigned long lastBeat = 0;

const int FS_MS = 20; 

void setup() {
  Serial.begin(115200);
  Wire.begin();


  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 was not found. Please check wiring/power.");
    while (1);
  }
  particleSensor.setup(); 
}


float readG(int pin) {
  int raw = analogRead(pin);
  float voltage = (raw / 1023.0) * 3.3;
  return (voltage - zeroG) / sensitivity;
}
void loop() {

  long irValue = particleSensor.getIR();

float g_slave = readG(A0);

  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    heartRate = 60.0 / (delta / 1000.0);
  }


  long redValue = particleSensor.getRed();
  if (redValue > 0 && irValue > 0) {
    spo2 = 100.0 - (5.0 * abs(redValue - irValue) / redValue); 
  }


  dht11.readTemperatureHumidity(temperature, humidity);

  Serial.print(temperature);
  Serial.print(",");
  Serial.print(humidity);
  Serial.print(",");
  Serial.print(heartRate);
  Serial.print(",");
  Serial.print(spo2);
  Serial.print(",");
  Serial.println(g_slave);

  delay(100);
}