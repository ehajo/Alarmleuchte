#include <ESP8266WiFi.h>
#include <espnow.h>

#define PIN_PILZ         D2       // D2 = GPIO4
#define SEND_PERIOD_MS   200
#define DEBOUNCE_MS      10
#define STABLE_MS        200

// Polarität: true => gedrückt = LOW (gegen GND) | false => gedrückt = HIGH
#define PILZ_ACTIVE_LOW  true

// MAC des Empfängers (vom Empfänger-Serial übernehmen!)
uint8_t peerMac[] = {0xAF, 0xFE, 0xDE, 0xAD, 0xBE, 0xEF};

typedef struct {
  uint32_t seq;
  uint32_t ms;
  uint8_t  pilzPressed;   // 1=Pilz gedrückt (AUS), 0=Pilz losgelassen (EIN)
} CtrlMsg;

uint32_t seq = 0;

static inline uint8_t rawPressed() {
  int r = digitalRead(PIN_PILZ);
  if (PILZ_ACTIVE_LOW) return (r == LOW) ? 1 : 0;
  else                 return (r == HIGH) ? 1 : 0;
}

void OnSent(uint8_t*, uint8_t) {}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PILZ, INPUT_PULLUP);   // interner Pull-Up

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnSent);

  if (esp_now_add_peer(peerMac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0) != 0) {
    Serial.println("Peer add failed");
    while (true) delay(1000);
  }
}

void loop() {
  static uint32_t lastSend = 0;
  static uint8_t debounced = rawPressed();
  static uint8_t lastRaw   = debounced;
  static uint32_t tEdge    = 0;      // Zeit der letzten Flanke
  static uint32_t tStable  = 0;      // Zeit, seit der Zustand stabil ist

  uint32_t now = millis();
  uint8_t r = rawPressed();

  // Entprellen
  if (r != lastRaw) { lastRaw = r; tEdge = now; }
  if ((now - tEdge) > DEBOUNCE_MS && debounced != r) {
    debounced = r;
    tStable = now;
  }

  if (now - lastSend >= SEND_PERIOD_MS) {
    lastSend = now;

    CtrlMsg m;
    m.seq = ++seq;
    m.ms  = now;

    // Nur wenn Zustand >= STABLE_MS stabil ist, übernehmen – sonst alten Zustand halten
    static uint8_t stableOut = debounced;
    if ((now - tStable) >= STABLE_MS) stableOut = debounced;

    // Mapping: gedrückt => AUS
    m.pilzPressed = stableOut ? 0 : 1;

    esp_now_send(peerMac, (uint8_t*)&m, sizeof(m));

    // Debug
    // Serial.printf("raw=%u debounced=%u out(pressed)=%u\n", r, debounced, m.pilzPressed);
  }
}
