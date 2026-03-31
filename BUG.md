# Bug-Prüfung `sunray-core`

## Keine offenen Punkte mehr alles behoben! ###


Dieses Dokument hält nur noch die Punkte fest, die gegen den aktuellen Stand
des Repos geprüft wurden. Veraltete oder bereits behobene Hinweise sind hier
bewusst entfernt oder als "nicht zutreffend" eingeordnet.

## Behoben

### 1. `MqttClient::getHostname()` kann ohne garantierte Null-Terminierung lesen
Datei: [`core/MqttClient.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp)

Status: behoben

Beschreibung:
- `gethostname()` garantiert keine Null-Terminierung, wenn der Hostname den
  Puffer vollständig ausfüllt.
- Danach wird direkt `std::string(hostname)` gebaut.

Risiko:
- Undefiniertes Verhalten bei sehr langen Hostnamen.

Umsetzung:
- Hostname-Puffer wird initialisiert und das letzte Byte explizit auf `'\0'`
  gesetzt.

### 2. `Config::load()` verschluckt kaputte JSON-Konfiguration still
Datei: [`core/Config.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Config.cpp)

Status: behoben

Beschreibung:
- Bei Parse-Fehlern wird auf Defaults zurückgefallen.
- Es gibt dabei aktuell kein Warn- oder Error-Logging.

Risiko:
- Der Roboter läuft mit Defaults, ohne dass der Benutzer die defekte Datei
  bemerkt.

Umsetzung:
- Parse-Fehler werden jetzt auf `stderr` ausgegeben.

### 3. MQTT-Publish-Rückgabewerte werden nicht geprüft
Datei: [`core/MqttClient.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp)

Status: behoben

Beschreibung:
- `mosquitto_publish()` wird für Discovery und Laufzeitdaten aufgerufen.
- Rückgabewerte werden nicht ausgewertet.

Risiko:
- Publish-Fehler bleiben unsichtbar.

Umsetzung:
- Publish-Rückgabewerte werden geprüft und bei Fehlern geloggt.

### 4. Unbekannte WebSocket-/MQTT-Commands werden still ignoriert
Datei: [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)

Status: behoben

Beschreibung:
- In den Command-Handlern gibt es kein Fallback-Logging für unbekannte Befehle.

Risiko:
- Schwierige Fehlersuche bei Tippfehlern, veralteten Clients oder falschen
  Payloads.

Umsetzung:
- Unbekannte WebSocket-, MQTT- und Simulator-Commands werden warnend geloggt.

## Ebenfalls verbessert

### 5. `mosquitto_connect_async()`-Fehler führen nicht zu einem frühen Abbruch
Datei: [`core/MqttClient.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp)

Status: verbessert

Beschreibung:
- Wenn `mosquitto_connect_async()` fehlschlägt, wird gewarnt.
- Danach wird trotzdem `mosquitto_loop_start()` gestartet.

Einordnung:
- Das ist nicht zwingend ein akuter Bug.
- Für harte Konfigurationsfehler ist das Verhalten aber unsauber und könnte
  klarer gemacht werden.

Umsetzung:
- Bei einem direkten `connect_async()`-Fehler wird der MQTT-Start jetzt sauber
  abgebrochen und die Mosquitto-Ressourcen werden freigegeben.

## Geprüft und aktuell nicht zutreffend

### 6. Fehlende Thread-Safety in `Config::get()`
Datei: [`core/Config.h`](/mnt/LappiDaten/Projekte/sunray-core/core/Config.h)

Einordnung:
- Nicht zutreffend.
- `Config::get()` nimmt bereits einen `std::lock_guard<std::mutex>`.

### 7. Ressourcenleck im Signal-Handler durch `robot->stop()`
Datei: [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp), [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)

Einordnung:
- Nicht zutreffend.
- `robot->stop()` stößt den geordneten Shutdown an, indem nur das
  Lauf-Flag zurückgesetzt wird.
- Danach verlässt `Robot::loop()` sauber die Schleife, fährt Hardware herunter,
  und `main()` stoppt anschließend `mqttClient` und `wsServer`.

### 8. Fehlende Fehlerbehandlung in `resolveWebRoot()`
Datei: [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)

Einordnung:
- Nicht zutreffend.
- `read_symlink()` wird bereits über `std::error_code` geprüft.

### 9. Leere MQTT-Nachrichten würden ungeprüft verarbeitet
Datei: [`core/MqttClient.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp)

Einordnung:
- Nicht zutreffend.
- `msg`, `payload` und `payloadlen <= 0` werden bereits geprüft.
