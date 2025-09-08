#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>                // für NTP (empfohlen)
// #define ENABLE_OTA           // ← aktivieren, wenn du OTA nutzen willst
#ifdef ENABLE_OTA
  #include <ArduinoOTA.h>
#endif

#include "config.h"

// ------------------ Einstellungen ------------------
#define RELAY_PIN 14                  // GPIO14 (D5) fürs Relais
// #define DEBUG                      // Debug-Ausgaben aktivieren
#ifdef DEBUG
  #define DBG(x) Serial.println(x)
#else
  #define DBG(x)
#endif

// Twitch IRC
static const char* TWITCH_SERVER = "irc.chat.twitch.tv";
static const int   TWITCH_PORT   = 6667;

// Custom Reward
static const char* CUSTOM_REWARD_ID = "123-123-123";  // anpassen

// Zeiten
static const unsigned long ALARM_DURATION_MS       = 10000UL;
static const unsigned long TOKEN_CHECK_INTERVAL_MS = 60000UL; // 1 min
static const unsigned long TOKEN_REFRESH_SAFETY    = 300UL;   // 5 min vorher
static const unsigned long RECONNECT_DELAY_MS      = 5000UL;

// NTP (empfohlen; sorgt für exakte Ablaufzeiten)
static const char* NTP1 = "pool.ntp.org";
static const char* NTP2 = "time.nist.gov";

// ------------------ Globale Variablen ------------------
WiFiClient ircClient;

bool alarmActive = false;
unsigned long alarmStart = 0;

unsigned long tokenExpiryEpoch = 0;
unsigned long lastTokenCheck = 0;

unsigned long lastOtaHandle = 0;
static const unsigned long OTA_HANDLE_INTERVAL_MS = 1000UL;

// ------------------ Hilfsfunktionen ------------------
static void yieldBrief() { delay(1); yield(); }

static time_t nowEpoch() {
  time_t t = time(nullptr);
  if (t < 100000) { // falls NTP noch nicht da
    return 0;
  }
  return t;
}

static bool waitForNTP(unsigned long timeoutMs = 10000) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (nowEpoch() > 100000) return true;
    delay(50);
  }
  return false;
}

static String urlEncode(const String& s) {
  String out;
  out.reserve(s.length() * 3);
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    // Unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
    bool unreserved =
      (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
      (c >= '0' && c <= '9') || c == '-' || c == '.' ||
      c == '_' || c == '~';
    if (unreserved) {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0xF];
    }
  }
  return out;
}

// ------------------ OAuth: Validate ------------------
static bool validateAccessToken(unsigned long* expiresInOut = nullptr) {
  if (twitch_access_token.length() == 0) return false;

  WiFiClientSecure https;
  https.setInsecure(); // für einfachen Start; später CA-Pin verwenden

  HTTPClient http;
  if (!http.begin(https, "https://id.twitch.tv/oauth2/validate")) {
    DBG(F("validate: http.begin failed"));
    return false;
  }
  http.addHeader("Authorization", "OAuth " + twitch_access_token);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    DBG(String(F("validate: http code ")) + code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  auto err = deserializeJson(doc, payload);
  if (err) {
    DBG(String(F("validate: json error ")) + err.c_str());
    return false;
  }

  if (doc.containsKey("expires_in")) {
    unsigned long expiresIn = doc["expires_in"] | 0UL;
    if (expiresInOut) *expiresInOut = expiresIn;
    time_t now = nowEpoch();
    if (now != 0) tokenExpiryEpoch = now + expiresIn;
  }
  // optional: client_id, login, scopes prüfen
  return true;
}

// ------------------ OAuth: Refresh ------------------
static bool refreshAccessToken() {
  DBG(F("refresh: start"));

  WiFiClientSecure https;
  https.setInsecure();

  HTTPClient http;
  if (!http.begin(https, "https://id.twitch.tv/oauth2/token")) {
    DBG(F("refresh: http.begin failed"));
    return false;
  }

  String postData = "grant_type=refresh_token";
  postData += "&refresh_token=" + urlEncode(String(twitch_refresh_token));
  postData += "&client_id="     + urlEncode(String(twitch_client_id));
  postData += "&client_secret=" + urlEncode(String(twitch_client_secret));

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(postData);
  if (code != HTTP_CODE_OK) {
    DBG(String(F("refresh: http code ")) + code);
    DBG(http.getString());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // WICHTIG: Größerer JSON-Puffer
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    DBG(String(F("refresh: json error ")) + err.c_str());
    DBG(payload);
    return false;
  }

  // Robustere Feldprüfung
  if (!doc.containsKey("access_token") || !doc.containsKey("expires_in")) {
    DBG(F("refresh: response incomplete (missing keys)"));
    DBG(payload);
    return false;
  }

  const char* newAccess  = doc["access_token"];     // required
  unsigned long expiresIn = doc["expires_in"];      // required
  const char* newRefresh = doc["refresh_token"];    // optional (Twitch kann neuen liefern)

  if (!newAccess || expiresIn == 0) {
    DBG(F("refresh: response incomplete (null/zero)"));
    DBG(payload);
    return false;
  }

  twitch_access_token = String(newAccess);

  if (newRefresh && strlen(newRefresh) > 0) {
    DBG(F("refresh: server returned new refresh_token (not persisted)"));
    // Optional: hier persistieren (SPIFFS/EEPROM), wenn gewünscht
  }

  time_t now = nowEpoch();
  if (now != 0) tokenExpiryEpoch = now + expiresIn;

  DBG(F("refresh: ok"));
  return true;
}

// ------------------ OAuth: Gültigkeit sicherstellen ------------------
static bool ensureValidToken() {
  // Wenn wir keine echte Zeit haben (NTP noch nicht da), validieren wir per API
  if (twitch_access_token.length() == 0) {
    return refreshAccessToken();
  }

  time_t now = nowEpoch();
  if (now == 0) {
    // keine echte Zeit → sporadisch validieren statt rechnen
    unsigned long dummy;
    if (!validateAccessToken(&dummy)) {
      return refreshAccessToken();
    }
    return true;
  }

  if (tokenExpiryEpoch == 0 || tokenExpiryEpoch <= now || (tokenExpiryEpoch - now) <= TOKEN_REFRESH_SAFETY) {
    return refreshAccessToken();
  }

  return true;
}

// ------------------ IRC / Twitch ------------------
static bool isAuthError(const String& line) {
  if (line.indexOf("Login authentication failed") >= 0) return true;
  if (line.indexOf("Improperly formatted auth") >= 0) return true;
  if (line.indexOf("Error logging in") >= 0) return true;
  return false;
}

static void sendIRC(const String& s) {
  ircClient.print(s);
  ircClient.print("\r\n");
}

static void connectToTwitch() {
  DBG(F("IRC: connect..."));
  ircClient.stop();
  yieldBrief();

  if (!ensureValidToken()) {
    DBG(F("IRC: no valid token -> retry later"));
    delay(RECONNECT_DELAY_MS);
    return;
  }

  if (!ircClient.connect(TWITCH_SERVER, TWITCH_PORT)) {
    DBG(F("IRC: connect failed"));
    delay(RECONNECT_DELAY_MS);
    return;
  }

  // PASS/NICK/CAP/JOIN
  ircClient.print(F("PASS oauth:"));
  ircClient.print(twitch_access_token);
  ircClient.print("\r\n");

  ircClient.print(F("NICK "));
  ircClient.print(twitch_username);
  ircClient.print("\r\n");

  sendIRC(F("CAP REQ :twitch.tv/tags twitch.tv/commands"));

  ircClient.print(F("JOIN "));
  ircClient.print(twitch_channel);
  ircClient.print("\r\n");

  DBG(F("IRC: connected + joined"));
}

static void handlePing(const String& line) {
  if (line.startsWith("PING")) {
    sendIRC(F("PONG :tmi.twitch.tv"));
    DBG(F("IRC: PING->PONG"));
  }
}

// ------------------ Alarm ------------------
static void startAlarm() {
  digitalWrite(RELAY_PIN, HIGH);
  alarmActive = true;
  alarmStart = millis();

  // Chat-Feedback (optional)
  ircClient.print(F("PRIVMSG "));
  ircClient.print(twitch_channel);
  ircClient.print(F(" :ehajoAlarm ehajoAlarm ehajoAlarm \r\n"));
}

static void handleAlarm() {
  if (!alarmActive) return;

  unsigned long now = millis();
  if (now - alarmStart >= ALARM_DURATION_MS) {
    digitalWrite(RELAY_PIN, LOW);
    alarmActive = false;
    DBG(F("Alarm: off"));
  }
  // Sicherheitsnetz (doppelte Zeit)
  if (now - alarmStart > (ALARM_DURATION_MS * 2UL)) {
    digitalWrite(RELAY_PIN, LOW);
    alarmActive = false;
    DBG(F("Alarm: safety off"));
  }
}

// ------------------ Reward Parsing ------------------
static void processRewardMessage(const String& line) {
  int idx = line.indexOf("custom-reward-id=");
  if (idx < 0) return;

  int start = idx + strlen("custom-reward-id=");
  int end = line.indexOf(";", start);
  if (end < 0) end = line.length();
  String rid = line.substring(start, end);

  if (rid != CUSTOM_REWARD_ID) {
    DBG(String(F("Reward: other id: ")) + rid);
    return;
  }

  // Optional: Benutzertext extrahieren
  int msgStart = line.indexOf(" :", line.indexOf("PRIVMSG"));
  if (msgStart >= 0) {
    String userText = line.substring(msgStart + 2);
    userText.trim();
    if (userText.length()) DBG(String(F("Reward text: ")) + userText);
  }

  DBG(F("Reward: match -> alarm"));
  startAlarm();
}

// ------------------ Setup / Loop ------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  DBG("\nBooting...");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifi_password);
  DBG(F("WiFi: connecting..."));
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000UL) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    DBG(F("WiFi: failed -> reboot"));
    delay(2000);
    ESP.restart();
  }
  DBG(String(F("WiFi: ok, IP "))+WiFi.localIP().toString());

  // NTP (empfohlen)
  configTime(0, 0, NTP1, NTP2);
  if (waitForNTP(8000)) {
    DBG(F("NTP: time ok"));
  } else {
    DBG(F("NTP: timeout (weiter ohne echte Zeit)"));
  }

#ifdef ENABLE_OTA
  ArduinoOTA.onStart([](){ DBG(F("OTA: start")); });
  ArduinoOTA.onEnd([](){ DBG(F("OTA: end")); });
  ArduinoOTA.onError([](ota_error_t e){ DBG(String(F("OTA: err "))+e); });
  ArduinoOTA.begin();
#endif

  // Erstes Token sicherstellen + IRC verbinden
  if (!ensureValidToken()) {
    DBG(F("Token: unable -> reboot in 10s"));
    delay(10000);
    ESP.restart();
  }
  connectToTwitch();
}

void loop() {
#ifdef ENABLE_OTA
  if (millis() - lastOtaHandle >= OTA_HANDLE_INTERVAL_MS) {
    ArduinoOTA.handle();
    lastOtaHandle = millis();
  }
#endif

  handleAlarm();

  // Regelmäßig Token prüfen/erneuern (vor Ablauf)
  if (millis() - lastTokenCheck >= TOKEN_CHECK_INTERVAL_MS) {
    ensureValidToken();
    lastTokenCheck = millis();
  }

  // IRC-Verbindung prüfen
  if (!ircClient.connected()) {
    DBG(F("IRC: reconnect..."));
    connectToTwitch();
    delay(RECONNECT_DELAY_MS);
    return;
  }

  // IRC-Linien lesen
  while (ircClient.available()) {
    String line = ircClient.readStringUntil('\n');
    line.trim();
    if (!line.length()) break;

    DBG("RAW: " + line);
    handlePing(line);

    if (isAuthError(line)) {
      DBG(F("IRC: auth error -> refresh+reconnect"));
      if (refreshAccessToken()) {
        connectToTwitch();
      } else {
        DBG(F("refresh failed, wait..."));
        delay(15000);
      }
      return;
    }

    if (!alarmActive && line.indexOf("PRIVMSG") >= 0 && line.indexOf("custom-reward-id=") >= 0) {
      processRewardMessage(line);
    }
    yieldBrief();
  }
}
