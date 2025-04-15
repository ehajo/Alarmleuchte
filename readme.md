# Alarmleuchte – Twitch-gesteuerte Alarmsteuerung mit ESP8266

Dieses Projekt realisiert eine Alarmleuchte, die über einen ESP8266 angesteuert wird. Der ESP8266 liest den Twitch-Chat (über IRC mit OAuth) aus und reagiert auf den Befehl `!alarm` – allerdings nur von berechtigten Benutzern (Moderatoren und Abonnenten) und nur einmal pro Minute. Zusätzlich wird eine Nachricht im Chat versendet, wenn das System online ist, und OTA-Updates (Over-The-Air) sind integriert.

## Features

- **Twitch-Integration:** Liest den Twitch-Chat über das IRC-Protokoll mit OAuth aus und wertet die Chat-Nachrichten aus.
- **Berechtigungsprüfung:** Führt den `!alarm`-Befehl nur aus, wenn der Absender als Moderator oder Subscriber gekennzeichnet ist.
- **Nicht-blockierender Alarm:** Das Relais wird 10 Sekunden lang aktiviert, ohne dass der ESP8266 den Chat blockiert.
- **Befehlsrate begrenzt:** Der `!alarm`-Befehl wird nur einmal pro Minute akzeptiert.
- **OTA-Updates:** Mit Hilfe der ArduinoOTA-Bibliothek kannst du Firmware-Updates drahtlos durchführen.

## Voraussetzungen

- **Hardware:**
  - ESP8266-Modul (z. B. NodeMCU oder Wemos D1 Mini)
  - Relais (angeschlossen an z. B. Pin IO16)
  - (optional) Alarmleuchte

- **Software:**
  - [Arduino IDE](https://www.arduino.cc/en/software) oder [PlatformIO](https://platformio.org/)
  - ESP8266 Arduino Core (über den Boardverwalter in der Arduino IDE)
  - ArduinoOTA-Bibliothek (bereits im ESP8266 Core enthalten)

## Hardware-Aufbau

Verbinde das Relais mit dem ESP8266, wobei der Steuerpin an **IO16** angeschlossen wird. Stelle sicher, dass du die korrekten Spannungen und gegebenenfalls ein geeignetes Schaltrelais verwendest.

## Software-Konfiguration

1. **Konfiguration der Zugangsdaten:**
   Erstelle eine Datei `config.h` (siehe [config.h.template](#config-h-template)) und trage dort deine WLAN- und Twitch-Zugangsdaten ein.
   
   Beispielinhalt für `config.h`:
   ```cpp
   #ifndef CONFIG_H
   #define CONFIG_H

   // WLAN-Zugangsdaten
   const char* ssid           = "DEIN_WIFI_SSID";
   const char* wifi_password  = "DEIN_WIFI_PASSWORT";

   // Twitch-Zugangsdaten (mit OAuth)
   const char* twitch_username = "DEIN_TWITCH_USERNAME";  // Dein Twitch-Benutzername
   const char* twitch_oauth    = "oauth:DEIN_OAUTH_TOKEN";   // z. B. "oauth:abcdef1234567890"
   const char* twitch_channel  = "#DEIN_CHANNEL";           // Twitch Channel (mit führendem "#")

   #endif // CONFIG_H
   ```

2. **OTA-Passwort (optional):**
   Falls du den OTA-Zugang sichern möchtest, kannst du in deinem `setup()`-Bereich vor `ArduinoOTA.begin();` folgenden Code einfügen:
   ```cpp
   ArduinoOTA.setPassword("mein_heimliches_passwort");
   ```

## Upload und OTA

- **Direkter Upload:**
  Schließe deinen ESP8266 per USB an und lade den Sketch über die Arduino IDE hoch.

- **OTA-Upload:**
  Nachdem du den OTA-fähigen Sketch erstmals per USB hochgeladen hast, kannst du Firmware-Updates auch über das Netzwerk durchführen. Stelle dazu sicher, dass dein ESP8266 im selben Netzwerk wie dein Computer ist. In der Arduino IDE wird der ESP8266 dann als Netzwerkport angezeigt.

## Nutzung

- Sobald der ESP8266 mit Twitch verbunden ist, sendet er automatisch die Chat-Nachricht: **"Alarm online 🚨🚨🚨"**.
- Nutzer können im Twitch-Chat den Befehl `!alarm` eingeben.
- Der Befehl wird nur akzeptiert, wenn:
  - Der Absender als Moderator oder Subscriber gekennzeichnet ist.
  - Seit dem letzten ausgeführten Alarm mindestens 60 Sekunden vergangen sind.
- Das Relais (und damit die Alarmleuchte) wird 10 Sekunden lang aktiviert.

## Troubleshooting

- **Upload-Fehler:**
  Stelle sicher, dass dein ESP8266 im Flash-Modus ist (GPIO0 auf GND) und dass du den korrekten COM- oder Netzwerkport ausgewählt hast.

- **IRC-Verbindung:**
  Überprüfe deine Twitch-Zugangsdaten und sorge dafür, dass dein OAuth-Token gültig ist.

## Lizenz

Dieses Projekt ist unter der MIT-Lizenz veröffentlicht.

## Kontakt

Bei Fragen oder Verbesserungsvorschlägen kannst du gerne ein Issue erstellen oder einen Pull Request einreichen.
