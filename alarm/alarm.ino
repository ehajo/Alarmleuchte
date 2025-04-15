#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "config.h"  // Enthält deine sensiblen Zugangsdaten

#define RELAY_PIN 16  // IO16: Relais-Steuerung

const char* twitch_server = "irc.chat.twitch.tv";
const int   twitch_port   = 6667;

WiFiClient client;

// Variablen für den nicht-blockierenden Alarm
bool alarmActive = false;         
unsigned long alarmStartTime = 0; 
unsigned long lastAlarmTime = 0;

void setup() {
  Serial.begin(115200);

  // Relais initialisieren (ausgeschaltet)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Mit WLAN verbinden
  Serial.print("Verbinde mit WLAN...");
  WiFi.begin(ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWLAN verbunden!");

  // Einrichtung von OTA
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update startet...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update abgeschlossen!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Update: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler[%u]: ", error);
    if (error == OTA_AUTH_ERROR)    Serial.println("Auth Fehler");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Fehler");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Fehler");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Fehler");
    else if (error == OTA_END_ERROR)     Serial.println("End Fehler");
  });
  ArduinoOTA.setPassword("1234");

  ArduinoOTA.begin();

  // Verbindung zum Twitch IRC-Server herstellen
  Serial.print("Verbinde zu Twitch IRC...");
  if (client.connect(twitch_server, twitch_port)) {
    Serial.println("verbunden!");
    client.println("PASS " + String(twitch_oauth));
    client.println("NICK " + String(twitch_username));
    client.println("CAP REQ :twitch.tv/tags");
    client.println("JOIN " + String(twitch_channel));

    // Sende Chat-Nachricht, dass der Alarm online ist
    client.println("PRIVMSG " + String(twitch_channel) + " :Alarm online 🚨🚨🚨");
    Serial.println("Chat-Nachricht 'Alarm online 🚨🚨🚨' versendet.");

    // Erlaube, dass der erste !alarm-Befehl direkt ausgeführt werden kann
    lastAlarmTime = millis() - 60000UL;
  } else {
    Serial.println("Verbindung zu Twitch IRC fehlgeschlagen!");
  }
}

void loop() {
  // OTA-Updates verarbeiten
  ArduinoOTA.handle();

  // OTA sollte Vorrang haben, aber hier verarbeiten wir dennoch die Twitch-Nachrichten
  if (alarmActive) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (line.startsWith("PING")) {
        client.println("PONG :tmi.twitch.tv");
        Serial.println("PING beantwortet (während Alarm).");
      } else {
        Serial.println("Nachricht während Alarm verworfen: " + line);
      }
    }
    if (millis() - alarmStartTime >= 10000UL) {  // 10 Sekunden
      digitalWrite(RELAY_PIN, LOW);
      alarmActive = false;
      Serial.println("Alarm beendet; Relais ausgeschaltet.");
    }
  } else {
    if (client.connected() && client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      Serial.println(line);

      if (line.startsWith("PING")) {
        client.println("PONG :tmi.twitch.tv");
        Serial.println("PING beantwortet.");
      }
      else if (line.indexOf("PRIVMSG") != -1 && line.indexOf("!alarm") != -1) {
        if (millis() - lastAlarmTime >= 60000UL) {
          if (line.startsWith("@")) {
            int badgesIndex = line.indexOf("badges=");
            if (badgesIndex != -1) {
              int endIndex = line.indexOf(";", badgesIndex);
              String badges = (endIndex != -1)
                              ? line.substring(badgesIndex, endIndex)
                              : line.substring(badgesIndex);
              if (badges.indexOf("moderator") != -1 || badges.indexOf("subscriber") != -1) {
                Serial.println("Befehl !alarm von berechtigtem Nutzer erkannt. Relais wird aktiviert.");
                digitalWrite(RELAY_PIN, HIGH);
                alarmStartTime = millis();
                lastAlarmTime  = millis();
                alarmActive    = true;
              } else {
                Serial.println("Befehl !alarm ignoriert: Absender ist weder Moderator noch Subscriber.");
              }
            } else {
              Serial.println("Befehl !alarm ignoriert: Keine Badge-Informationen vorhanden.");
            }
          } else {
            Serial.println("Befehl !alarm ignoriert: Nachricht enthält keine Tag-Informationen.");
          }
        } else {
          Serial.println("Befehl !alarm ignoriert: Bereits ein Alarm in der letzten Minute verarbeitet.");
        }
      }
    }
  }
}
