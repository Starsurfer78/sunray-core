# Hardware Pinout

## Zweck

Arbeitsstand fuer Pin- und Signal-Mapping von Raspberry Pi und STM32F103 rund um Alfred.

## Aktueller Stand

Vollstaendige Hardwarebeweise fehlen. Diese Datei trennt daher bewusst zwischen bestaetigt, wahrscheinlich und offen.

## Tabelle

| Signal / Pin | Zielseite | Interface | Bedeutung | Confidence | Notiz |
| --- | --- | --- | --- | --- | --- |
| `/dev/ttyS0` | Pi | UART | Standard-Link Pi zu STM32 | High | Direkt aus aktiver Config |
| `/dev/i2c-1` | Pi | I2C | Haupt-I2C-Bus | High | Direkt aus aktiver Config |
| I2C-Mux `0x70` | Pi-Seite | I2C | Bus-Aufteilung fuer Subgeraete | High | Aktiv im Code modelliert |
| IMU `0x69` | Pi-Seite | I2C | IMU-Geraet | Medium | Aktiv im Code, physische Bestätigung offen |
| Port Expander `0x20` | Pi-Seite | I2C | GPIO-Erweiterung | Medium | Durch Config und Tests gestuetzt |
| LED_1 | Pi-logisch | Unklar, vermutlich Port Expander | WiFi/Status | Medium | In HAL und SerialDriver benannt |
| LED_2 | Pi-logisch | Unklar, vermutlich Port Expander | Error/Idle | Medium | Safety-relevant Status |
| LED_3 | Pi-logisch | Unklar, vermutlich Port Expander | GPS-Status | Medium | Safety-nahe Rueckmeldung |
| Buzzer | Pi-logisch | Unklar | Audible feedback | Medium | Backend modelliert |
| Motor GPIO Pin 17 = enable signal | Unbekannt | GPIO | Moegliches Drive-Enable | Low | Explizit verfolgen, nicht als Fakt behandeln |

## Power Distribution

- FACT: Repo modelliert Batterie- und Ladezustand
- UNKNOWN: Exakte Schaltpfade, Relais, FETs, Sicherungen und Power-Gating-Topologie

## Safety Relays

- UNKNOWN: Keine harte Repo-Evidenz fuer ein dediziertes Pi-seitig geschaltetes Safety-Relais

## Interrupt Vectors

- UNKNOWN: Keine belastbare Pi- oder STM32-Interruptzuordnung aus dem aktuellen Repo-Stand abgeleitet

## Reservierte Pins

- UNKNOWN: Noch nicht dokumentiert

## Known Unknowns

- Revisionsunterschiede Alfred-Hardware
- Direkte Pi-GPIO-Beteiligung an Motion Safety
- Exakte Dock-Detect-Verdrahtung

## Update Rules

- Jeder neue Pin-Eintrag braucht Quelle
- Confidence immer pflegen
- Unbelegte Aussagen nicht in High Confidence hochziehen
