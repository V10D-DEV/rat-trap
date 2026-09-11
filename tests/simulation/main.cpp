// Command-line simulation of the rat-trap node brain.
//
// Runs the exact same platform-independent code that will later run on the
// ESP32-C3, with mock reporters/storage/power. Reproduces the canonical
// scenarios and shows what would be transmitted / stored / slept.
#include <cstdio>

#include "rattrap/Types.h"
#include "rattrap/battery/BatteryCalculator.h"
#include "rattrap/core/TrapNode.h"
#include "tests/mocks/MockNetworkReporter.h"
#include "tests/mocks/MockPowerManager.h"
#include "tests/mocks/MockStorage.h"

using namespace rattrap;

namespace {

void banner() {
    std::printf(
        "============================================================\n"
        "            Rat Trap Node - Core Logic Simulation           \n"
        "      platform-independent firmware foundation (no ESP32)    \n"
        "============================================================\n");
}

TrapNode::Config makeConfig() {
    TrapNode::Config c;
    c.identity = DeviceIdentity{"warehouse_01", 27};
    c.engine.episode_timeout_ms = 30000;            // 30 s motion window
    c.engine.pass_through_confirm_delay_ms = 500;   // 500 ms pass-through confirm
    c.engine.min_pulse_interval_ms = 50;            // 50 ms debounce
    c.engine.require_both_beams_for_pass_through = true;
    c.daily_report_interval_ms = 24ULL * 3600ULL * 1000ULL;  // 24 h
    c.firmware_version = kFirmwareVersion;
    return c;
}

struct Harness {
    MockNetworkReporter net;
    MockPowerManager power;
    MockStorage storage;
    TrapNode::Config cfg = makeConfig();
    TrapNode node;

    explicit Harness(uint64_t boot_ms = 1000) : node(cfg, net, power, storage) {
        node.start(boot_ms);
        // Physical trap starts in the armed position.
        node.handleSensorEvent({SensorType::TRAP_SENSOR, false, boot_ms});
        std::printf("  [boot %llu] trap sensor = ARMED position -> state ARMED\n",
                    (unsigned long long)boot_ms);
    }
};

EventResult feed(TrapNode& node, SensorType s, bool state, uint64_t now,
                 const char* label) {
    EventResult r = node.handleSensorEvent({s, state, now});
    std::printf("  [%6llu] %-11s %s", (unsigned long long)now, label,
                state ? "TRIGGERED" : "clear    ");
    std::printf("  -> %s (trap=%s)\n", eventTypeName(r.type),
                trapStateName(r.trap_state));
    return r;
}

void dumpNetwork(const Harness& h) {
    if (h.net.event_log.empty() && h.net.status_log.empty()) {
        std::printf("  [network] nothing would be sent\n");
        return;
    }
    std::printf("  [network] what would have been transmitted:\n");
    for (const auto& rec : h.net.event_log) {
        std::printf("    > sendEvent  %-24s (%s / trap %d) rearm=%d notify=%d\n",
                    eventTypeName(rec.result.type),
                    rec.identity.warehouse_id.c_str(), rec.identity.trap_number,
                    (int)rec.result.rearming_required,
                    (int)rec.result.notify_network);
    }
    for (const auto& rec : h.net.status_log) {
        std::printf("    > dailyStatus trap=%s battery=%.2fV %d%% low=%d "
                    "cnt(ir1,ir2,pass,trig,esc)=(%u,%u,%u,%u,%u)\n",
                    trapStateName(rec.status.trap_state), rec.status.battery.voltage,
                    rec.status.battery.percentage, (int)rec.status.battery.low_battery,
                    rec.status.counters.ir1_activity, rec.status.counters.ir2_activity,
                    rec.status.counters.passed_through,
                    rec.status.counters.trap_triggered,
                    rec.status.counters.escaped_after_trigger);
    }
}

void section(const char* title) {
    std::printf("\n--- %s ---\n", title);
}

// --- Canonical scenario 1: mouse passes through ---------------------------
void scenarioMousePassesThrough() {
    section("Scenario 1: Mouse passes through (IR1 -> IR2, trap stays ARMED)");
    Harness h(1000);
    feed(h.node, SensorType::IR1, true, 1100, "IR1");
    feed(h.node, SensorType::IR1, false, 1250, "IR1");
    feed(h.node, SensorType::IR2, true, 1400, "IR2");
    feed(h.node, SensorType::IR2, false, 1550, "IR2");
    EventResult r = h.node.poll(2300);  // 1550 + 500 confirm + margin
    std::printf("\n  Result: %s\n", eventTypeName(r.type));
    std::printf("  Trap state: %s\n", trapStateName(h.node.status(2300).trap_state));
    dumpNetwork(h);
}

// --- Canonical scenario 2: mouse triggers the trap ------------------------
void scenarioMouseTriggersTrap() {
    section("Scenario 2: Mouse triggers the trap (IR1 -> mechanism fires)");
    Harness h(1000);
    feed(h.node, SensorType::IR1, true, 1200, "IR1");
    feed(h.node, SensorType::IR1, false, 1350, "IR1");
    feed(h.node, SensorType::TRAP_SENSOR, true, 2000, "Trap s");
    std::printf("\n  Result: %s\n", eventTypeName(EventType::TRAP_TRIGGERED));
    std::printf("  Trap state: %s\n",
                trapStateName(h.node.status(2000).trap_state));
    dumpNetwork(h);
}

// --- Canonical scenario 3: trigger, then movement => escape ----------------
void scenarioTriggersAndEscapes() {
    section("Scenario 3: Mouse triggers the trap, then moves/escapes");
    Harness h(1000);
    feed(h.node, SensorType::IR1, true, 1500, "IR1");
    feed(h.node, SensorType::IR1, false, 1600, "IR1");
    feed(h.node, SensorType::TRAP_SENSOR, true, 2200, "Trap s");
    EventResult r = feed(h.node, SensorType::IR2, true, 8000, "IR2");
    feed(h.node, SensorType::IR2, false, 8100, "IR2");
    std::printf("\n  Result: %s\n", eventTypeName(r.type));
    std::printf("  Trap state: %s\n", trapStateName(h.node.status(8100).trap_state));
    dumpNetwork(h);
}

// --- Canonical scenario 4: manual rearm ------------------------------------
void scenarioManualRearm() {
    section("Scenario 4: Trap is manually re-armed");
    Harness h(1000);
    feed(h.node, SensorType::TRAP_SENSOR, true, 1200, "Trap s");  // sprung
    EventResult r = feed(h.node, SensorType::TRAP_SENSOR, false, 3000, "Trap s");
    std::printf("\n  Result: %s\n", eventTypeName(r.type));
    std::printf("  Trap state: %s\n", trapStateName(h.node.status(3000).trap_state));
    feed(h.node, SensorType::IR1, true, 4000, "IR1");   // normal operation resumes
    feed(h.node, SensorType::IR1, false, 4100, "IR1");
    feed(h.node, SensorType::IR2, true, 4200, "IR2");
    feed(h.node, SensorType::IR2, false, 4300, "IR2");
    EventResult pass = h.node.poll(5000);
    std::printf("\n  After rearm: %s (trap=%s)\n", eventTypeName(pass.type),
                trapStateName(h.node.status(5000).trap_state));
    dumpNetwork(h);
}

// --- Scenario 5: single beam, nothing happens ------------------------------
void scenarioSingleBeamTimeout() {
    section("Scenario 5: IR1 alone - sequence times out, trap stays ARMED");
    Harness h(1000);
    feed(h.node, SensorType::IR1, true, 1100, "IR1");
    feed(h.node, SensorType::IR1, false, 1200, "IR1");
    EventResult r = h.node.poll(1200 + 30000 + 1);
    std::printf("  [%6llu] poll: %s\n", (unsigned long long)(1200 + 30000 + 1),
                eventTypeName(r.type));
    std::printf("  Trap state: %s\n", trapStateName(h.node.status(1200 + 30000 + 1).trap_state));
    dumpNetwork(h);
}

// --- Scenario 6: noisy/duplicate sensor edges ------------------------------
void scenarioNoiseAndDuplicates() {
    section("Scenario 6: Duplicate / noisy sensor edges are filtered");
    Harness h(1000);
    feed(h.node, SensorType::IR1, true, 1100, "IR1");
    feed(h.node, SensorType::IR1, true, 1105, "IR1");    // duplicate block - ignored
    feed(h.node, SensorType::IR1, false, 1120, "IR1");
    feed(h.node, SensorType::IR1, true, 1130, "IR1");    // inside 50 ms cooldown
    feed(h.node, SensorType::IR1, true, 1250, "IR1");    // real edge again
    feed(h.node, SensorType::IR1, false, 1260, "IR1");
    EventResult r = h.node.poll(1260 + 30000 + 1);       // discard incomplete
    std::printf("  [%6llu] poll: %s (noise never produced an event)\n",
                (unsigned long long)(1260 + 30000 + 1), eventTypeName(r.type));
    std::printf("  Trap state: %s\n", trapStateName(h.node.status(1260 + 30000 + 1).trap_state));
    dumpNetwork(h);
}

// --- Scenario 7: unexpected trap activation --------------------------------
void scenarioUnexpectedTrigger() {
    section("Scenario 7: Trap fires with no IR (e.g. vibration/force)");
    Harness h(1000);
    EventResult r = feed(h.node, SensorType::TRAP_SENSOR, true, 3000, "Trap s");
    std::printf("\n  Result: %s (still reported, state must be fixed)\n",
                eventTypeName(r.type));
    std::printf("  Trap state: %s\n", trapStateName(h.node.status(3000).trap_state));
    dumpNetwork(h);
}

// --- Scenario 8: reboot / state restoration --------------------------------
void scenarioRebootRestore() {
    section("Scenario 8: Reboot restores counters and trap state");
    {
        Harness h(1000);
        feed(h.node, SensorType::IR1, true, 1200, "IR1");
        feed(h.node, SensorType::IR1, false, 1300, "IR1");
        feed(h.node, SensorType::IR2, true, 1400, "IR2");
        feed(h.node, SensorType::IR2, false, 1500, "IR2");
        h.node.poll(2200);  // PASSED_THROUGH
        feed(h.node, SensorType::TRAP_SENSOR, true, 3000, "Trap s");
        std::printf("  stored before reboot: ir1=%u ir2=%u pass=%u trig=%u state=%s\n",
                    h.storage.stored.counters.ir1_activity,
                    h.storage.stored.counters.ir2_activity,
                    h.storage.stored.counters.passed_through,
                    h.storage.stored.counters.trap_triggered,
                    trapStateName(h.storage.stored.trap_state));

        // Reboot with the SAME storage (same MockStorage instance).
        MockNetworkReporter net2;
        MockPowerManager power2;
        TrapNode::Config cfg2 = makeConfig();
        TrapNode node2(cfg2, net2, power2, h.storage);
        node2.start(10000);

        NodeStatus s = node2.status(10000);
        std::printf("  after reboot: state=%s counters=(ir1=%u,ir2=%u,pass=%u,trig=%u) "
                    "last_event=%s\n",
                    trapStateName(s.trap_state), s.counters.ir1_activity,
                    s.counters.ir2_activity, s.counters.passed_through,
                    s.counters.trap_triggered, eventTypeName(s.last_event));

        // Sensor confirms the sprung position - expected, no false trigger.
        EventResult r = node2.handleSensorEvent({SensorType::TRAP_SENSOR, true, 10000});
        std::printf("  sensor confirms sprung position -> %s (not re-reported)\n",
                    eventTypeName(r.type));
    }
}

// --- Scenario 9: battery + 24 h status + sleep intent -----------------------
void scenarioBatteryDailyStatus() {
    section("Scenario 9: Battery estimation and 24 h status scheduling");
    Harness h(1000);
    h.power.print = true;

    std::printf("  Li-ion curve samples (default 1S):\n");
    const BatteryConfig cfg = defaultLiIon1SConfig();
    const double samples[] = {4.15, 4.05, 3.95, 3.70, 3.40, 3.29};
    for (double v : samples) {
        BatteryStatus b = BatteryCalculator::evaluate(v, cfg);
        std::printf("    %.2f V -> %3d%%  low=%d\n", v, b.percentage, (int)b.low_battery);
    }

    h.node.updateBattery(BatteryCalculator::evaluate(3.95, cfg));

    const uint64_t day = h.cfg.daily_report_interval_ms;
    NodeStatus s1 = h.node.status(1000);
    std::printf("  current: battery=%.2fV %d%% low=%d next_daily_report due? %s\n",
                s1.battery.voltage, s1.battery.percentage,
                (int)s1.battery.low_battery,
                h.node.dailyReportDue(1000) ? "YES" : "no");
    h.node.maybeSendDailyReport(1000);

    std::printf("  after 23h59m: due? %s\n", h.node.dailyReportDue(day - 1000) ? "YES" : "no");
    h.node.maybeSendDailyReport(day - 1000);

    std::printf("  after 24h00m: due? %s -> sending\n",
                h.node.dailyReportDue(day + 1000) ? "YES" : "no");
    h.node.maybeSendDailyReport(day + 1000);

    std::printf("  application requests deep sleep for sensor OR 24 h wake:\n");
    h.node.requestSleep(day + 1000);
    dumpNetwork(h);
}

}  // namespace

int main() {
    banner();
    scenarioMousePassesThrough();
    scenarioMouseTriggersTrap();
    scenarioTriggersAndEscapes();
    scenarioManualRearm();
    scenarioSingleBeamTimeout();
    scenarioNoiseAndDuplicates();
    scenarioUnexpectedTrigger();
    scenarioRebootRestore();
    scenarioBatteryDailyStatus();
    std::printf("\nDone. (All logic ran on the PC; no ESP32/Arduino code involved.)\n");
    return 0;
}