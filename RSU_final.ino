#include <SPI.h>
#include <LoRa.h>

// LoRa Pins
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26

#define LORA_BAND 433E6

// LEDs
#define RED_LED 2
#define GREEN_LED 13

int ackCount = 0;
unsigned long lastAckTime = 0;
const unsigned long TIMEOUT = 10000; // 10 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🚦 RSU Booting...");

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("❌ LoRa init failed!");
    while (1);
  }

  Serial.println("✅ LoRa Initialised");
}

void loop() {

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) msg += (char)LoRa.read();

    Serial.print("📩 Received: ");
    Serial.println(msg);

    if (msg.startsWith("ACK_FROM_NV")) {
      ackCount++;
      lastAckTime = millis();

      Serial.print("🔢 ACK Count = ");
      Serial.println(ackCount);

      updateLEDState();
    }
  }

  // ---- 10-second timeout after LAST ACK ----
  if (ackCount > 0 && (millis() - lastAckTime >= TIMEOUT)) {
    sendFinalResult();
    ackCount = 0;  // reset
  }
}

// ======================================================
// LED Traffic Logic
// ======================================================
void updateLEDState() {

  if (ackCount < 5) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
  }
  else if (ackCount >= 5 && ackCount <= 9) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
  }
  else if (ackCount >= 10) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
  }
}

// ======================================================
// Send Final Traffic Result to EV
// ======================================================
void sendFinalResult() {

  String action;
  String status;

  if (ackCount < 5) {
    status = "RED";
    action = "LOW_TRAFFIC_STOP";
  }
  else if (ackCount >= 5 && ackCount <= 9) {
    status = "GREEN";
    action = "PATH_FREE";
  }
  else {
    status = "RED";
    action = "HEAVY_TRAFFIC_WARNING";
  }

  Serial.println("=========================================");
  Serial.println("⏳ 10 sec passed — Sending FINAL RESULT");
  Serial.print("📦 Final ACK Count: ");
  Serial.println(ackCount);
  Serial.print("🚦 LED STATUS: ");
  Serial.println(status);
  Serial.print("📘 ACTION: ");
  Serial.println(action);

  // ---- Send to EV ----
  LoRa.beginPacket();
  LoRa.print("COUNT=");
  LoRa.print(ackCount);
  LoRa.print("|STATUS=");
  LoRa.print(status);
  LoRa.print("|ACTION=");
  LoRa.print(action);
  LoRa.endPacket();

  Serial.println("📤 MSG SENT");
  Serial.println("=========================================");
}
