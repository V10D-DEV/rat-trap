#pragma once

#include <cstdint>
#include <string>

namespace rattrap {

constexpr const char* kFirmwareVersion = "0.1.0";

// Stable identity of a physical node. The ESP32-C3 layer will read these from
// configuration/storage, never from a random UUID.
struct DeviceIdentity {
    std::string warehouse_id;
    int trap_number = 0;
};

enum class SensorType {
    IR1,
    IR2,
    TRAP_SENSOR
};

// Logical mechanical state of the trap.
//   ARMED         - ready, trap sensor reports armed position
//   TRIGGERED     - transient: a live armed->triggered edge was just observed
//   NEEDS_REARMING- persistent: the mechanism is sprung and must be re-armed
//   UNKNOWN       - no sensor reading accepted yet (boot before first read)
enum class TrapState {
    ARMED,
    TRIGGERED,
    NEEDS_REARMING,
    UNKNOWN
};

enum class EventType {
    NONE,
    IR_ACTIVITY,
    PASSED_THROUGH,
    TRAP_TRIGGERED,
    ESCAPED_AFTER_TRIGGER,
    TRAP_REARMED,
    UNKNOWN
};

// Raw logical sensor sample, already decoded by the future hardware layer.
//   IR beams:        state == true  -> beam blocked
//                   state == false -> beam clear
//   TRAP_SENSOR:     state == true  -> TRIGGERED_POSITION (mechanism sprung)
//                   state == false -> ARMED_POSITION
// The ESP32-C3 adapter owns the physical<->logical polarity mapping.
struct SensorEvent {
    SensorType sensor = SensorType::IR1;
    bool state = false;
    uint64_t timestamp_ms = 0;
};

// Result of interpreting one or more sensor samples.
struct EventResult {
    EventType type = EventType::NONE;
    TrapState trap_state = TrapState::UNKNOWN;
    SensorType cause = SensorType::IR1;
    bool rearming_required = false;
    bool notify_network = false;
};

struct BatteryStatus {
    double voltage = 0.0;
    int percentage = -1;
    bool low_battery = false;
};

struct EventCounters {
    uint32_t ir1_activity = 0;
    uint32_t ir2_activity = 0;
    uint32_t passed_through = 0;
    uint32_t trap_triggered = 0;
    uint32_t escaped_after_trigger = 0;

    void reset() noexcept { *this = EventCounters{}; }
};

// Complete, platform-independent view of a node at a point in time.
struct NodeStatus {
    DeviceIdentity identity;
    TrapState trap_state = TrapState::UNKNOWN;
    EventType last_event = EventType::NONE;
    uint64_t last_event_time_ms = 0;
    BatteryStatus battery;
    EventCounters counters;
    bool rearming_required = false;
    std::string firmware_version;
    uint64_t uptime_ms = 0;
    uint64_t next_daily_report_ms = 0;
};

// What the storage layer persists across reboots.
struct PersistentState {
    EventCounters counters;
    EventType last_event = EventType::NONE;
    uint64_t last_event_time_ms = 0;
    TrapState trap_state = TrapState::UNKNOWN;
};

inline const char* sensorTypeName(SensorType t) noexcept {
    switch (t) {
        case SensorType::IR1: return "IR1";
        case SensorType::IR2: return "IR2";
        case SensorType::TRAP_SENSOR: return "TRAP_SENSOR";
    }
    return "?";
}

inline const char* trapStateName(TrapState t) noexcept {
    switch (t) {
        case TrapState::ARMED: return "ARMED";
        case TrapState::TRIGGERED: return "TRIGGERED";
        case TrapState::NEEDS_REARMING: return "NEEDS_REARMING";
        case TrapState::UNKNOWN: return "UNKNOWN";
    }
    return "?";
}

inline const char* eventTypeName(EventType t) noexcept {
    switch (t) {
        case EventType::NONE: return "NONE";
        case EventType::IR_ACTIVITY: return "IR_ACTIVITY";
        case EventType::PASSED_THROUGH: return "PASSED_THROUGH";
        case EventType::TRAP_TRIGGERED: return "TRAP_TRIGGERED";
        case EventType::ESCAPED_AFTER_TRIGGER: return "ESCAPED_AFTER_TRIGGER";
        case EventType::TRAP_REARMED: return "TRAP_REARMED";
        case EventType::UNKNOWN: return "UNKNOWN";
    }
    return "?";
}

}  // namespace rattrap