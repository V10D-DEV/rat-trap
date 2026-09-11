#include "rattrap/core/TrapNode.h"

namespace rattrap {

TrapNode::TrapNode(const Config& config,
                   NetworkReporter& network,
                   PowerManager& power,
                   Storage& storage)
    : config_(config),
      trap_(),
      engine_(config_.engine, trap_),
      state_(config_.identity, config_.firmware_version),
      network_(network),
      power_(power),
      storage_(storage) {}

void TrapNode::start(uint64_t now_ms) {
    boot_time_ms_ = now_ms;
    last_daily_report_ms_ = now_ms;

    PersistentState persisted;
    if (storage_.load(persisted)) {
        state_.restore(persisted);
        trap_.seedState(persisted.trap_state);
    }
    syncTrapState();
}

EventResult TrapNode::handleSensorEvent(const SensorEvent& ev) {
    EventResult r = engine_.processSensorEvent(ev);
    state_.apply(r, ev.timestamp_ms);
    syncTrapState();

    if (r.notify_network) {
        reportToNetwork(r);
    }
    if (r.type != EventType::NONE && r.type != EventType::UNKNOWN) {
        storage_.save(state_.persist());
    }
    return r;
}

EventResult TrapNode::poll(uint64_t now_ms) {
    EventResult r = engine_.poll(now_ms);
    state_.apply(r, now_ms);
    syncTrapState();

    if (r.notify_network) {
        reportToNetwork(r);
    }
    if (r.type != EventType::NONE && r.type != EventType::UNKNOWN) {
        storage_.save(state_.persist());
    }
    return r;
}

NodeStatus TrapNode::status(uint64_t now_ms) const {
    NodeStatus s = state_.snapshot();
    s.identity = config_.identity;
    s.trap_state = trap_.state();
    s.rearming_required = trap_.rearmingRequired();
    s.firmware_version = config_.firmware_version;
    s.uptime_ms = now_ms > boot_time_ms_ ? now_ms - boot_time_ms_ : 0;
    s.next_daily_report_ms = nextWakeAtMs();
    return s;
}

bool TrapNode::dailyReportDue(uint64_t now_ms) const {
    return now_ms >= last_daily_report_ms_ + config_.daily_report_interval_ms;
}

void TrapNode::maybeSendDailyReport(uint64_t now_ms) {
    if (!dailyReportDue(now_ms)) {
        return;
    }
    last_daily_report_ms_ = now_ms;
    network_.sendDailyStatus(status(now_ms));
}

void TrapNode::requestSleep(uint64_t now_ms) const {
    power_.sleep(SleepIntent::WAKE_ON_SENSOR_OR_INTERVAL, now_ms, nextWakeAtMs());
}

void TrapNode::syncTrapState() {
    state_.setTrapState(trap_.state());
}

void TrapNode::reportToNetwork(const EventResult& result) {
    network_.sendEvent(config_.identity, result);
}

}  // namespace rattrap