#pragma once

#include <string>

#include "rattrap/Types.h"

namespace rattrap {

// Aggregates the complete logical node state: identity, trap state, counters,
// last event, battery. Pure application state; the ESP32-C3 layer reads it via
// snapshot() for status reports and persist() for storage.
class NodeState {
public:
    NodeState(const DeviceIdentity& identity, std::string firmware_version = kFirmwareVersion);

    void apply(const EventResult& result, uint64_t now_ms);
    void setTrapState(TrapState state);
    void setBattery(const BatteryStatus& battery) noexcept { battery_ = battery; }
    void setUptime(uint64_t uptime_ms) noexcept { uptime_ms_ = uptime_ms; }
    void setNextDailyReportMs(uint64_t ms) noexcept { next_daily_report_ms_ = ms; }

    void restore(const PersistentState& persisted) noexcept;
    PersistentState persist() const noexcept;

    TrapState trapState() const noexcept { return trap_state_; }
    bool rearmingRequired() const noexcept { return rearming_required_; }
    const EventCounters& counters() const noexcept { return counters_; }
    EventType lastEvent() const noexcept { return last_event_; }
    uint64_t lastEventTimeMs() const noexcept { return last_event_time_ms_; }
    const DeviceIdentity& identity() const noexcept { return identity_; }
    const std::string& firmwareVersion() const noexcept { return firmware_version_; }

    NodeStatus snapshot() const noexcept;

private:
    DeviceIdentity identity_;
    std::string firmware_version_;
    TrapState trap_state_ = TrapState::UNKNOWN;
    bool rearming_required_ = false;
    EventType last_event_ = EventType::NONE;
    uint64_t last_event_time_ms_ = 0;
    BatteryStatus battery_;
    EventCounters counters_;
    uint64_t uptime_ms_ = 0;
    uint64_t next_daily_report_ms_ = 0;
};

}  // namespace rattrap