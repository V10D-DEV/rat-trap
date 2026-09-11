#include "rattrap/core/EventEngine.h"

#include <cstddef>

namespace rattrap {

namespace {

std::size_t beamIndex(SensorType s) {
    return s == SensorType::IR2 ? 1 : 0;
}

EventResult noneResult(TrapState ts, bool rearming) {
    EventResult r;
    r.type = EventType::NONE;
    r.trap_state = ts;
    r.rearming_required = rearming;
    return r;
}

}  // namespace

EventEngine::EventEngine(const EventEngineConfig& config, TrapManager& trap)
    : config_(config), trap_(trap) {}

void EventEngine::reset() {
    phase_ = MotionPhase::IDLE;
    beam_broken_[0] = beam_broken_[1] = false;
    beams_seen_[0] = beams_seen_[1] = false;
    pass_through_pending_ = false;
    pass_through_check_ms_ = 0;
    last_activity_ms_ = 0;
    last_pulse_ms_[0] = last_pulse_ms_[1] = 0;
    last_event_ts_ms_ = 0;
    escape_reported_ = false;
}

EventResult EventEngine::processSensorEvent(const SensorEvent& ev) {
    if (ev.timestamp_ms < last_event_ts_ms_) {
        return noneResult(trap_.state(), trap_.rearmingRequired());
    }
    last_event_ts_ms_ = ev.timestamp_ms;

    switch (ev.sensor) {
        case SensorType::IR1:
        case SensorType::IR2:
            return handleIrEdge(ev.sensor, ev.state, ev.timestamp_ms);
        case SensorType::TRAP_SENSOR:
            return handleTrapReading(ev.state, ev.timestamp_ms);
    }
    return noneResult(trap_.state(), trap_.rearmingRequired());
}

EventResult EventEngine::poll(uint64_t now_ms) {
    if (trap_.state() == TrapState::TRIGGERED) {
        trap_.confirmTrigger(now_ms);
    }

    EventResult r = maybeFinishPassThrough(now_ms);
    if (r.type != EventType::NONE) {
        return r;
    }

    if (phase_ == MotionPhase::MOTION_DETECTED &&
        now_ms - last_activity_ms_ >= config_.episode_timeout_ms) {
        // Nothing more happened: discard the incomplete sequence.
        endEpisode();
    }

    return noneResult(trap_.state(), trap_.rearmingRequired());
}

EventResult EventEngine::handleIrEdge(SensorType beam, bool rising, uint64_t now_ms) {
    const std::size_t idx = beamIndex(beam);

    if (rising) {
        // Coarse per-sensor debounce.
        if (now_ms - last_pulse_ms_[idx] < config_.min_pulse_interval_ms) {
            return noneResult(trap_.state(), trap_.rearmingRequired());
        }
        last_pulse_ms_[idx] = now_ms;

        if (beam_broken_[idx]) {
            // Duplicate "blocked" while already blocked -> noise.
            return noneResult(trap_.state(), trap_.rearmingRequired());
        }
        beam_broken_[idx] = true;
        last_activity_ms_ = now_ms;

        if (trapSprung()) {
            // Movement while the mechanism is sprung -> likely an escape.
            EventResult r;
            r.type = escape_reported_ ? EventType::IR_ACTIVITY
                                      : EventType::ESCAPED_AFTER_TRIGGER;
            if (r.type == EventType::ESCAPED_AFTER_TRIGGER) {
                escape_reported_ = true;
            }
            r.trap_state = TrapState::NEEDS_REARMING;
            r.cause = beam;
            r.rearming_required = true;
            r.notify_network = (r.type == EventType::ESCAPED_AFTER_TRIGGER);
            return r;
        }

        beams_seen_[idx] = true;
        if (phase_ == MotionPhase::IDLE) {
            beginEpisode(beam, now_ms);
        } else {
            last_activity_ms_ = now_ms;
            pass_through_pending_ = false;  // a beam is busy again; re-check on clear
        }

        EventResult r = noneResult(trap_.state(), trap_.rearmingRequired());
        r.type = EventType::IR_ACTIVITY;
        r.cause = beam;
        return r;
    }

    // Falling edge.
    if (!beam_broken_[idx]) {
        return noneResult(trap_.state(), trap_.rearmingRequired());  // duplicate clear
    }
    beam_broken_[idx] = false;
    last_activity_ms_ = now_ms;

    if (trapSprung()) {
        return noneResult(trap_.state(), trap_.rearmingRequired());
    }

    // Re-evaluate the pass-through confirmation once every beam is clear.
    if (!beam_broken_[0] && !beam_broken_[1]) {
        const bool traversalSeen =
            config_.require_both_beams_for_pass_through
                ? (beams_seen_[0] && beams_seen_[1])
                : (beams_seen_[0] || beams_seen_[1]);
        if (phase_ == MotionPhase::MOTION_DETECTED && traversalSeen) {
            pass_through_pending_ = true;
            pass_through_check_ms_ = now_ms + config_.pass_through_confirm_delay_ms;
        }
    }

    return noneResult(trap_.state(), trap_.rearmingRequired());
}

EventResult EventEngine::handleTrapReading(bool triggered, uint64_t now_ms) {
    const TrapSensorPosition pos = triggered ? TrapSensorPosition::TRIGGERED_POSITION
                                             : TrapSensorPosition::ARMED_POSITION;

    EventResult r = noneResult(trap_.state(), trap_.rearmingRequired());
    TrapManager::SensorUpdate up = trap_.notifyPosition(pos, now_ms);
    if (!up.changed) {
        return r;
    }

    if (up.fresh_activation) {
        trap_.confirmTrigger(now_ms);
        endEpisode();
        r.type = EventType::TRAP_TRIGGERED;
        r.cause = SensorType::TRAP_SENSOR;
        r.trap_state = TrapState::NEEDS_REARMING;
        r.rearming_required = true;
        r.notify_network = true;
        return r;
    }

    if (up.rearmed) {
        escape_reported_ = false;
        endEpisode();
        r.type = EventType::TRAP_REARMED;
        r.cause = SensorType::TRAP_SENSOR;
        r.trap_state = TrapState::ARMED;
        r.rearming_required = false;
        r.notify_network = false;
        return r;
    }

    // First known position (boot) or an armed->triggered edge that was not
    // observed live: adopt the state silently.
    r.trap_state = trap_.state();
    r.rearming_required = trap_.rearmingRequired();
    return r;
}

EventResult EventEngine::maybeFinishPassThrough(uint64_t now_ms) {
    EventResult r = noneResult(trap_.state(), trap_.rearmingRequired());
    if (!pass_through_pending_) {
        return r;
    }
    if (trap_.state() != TrapState::ARMED) {
        // The trap snapped inside the confirmation window: pass-through
        // is cancelled in favour of the trap activation.
        pass_through_pending_ = false;
        return r;
    }
    if (now_ms < pass_through_check_ms_) {
        return r;
    }

    r.type = EventType::PASSED_THROUGH;
    r.trap_state = TrapState::ARMED;
    r.rearming_required = false;
    r.notify_network = true;
    endEpisode();
    return r;
}

void EventEngine::beginEpisode(SensorType beam, uint64_t now_ms) {
    phase_ = MotionPhase::MOTION_DETECTED;
    beams_seen_[0] = beams_seen_[1] = false;
    beams_seen_[beamIndex(beam)] = true;
    pass_through_pending_ = false;
    pass_through_check_ms_ = 0;
    last_activity_ms_ = now_ms;
}

void EventEngine::endEpisode() {
    phase_ = MotionPhase::IDLE;
    beam_broken_[0] = beam_broken_[1] = false;
    beams_seen_[0] = beams_seen_[1] = false;
    pass_through_pending_ = false;
    pass_through_check_ms_ = 0;
    last_activity_ms_ = 0;
}

}  // namespace rattrap