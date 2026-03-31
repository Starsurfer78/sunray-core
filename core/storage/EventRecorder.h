#pragma once

#include "core/storage/EventRecord.h"
#include "core/storage/HistoryDatabase.h"

namespace sunray {

class EventRecorder {
public:
    void bind(HistoryDatabase* db, Logger* logger);
    bool available() const;
    void record(const EventRecord& event);

private:
    HistoryDatabase* db_     = nullptr;
    Logger*          logger_ = nullptr;
    bool             warnedWriteFailure_ = false;
};

} // namespace sunray
