# Alarmleuchte – Alarmsteuerung mit ESP8266 und Fernbedienung

Diese Version ist für zB meinen Messebetrieb gedacht. Ein Not-Aus-Taster mit ESP steuert über Funk die Funktion der Alarmleuchte an.

## Features

- **Not-Aus-Taster:** In meinem Fall habe ich einen Not-Aus-Taster zweckentfremdet, es funktioniert natürlich jeder Schalter

## Voraussetzungen

- **Hardware:**
  - 2x ESP8266-Modul (z. B. NodeMCU oder Wemos D1 Mini)
  - Relais (angeschlossen an z. B. Pin IO16)
  - Alarmleuchte
  - Taster in beliebiger Form (in meinem Fall Not-Aus zwecks "schaut cool aus")

- **Software:**
  - [Arduino IDE](https://www.arduino.cc/en/software) oder [PlatformIO](https://platformio.org/)
  - ESP8266 Arduino Core (über den Boardverwalter in der Arduino IDE)
  - ArduinoOTA-Bibliothek (bereits im ESP8266 Core enthalten)

## Hardware-Aufbau

Verbinde das Relais mit dem ESP8266, wobei der Steuerpin an **IO16** angeschlossen wird. Stelle sicher, dass du die korrekten Spannungen und gegebenenfalls ein geeignetes Schaltrelais verwendest.

## Software-Konfiguration

1. **MAC-Adresse**
   Verbinde die Alarmleuchte mit dem PC und lies die MAC-Adresse ab, die per UART ausgegeben wird. Diese muss in die Software des Ansteurungs-ESP eingetragen werden vor dem Compilieren.

## Lizenz

Dieses Projekt ist unter der MIT-Lizenz veröffentlicht.

## Kontakt

Bei Fragen oder Verbesserungsvorschlägen kannst du gerne ein Issue erstellen oder einen Pull Request einreichen.
