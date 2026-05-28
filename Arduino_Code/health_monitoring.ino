#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// WiFi
const char* ssid = "Narzo70x5g";
const char* password = "SuryaSurya";

// Firebase
String firebaseURL = "https://healthbelt1-default-rtdb.asia-southeast1.firebasedatabase.app";

// Sensors
MAX30105 particleSensor;
MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

// Pins
#define BUTTON_PIN 4
#define BUZZER_PIN 5

bool lastButtonState = HIGH;
unsigned long lastFallTime = 0;

// Heart variables
long irValue;
int heartRate = 75;
int spo2 = 98;

void sendToFirebase(int hr, int spo2Val, float lat, float lng, String alert) {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    http.begin(firebaseURL + "/health/heartrate.json");
    http.PUT(String(hr));
    http.end();

    http.begin(firebaseURL + "/health/spo2.json");
    http.PUT(String(spo2Val));
    http.end();

    http.begin(firebaseURL + "/health/latitude.json");
    http.PUT(String(lat, 6));
    http.end();

    http.begin(firebaseURL + "/health/longitude.json");
    http.PUT(String(lng, 6));
    http.end();

    http.begin(firebaseURL + "/health/fall.json");
    http.PUT("\"" + alert + "\"");
    http.end();

    Serial.println("✅ Firebase Updated");
  }
}

void setup() {

  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // WiFi
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connected");

  // I2C
  Wire.begin(21, 22);

  // MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not found");
    while (1);
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);

  // MPU6050
  mpu.initialize();

  // GPS
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("🚀 System Ready");
}

void loop() {

  String alert = "Normal";

  // ❤️ HEART RATE
  irValue = particleSensor.getIR();

  if (checkForBeat(irValue)) {

    static long lastBeat = 0;

    long delta = millis() - lastBeat;
    lastBeat = millis();

    heartRate = 60 / (delta / 1000.0);

    if (heartRate < 50 || heartRate > 120)
      heartRate = 75;
  }

  // Simulated SpO2
  spo2 = random(96, 99);

  // 📡 GPS
  while (gpsSerial.available())
    gps.encode(gpsSerial.read());

  float lat = gps.location.lat();
  float lng = gps.location.lng();

  // 🤖 FALL DETECTION
  int16_t ax, ay, az, gx, gy, gz;

  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float Gx = gx / 131.0;
  float Gy = gy / 131.0;
  float Gz = gz / 131.0;

  float gyroMag = sqrt(Gx*Gx + Gy*Gy + Gz*Gz);

  Serial.print("GyroMag: ");
  Serial.println(gyroMag);

  if (gyroMag > 100 && millis() - lastFallTime > 3000) {

    alert = "🚨 FALL DETECTED";

    digitalWrite(BUZZER_PIN, HIGH);

    sendToFirebase(heartRate, spo2, lat, lng, alert);

    Serial.println(alert);

    delay(3000);

    digitalWrite(BUZZER_PIN, LOW);

    lastFallTime = millis();
  }

  // 🔘 BUTTON
  bool btn = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && btn == LOW) {

    alert = "🚨 EMERGENCY BUTTON";

    digitalWrite(BUZZER_PIN, HIGH);

    sendToFirebase(heartRate, spo2, lat, lng, alert);

    Serial.println(alert);

    delay(3000);

    digitalWrite(BUZZER_PIN, LOW);
  }

  lastButtonState = btn;

  // ☁️ NORMAL UPDATE
  sendToFirebase(heartRate, spo2, lat, lng, alert);

  // Serial Monitor
  Serial.print("❤️ HR: ");
  Serial.print(heartRate);

  Serial.print(" | 🫁 SpO2: ");
  Serial.print(spo2);

  Serial.print(" | 📍 ");
  Serial.print(lat,6);
  Serial.print(",");
  Serial.println(lng,6);

  delay(1000);
}