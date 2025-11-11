#include <ESP8266WiFi.h>
#include <espnow.h>

// Relais-Logik (viele Module sind aktiv LOW)
#define RELAY_ACTIVE_LOW  false

#define PIN_RELAIS    14
#define FAILSAFE_MS   1000
#define ARM_REQUIRED  3        // wie viele konsistente "EIN"-Nachrichten nötig
#define CHECK_PERIOD  50       // ms

typedef struct {
  uint32_t seq;
  uint32_t ms;
  uint8_t  pilzPressed;   // 1=Pilz gedrückt (AUS), 0=losgelassen (EIN)
} CtrlMsg;

volatile uint32_t lastRxMs = 0;
volatile uint8_t  pilzPressed = 1;   // sicher AUS starten

// Arming/Filter
volatile uint8_t  consecutiveEnable = 0; // Zählt aufeinanderfolgende "EIN"-Signale

static inline void relayWriteActive(bool active) {
  if (RELAY_ACTIVE_LOW) digitalWrite(PIN_RELAIS, active ? LOW : HIGH);
  else                  digitalWrite(PIN_RELAIS, active ? HIGH : LOW);
}

static inline void applyOutputs() {
  bool heartbeatOK = (millis() - lastRxMs <= FAILSAFE_MS);

  // Logik:
  // - Pilz gedrückt  -> AUS (sofort)
  // - Kein Heartbeat -> AUS
  // - Pilz losgelassen + genug konsistente Messages -> EIN
  bool wantActive = false;
  if (!pilzPressed && heartbeatOK && (consecutiveEnable >= ARM_REQUIRED)) {
    wantActive = true;
  }

  relayWriteActive(wantActive);
}

void OnRecv(uint8_t*, uint8_t* data, uint8_t len) {
  if (len == sizeof(CtrlMsg)) {
    CtrlMsg m; memcpy(&m, data, sizeof(m));
    lastRxMs = millis();

    // Zustand übernehmen
    pilzPressed = m.pilzPressed;

    // Arming-FSM
    if (pilzPressed == 0) {
      if (consecutiveEnable < 255) consecutiveEnable++;
    } else {
      consecutiveEnable = 0; // sobald Pilz gedrückt -> wieder AUS erzwingen
    }
  }
  applyOutputs();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELAIS, OUTPUT);

  // sicher AUS
  if (RELAY_ACTIVE_LOW) digitalWrite(PIN_RELAIS, HIGH);
  else                  digitalWrite(PIN_RELAIS, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnRecv);

  Serial.print("Empfänger MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.printf("Relais-Logik: %s\n", RELAY_ACTIVE_LOW ? "ACTIVE LOW" : "ACTIVE HIGH");

  lastRxMs = 0;          // erzwingt AUS bis echte Daten kommen
  consecutiveEnable = 0; // arming zurücksetzen
  applyOutputs();
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= CHECK_PERIOD) {
    last = now;
    applyOutputs();
  }
}
