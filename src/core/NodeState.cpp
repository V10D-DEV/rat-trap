#include "rattrap/core/NodeState.h"

#include <utility>

namespace rattrap {

NodeState::NodeState(const DeviceIdentity& identity, std::string firmware_version)
    : identity_(identity), firmware_version_(std::move(firmware_version)) {}

void NodeState::setTrapState(TrapState state) {
    trap_state_ = state;
    rearming_required_ = (state == TrapState::NEEDS_REARMING ||
                          state == TrapState::TRIGGERED);
}

void NodeState::apply(const EventResult& result, uint64_t now_ms) {
    setTrapState(result.trap_state);

    if (result.type == EventType::NONE || result.type == EventType::UNKNOWN) {
        return;
    }

    switch (result.type) {
        case EventType::IR_ACTIVITY:
            if (result.cause == SensorType::IR1) {
                ++counters_.ir1_activity;
            } else if (result.cause == SensorType::IR2) {
                ++counters_.ir2_activity;
            }
            break;
        case EventType::PASSED_THROUGH:
            ++counters_.passed_through;
            break;
        case EventType::TRAP_TRIGGERED:
            ++counters_.trap_triggered;
            break;
        case EventType::ESCAPED_AFTER_TRIGGER:
            ++counters_.escaped_after_trigger;
            if (result.cause == SensorType::IR1) {
                ++counters_.ir1_activity;
            } else if (result.cause == SensorType::IR2) {
                ++counters_.ir2_activity;
            }
            break;
        case EventType::TRAP_REARMED:
            break;
        default:
            break;
    }

    last_event_ = result.type;
    last_event_time_ms_ = now_ms;
}

void NodeState::restore(const PersistentState& persisted) noexcept {
    counters_ = persisted.counters;
    last_event_ = persisted.last_event;
    last_event_time_ms_ = persisted.last_event_time_ms;
    setTrapState(persisted.trap_state);
}

PersistentState NodeState::persist() const noexcept {
    PersistentState p;
    p.counters = counters_;
    p.last_event = last_event_;
    p.last_event_time_ms = last_event_time_ms_;
    p.trap_state = trap_state_;
    return p;
}

NodeStatus NodeState::snapshot() const noexcept {
    NodeStatus s;
    s.identity = identity_;
    s.trap_state = trap_state_;
    s.last_event = last_event_;
    s.last_event_time_ms = last_event_time_ms_;
    s.battery = battery_;
    s.counters = counters_;
    s.rearming_required = rearming_required_;
    s.firmware_version = firmware_version_;
    s.uptime_ms = uptime_ms_;
    s.next_daily_report_ms = next_daily_report_ms_;
    return s;
}

}  // namespace rattrap