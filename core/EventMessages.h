#pragma once

#include <string>

namespace sunray::messages {

std::string humanReadableReasonMessage(const std::string& eventReason,
                                       const std::string& errorCode = "");

std::string humanReadableTransitionMessage(const std::string& beforeOp,
                                           const std::string& afterOp,
                                           const std::string& eventReason,
                                           const std::string& errorCode = "");

} // namespace sunray::messages
