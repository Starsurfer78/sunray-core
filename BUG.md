# Potenzielle Bugs in sunray-core

Dieses Dokument listet potenzielle Bugs und Probleme, die ich als Bug-Hunter im Code von sunray-core gefunden habe. Diese basieren auf statischer Code-Analyse und könnten zu Laufzeitfehlern, Speicherlecks, Race Conditions oder anderen Problemen führen.

## Kritische Bugs

### 1. Buffer-Overflow in `MqttClient::getHostname()` (MqttClient.cpp:175-181)
**Beschreibung:** Die Funktion verwendet ein Stack-Array von 256 Bytes für den Hostnamen, aber `gethostname()` garantiert keine Null-Terminierung, wenn der Hostname länger als 255 Bytes ist. Dies kann zu undefiniertem Verhalten führen, wenn `std::string(hostname)` aufgerufen wird.

**Code:**
```cpp
char hostname[256];
if (gethostname(hostname, sizeof(hostname)) == 0) {
    return std::string(hostname);  // Potenziell gefährlich
}
```

**Risiko:** Wenn der Hostname >= 256 Bytes ist, ist das Array nicht null-terminiert, was zu einem Absturz oder falschen Daten führen kann.

**Fix:** Verwende `std::string` mit Längenbegrenzung oder prüfe auf Null-Terminierung.

### 2. Fehlende Thread-Safety in `Config::get()` (Config.cpp)
**Beschreibung:** Die `get()`-Methode greift auf `data_` zu ohne Lock, obwohl `load()` und `save()` Mutexe verwenden. Dies kann zu Race Conditions führen, wenn ein Thread die Config lädt/speichert während ein anderer liest.

**Risiko:** Datenkonsistenz-Probleme in Multi-Threaded-Umgebungen.

**Fix:** Füge Locks zu allen Zugriffen auf `data_` hinzu.

### 3. Ressourcenlecks im Signal-Handler (main.cpp:32-35)
**Beschreibung:** Der Signal-Handler ruft nur `robot->stop()` auf, aber `wsServer` und `mqttClient` werden nicht gestoppt. Dies kann zu offenen Sockets, Threads und Speicherlecks führen.

**Code:**
```cpp
static void signalHandler(int) {
    if (g_robot) g_robot->stop();
}
```

**Risiko:** Ressourcenlecks bei abnormaler Terminierung.

**Fix:** Stoppe alle Komponenten im Signal-Handler oder verwende atexit().

## Mittelschwere Bugs

### 4. Stille Fehler in `Config::load()` (Config.cpp:218-235)
**Beschreibung:** Wenn die JSON-Konfigurationsdatei korrupt ist, wird sie still ignoriert und die Defaults verwendet. Der Benutzer erhält keine Warnung.

**Risiko:** Unerwartetes Verhalten ohne Feedback an den Benutzer.

**Fix:** Logge einen Fehler oder Exception werfen.

### 5. Fehlende Fehlerbehandlung in `resolveWebRoot()` (main.cpp:40-62)
**Beschreibung:** `read_symlink()` kann fehlschlagen, aber der Fehler wird nicht behandelt.

**Risiko:** Falscher Web-Root-Pfad.

**Fix:** Prüfe den Rückgabewert von `read_symlink()`.

### 6. Unbehandelte MQTT-Publish-Fehler (MqttClient.cpp)
**Beschreibung:** `mosquitto_publish()` wird in `publish()` und `publishDiscoveryEntity()` aufgerufen, aber der Rückgabewert wird nicht geprüft. Publish kann fehlschlagen.

**Risiko:** Stille Fehler bei MQTT-Kommunikation.

**Fix:** Prüfe Rückgabewerte und logge Fehler.

### 7. Potenzielle Race Condition in MQTT-Start (MqttClient.cpp:207-250)
**Beschreibung:** `mosquitto_connect_async()` kann fehlschlagen, aber `mosquitto_loop_start()` wird trotzdem aufgerufen.

**Risiko:** Ungültiger Zustand des MQTT-Clients.

**Fix:** Prüfe den Rückgabewert von `connect_async()`.

## Geringfügige Probleme

### 8. Unbehandelte unbekannte Commands (main.cpp)
**Beschreibung:** In WebSocket- und MQTT-Command-Handlern werden unbekannte Commands still ignoriert, ohne Feedback.

**Risiko:** Schwierige Fehlersuche.

**Fix:** Logge unbekannte Commands oder sende Fehlerantwort.

### 9. Fehlende Null-Checks in MQTT-Nachrichten (MqttClient.cpp:75-85)
**Beschreibung:** In `onMessageCb()` wird `msg->payload` verwendet, aber obwohl `msg` und `payload` geprüft werden, könnte `payloadlen` 0 sein.

**Risiko:** Leere Nachrichten werden verarbeitet.

**Fix:** Zusätzliche Validierung.

## Empfehlungen

1. **Code-Reviews:** Implementiere obligatorische Code-Reviews für Threading- und Speichercode.
2. **Unit-Tests:** Erhöhe Testabdeckung, besonders für Fehlerfälle.
3. **Logging:** Verbessere Fehlerlogging für bessere Diagnose.
4. **Static Analysis:** Verwende Tools wie Clang-Tidy oder Coverity für automatische Bug-Detection.

Dies ist keine exhaustive Liste – weitere Analyse mit spezialisierten Tools wird empfohlen.</content>
<parameter name="filePath">/mnt/LappiDaten/Projekte/sunray-core/BUG.md