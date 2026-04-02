#include "core/EventMessages.h"

namespace sunray::messages {

std::string humanReadableReasonMessage(const std::string& eventReason,
                                       const std::string& errorCode) {
    if (eventReason == "start_rejected_no_map") {
        return "Start nicht möglich: Es ist keine aktive Karte geladen.";
    }
    if (eventReason == "start_rejected_no_mow_points") {
        return "Start nicht möglich: Die aktive Karte enthält keine Mähpunkte.";
    }
    if (eventReason == "battery_low") {
        return "Akku schwach: Der Roboter fährt zur Ladestation.";
    }
    if (eventReason == "battery_critical") {
        return "Akku kritisch: Der Roboter wurde aus Sicherheitsgründen gestoppt.";
    }
    if (eventReason == "gps_signal_lost") {
        return "GPS-Signal verloren: Der Roboter wartet auf ein stabiles Signal.";
    }
    if (eventReason == "gps_fix_timeout") {
        return "GPS-Fix zu lange ausgeblieben: Die Mission wurde sicher unterbrochen.";
    }
    if (eventReason == "gps_recovered") {
        return "GPS wieder stabil: Der Roboter kann den Auftrag fortsetzen.";
    }
    if (eventReason == "lift_triggered") {
        return "Anhebesensor ausgelöst: Bitte Roboter prüfen und sicher absetzen.";
    }
    if (eventReason == "motor_fault") {
        return "Mähmotorfehler erkannt: Bitte Mähantrieb und Verkabelung prüfen.";
    }
    if (eventReason == "bumper_triggered") {
        return "Hindernis erkannt: Der Roboter weicht aus.";
    }
    if (eventReason == "stuck_detected") {
        return "Fortbewegung blockiert: Der Roboter startet ein Befreiungsmanöver.";
    }
    if (eventReason == "stuck_recovery_exhausted") {
        return "Wiederholtes Festfahren: Der Roboter wurde in einen sicheren Fehlerzustand versetzt.";
    }
    if (eventReason == "rain_detected") {
        return "Regen erkannt: Der Mähauftrag wird pausiert.";
    }
    if (eventReason == "dock_failed") {
        return "Docking fehlgeschlagen: Die Ladestation konnte nicht sicher erreicht werden.";
    }
    if (eventReason == "undock_failed") {
        return "Ausparken fehlgeschlagen: Der Roboter konnte die Ladestation nicht sicher verlassen.";
    }
    if (eventReason == "undock_charger_timeout") {
        return "Ausparken fehlgeschlagen: Der Ladekontakt wurde nicht rechtzeitig getrennt.";
    }
    if (eventReason == "resume_blocked_map_changed") {
        return "Fortsetzen blockiert: Die Karte wurde seit dem Missionsstart verändert.";
    }
    if (eventReason == "perimeter_violated") {
        return "Perimeter verletzt: Der Roboter hat den erlaubten Bereich verlassen.";
    }
    if (eventReason == "op_watchdog_timeout") {
        return "Sicherheits-Timeout: Der aktuelle Fahrzustand dauerte zu lange.";
    }
    if (eventReason == "mcu_comm_lost") {
        return "STM32-Kommunikation verloren: Der Roboter wurde lokal in einen sicheren Fehlerzustand versetzt.";
    }
    if (eventReason == "charger_connected") {
        return "Ladekontakt erkannt: Der Roboter lädt.";
    }
    if (eventReason == "undocking") {
        return "Der Roboter verlässt die Ladestation.";
    }
    if (eventReason == "navigating_to_start") {
        return "Der Roboter fährt zum Startpunkt.";
    }
    if (eventReason == "mission_active") {
        return "Der Mähauftrag läuft.";
    }
    if (eventReason == "docking") {
        return "Der Roboter fährt zur Ladestation.";
    }
    if (eventReason == "charging") {
        return "Der Roboter lädt.";
    }
    if (eventReason == "waiting_for_rain") {
        return "Der Roboter wartet, bis der Regen vorbei ist.";
    }
    if (eventReason == "gps_recovery_wait") {
        return "Der Roboter wartet auf GPS-Erholung.";
    }
    if (eventReason == "obstacle_recovery") {
        return "Der Roboter führt ein Ausweichmanöver durch.";
    }
    if (eventReason == "kidnap_detected") {
        return "Positionssprung erkannt: Bitte Standort und GPS prüfen.";
    }
    if (errorCode == "ERR_LIFT") {
        return "Fehlerzustand aktiv: Der Anhebesensor ist ausgelöst.";
    }
    if (errorCode == "ERR_MOTOR_FAULT") {
        return "Fehlerzustand aktiv: Ein Mähmotorfehler liegt an.";
    }
    if (errorCode == "ERR_GPS_TIMEOUT") {
        return "Fehlerzustand aktiv: Es gab zu lange keinen gültigen GPS-Fix.";
    }
    if (errorCode == "ERR_MCU_COMMS") {
        return "Fehlerzustand aktiv: Die Kommunikation zum STM32 ist ausgefallen.";
    }
    if (errorCode == "ERR_STUCK") {
        return "Fehlerzustand aktiv: Der Roboter konnte sich nicht mehr zuverlässig selbst befreien.";
    }
    if (errorCode == "ERR_BATTERY_CRITICAL") {
        return "Fehlerzustand aktiv: Die Batteriespannung ist kritisch niedrig.";
    }
    return "Der Roboter hat einen Statuswechsel oder ein relevantes Ereignis gemeldet.";
}

std::string humanReadableTransitionMessage(const std::string& beforeOp,
                                           const std::string& afterOp,
                                           const std::string& eventReason,
                                           const std::string& errorCode) {
    if (afterOp == "Error") {
        return humanReadableReasonMessage(eventReason, errorCode);
    }
    if (beforeOp == "Charge" && afterOp == "Undock") {
        return "Der Roboter startet und verlässt die Ladestation.";
    }
    if (afterOp == "NavToStart") {
        return "Der Roboter fährt zum Startpunkt der Mission.";
    }
    if (afterOp == "Mow") {
        return "Der Roboter beginnt mit dem Mähen.";
    }
    if (afterOp == "Dock") {
        return humanReadableReasonMessage(eventReason, errorCode);
    }
    if (afterOp == "GpsWait") {
        return humanReadableReasonMessage(eventReason, errorCode);
    }
    if (afterOp == "WaitRain") {
        return humanReadableReasonMessage(eventReason, errorCode);
    }
    if (afterOp == "EscapeReverse" || afterOp == "EscapeForward") {
        return humanReadableReasonMessage(eventReason, errorCode);
    }
    if (afterOp == "Charge") {
        return "Der Roboter hat die Ladestation erreicht und lädt jetzt.";
    }
    if (afterOp == "Idle") {
        return "Der Roboter ist bereit und wartet auf den nächsten Auftrag.";
    }
    return "Statuswechsel: " + beforeOp + " -> " + afterOp;
}

} // namespace sunray::messages
