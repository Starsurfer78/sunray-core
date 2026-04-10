import type { Telemetry } from "../api/types";
import type { ConnectionState } from "../stores/connection";
import type { RobotMap } from "../stores/map";
import { BATTERY_MIN_V, BATTERY_MAX_V } from "../stores/telemetry";

export type UiTone = "success" | "warning" | "error" | "info";

export type PreflightCheck = {
  label: string;
  ok: boolean;
  value: string;
  hint: string;
};

export type UiNotice = {
  tone: UiTone;
  title: string;
  detail: string;
  action: string;
};

const START_ACCURACY_M = 0.08;
const FRESH_MS = 5000;

export function humanizeReason(reason: string): string {
  if (!reason || reason === "none") return "Kein besonderer Hinweis";
  return reason
    .replace(/^ERR_/, "")
    .replace(/_/g, " ")
    .toLowerCase()
    .replace(/^\w/, (char) => char.toUpperCase());
}

export function batteryPercentFromVoltage(voltage: number): number {
  const raw = ((voltage - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V)) * 100;
  return Math.max(0, Math.min(100, Math.round(raw)));
}

export function isConnectionFresh(
  connection: ConnectionState,
  nowMs: number,
  maxAgeMs = FRESH_MS,
): boolean {
  return connection.connected && nowMs - connection.lastSeen <= maxAgeMs;
}

export function isGpsReady(telemetry: Telemetry): boolean {
  return (
    telemetry.gps_sol === 4 ||
    (telemetry.gps_sol === 5 &&
      telemetry.gps_acc > 0 &&
      telemetry.gps_acc <= START_ACCURACY_M)
  );
}

export function hasUsableMap(map: RobotMap): boolean {
  return map.perimeter.length >= 3 && map.dock.length >= 2;
}

export function hasDockPath(map: RobotMap): boolean {
  return map.dock.length >= 2;
}

export function getPreflightChecks(
  telemetry: Telemetry,
  connection: ConnectionState,
  map: RobotMap,
  nowMs: number,
): PreflightCheck[] {
  const batteryPercent = batteryPercentFromVoltage(telemetry.battery_v);
  const fresh = isConnectionFresh(connection, nowMs);
  const gpsReady = isGpsReady(telemetry);
  const mapReady = hasUsableMap(map);
  const dockReady = hasDockPath(map);
  const errorFree =
    telemetry.op !== "Error" &&
    !telemetry.motor_err &&
    !telemetry.lift &&
    !telemetry.bumper_l &&
    !telemetry.bumper_r;

  return [
    {
      label: "Verbindung",
      ok: fresh,
      value: fresh ? "Stabil" : connection.connected ? "Veraltet" : "Getrennt",
      hint: fresh
        ? "Live-Telemetrie ist verfügbar"
        : "Verbindung oder frische Telemetrie fehlt",
    },
    {
      label: "GPS",
      ok: gpsReady,
      value: telemetry.gps_text || "---",
      hint: gpsReady
        ? `Genauigkeit ${telemetry.gps_acc.toFixed(2)} m`
        : "Auf RTK Fix oder guten RTK Float warten",
    },
    {
      label: "Akku",
      ok: batteryPercent >= 20,
      value: `${batteryPercent}%`,
      hint:
        batteryPercent >= 20
          ? `${telemetry.battery_v.toFixed(1)} V`
          : "Vor dem Start besser laden oder andocken",
    },
    {
      label: "Karte",
      ok: mapReady,
      value: map.perimeter.length >= 3 ? "Vorhanden" : "Fehlt",
      hint: mapReady
        ? `${map.perimeter.length} Perimeterpunkte`
        : "Aktive Karte oder Perimeter fehlt",
    },
    {
      label: "Dock",
      ok: dockReady,
      value: dockReady ? "Pfad ok" : "Fehlt",
      hint: dockReady
        ? `${map.dock.length} Docking-Punkte`
        : "Docking-Pfad mit mindestens 2 Punkten fehlt",
    },
    {
      label: "Sicherheit",
      ok: errorFree,
      value: errorFree ? "Frei" : "Blockiert",
      hint: errorFree
        ? "Keine aktiven Sensor- oder Fehlerblocker"
        : "Lift, Bumper, Mähmotorfehler oder Error aktiv",
    },
  ];
}

export function getStartRelease(
  telemetry: Telemetry,
  connection: ConnectionState,
  map: RobotMap,
  nowMs: number,
) {
  const checks = getPreflightChecks(telemetry, connection, map, nowMs);
  const blockers = checks.filter((check) => !check.ok);
  return {
    checks,
    allowed: blockers.length === 0,
    blockers,
  };
}

export function getPrimaryNotice(
  telemetry: Telemetry,
  connection: ConnectionState,
  map: RobotMap,
  nowMs: number,
): UiNotice {
  const startRelease = getStartRelease(telemetry, connection, map, nowMs);

  if (!connection.connected) {
    return {
      tone: "warning",
      title: "Verbindung unterbrochen",
      detail: "Der Roboter sendet gerade keine Live-Daten.",
      action: "WLAN und Stromversorgung prüfen, dann Telemetrie abwarten.",
    };
  }

  if (telemetry.op === "Error") {
    return {
      tone: "error",
      title: "Stoerung aktiv",
      detail:
        telemetry.error_code || humanizeReason(telemetry.event_reason || "Unbekannter Fehler"),
      action: "Ursache beheben, dann Stop oder quittieren und neu starten.",
    };
  }

  if (telemetry.lift || telemetry.bumper_l || telemetry.bumper_r || telemetry.motor_err) {
    return {
      tone: "error",
      title: "Sicherheitsblocker aktiv",
      detail: "Lift, Bumper oder Mähmotorfehler verhindern den normalen Betrieb.",
      action: "Roboter freistellen, Sensoren prüfen und erst danach neu starten.",
    };
  }

  if (telemetry.op === "GpsWait") {
    return {
      tone: "warning",
      title: "GPS-Wartezustand",
      detail: "Der Roboter wartet auf ein stabiles GPS-/RTK-Signal.",
      action: "Roboter nicht bewegen und auf besseren Empfang warten.",
    };
  }

  if (telemetry.state_phase === "waiting_rain" || telemetry.op === "WaitRain") {
    return {
      tone: "warning",
      title: "Wetterpause",
      detail: "Der Betrieb ist wegen Regen oder Regenlogik pausiert.",
      action: "Abtrocknen lassen oder später erneut starten.",
    };
  }

  if (telemetry.op === "Dock") {
    return {
      tone: "info",
      title: "Docking laeuft",
      detail: "Der Roboter faehrt gerade zur Ladestation.",
      action: "Fahrweg freihalten und den Vorgang nicht unterbrechen.",
    };
  }

  if (telemetry.op === "Charge") {
    return {
      tone: "info",
      title: "Roboter laedt",
      detail: "Der Roboter steht an der Station und laedt.",
      action: "Für einen Mähstart erst abdocken oder Start auslösen.",
    };
  }

  if (!startRelease.allowed) {
    return {
      tone: "warning",
      title: "Start noch blockiert",
      detail: startRelease.blockers[0]?.hint ?? "Mindestens ein Preflight-Punkt ist noch offen.",
      action: "Preflight rechts abarbeiten, bis alle Punkte gruen sind.",
    };
  }

  return {
    tone: "success",
    title: "Startfreigabe erteilt",
    detail: "GPS, Akku, Karte, Dock und Verbindung wirken betriebsbereit.",
    action: "Mission starten oder per Knopfdruck am Roboter auslösen.",
  };
}

export function getRecoveryNotice(telemetry: Telemetry): UiNotice {
  if (telemetry.op === "Error") {
    return {
      tone: "error",
      title: "Störung",
      detail: telemetry.error_code || humanizeReason(telemetry.event_reason),
      action: "Fehlerursache beseitigen und danach quittieren.",
    };
  }
  if (telemetry.op === "GpsWait") {
    return {
      tone: "warning",
      title: "GPS-Wartezustand",
      detail: "RTK-/GPS-Signal fehlt oder ist unbrauchbar.",
      action: "Standort nicht verändern und auf stabiles Signal warten.",
    };
  }
  if (telemetry.op === "WaitRain" || telemetry.state_phase === "waiting_rain") {
    return {
      tone: "warning",
      title: "Wetterpause",
      detail: "Regen oder Regenlogik blockiert den Mähbetrieb.",
      action: "Trockenphase abwarten oder später erneut starten.",
    };
  }
  if (telemetry.op === "Dock") {
    return {
      tone: "info",
      title: "Docking",
      detail: "Der Roboter kehrt zur Ladestation zurück.",
      action: "Fahrweg freihalten und Docking beobachten.",
    };
  }
  if (telemetry.op === "Charge") {
    return {
      tone: "info",
      title: "Lädt",
      detail: "Der Roboter ist an der Ladestation und laedt.",
      action: "Für neue Aufgaben Akku und Plan prüfen.",
    };
  }
  return {
    tone: "success",
    title: "Bereit",
    detail: humanizeReason(telemetry.event_reason),
    action: "Keine Aktion erforderlich.",
  };
}
