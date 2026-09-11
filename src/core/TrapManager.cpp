#include "rattrap/core/TrapManager.h"

namespace rattrap {

TrapManager::TrapManager() = default;

TrapState TrapManager::state() const noexcept { return state_; }

bool TrapManager::rearmingRequired() const noexcept {
    return state_ == TrapState::NEEDS_REARMING || state_ == TrapState::TRIGGERED;
}

TrapSensorPosition TrapManager::position() const noexcept { return pos_; }

uint64_t TrapManager::lastChangeMs() const noexcept { return last_change_ms_; }

TrapManager::SensorUpdate TrapManager::notifyPosition(TrapSensorPosition pos,
                                                      uint64_t now_ms) {
    SensorUpdate u;

    if (pos == pos_) {
        // Identical reading: settle any un-confirmed transient (bounce).
        if (transitioning_) {
            transitioning_ = false;
            state_ = (pos == TrapSensorPosition::ARMED_POSITION)
                         ? TrapState::ARMED
                         : TrapState::NEEDS_REARMING;
            last_change_ms_ = now_ms;
        }
        return u;  // changed == false
    }

    const bool prev_armed = (pos_ == TrapSensorPosition::ARMED_POSITION);
    u.changed = true;
    last_change_ms_ = now_ms;

    if (transitioning_) {
        // Changed again before the engine confirmed the trigger.
        transitioning_ = false;
        pos_ = pos;
        if (pos == TrapSensorPosition::ARMED_POSITION) {
            state_ = TrapState::ARMED;
            u.rearmed = true;
        } else {
            state_ = TrapState::NEEDS_REARMING;
        }
        return u;
    }

    if (state_ == TrapState::UNKNOWN) {
        // First known reading at boot: adopt without claiming a live event.
        pos_ = pos;
        state_ = (pos == TrapSensorPosition::ARMED_POSITION)
                     ? TrapState::ARMED
                     : TrapState::NEEDS_REARMING;
        return u;
    }

    pos_ = pos;
    if (pos == TrapSensorPosition::ARMED_POSITION) {
        // Mechanically back in the armed position -> physical re-arm.
        state_ = TrapState::ARMED;
        u.rearmed = true;
    } else if (prev_armed) {
        // Live armed -> triggered edge: a real activation.
        state_ = TrapState::TRIGGERED;
        transitioning_ = true;
        u.fresh_activation = true;
    } else {
        state_ = TrapState::NEEDS_REARMING;
    }
    return u;
}

void TrapManager::confirmTrigger(uint64_t now_ms) {
    transitioning_ = false;
    state_ = TrapState::NEEDS_REARMING;
    last_change_ms_ = now_ms;
}

void TrapManager::seedState(TrapState persisted) {
    state_ = persisted;
    transitioning_ = false;
    switch (persisted) {
        case TrapState::ARMED:
            pos_ = TrapSensorPosition::ARMED_POSITION;
            break;
        case TrapState::TRIGGERED:
        case TrapState::NEEDS_REARMING:
            pos_ = TrapSensorPosition::TRIGGERED_POSITION;
            break;
        default:
            pos_ = TrapSensorPosition::UNKNOWN;
            break;
    }
}

void TrapManager::reset() {
    pos_ = TrapSensorPosition::UNKNOWN;
    state_ = TrapState::UNKNOWN;
    transitioning_ = false;
    last_change_ms_ = 0;
}

}  // namespace rattrap