=============================================================================
SUNRAY-CORE: MASTER BOOTSTRAP SETUP
=============================================================================

Ziel:
Erstelle automatisch die vollständige Projektstruktur, Prompt-Struktur,
Memory-Struktur und Analyse-/Task-Dateien für das Projekt:

E:\TRAE\sunray-core\

Die Dateien sollen direkt angelegt und vollständig mit Inhalt befüllt werden.
Alle Dateien sind Markdown-Dateien (.md), klar strukturiert, sofort nutzbar
und auf das Projekt sunray-core abgestimmt.

WICHTIG:
- Nicht nur leere Dateien erzeugen.
- Jede Datei vollständig ausformulieren.
- Bestehende Inhalte nur überschreiben, wenn die Datei noch nicht sinnvoll existiert.
- Falls Dateien bereits existieren, ergänze vorsichtig oder schlage Merge vor.
- Fokus: Alfred, sunray-core, RPi 4B + STM32F103, RTK, MQTT, Safety, Home Assistant.
- sunray-core ist das Zielsystem, keine GreenGuard-Migration.
- Alfred läuft bereits produktiv.
- Zuerst Discovery/Analyse, danach Bug-Suche, dann Verbesserungen/Future Features.
- Safety-kritische Aspekte immer explizit markieren.
- Unbekannte Hardware-Fakten niemals erfinden.
- Immer zwischen FACT / INFERENCE / UNKNOWN unterscheiden, wo relevant.

=============================================================================
ZU ERSTELLENDE DATEISTRUKTUR
=============================================================================

/mnt/LappiDaten/Projekte/sunray-core
│
├── TASK.md
├── SYSTEM_OVERVIEW.md
├── PROJECT_MAP.md
├── HARDWARE_MAP.md
├── SAFETY_MAP.md
├── BUILD_NOTES.md
├── BUG_REPORT.md
├── IMPROVEMENT_BACKLOG.md
├── FUTURE_FEATURES.md
│
├── .claude-prompts\
│   ├── firmware-safety.md
│   ├── rtk-integration.md
│   ├── mqtt-protocol.md
│   ├── bug-hunting.md
│   ├── ha-integration.md
│   ├── repo-scanner.md
│   ├── hardware-mapper.md
│   ├── safety-auditor.md
│   ├── architect-future.md
│   └── verifier.md
│
└── .memory\
    ├── session-notes.md
    ├── rtk-context.md
    ├── hardware-pinout.md
    ├── mqtt-topics.md
    ├── cost-optimization.md
    ├── known-issues.md
    ├── runtime-knowledge.md
    ├── architecture-decisions.md
    └── safety-findings.md

=============================================================================
ALLGEMEINE FORMAT-REGELN
=============================================================================

Für alle Dateien:
- Markdown (.md)
- Klare H1 / H2 / H3 Struktur
- Scannable, prägnant, aber vollständig
- Codex-/Agent-ready
- Keine Fülltexte
- Keine generischen Aussagen ohne Nutzen
- Tabellen dort, wo sie technisch sinnvoll sind
- Safety-relevante Bereiche klar hervorheben
- Projektbezug immer auf sunray-core und Alfred ausrichten

Für Prompt-Dateien in .claude-prompts:
Verwende möglichst dieses Schema:
1. Titel
2. Role / Identity
3. Mission
4. Scope
5. Rules
6. Constraints
7. Required Inputs
8. Expected Output Format
9. Failure Conditions / Escalation

Für Memory-Dateien in .memory:
Verwende möglichst dieses Schema:
1. Titel
2. Zweck
3. Aktueller Stand
4. Strukturierte Listen / Tabellen
5. Known Unknowns
6. Update Rules

=============================================================================
PROJEKTKONTEXT
=============================================================================

sunray-core ist ein neues Betriebssystem bzw. ein neuer System-Stack
für eine Alfred/Sunray Robotermäher-Plattform.

Rahmenbedingungen:
- Alfred läuft bereits produktiv mit sunray-core
- Plattform: Raspberry Pi 4B + STM32F103
- Tooling: PlatformIO, C++17
- Themen: RTK GPS, MQTT, Home Assistant Integration, Hardware Mapping, Safety
- Es gibt weiterhin unbekannte Hardware-Bindings
- Port-Expander ist möglich, aber nicht vollständig belegt
- Pi ↔ STM32 Kommunikation ist relevant
- Motor Enable / Stop / Watchdog / Failover sind sicherheitskritisch

Analyse-Reihenfolge:
1. Ist-Zustand erfassen
2. Welche Datei macht was
3. Hardware erfassen
4. Bugs finden
5. Verbesserungen sammeln
6. Zukünftige Funktionen definieren

Leitlinie:
Erst beschreiben, dann beweisen, dann ändern.

=============================================================================
DETAILANFORDERUNGEN – ROOT DATEIEN
=============================================================================

1. TASK.md
Erstelle eine operative Master-Task-Datei als Single Source of Truth.

Inhalt:
- Project State
- Context Snapshot
- Phase 1 – Baseline Discovery
  - Task 1.1 System Overview
  - Task 1.2 Project Map
  - Task 1.3 Hardware Map
  - Task 1.4 Safety Map
  - Task 1.5 Build & Deployment Notes
- Phase 2 – Static Analysis
  - Task 2.1 Static Bug Sweep
  - Task 2.2 Improvement Backlog
  - Task 2.3 Future Features
- Phase 3 – Controlled Execution
  - Task 3.1 Prioritized Fix Execution
  - Task 3.2 Verifier Review
  - Task 3.3 Documentation Consolidation
- Task Execution Template
- Task History Template

2. SYSTEM_OVERVIEW.md
Inhalt:
- Runtime Topology
- Pi Responsibilities
- STM32 Responsibilities
- Pi ↔ STM32 Communication
- Runtime Modes
- FACT / INFERENCE / UNKNOWN section

3. PROJECT_MAP.md
Inhalt:
- Core Runtime
- Hardware Layer
- Motion / Navigation
- Safety
- Communication
- Build / Config
- Tests / Tools
- Für jede Gruppe: Rolle, typische Dateien, Risiken, offene Fragen

4. HARDWARE_MAP.md
Inhalt:
- Confirmed / Likely / Unknown
- Signal Table mit:
  - Signal
  - Pi/STM32
  - Interface
  - Source
  - Meaning
  - Safety Critical
  - Confidence
- Motor enable, e-stop, dock detect, battery telemetry, UART/I2C/CAN/Port Expander
- Keine unbewiesenen Pin-Behauptungen als Fakten darstellen

5. SAFETY_MAP.md
Inhalt:
- Immediate Stop Paths
- Controlled Stop Paths
- Recovery Paths
- Critical Questions
- Safety verification checklist

6. BUILD_NOTES.md
Inhalt:
- Build targets
- Linux / Alfred relevant paths
- config headers
- deployment
- startup services
- update path
- rollback/recovery considerations
- Open Unknowns

7. BUG_REPORT.md
Inhalt:
- Critical / High / Medium / Low / Plausible but Unproven
- Finding Template
- Status / Reproduction / Risk / Minimal Fix

8. IMPROVEMENT_BACKLOG.md
Inhalt:
- Diagnostics
- Architecture
- Reliability
- Testability
- Deployment Safety
- Priorisierung nach Nutzen / Risiko / Aufwand

9. FUTURE_FEATURES.md
Inhalt:
- Safety
- Navigation
- Serviceability
- Remote / UX
- Diagnostics
- Maintenance
- Für jede Feature-Idee:
  - Nutzen
  - Betroffene Module
  - Risiko
  - Abhängigkeiten
  - Komplexität
  - Blocker

=============================================================================
DETAILANFORDERUNGEN – .CLAUDE-PROMPTS
=============================================================================

10. firmware-safety.md
- Identity: C++17 Embedded Systems Spezialist für RTK autonome Mähroboter
- Platform: RPi 4B + STM32F103, PlatformIO, C++17
- Fokus: Sicherheit, Interrupt-Safety, Race-Conditions, deterministisches Verhalten
- Motor-Control ist kritisch
- Code-Review erforderlich
- Keine Halluzinationen zu Hardware
- Basierend auf Safety/Risk Assessment Pattern
- Soll minimale, sichere Änderungen erzwingen
- Output mit:
  - Scope
  - Risks
  - Assumptions
  - Verification
  - Status

11. rtk-integration.md
- RTK GPS Spezialist
- sunray-core / rtk-library v2.1 Kontext
- Zielgenauigkeit: <5 cm
- Compass calibration
- GPS failsafe
- multi-satellite strategies
- edge cases
- debugging RTK-specific issues
- outputorientiert und technisch

12. mqtt-protocol.md
- MQTT Safety Protocol Expert
- Topics: robot/lawnmower/#
- QoS=2 für safety-critical messages
- Disconnect handling
- heartbeat
- message validation
- Home Assistant MQTT Discovery
- Retain-Regeln
- Timeout- und Recovery-Denken

13. bug-hunting.md
- Verification Specialist Pattern
- adversarial review
- Fokus: Race Conditions, Memory Leaks, GPIO Fehler, Timeout Bugs, State Machine Bugs
- Tools: grep, git-log, valgrind-output, static reasoning
- Output muss PASS / FAIL / PARTIAL enthalten
- Mit Begründung, Risiken und Nachtests

14. ha-integration.md
- Home Assistant Integration Spezialist
- MQTT Discovery für sensor, switch, binary_sensor, button, select, number, device_tracker, update, camera falls sinnvoll
- Fokus auf Zuverlässigkeit, Offline-/Off-Grid-Fähigkeit, robuste Discovery, klare Entity-Zustände
- Automations- und Diagnose-Muster

15. repo-scanner.md
- Rolle: Repo-Struktur erfassen
- Ziel: Dateiverantwortung, Entry Points, Build-Pfade, Modulgrenzen, Risikobereiche
- Muss FACT / INFERENCE / UNKNOWN nutzen
- Output:
  - Repository Shape
  - Entry Points
  - Build System
  - Platform Separation
  - Safety-Relevant Areas
  - High-Risk Files
  - Unknowns

16. hardware-mapper.md
- Rolle: Hardware/Software Bindings erfassen
- Fokus:
  - Pi ↔ STM32
  - GPIO
  - UART
  - I2C
  - SPI
  - CAN
  - mögliche Port-Expander
  - safety-critical IO
- Muss Confidence Levels vergeben
- Keine Spekulationen als Fakten
- Output als strukturierte Mapping-Analyse

17. safety-auditor.md
- Rolle: Safety-Pfade adversarial prüfen
- Fokus:
  - emergency stop
  - motor enable
  - watchdog
  - communication loss
  - battery critical
  - GPS invalid
  - docking safety
- Output:
  - confirmed safeguards
  - weak points
  - unknowns
  - regression risks

18. architect-future.md
- Rolle: Verbesserungen und zukünftige Funktionen architektonisch bewerten
- Fokus:
  - realistische Erweiterungen
  - Modulgrenzen respektieren
  - Risiken / Abhängigkeiten
  - keine Wunschlisten ohne Architekturbezug
- Output:
  - improvement candidates
  - future features
  - blockers
  - complexity notes

19. verifier.md
- Rolle: unabhängige Prüfung fertiger Änderungen
- adversarial, knapp, hart
- prüft:
  - scope fit
  - unintended side effects
  - build/test impact
  - safety implications
  - hidden assumptions
- Output:
  - PASS / FAIL / PARTIAL
  - Findings
  - Required follow-ups

=============================================================================
DETAILANFORDERUNGEN – .MEMORY
=============================================================================

20. session-notes.md
- Last Session Log
- Was wurde gemacht
- TODO
- kritisch/offen
- Markdown mit Emojis: ✅ ⚠️ 🔴 🟡 🟢
- Update nach jeder Session
- Tracking von Issues, Blockers, Completed Features
- Struktur als laufendes Arbeitslog

21. rtk-context.md
- RTK technisches Wissen
- Kalibrierung
- bekannte Edge Cases
- Satellitengeometrie
- Accuracy baseline <5 cm Ziel
- Compass Integration
- magnetic declination
- Failover Strategien

22. hardware-pinout.md
- RPi GPIO Pinouts
- STM32F103 Pin Mapping
- Power Distribution
- Safety Relays
- Interrupt vectors
- reservierte Pins
- Motor GPIO Pin 17 = enable signal als EXPLIZITER EINTRAG
- Aber mit Confidence-Feld, nicht blind als bewiesene Wahrheit
- Format als Tabelle + Notizen

23. mqtt-topics.md
- vollständige Topic-Hierarchie für robot/lawnmower/#
- Payload schemas
- QoS level pro Topic
- retain policy
- examples
- safety checks
- discovery topics für Home Assistant

24. cost-optimization.md
- Claude/API Cost Tracking
- Baseline: $4.96 für 19 Stunden
- per-module context loading
- ctx-profile metadata
- token budget tracking
- cache hit strategies
- hard-stop task boundaries
- praktische Optimierungsregeln

25. known-issues.md
- aktuelle Bugs mit Status:
  - Investigating
  - In Progress
  - Resolved
- Workarounds
- Performance Bottlenecks
- Deprecated Code Sections
- 30min MQTT Disconnect Bug als eigener Eintrag mit:
  - Status
  - Debug steps
  - Hypothesen
  - next checks

26. runtime-knowledge.md
- Boot- und Laufzeitwissen
- service order
- startup assumptions
- reconnect behavior
- watchdog chain
- crash handling assumptions
- deployment runtime notes

27. architecture-decisions.md
- ADR-artige Liste
- wichtige Architekturentscheidungen
- Kontext
- Entscheidung
- Konsequenzen
- offene Risiken
- Format als strukturierte Einträge

28. safety-findings.md
- Safety-relevante Beobachtungen
- confirmed safeguards
- suspected weaknesses
- open validation points
- incidents / near misses
- Follow-up actions

DATEI-ERSTELLUNGSMODUS

Wenn Schreibzugriff auf das Projekt besteht:
- lege alle Verzeichnisse an, falls sie fehlen
- schreibe jede Datei direkt an den angegebenen Pfad
- gib nach jeder Datei kurz den Status aus: CREATED / UPDATED / SKIPPED
- überschreibe bestehende Dateien nur dann vollständig, wenn sie leer, generisch oder klar veraltet sind
- wenn eine Datei bereits brauchbaren projektspezifischen Inhalt hat, merge sinnvoll statt blind zu ersetzen

Wenn kein Schreibzugriff besteht:
- gib die Dateien vollständig im angegebenen Ausgabeformat aus

=============================================================================
AUSGABEFORMAT
=============================================================================

Erstelle ALLE Dateien vollständig.

Format der Antwort:
---
# <DATEIPFAD>
<vollständiger Inhalt der Datei>
---

Reihenfolge:
1. Root-Dateien
2. .claude-prompts
3. .memory

Falls die Ausgabe zu lang wird:
- liefere die Dateien trotzdem vollständig weiter
- arbeite streng in der vorgegebenen Reihenfolge
- keine Zusammenfassung statt Inhalt
- keine Platzhalter wie “TODO später”
- keine ausgelassenen Dateien

=============================================================================
ARBEITSAUFTRAG
=============================================================================

Erstelle jetzt die komplette Struktur mit vollem Inhalt.
Beginne mit:

---
# TASK.md
...
---

und fahre dann exakt in der oben definierten Reihenfolge fort.