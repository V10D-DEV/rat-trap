#pragma once

#include <string>

#include "rattrap/Types.h"
#include "rattrap/core/EventEngine.h"
#include "rattrap/core/NodeState.h"
#include "rattrap/core/TrapManager.h"
#include "rattrap/interfaces/NetworkReporter.h"
#include "rattrap/interfaces/PowerManager.h"
#include "rattrap/interfaces/Storage.h"

namespace rattrap {

// Thin facade that wires the brain together and is the integration seam for
// the future ESP32-C3 adapter layer:
//
//   - ESP32 GPIO/isr callback  -> handleSensorEvent()
//   - ESP32 timer/loop         -> poll()
//   - ADC + voltage divider    -> updateBattery()
//   - RTC/alarm                -> dailyReportDue()/maybeSendDailyReport()
//   - deep sleep               -> requestSleep()
class TrapNode {
public:
    struct Config {
        DeviceIdentity identity;
        EventEngineConfig engine = EventEngineConfig{};
        uint64_t daily_report_interval_ms = 24ULL * 3600ULL * 1000ULL;
        std::string firmware_version = kFirmwareVersion;
    };

    TrapNode(const Config& config,
             NetworkReporter& network,
             PowerManager& power,
             Storage& storage);

    // Restores persisted state (counters, last event, trap state) at boot.
    void start(uint64_t now_ms);

    EventResult handleSensorEvent(const SensorEvent& ev);
    EventResult poll(uint64_t now_ms);

    void updateBattery(const BatteryStatus& battery) { state_.setBattery(battery); }

    NodeStatus status(uint64_t now_ms) const;

    bool dailyReportDue(uint64_t now_ms) const;
    void maybeSendDailyReport(uint64_t now_ms);
    bool saveNow() { return storage_.save(state_.persist()); }

    // Asks the PowerManager for "sleep, wake on sensor or after 24 h".
    void requestSleep(uint64_t now_ms) const;
    uint64_t nextWakeAtMs() const {
        return last_daily_report_ms_ + config_.daily_report_interval_ms;
    }

    const TrapManager& trapManager() const noexcept { return trap_; }
    const EventEngine& engine() const noexcept { return engine_; }
    const NodeState& state() const noexcept { return state_; }

private:
    void syncTrapState();
    void reportToNetwork(const EventResult& result);

    Config config_;
    TrapManager trap_;
    EventEngine engine_;
    NodeState state_;
    NetworkReporter& network_;
    PowerManager& power_;
    Storage& storage_;

    uint64_t boot_time_ms_ = 0;
    uint64_t last_daily_report_ms_ = 0;
};

}  // namespace rattrap