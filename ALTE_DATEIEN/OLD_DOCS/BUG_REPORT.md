# BUG_REPORT.md — sunray-core

Erstellt: 2026-03-23
Analysiert: Robot.cpp, SerialRobotDriver.cpp, WebSocketServer.cpp,
            useMowPath.ts, MapEditor.vue

---

## BUG-001 — Null-Pointer-Dereferenz in checkBattery()

DATEI: `core/Robot.cpp`
ZEILE: 345, 348
BUG: `opMgr_.activeOp()` kann `nullptr` zurückgeben (z.B. direkt nach `init()`
bevor der erste Op gesetzt ist, oder in Edge-Cases des OpManagers). In
`activeOpName()` (Z. 275) wird `nullptr` explizit abgefangen — in
`checkBattery()` jedoch nicht. Bei Critical-Battery-Event wird direkt
`opMgr_.activeOp()->onBatteryUndervoltage(ctx)` aufgerufen → Crash.
IMPACT: **hoch**
FIX:
```cpp
Op* op = opMgr_.activeOp();
if (op) op->onBatteryUndervoltage(ctx);
```

---

## BUG-002 — Buzzer wird bei kritischer Batterie AUSGESCHALTET statt eingeschaltet

DATEI: `core/Robot.cpp`
ZEILE: 342
BUG: `hw_->setBuzzer(false)` bei kritischer Batterie schaltet den Buzzer aus.
Der Benutzer wird damit nicht akustisch gewarnt. Der Aufruf sollte `true` sein
(Alarm auslösen). Identische Signatur wie alle anderen `setBuzzer`-Aufrufe
deutet auf Copy-Paste-Fehler hin.
IMPACT: **hoch** (Safety-Feature komplett inaktiv)
FIX:
```cpp
hw_->setBuzzer(true);   // Alarm bei kritischer Batterie
```

---

## BUG-003 — rawTicks werden über `fieldInt()` (int) gespeichert — Overflow bei langen Mähsessions

DATEI: `hal/SerialRobotDriver/SerialRobotDriver.cpp`
ZEILE: 329–331
BUG: `fieldInt()` gibt `int` (signed 32-bit) zurück. Die STM32-Tick-Counter
sind `uint32` im Alfred-Firmware und können Werte > 2 147 483 647 erreichen
(INT_MAX). Ab diesem Wert wirft `std::stoi()` `std::out_of_range` → der
catch-Block ignoriert das Frame still, die Ticks werden auf 0 zurückgesetzt.
BUG-05 ("long cast") im Subtraktionsschritt löst das Problem nur halb: der
Overflow tritt bereits beim *Einlesen* auf.
Formel: 120 Ticks/m × 1 m/s × 2 147 483 647 Ticks ÷ 50 Hz ≈ 357 083 Minuten
(~248 Tage Dauerbetrieb), aber bei Testläufen mit schnellen Motoren früher.
IMPACT: **mittel**
FIX:
```cpp
static unsigned long fieldULong(const std::string& s) {
    return std::stoul(s);  // unsigned long, kein Overflow bei uint32
}
// Ersetze fieldInt() durch fieldULong() in parseMotorFrame() Z. 329–331
```

---

## BUG-004 — CRC-Algorithmus im Code (Summe) stimmt nicht mit Dokumentation (XOR) überein

DATEI: `hal/SerialRobotDriver/SerialRobotDriver.cpp`
ZEILE: 246–250, 280–283
BUG: `calcCrc()` und `verifyCrc()` verwenden **Byte-Addition** (nicht XOR).
`module_index.md` und der Header-Kommentar in `SerialRobotDriver.h` beschreiben
"CRC: XOR". Falls die Alfred-STM32-Firmware tatsächlich XOR verwendet, schlägt
jede CRC-Verifizierung fehl → alle eingehenden Frames werden mit "CRC error"
verworfen; der Robot sieht einen dauerhaft nicht verbundenen MCU.
IMPACT: **kritisch** (wenn Alfred-Firmware XOR nutzt — auf Pi-Hardware zu verifizieren)
FIX: Entweder beide Seiten auf XOR umstellen:
```cpp
uint8_t crc = 0;
for (unsigned char c : s) crc ^= c;  // XOR statt +=
```
oder die Dokumentation korrigieren wenn beide tatsächlich Summe verwenden.

---

## BUG-005 — `broadcastNmea()` ruft `send_text()` aus Robot-Thread auf — Data Race mit serverThread_

DATEI: `core/WebSocketServer.cpp`
ZEILE: 603–621 (broadcastNmea), 579–582 (serverThread_ Push-Loop)
BUG: `broadcastNmea()` wird von `Robot::run()` (Main-Thread) aufgerufen und
ruft `c->send_text(msg)` direkt auf den WebSocket-Connections auf. Gleichzeitig
ruft der `serverThread_` ebenfalls `c->send_text()` auf denselben Connections
(Z. 579–582). Crow v1.2 garantiert kein Thread-Safety für gleichzeitige
`send_text()`-Aufrufe auf derselben Connection aus verschiedenen Threads.
Resultat: korrupte WebSocket-Frames oder Heap-Korruption (Undefined Behavior).
IMPACT: **hoch** (sporadische Crashes im Betrieb)
FIX: NMEA-Nachrichten in eine `std::queue<std::string>` mit Mutex einspeisen;
der `serverThread_` leert die Queue und sendet alle ausstehenden Nachrichten im
selben Loop-Tick wie die Telemetrie.

---

## BUG-006 — `gpsFixAge_ms` hardcoded 9 999 999 ms — GPS-Timeout-Logik kaputt

DATEI: `core/Robot.cpp`
ZEILE: 135, 228
BUG: `OpContext.gpsFixAge_ms` wird in `run()` und `makeCommandCtx()` immer auf
9 999 999 ms (~2,78 Stunden) gesetzt. `GpsWaitFixOp` prüft intern die Fix-Age,
um nach 2 Minuten in `ErrorOp` zu wechseln. Mit dem festen Wert sieht der Op
immer eine "sehr alte" Fix-Age — abhängig von der konkreten Timeout-Bedingung
in `GpsWaitFixOp` kann das entweder sofortigen Timeout (wenn `> Schwellwert`)
oder nie-Timeout (wenn fix-age-unabhängig geprüft) verursachen.
IMPACT: **mittel** (GPS-Recovery-Logik unzuverlässig bis Phase 2 fix)
FIX: Echte Fix-Age aus `lastGps_.dgpsAge_ms` ableiten oder alternativ eine
eigene Zeitstempel-Differenz berechnen (`now_ms_ - gpsLastFixTime_ms_`).

---

## BUG-007 — Exception-Message unsanitized im JSON-Error-Body (POST /api/map/geojson)

DATEI: `core/WebSocketServer.cpp`
ZEILE: 481–483
BUG: `e.what()` wird per roher String-Konkatenation in einen JSON-String
eingefügt. Enthält `e.what()` ein `"` oder `\`, ist die Response kein valides
JSON. Beispiel: nlohmann/json parse-Error `"unexpected '\"' at position 5"` →
der Response-Body wird zu `{"error":"unexpected '"' at position 5"}` — invalid
JSON, löst im Client einen weiteren Parse-Error aus.
IMPACT: **niedrig** (nur im Fehlerfall, keine Security-Implikation lokal)
FIX:
```cpp
nlohmann::json err; err["error"] = std::string(e.what());
return crow::response(400, err.dump());
```

---

## BUG-008 — routeAroundExclusions() behandelt nur vertikale Umgehungen

DATEI: `webui/src/composables/useMowPath.ts`
ZEILE: 519–531
BUG: Die Bypass-Punkte werden nur oberhalb/unterhalb (`yMax + margin`,
`yMin - margin`) der Exclusion-Bounding-Box berechnet. Liegt eine Exclusion
horizontal neben dem Transit-Segment (z.B. Blumenbeet links/rechts zwischen
zwei Mähstreifen), sind weder `above` noch `below` gültige Bypass-Punkte (sie
liegen außerhalb des Mähbereichs). Die Funktion gibt `[]` zurück → `nextStart`
wird ohne Umweg angesteuert → der Robot fährt **direkt durch die No-Go-Zone**.
IMPACT: **hoch** (No-Go-Zone wird während Bahnen-Transit überfahren)
FIX: Zusätzlich linke/rechte Bypass-Punkte testen:
```typescript
const left:  Pt = [Math.min(from[0], to[0]) - margin, midY]
const right: Pt = [Math.max(from[0], to[0]) + margin, midY]
if (inMowArea(left)  && segOk(from, left)  && segOk(left,  to)) return [left]
if (inMowArea(right) && segOk(from, right) && segOk(right, to)) return [right]
```

---

## BUG-009 — K-Turn: Overshoot-Berechnung verwendet `stripLen * 2` statt `segLen * 0.4`

DATEI: `webui/src/composables/useMowPath.ts`
ZEILE: 682–684
BUG: Der Kommentar beschreibt "exakte Rekonstruktion aus generateStrips
(safeInset = min(inset, segLen * 0.4))". `generateStrips` berechnet:
```typescript
const safeInset = Math.min(inset, segLen * 0.4)   // segLen = clipped strip length
```
`kTurnTransition` berechnet:
```typescript
const stripSafeInset = Math.min(inset, stripLen * 2)  // stripLen ≠ segLen
```
`stripLen` (Länge nach Inset) ist kleiner als `segLen`. `stripLen * 2` ist ein
viel größerer Wert als `segLen * 0.4` bei kurzen Streifen. Bei kurzen
Rand-Streifen an Polygon-Ecken (wo `safeInset = segLen * 0.4 << inset`) erzeugt
der K-Turn einen zu großen Overshoot → Wendemanöver verlässt den Mähbereich.
IMPACT: **mittel** (falsche K-Turns an konkaven Polygon-Ecken)
FIX: Overshoot konsistent aus `inset` + Strip-Längen-Anteil berechnen, oder
`safeInset = Math.min(inset, (stripLen / (1 - 0.8)) * 0.4)` zur Rückrechnung.

---

## BUG-010 — Double-Click fügt Spurious-Vertex ein bevor closePolygon() aufgerufen wird

DATEI: `webui/src/views/MapEditor.vue`
ZEILE: 654–671
BUG: Ein Doppelklick feuert `canvasClick` zweimal **bevor** `canvasDblClick`
ausgelöst wird. Beim Polygon-Zeichnen (perimeter/exclusion/zone) fügt der zweite
`canvasClick` einen Punkt am Doppelklick-Ort in `drawingPts` ein. Danach ruft
`canvasDblClick` → `closePolygon()` auf. Das fertige Polygon enthält damit einen
doppelten Endpunkt an der Schließ-Position — das erzeugt eine Selbst-Überschneidung
oder "Spike" im Polygon, was alle Clipping-Algorithmen korrumpiert.
IMPACT: **hoch** (jedes per Doppelklick geschlossene Polygon ist defekt)
FIX: In `canvasClick` am Anfang prüfen:
```typescript
if (e.detail >= 2) return   // ignore clicks that are part of a dblclick
```

---

## BUG-011 — fitView() crasht mit "Maximum call stack exceeded" bei großen Mähpfaden

DATEI: `webui/src/views/MapEditor.vue`
ZEILE: 421–422
BUG: `Math.min(...xs)` und `Math.max(...xs)` nutzen Spread-Operator um alle
Punkte als Funktionsargumente zu übergeben. JS-Engines limitieren Argument-Count
(V8: ~65 536, SpiderMonkey: ~500 000 — aber in der Praxis oft weniger). Ein
Mähpfad mit >10 000 Waypoints (realistisch für große Gärten) führt zum Crash
von `fitView()`. Der Stack-Fehler ist unkritisch für die Karte selbst, aber
`fitView()` beim Laden schlägt komplett fehl → Map-View nicht zentriert.
IMPACT: **niedrig** (nur fitView, keine Datenverlust)
FIX:
```typescript
const minX = xs.reduce((a, b) => a < b ? a : b, Infinity)
const maxX = xs.reduce((a, b) => a > b ? a : b, -Infinity)
```

---

## BUG-012 — Zone.settings.pattern ('stripe'/'spiral') wird in computePreview() komplett ignoriert

DATEI: `webui/src/views/MapEditor.vue`
ZEILE: 545–550
BUG: `computePreview()` übergibt nur `stripWidth` aus den Zone-Einstellungen an
`computeMowPath()`. Das Feld `pattern` ('stripe' vs 'spiral') aus
`zone.settings.pattern` hat keine Entsprechung in `MowPathSettings` und wird
nie verwendet. Zonen mit `pattern: 'spiral'` erhalten trotzdem Streifen-Bahnen.
Das UI-Dropdown für "Muster" ist funktionslos.
IMPACT: **mittel** (UI-Feature ohne Wirkung, Benutzer wird irregeführt)
FIX: `computeMowPath()` um `pattern`-Parameter erweitern und Spiral-Modus
implementieren, oder das Pattern-Dropdown aus dem UI entfernen bis das Feature
implementiert ist.

---

## Zusammenfassung

| ID      | Datei                        | Zeile     | Impact   |
|---------|------------------------------|-----------|----------|
| BUG-001 | core/Robot.cpp               | 345, 348  | hoch     |
| BUG-002 | core/Robot.cpp               | 342       | hoch     |
| BUG-003 | SerialRobotDriver.cpp        | 329–331   | mittel   |
| BUG-004 | SerialRobotDriver.cpp        | 246–250   | kritisch |
| BUG-005 | core/WebSocketServer.cpp     | 603–621   | hoch     |
| BUG-006 | core/Robot.cpp               | 135, 228  | mittel   |
| BUG-007 | core/WebSocketServer.cpp     | 481–483   | niedrig  |
| BUG-008 | useMowPath.ts                | 519–531   | hoch     |
| BUG-009 | useMowPath.ts                | 682–684   | mittel   |
| BUG-010 | MapEditor.vue                | 654–671   | hoch     |
| BUG-011 | MapEditor.vue                | 421–422   | niedrig  |
| BUG-012 | MapEditor.vue                | 545–550   | mittel   |
