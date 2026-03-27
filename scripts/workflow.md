# Workflow — Wie du einen Task startest

## 1. TASK.md ausfüllen (2 Minuten)

Öffne `TASK.md` und befülle:
- **ID + Ziel** — was genau soll passieren?
- **Aufgabe präzise** — Input, Output, Dateiname, Funktionsname
- **Kontext-Tabelle** — maximal 4 Files, nur die wirklich nötigen
- **Memory-Häkchen** — nur relevante Module ankreuzen
- **Erwartetes Ergebnis** — welcher Test wird grün?
- **Modell-Häkchen** — Haiku, Sonnet oder Opus?

## 2. Claude Code starten

```bash
# Im Projektordner:
claude

# Oder mit explizitem Modell (spart Geld):
claude --model claude-haiku-4-5-20251001   # für einfache Tasks
claude --model claude-sonnet-4-5           # für mittlere Tasks
```

Claude Code liest automatisch CLAUDE.md → dann nur was in TASK.md steht.

## 3. Nach dem Task

- TASK.md: Status auf `[x]` setzen
- Nächsten Task vorbereiten: TASK.md neu befüllen
- Alles andere macht Claude Code laut CLAUDE.md Coding Rules

---

## Modell-Entscheidungshilfe

| Situation | Modell |
|-----------|--------|
| Eine Funktion implementieren, Pattern bekannt | Haiku |
| Neues Modul anlegen (<150 Zeilen) | Haiku |
| Tests schreiben | Haiku |
| Mehrere Files ändern, neue Logik | Sonnet |
| EKF-Mathe, A*-Algorithmus | Sonnet |
| Architektur-Entscheidung, kritischer Bug | Opus |

## Token-Kostenkontrolle auf einen Blick

| Was | Wirkung |
|-----|---------|
| TASK.md ausfüllen | −70% Kontext |
| Haiku statt Sonnet | −95% Kosten pro Token |
| 1 Funktion pro Task | −80% interne Schritte |
| "Nicht anfassen"-Liste | verhindert unnötige Reads |
