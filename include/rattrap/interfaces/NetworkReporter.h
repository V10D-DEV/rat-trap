#pragma once

#include "rattrap/Types.h"

namespace rattrap {

// Abstract network reporting channel. The application only issues logical
// requests; it never touches Wi-Fi.
//
// TODO(esp32-c3): implement with ESP-IDF/library Wi-Fi stack. This is one of
// the three adapters the ESP32-C3 layer will provide.
class NetworkReporter {
public:
    virtual ~NetworkReporter() = default;

    // Report a logical event (PASSED_THROUGH, TRAP_TRIGGERED, ...).
    virtual bool sendEvent(const DeviceIdentity& identity, const EventResult& event) = 0;

    // Report the full node status, sent approximately once per 24 h.
    virtual bool sendDailyStatus(const NodeStatus& status) = 0;
};

}  // namespace rattrap