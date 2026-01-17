// ============================================
// EMERGENCY VEHICLE WITH LIFI → LORA FALLBACK
// ============================================

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ---- OLED CONFIGURATION ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define I2C_ADDRESS 0x3C  
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- LoRa PINS ----
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26
#define LORA_BAND  433E6   

// ---- LiFi / Status LED ----
#define LED_PIN 2

// ---- Ultrasonic ----
#define TRIG_PIN 25
#define ECHO_PIN 34

bool alertSent = false;
const int STABLE_COUNT = 3;
int stableHits = 0;
bool triedLifi = false;
unsigned long lifiStartTime = 0;
const unsigned long LIFI_TIMEOUT = 3000;   // 3 seconds

// ---- Measure distance ----
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return -1;   // no echo received
  return duration * 0.0343 / 2;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);

  Wire.begin(21, 22);
  display.begin(I2C_ADDRESS, true);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 8);
  display.println("Emergency Vehicle");
  display.display();
  delay(1000);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    display.clearDisplay();
    display.setCursor(0, 8);
    display.println("LoRa init failed!");
    display.display();
    while(1);
  }

  display.clearDisplay();
  display.setCursor(0, 8);
  display.println("LoRa ready");
  display.display();
  delay(1500);
  display.clearDisplay();
  display.display();
}

void loop() {

  // ---------- ULTRASONIC READING ----------
  float distance = getDistance();
  Serial.print("Distance: ");
  Serial.println(distance);

  // ----- First try LIFI before LORA -----
  if (!triedLifi) {
    triedLifi = true;
    lifiStartTime = millis();
    stableHits = 0;
  }

  // ----- If obstacle detected -----
  if (distance > 0 && distance <= 10) {
    stableHits++;

    digitalWrite(LED_PIN, HIGH);  // LiFi ON

    display.clearDisplay();
    display.setCursor(0, 8);
    display.println("Li-Fi Mode");
    display.setCursor(0, 24);
    display.println("Obstacle <=10cm");
    display.display();

    // If stable readings (success)
    if (stableHits >= STABLE_COUNT) {
      Serial.println("LiFi Transmission SUCCESS");
      return;    // DO NOT use LoRa
    }
  }

  // ---------- LIFI TIMEOUT / FAIL ----------
  if (millis() - lifiStartTime >= LIFI_TIMEOUT && !alertSent) {

    // LiFi failed
    digitalWrite(LED_PIN, LOW);

    Serial.println("LiFi FAILED → using LoRa fallback");

    display.clearDisplay();
    display.setCursor(0, 8);
    display.println("Li-Fi Failed");
    display.setCursor(0, 24);
    display.println("Sending via LoRa");
    display.display();
    delay(1500);

    // send lora instead
    sendSingleAlert();
    alertSent = true;
  }

  // ==========================================================
  //                RECEIVE FROM RSU (UPDATED)
  // ==========================================================
  int packetSize = LoRa.parsePacket();
  if (packetSize) {

    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();
    Serial.print("RSU Data: ");
    Serial.println(incoming);

    // Expected format:
    // COUNT=xx;STATUS=RED/GREEN;ACTION=xxxx

    String countStr = "";
    String statusStr = "";
    String actionStr = "";

    int cIndex = incoming.indexOf("COUNT=");
    int sIndex = incoming.indexOf("STATUS=");
    int aIndex = incoming.indexOf("ACTION=");

    if (cIndex != -1) {
      int end = incoming.indexOf(';', cIndex);
      countStr = incoming.substring(cIndex + 6, end);
    }

    if (sIndex != -1) {
      int end = incoming.indexOf(';', sIndex);
      statusStr = incoming.substring(sIndex + 7, end);
    }

    if (aIndex != -1) {
      int end = incoming.indexOf(';', aIndex);
      if (end == -1) end = incoming.length();
      actionStr = incoming.substring(aIndex + 7, end);
    }

    // -------- DISPLAY ON OLED --------
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("RSU DATA");

    display.setCursor(0, 16);
    display.print("COUNT: ");
    display.println(countStr);

    display.setCursor(0, 28);
    display.print("STATUS: ");
    display.println(statusStr);

    display.setCursor(0, 40);
    display.print("ACTION: ");
    display.println(actionStr);

    display.display();
  }
}

// ==========================================================
//                  SEND ALERT FUNCTION
// ==========================================================
void sendSingleAlert() {
  Serial.println("Transmitting alert...");

  display.clearDisplay();
  display.setCursor(0, 8);
  display.println("Transmitting...");
  display.display();

  digitalWrite(LED_PIN, HIGH);  
  delay(250);
  digitalWrite(LED_PIN, LOW);

  LoRa.beginPacket();
  LoRa.print("ALERT_FROM_EV");
  LoRa.endPacket();

  Serial.println("Alert sent!");

  display.clearDisplay();
  display.setCursor(0, 8);
  display.println("Alert Sent!");
  display.display();
}
