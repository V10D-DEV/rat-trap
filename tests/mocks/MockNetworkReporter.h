#pragma once

#include <cstdio>
#include <vector>

#include "rattrap/interfaces/NetworkReporter.h"

// Test/mock implementation of NetworkReporter: records what *would* have been
// transmitted. No Wi-Fi here. The ESP32-C3 layer will replace this.
namespace rattrap {

class MockNetworkReporter final : public NetworkReporter {
public:
    struct EventRecord {
        DeviceIdentity identity;
        EventResult result;
        uint64_t at_ms = 0;
    };
    struct StatusRecord {
        NodeStatus status;
        uint64_t at_ms = 0;
    };

    void setTime(uint64_t now_ms) { now_ = now_ms; }

    bool sendEvent(const DeviceIdentity& identity, const EventResult& event) override {
        ++event_count;
        event_log.push_back({identity, event, now_});
        return !fail_events;
    }

    bool sendDailyStatus(const NodeStatus& status) override {
        ++status_count;
        status_log.push_back({status, now_});
        return !fail_status;
    }

    uint64_t now_ = 0;
    size_t event_count = 0;
    size_t status_count = 0;
    bool fail_events = false;
    bool fail_status = false;
    std::vector<EventRecord> event_log;
    std::vector<StatusRecord> status_log;
};

}  // namespace rattrap