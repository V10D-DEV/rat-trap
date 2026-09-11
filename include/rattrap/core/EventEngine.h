#pragma once

#include <cstdint>

#include "rattrap/Types.h"
#include "rattrap/core/TrapManager.h"

namespace rattrap {

struct EventEngineConfig {
    uint64_t episode_timeout_ms = 30000;             // no IR activity -> discard the sequence
    uint64_t pass_through_confirm_delay_ms = 500;    // all-clear, then wait this window; a trap snap here wins
    uint64_t min_pulse_interval_ms = 50;             // per-sensor debounce cooldown
    bool require_both_beams_for_pass_through = true;
};

// Interprets the sensor-sample stream into logical events using temporal
// context and timeouts. All timing comes from caller-supplied timestamps;
// this class never sleeps or blocks.
//
// Internal motion state machine:
//            IDLE
//             |   IR beam blocked (edge)
//             v
//     MOTION_DETECTED  <-- recurring IR activity refreshes the timeout
//       |                     |
//       | trap activates      | nothing for episode_timeout_ms
//       v                     v
//   TRAP_TRIGGERED        IDLE (sequence discarded, trap stays ARMED)
//   (then NEEDS_REARMING, any IR after that = ESCAPED_AFTER_TRIGGER)
//
// Pass-through is only confirmed once every needed IR beam has been blocked
// and cleared again, the trap is still ARMED, and the confirm delay elapsed.
class EventEngine {
public:
    EventEngine(const EventEngineConfig& config, TrapManager& trap);

    EventResult processSensorEvent(const SensorEvent& ev);
    EventResult poll(uint64_t now_ms);

    TrapState trapState() const noexcept { return trap_.state(); }
    const EventEngineConfig& config() const noexcept { return config_; }

    void reset();

private:
    enum class MotionPhase { IDLE, MOTION_DETECTED };

    EventResult handleIrEdge(SensorType beam, bool rising, uint64_t now_ms);
    EventResult handleTrapReading(bool triggered, uint64_t now_ms);
    EventResult maybeFinishPassThrough(uint64_t now_ms);
    void beginEpisode(SensorType beam, uint64_t now_ms);
    void endEpisode();
    bool trapSprung() const noexcept {
        return trap_.state() == TrapState::TRIGGERED ||
               trap_.state() == TrapState::NEEDS_REARMING;
    }

    EventEngineConfig config_;
    TrapManager& trap_;

    MotionPhase phase_ = MotionPhase::IDLE;
    bool beam_broken_[2] = {false, false};
    bool beams_seen_[2] = {false, false};
    bool pass_through_pending_ = false;
    uint64_t pass_through_check_ms_ = 0;
    uint64_t last_activity_ms_ = 0;
    uint64_t last_pulse_ms_[2] = {0, 0};
    uint64_t last_event_ts_ms_ = 0;
    bool escape_reported_ = false;
};

}  // namespace rattrap