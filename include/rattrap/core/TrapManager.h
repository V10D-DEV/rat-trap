#pragma once

#include <cstdint>

#include "rattrap/Types.h"

namespace rattrap {

// Logical position of the trap's mechanical sensor. The ESP32-C3 layer
// converts its electrical polarity into these values before feeding the core.
enum class TrapSensorPosition {
    UNKNOWN,
    ARMED_POSITION,
    TRIGGERED_POSITION
};

// Owns the persistent mechanical trap state. It only changes when the trap
// sensor changes position. A triggered trap NEVER automatically becomes armed;
// it stays NEEDS_REARMING until the sensor reports the armed position again.
class TrapManager {
public:
    TrapManager();

    TrapState state() const noexcept;
    bool rearmingRequired() const noexcept;
    TrapSensorPosition position() const noexcept;
    uint64_t lastChangeMs() const noexcept;

    struct SensorUpdate {
        bool changed = false;           // position differs from previous known
        bool fresh_activation = false;  // armed -> triggered observed live now
        bool rearmed = false;           // triggered -> armed observed now
    };

    // Apply a new logical reading of the trap sensor.
    SensorUpdate notifyPosition(TrapSensorPosition pos, uint64_t now_ms);

    // Called by the EventEngine once a live activation is classified; settles
    // the transient TRIGGERED state to the persistent NEEDS_REARMING.
    void confirmTrigger(uint64_t now_ms);

    // Seed from persisted state before the first sensor sample is available.
    void seedState(TrapState persisted);

    void reset();

private:
    TrapSensorPosition pos_ = TrapSensorPosition::UNKNOWN;
    TrapState state_ = TrapState::UNKNOWN;
    uint64_t last_change_ms_ = 0;
    bool transitioning_ = false;
};

}  // namespace rattrap