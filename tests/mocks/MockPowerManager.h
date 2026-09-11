#pragma once

#include <cstdio>

#include "rattrap/interfaces/PowerManager.h"

namespace rattrap {

class MockPowerManager final : public PowerManager {
public:
    void sleep(SleepIntent intent, uint64_t now_ms, uint64_t wake_at_ms) override {
        ++sleep_count;
        last_intent = intent;
        last_now_ms = now_ms;
        last_wake_at_ms = wake_at_ms;
        if (print) {
            std::printf("  > sleep: intent=%s now=%llu wake_at=%llu\n",
                        intent == SleepIntent::WAKE_ON_SENSOR ? "SENSOR" :
                        intent == SleepIntent::WAKE_AFTER_INTERVAL ? "INTERVAL" :
                        "SENSOR_OR_INTERVAL",
                        (unsigned long long)now_ms,
                        (unsigned long long)wake_at_ms);
        }
    }

    size_t sleep_count = 0;
    SleepIntent last_intent = SleepIntent::WAKE_ON_SENSOR;
    uint64_t last_now_ms = 0;
    uint64_t last_wake_at_ms = 0;
    bool print = false;
};

}  // namespace rattrap