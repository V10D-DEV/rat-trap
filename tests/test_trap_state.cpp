// Unit tests for TrapManager (persistent mechanical state) and NodeState
// (counters, status, persist/restore round-trip via MockStorage).
#include <cstdio>

#include "rattrap/core/NodeState.h"
#include "rattrap/core/TrapManager.h"
#include "tests/TestFramework.h"
#include "tests/mocks/MockStorage.h"

using namespace rattrap;

static void testInitialUnknown() {
    TrapManager tm;
    CHECK(tm.state() == TrapState::UNKNOWN);
    CHECK(!tm.rearmingRequired());
}

static void testArmFromUnknown() {
    TrapManager tm;
    TrapManager::SensorUpdate up = tm.notifyPosition(TrapSensorPosition::ARMED_POSITION, 100);
    CHECK(up.changed);
    CHECK(!up.fresh_activation);
    CHECK(!up.rearmed);
    CHECK(tm.state() == TrapState::ARMED);
}

static void testTriggerFlow() {
    TrapManager tm;
    tm.notifyPosition(TrapSensorPosition::ARMED_POSITION, 0);

    TrapManager::SensorUpdate up = tm.notifyPosition(TrapSensorPosition::TRIGGERED_POSITION, 500);
    CHECK(up.changed);
    CHECK(up.fresh_activation);
    CHECK(tm.state() == TrapState::TRIGGERED);      // transient until confirmed

    tm.confirmTrigger(600);
    CHECK(tm.state() == TrapState::NEEDS_REARMING); // persistent until re-arm
    CHECK(tm.rearmingRequired());

    // Duplicate triggered reading: no change, no duplicate event.
    up = tm.notifyPosition(TrapSensorPosition::TRIGGERED_POSITION, 700);
    CHECK(!up.changed);
    CHECK(!up.fresh_activation);
}

static void testRearm() {
    TrapManager tm;
    tm.notifyPosition(TrapSensorPosition::ARMED_POSITION, 0);
    tm.notifyPosition(TrapSensorPosition::TRIGGERED_POSITION, 100);
    tm.confirmTrigger(101);
    CHECK(tm.rearmingRequired());

    TrapManager::SensorUpdate up = tm.notifyPosition(TrapSensorPosition::ARMED_POSITION, 200);
    CHECK(up.changed);
    CHECK(up.rearmed);
    CHECK(tm.state() == TrapState::ARMED);
    CHECK(!tm.rearmingRequired());
}

static void testUnconfirmedBounceReturnsToArmed() {
    TrapManager tm;
    tm.notifyPosition(TrapSensorPosition::ARMED_POSITION, 0);
    tm.notifyPosition(TrapSensorPosition::TRIGGERED_POSITION, 10);   // transient
    TrapManager::SensorUpdate up = tm.notifyPosition(TrapSensorPosition::ARMED_POSITION, 20);
    CHECK(up.rearmed);
    CHECK(tm.state() == TrapState::ARMED);
}

static void testUnexpectedSprungAtBootWithoutState() {
    TrapManager tm;
    TrapManager::SensorUpdate up = tm.notifyPosition(TrapSensorPosition::TRIGGERED_POSITION, 0);
    CHECK(up.changed);
    CHECK(!up.fresh_activation);   // not observed as a live armed->triggered edge
    CHECK(tm.state() == TrapState::NEEDS_REARMING);
}

static void testSeedState() {
    TrapManager tm;
    tm.seedState(TrapState::NEEDS_REARMING);
    CHECK(tm.state() == TrapState::NEEDS_REARMING);
    CHECK(tm.position() == TrapSensorPosition::TRIGGERED_POSITION);

    // Consistent with a sprung sensor: identical reading, no event.
    TrapManager::SensorUpdate up = tm.notifyPosition(TrapSensorPosition::TRIGGERED_POSITION, 500);
    CHECK(!up.changed);
}

static void testReset() {
    TrapManager tm;
    tm.notifyPosition(TrapSensorPosition::TRIGGERED_POSITION, 0);
    tm.reset();
    CHECK(tm.state() == TrapState::UNKNOWN);
    CHECK(tm.position() == TrapSensorPosition::UNKNOWN);
}

static void testNodeStateCounters() {
    DeviceIdentity id{"warehouse_01", 27};
    NodeState ns(id, "0.1.0");

    EventResult r_ir1;
    r_ir1.type = EventType::IR_ACTIVITY; r_ir1.cause = SensorType::IR1;
    r_ir1.trap_state = TrapState::ARMED;
    ns.apply(r_ir1, 100);

    EventResult r_ir2;
    r_ir2.type = EventType::IR_ACTIVITY; r_ir2.cause = SensorType::IR2;
    r_ir2.trap_state = TrapState::ARMED;
    ns.apply(r_ir2, 200);

    EventResult r_pass;
    r_pass.type = EventType::PASSED_THROUGH; r_pass.trap_state = TrapState::ARMED;
    ns.apply(r_pass, 900);

    EventResult r_trig;
    r_trig.type = EventType::TRAP_TRIGGERED;
    r_trig.trap_state = TrapState::NEEDS_REARMING;
    r_trig.rearming_required = true;
    ns.apply(r_trig, 1000);

    EventResult r_esc;
    r_esc.type = EventType::ESCAPED_AFTER_TRIGGER;
    r_esc.cause = SensorType::IR1;
    r_esc.trap_state = TrapState::NEEDS_REARMING;
    r_esc.rearming_required = true;
    ns.apply(r_esc, 2000);

    const EventCounters& c = ns.counters();
    CHECK_EQ(c.ir1_activity, 2);
    CHECK_EQ(c.ir2_activity, 1);
    CHECK_EQ(c.passed_through, 1);
    CHECK_EQ(c.trap_triggered, 1);
    CHECK_EQ(c.escaped_after_trigger, 1);
    CHECK(ns.trapState() == TrapState::NEEDS_REARMING);
    CHECK(ns.rearmingRequired());
    CHECK(ns.lastEvent() == EventType::ESCAPED_AFTER_TRIGGER);
    CHECK_EQ(ns.lastEventTimeMs(), 2000);
}

static void testPersistRoundTrip() {
    DeviceIdentity id{"warehouse_01", 27};
    NodeState ns(id, "0.1.0");
    ns.setTrapState(TrapState::ARMED);

    EventResult r;
    r.type = EventType::TRAP_TRIGGERED;
    r.trap_state = TrapState::NEEDS_REARMING;
    r.rearming_required = true;
    ns.apply(r, 5000);
    ns.setBattery(BatteryStatus{3.71, 51, false});

    MockStorage storage;
    CHECK(storage.save(ns.persist()));

    NodeState restored(id, "0.1.0");
    PersistentState loaded;
    CHECK(storage.load(loaded));
    restored.restore(loaded);

    const NodeStatus a = ns.snapshot();
    const NodeStatus b = restored.snapshot();
    CHECK_EQ(a.counters.trap_triggered, b.counters.trap_triggered);
    CHECK(a.last_event == b.last_event);
    CHECK_EQ(a.last_event_time_ms, b.last_event_time_ms);
    CHECK(a.trap_state == b.trap_state);
    CHECK(a.rearming_required);
    CHECK_EQ(b.trap_state, TrapState::NEEDS_REARMING);
}

static void testSnapshotFields() {
    DeviceIdentity id{"warehouse_01", 27};
    NodeState ns(id, "1.2.3");
    ns.setTrapState(TrapState::ARMED);
    ns.setBattery(BatteryStatus{4.05, 88, false});
    ns.setUptime(1234);
    ns.setNextDailyReportMs(86400000);

    NodeStatus s = ns.snapshot();
    CHECK_EQ(s.identity.trap_number, 27);
    CHECK_EQ(s.identity.warehouse_id, std::string("warehouse_01"));
    CHECK(s.trap_state == TrapState::ARMED);
    CHECK_EQ(s.battery.percentage, 88);
    CHECK_EQ(s.uptime_ms, 1234);
    CHECK_EQ(s.next_daily_report_ms, 86400000);
    CHECK_EQ(s.firmware_version, std::string("1.2.3"));
}

int main() {
    std::printf("== test_trap_state ==\n");
    testInitialUnknown();
    testArmFromUnknown();
    testTriggerFlow();
    testRearm();
    testUnconfirmedBounceReturnsToArmed();
    testUnexpectedSprungAtBootWithoutState();
    testSeedState();
    testReset();
    testNodeStateCounters();
    testPersistRoundTrip();
    testSnapshotFields();
    std::printf("  %d checks, %d failures\n", testfw::gChecks(), testfw::gFailures());
    return testfw::gFailures() == 0 ? 0 : 1;
}