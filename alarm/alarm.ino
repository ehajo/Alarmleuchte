#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "config.h"  // Enthält WLAN- und Twitch-Zugangsdaten

#define RELAY_PIN 14  // IO14: Relais-Steuerung
// #define DEBUG         // Aktiviere Debug-Ausgaben
#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
#endif

// Twitch IRC-Server-Daten
const char* twitch_server = "irc.chat.twitch.tv";
const int twitch_port = 6667;

// Custom Reward ID
const char* customRewardId = "123123123";  // Beispiel: "abc123def456"

// Zeitkonstanten
const unsigned long ALARM_DURATION_MS = 10000UL;
const unsigned long OTA_CHECK_INTERVAL_MS = 1000UL;

// WiFi-Client
WiFiClient client;

// Variablen für den nicht-blockierenden Alarm
bool alarmActive = false;
unsigned long alarmStartTime = 0;

void setup() {
  Serial.begin(115200);

  // Relais initialisieren (ausgeschaltet)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Mit WLAN verbinden
  DEBUG_PRINT(F("Verbinde mit WLAN..."));
  unsigned long wifiTimeout = millis();
  WiFi.begin(ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED && millis() - wifiTimeout < 30000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINT(F("WLAN-Verbindung fehlgeschlagen! Neustart..."));
    ESP.restart();
  }
  DEBUG_PRINT(F("\nWLAN verbunden!"));

  // Einrichtung von OTA
  ArduinoOTA.onStart([]() { DEBUG_PRINT(F("OTA Update startet...")); });
  ArduinoOTA.onEnd([]() { DEBUG_PRINT(F("\nOTA Update abgeschlossen!")); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Update: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DEBUG_PRINT(F("Auth Fehler"));
    else if (error == OTA_BEGIN_ERROR) DEBUG_PRINT(F("Begin Fehler"));
    else if (error == OTA_CONNECT_ERROR) DEBUG_PRINT(F("Connect Fehler"));
    else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINT(F("Receive Fehler"));
    else if (error == OTA_END_ERROR) DEBUG_PRINT(F("End Fehler"));
  });
  ArduinoOTA.setPassword("mein_heimliches_passwort"); // Sicherheit
  ArduinoOTA.begin();

  // Verbindung zum Twitch IRC-Server
  connectToTwitch();
}

void connectToTwitch() {
  DEBUG_PRINT(F("Verbinde zu Twitch IRC..."));
  client.stop(); // Vorherige Verbindung schließen
  if (client.connect(twitch_server, twitch_port)) {
    DEBUG_PRINT(F("verbunden!"));
    client.print(F("PASS "));
    client.println(twitch_oauth);
    client.print(F("NICK "));
    client.println(twitch_username);
    client.println(F("CAP REQ :twitch.tv/tags twitch.tv/commands"));
    client.print(F("JOIN "));
    client.println(twitch_channel);
  } else {
    DEBUG_PRINT(F("Verbindung zu Twitch IRC fehlgeschlagen!"));
  }
}

void handlePing(const String& line) {
  if (line.startsWith("PING")) {
    client.println(F("PONG :tmi.twitch.tv"));
    DEBUG_PRINT(F("PING beantwortet."));
  }
}

void handleAlarm() {
  if (alarmActive && millis() - alarmStartTime >= ALARM_DURATION_MS) {
    digitalWrite(RELAY_PIN, LOW);
    alarmActive = false;
    DEBUG_PRINT(F("Alarm beendet; Relais ausgeschaltet."));
  }
  // Sicherheitsabschaltung
  if (alarmActive && millis() - alarmStartTime > ALARM_DURATION_MS * 2) {
    digitalWrite(RELAY_PIN, LOW);
    alarmActive = false;
    DEBUG_PRINT(F("Sicherheitsabschaltung: Relais war zu lange aktiv!"));
  }
}

void processRewardMessage(const String& line) {
  // Prüfe auf Custom Reward
  int rewardIndex = line.indexOf("custom-reward-id=");
  if (rewardIndex == -1) {
    DEBUG_PRINT(F("Keine Custom Reward ID gefunden."));
    return;
  }

  // Extrahiere Reward-ID
  int rewardStart = rewardIndex + strlen("custom-reward-id=");
  int rewardEnd = line.indexOf(";", rewardStart);
  if (rewardEnd == -1) rewardEnd = line.length();
  String rewardId = line.substring(rewardStart, rewardEnd);
  if (!rewardId.equals(customRewardId)) {
    DEBUG_PRINT(F("Custom Reward ignoriert: Falsche Reward-ID: ") + rewardId);
    return;
  }

  // Extrahiere Benutzertext
  int textStart = line.indexOf(" :", line.indexOf("PRIVMSG"));
  String userText = "";
  if (textStart != -1) {
    userText = line.substring(textStart + 2); // Überspringe " :"
    userText.trim();
  }

  // Reward erkannt, Relais aktivieren
  DEBUG_PRINT(F("Custom Reward erkannt. Relais wird aktiviert."));
  if (userText.length() > 0) {
    DEBUG_PRINT("Benutzertext: " + userText);
  } else {
    DEBUG_PRINT(F("Kein Benutzertext angegeben."));
  }
  digitalWrite(RELAY_PIN, HIGH);
  client.print(F("PRIVMSG "));
  client.print(twitch_channel);
  client.println(F(" :ehajoAlarm ehajoAlarm ehajoAlarm "));
  alarmStartTime = millis();
  alarmActive = true;
}

void loop() {
  static unsigned long lastOtaCheck = 0;

  // OTA-Updates periodisch prüfen
  if (millis() - lastOtaCheck >= OTA_CHECK_INTERVAL_MS) {
    ArduinoOTA.handle();
    lastOtaCheck = millis();
  }

  // Alarmsteuerung
  handleAlarm();

  // Twitch-Verbindung prüfen
  if (!client.connected()) {
    connectToTwitch();
    delay(5000); // Verzögerung vor Wiederverbindung
    return;
  }

  // Nachrichten verarbeiten
  if (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    DEBUG_PRINT("Raw: " + line); // Rohdaten für Debugging

    handlePing(line);

    if (!alarmActive && line.indexOf("PRIVMSG") != -1 && line.indexOf("custom-reward-id=") != -1) {
      processRewardMessage(line);
    }
  }
}