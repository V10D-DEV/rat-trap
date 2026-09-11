#pragma once

#include <cstdint>

namespace rattrap {

// What the application wants from the power subsystem.
enum class SleepIntent {
    WAKE_ON_SENSOR,           // interrupt on any sensor edge
    WAKE_AFTER_INTERVAL,      // RTC/deep-sleep timer only (e.g. 24 h report)
    WAKE_ON_SENSOR_OR_INTERVAL
};

// Abstract deep-sleep control. The application requests the *intent*; the
// implementation decides the actual hardware mechanics.
//
// TODO(esp32-c3): implement with ESP32 deep sleep + RTC timer + EXT1 wake on
// the sensor pins. wake_at_ms is only meaningful for the INTERVAL intents
// (pass 0 for WAKE_ON_SENSOR).
class PowerManager {
public:
    virtual ~PowerManager() = default;

    virtual void sleep(SleepIntent intent, uint64_t now_ms, uint64_t wake_at_ms) = 0;
};

}  // namespace rattrap