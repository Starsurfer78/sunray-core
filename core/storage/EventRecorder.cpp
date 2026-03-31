#include "core/storage/EventRecorder.h"

namespace sunray {

void EventRecorder::bind(HistoryDatabase* db, Logger* logger) {
    db_ = db;
    logger_ = logger;
}

bool EventRecorder::available() const {
    return db_ != nullptr && db_->available();
}

void EventRecorder::record(const EventRecord& event) {
    if (db_ == nullptr || logger_ == nullptr || !db_->available()) return;

    if (!db_->appendEvent(event, *logger_) && !warnedWriteFailure_) {
        warnedWriteFailure_ = true;
        logger_->warn("EventRecorder", "event persistence failed; suppressing further warnings");
        return;
    }

    warnedWriteFailure_ = false;
}

} // namespace sunray
