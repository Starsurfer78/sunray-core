# SUNRAY_CORE_VS_NAVIMOW_VS_LUBA.md

Stand: 2026-03-27

Vergleich zwischen:

- `sunray-core`
- `Segway Navimow`
- `Mammotion Luba`

Ziel dieses Dokuments ist keine Kaufberatung im engeren Sinn, sondern eine ehrliche Einordnung aus Architektur-, Produkt- und Entwicklerperspektive.

---

## Kurzfazit

Die drei Systeme verfolgen unterschiedliche Grundideen:

- `sunray-core` ist eine offene, entwicklergetriebene Plattform
- `Navimow` ist ein stark integriertes Consumer-Produkt mit Fokus auf einfacher Nutzung
- `Luba` ist ebenfalls ein Consumer-Produkt, aber mit staerkerem Fokus auf Traktion, Gelaende und robuste Outdoor-Performance

Der wichtigste Unterschied ist deshalb nicht nur Technik, sondern Produktphilosophie:

- `sunray-core` gibt Kontrolle
- `Navimow` gibt Komfort
- `Luba` gibt Komfort plus Gelaende-Fokus

---

## Grundcharakter

| Bereich | sunray-core | Segway Navimow | Mammotion Luba |
|---|---|---|---|
| Typ | offene Linux-/Pi-Plattform | fertiger Consumer-Maehroboter | fertiger Consumer-Maehroboter |
| Rolle des Nutzers | Entwickler / Integrator / Bastler / Betreiber | Endkunde | Endkunde |
| Offenheit | hoch | sehr gering | sehr gering |
| Anpassbarkeit | sehr hoch | niedrig | niedrig |
| Herstellerabhaengigkeit | gering bis mittel | hoch | hoch |
| Reparierbarkeit | prinzipiell hoch | begrenzt | begrenzt |

---

## Technische Einordnung

### sunray-core

`sunray-core` ist eine offene Robotik-Softwarebasis fuer Linux, aktuell mit:

- C++17-Core
- WebUI
- Op-/State-Machine
- Karteneditor
- Telemetrie
- RTK-/GPS-/Odometrie-/IMU-Einbindung
- EKF-basierter Pose-Schaetzung
- Simulationsmodus

Die Staerke liegt nicht in einer fertigen „Out-of-the-box“-Integration, sondern in:

- Transparenz
- Testbarkeit
- Architekturkontrolle
- eigener Weiterentwicklung

Das System ist damit besonders stark fuer:

- eigene Hardwareplattformen
- Forschungs-/Hobby-/Maker-Projekte
- tiefe Diagnostik
- langfristige technische Unabhaengigkeit

### Segway Navimow

Navimow ist ein klassisches Produkt mit stark integrierter Hard-/Software und appzentrierter Nutzerfuehrung.
Die offizielle Positionierung setzt klar auf wire-free / virtual boundary und einfache Inbetriebnahme.

Typische Stärken:

- niedrige Einstiegshuerde
- starke Produktintegration
- gute App-/UX-Orientierung
- wenig Bastelaufwand

Typische Grenzen:

- wenig Einblick in interne Logik
- kaum Anpassbarkeit
- geringe Systemoffenheit

### Mammotion Luba

Luba ist ebenfalls ein fertiges Produkt, wirkt im Marktbild aber robuster auf schwieriges Terrain ausgerichtet als klassische Komfortsysteme.
Je nach Modell stehen Themen wie:

- All-Terrain
- AWD / Traktion
- Hangtauglichkeit
- groessere Flaechen / komplexere Aussenbereiche

staerker im Vordergrund.

Die Systemlogik bleibt aber auch hier primaer ein Herstellerprodukt und keine offene Plattform.

---

## Vergleich nach Perspektive

### 1. Offenheit und Architektur

| Frage | sunray-core | Navimow | Luba |
|---|---|---|---|
| Kann man die State-Machine verstehen und veraendern? | ja | praktisch nein | praktisch nein |
| Kann man Telemetrie und Verhalten tief nachvollziehen? | ja | eingeschraenkt | eingeschraenkt |
| Kann man eigene Hardware / Sensorik integrieren? | ja | nicht realistisch | nicht realistisch |
| Kann man das System langfristig selbst weiterentwickeln? | ja | kaum | kaum |

Hier liegt `sunray-core` klar vorne, wenn der Nutzer selbst gestalten will.

### 2. Benutzererlebnis

| Frage | sunray-core | Navimow | Luba |
|---|---|---|---|
| Schnell einsatzbereit | eher nein | ja | ja |
| Geringe technische Einstiegshuerde | nein | ja | ja |
| App-/Produkt-Reife | projektabhaengig | hoch | hoch |
| „Einfach nur maehen“ | noch nicht die Primaerstaerke | Primaerziel | Primaerziel |

Hier liegen `Navimow` und `Luba` klar vor `sunray-core`.

### 3. Gelaende und Robustheit

| Frage | sunray-core | Navimow | Luba |
|---|---|---|---|
| Verhalten auf schwierigem Terrain | stark hardwareabhaengig | produktabhaengig, eher komfortorientiert | staerker gelaendeorientiert |
| AWD / Traktionsfokus | nur wenn selbst gebaut | modellabhaengig | starkes Verkaufsargument |
| Optimierung fuer schwierige Gaerten | moeglich, aber Eigenleistung | begrenzt durch Produktdesign | staerker im Fokus |

Wenn schwieriges Terrain zentral ist, wirkt `Luba` im Consumer-Bereich derzeit naeher an diesem Bedarf.

### 4. Entwickler- und Maker-Perspektive

| Frage | sunray-core | Navimow | Luba |
|---|---|---|---|
| fuer eigenes Tuning geeignet | sehr | kaum | kaum |
| fuer Forschung / Experimente geeignet | sehr | schwach | schwach |
| fuer tiefe Diagnose geeignet | sehr | begrenzt | begrenzt |
| fuer eigene Integrationen geeignet | sehr | praktisch nein | praktisch nein |

Hier ist `sunray-core` die klar staerkste Option.

---

## Wo sunray-core staerker ist

- volle Kontrolle ueber Verhalten, Routing, Telemetrie und Diagnose
- keine Black Box auf Softwareebene
- anpassbar an andere Hardware und andere Projektziele
- sehr gut fuer schrittweise technische Weiterentwicklung
- Simulationsmodus und Testbarkeit sind fuer Entwickler ein grosser Vorteil
- langfristig potenziell nachhaltiger fuer Nutzer, die selbst betreiben und verstehen wollen

---

## Wo Navimow oder Luba staerker sind

- deutlich geringere Einstiegshuerde
- fertigeres Benutzererlebnis
- weniger Integrationsarbeit
- klareres Produkt statt Plattform
- vermutlich bessere „out of the box“-Erwartungserfuellung fuer normale Endkunden

Zusaetzlich:

- `Navimow` wirkt staerker auf Einfachheit, Produktdesign und bequeme Inbetriebnahme optimiert
- `Luba` wirkt staerker auf schwierige Gelaende- und Traktionssituationen optimiert

---

## Ehrliche Einordnung

`sunray-core` ist aktuell kein direkter 1:1-Ersatz fuer ein voll integriertes Serienprodukt wie Navimow oder Luba.

Es ist eher:

- eine technische Plattform
- ein kontrollierbarer Robotik-Stack
- eine Basis fuer ein eigenes, langfristig beherrschbares System

Navimow und Luba sind eher:

- fertige Produktwelten
- stark herstellergefuehrt
- fuer Endkunden optimiert

Deshalb ist die zentrale Frage nicht nur „welches System ist besser?“, sondern:

- Will ich ein Produkt?
- Oder will ich eine Plattform, die ich selbst wirklich beherrschen kann?

---

## Entscheidungshilfe

### sunray-core passt besser, wenn ...

- du das System verstehen und veraendern willst
- du eigene Hardware oder eigene Logik einsetzen willst
- dir Offenheit wichtiger ist als sofortige Produktreife
- du tiefere Diagnose- und Entwicklerwerkzeuge brauchst
- du bereit bist, Integrations- und Testarbeit selbst zu leisten

### Navimow passt besser, wenn ...

- du moeglichst wenig technische Reibung willst
- eine gute Endkunden-Erfahrung im Vordergrund steht
- du ein fertiges, bequemes Serienprodukt willst
- du keine eigene Robotik-Plattform entwickeln moechtest

### Luba passt besser, wenn ...

- du ebenfalls ein fertiges Serienprodukt willst
- dein Garten schwieriger ist
- Traktion, AWD und Outdoor-Robustheit wichtiger sind
- du nicht selbst tief in die Systemtechnik einsteigen moechtest

---

## Einordnung zur Herkunft von sunray-core

`sunray-core` steht genealogisch und philosophisch naeher an offenen Ardumower-/Sunray-Ideen als an geschlossenen Consumer-Produkten.

Es ist daher sinnvoller, `sunray-core` nicht als „billigere Alternative zu Navimow oder Luba“ zu lesen, sondern als:

- offene Weiterentwicklung
- technische Plattform
- eigenstaendige Robotik-Basis

---

## Quellen / Referenzen

- Ardumower Sunray:
  <https://github.com/Ardumower/Sunray>
- sunray-core:
  <https://github.com/Starsurfer78/sunray-core>
- Segway Navimow:
  <https://navimow.segway.com/>
- Mammotion:
  <https://mammotion.com/>

Hinweis:
Die Einordnung von Navimow und Luba ist bewusst qualitativ und produktstrategisch gehalten.
Sie soll `sunray-core` realistisch positionieren, nicht Herstellerdatenblatt gegen Datenblatt vergleichen.
