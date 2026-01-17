#include <SPI.h>
#include <LoRa.h>

// ==== LoRa Configuration ====
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26
#define LORA_BAND  433E6

// ==== Pin Configuration ====
#define LED_PIN       2        // Status LED
#define BUZZER_PIN    4        // Buzzer
#define BUTTON_PIN    15       // Pushbutton (ACK button)
#define LDR_PIN       35       // LDR input (Li-Fi receiver)

// ==== Variables ====
bool alertReceived = false;    
bool lifiAlert = false;        
int ackCount = 0;

const int LDR_THRESHOLD = 2500;   // adjust after testing (0–4095)

// ======================================================================
// SETUP
// ======================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("🚗 Normal Vehicle Node Booting...");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // --- LoRa Setup ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("❌ LoRa init failed!");
    while (1);
  }
  Serial.println("✅ LoRa init success!");
}

// ======================================================================
// LOOP
// ======================================================================
void loop() {

  // --------------------------------------------------------------------
  // 1. CHECK LDR (Li-Fi Signal From EV)
  // --------------------------------------------------------------------
  int ldrValue = analogRead(LDR_PIN);
  Serial.print("LDR Value: ");
  Serial.println(ldrValue);

  if (ldrValue > LDR_THRESHOLD && !lifiAlert) {
    lifiAlert = true;
    alertReceived = true;
    Serial.println("📡 Li-Fi signal detected from EMV!");
    buzzOnce();
  }

  // --------------------------------------------------------------------
  // 2. CHECK LoRa alert only if Li-Fi did NOT activate
  // --------------------------------------------------------------------
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    Serial.print("📩 LoRa Received: ");
    Serial.println(incoming);

    if (incoming == "ALERT_FROM_EV") {
      alertReceived = true;
      ackCount = 0;
      buzzOnce();
      Serial.println("🚨 Emergency Vehicle Alert (LoRa) Received!");
    }
    else if (incoming.startsWith("RSU01")) {
      Serial.println("📡 Message from RSU: " + incoming);
    }
  }

  // --------------------------------------------------------------------
  // 3. BUTTON PRESS → Send ACK to RSU
  // --------------------------------------------------------------------
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW) {
    Serial.println("🔘 Button Pressed!");
    blinkLED();

    if (alertReceived) {
      ackCount++;
      sendAckToRSU(ackCount);
    }
  }
}

// ======================================================================
// HELPER FUNCTIONS
// ======================================================================
void buzzOnce() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(400);
  digitalWrite(BUZZER_PIN, LOW);
}

void blinkLED() {
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);
  delay(200);
}

// ---- ACK Sender ----
void sendAckToRSU(int count) {
  String msg = "ACK_FROM_NV, Count=" + String(count);
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  Serial.println("📨 Sent to RSU: " + msg);
}
