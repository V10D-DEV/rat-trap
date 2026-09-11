// Unit tests for EventEngine: sequences, edges, noise, timeouts, trap logic.
#include <cstdio>

#include "rattrap/core/EventEngine.h"
#include "rattrap/core/TrapManager.h"
#include "tests/TestFramework.h"

using namespace rattrap;

static EventEngine makeEngine(TrapManager& tm,
                              uint64_t timeout = 30000,
                              uint64_t confirm = 500,
                              uint64_t minpulse = 50) {
    EventEngineConfig c;
    c.episode_timeout_ms = timeout;
    c.pass_through_confirm_delay_ms = confirm;
    c.min_pulse_interval_ms = minpulse;
    c.require_both_beams_for_pass_through = true;
    return EventEngine(c, tm);
}

static void arm(EventEngine& e, uint64_t t = 10) {
    e.processSensorEvent({SensorType::TRAP_SENSOR, false, t});
    CHECK(e.trapState() == TrapState::ARMED);
}

static void testIr1AloneTimesOut() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);

    CHECK(e.processSensorEvent({SensorType::IR1, true, 100}).type == EventType::IR_ACTIVITY);
    CHECK(e.processSensorEvent({SensorType::IR1, false, 150}).type == EventType::NONE);
    CHECK(e.trapState() == TrapState::ARMED);

    // Inside the window: nothing.
    CHECK(e.poll(200).type == EventType::NONE);
    CHECK(e.trapState() == TrapState::ARMED);

    // Timeout: the incomplete sequence is discarded.
    CHECK(e.poll(150 + 30000 + 1).type == EventType::NONE);
    CHECK(e.trapState() == TrapState::ARMED);
}

static void testIr2AloneTimesOut() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);
    CHECK(e.processSensorEvent({SensorType::IR2, true, 100}).type == EventType::IR_ACTIVITY);
    CHECK(e.processSensorEvent({SensorType::IR2, false, 150}).type == EventType::NONE);
    CHECK(e.poll(150 + 30000 + 1).type == EventType::NONE);
    CHECK(e.trapState() == TrapState::ARMED);
}

static void testPassThroughIr1ThenIr2() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);

    CHECK(e.processSensorEvent({SensorType::IR1, true, 100}).type == EventType::IR_ACTIVITY);
    CHECK(e.processSensorEvent({SensorType::IR1, false, 200}).type == EventType::NONE);
    CHECK(e.processSensorEvent({SensorType::IR2, true, 300}).type == EventType::IR_ACTIVITY);
    CHECK(e.processSensorEvent({SensorType::IR2, false, 400}).type == EventType::NONE);

    // Confirm delay is 500 ms after the beam clears (clear @400 -> check @900).
    CHECK(e.poll(500).type == EventType::NONE);
    EventResult r = e.poll(900);
    CHECK(r.type == EventType::PASSED_THROUGH);
    CHECK(r.trap_state == TrapState::ARMED);
    CHECK(!r.rearming_required);
    CHECK(r.notify_network);

    // A fresh episode afterwards works again.
    CHECK(e.processSensorEvent({SensorType::IR1, true, 1000}).type == EventType::IR_ACTIVITY);
    CHECK(e.trapState() == TrapState::ARMED);
}

static void testPassThroughIr2ThenIr1() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);
    e.processSensorEvent({SensorType::IR2, true, 100});
    e.processSensorEvent({SensorType::IR2, false, 200});
    e.processSensorEvent({SensorType::IR1, true, 300});
    e.processSensorEvent({SensorType::IR1, false, 400});
    EventResult r = e.poll(900);
    CHECK(r.type == EventType::PASSED_THROUGH);
    CHECK(e.trapState() == TrapState::ARMED);
}

static void testIrThenTrapActivation() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);

    e.processSensorEvent({SensorType::IR1, true, 100});
    e.processSensorEvent({SensorType::IR1, false, 200});

    EventResult r = e.processSensorEvent({SensorType::TRAP_SENSOR, true, 400});
    CHECK(r.type == EventType::TRAP_TRIGGERED);
    CHECK(r.rearming_required);
    CHECK(e.trapState() == TrapState::NEEDS_REARMING);

    // Repeated triggered reading: no duplicate event.
    EventResult r2 = e.processSensorEvent({SensorType::TRAP_SENSOR, true, 500});
    CHECK(r2.type == EventType::NONE);
}

static void testUnexpectedTrapActivationWithoutIr() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);
    EventResult r = e.processSensorEvent({SensorType::TRAP_SENSOR, true, 100});
    CHECK(r.type == EventType::TRAP_TRIGGERED);
    CHECK(e.trapState() == TrapState::NEEDS_REARMING);
}

static void testTriggerThenEscape() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);

    e.processSensorEvent({SensorType::IR1, true, 100});
    e.processSensorEvent({SensorType::IR1, false, 200});
    EventResult trig = e.processSensorEvent({SensorType::TRAP_SENSOR, true, 300});
    CHECK(trig.type == EventType::TRAP_TRIGGERED);

    // Further IR activity after the trigger = escape.
    EventResult esc = e.processSensorEvent({SensorType::IR2, true, 500});
    CHECK(esc.type == EventType::ESCAPED_AFTER_TRIGGER);
    CHECK(esc.trap_state == TrapState::NEEDS_REARMING);
    CHECK(esc.rearming_required);
    CHECK(esc.notify_network);

    // Later IR activity is de-duplicated: reported as plain activity only.
    EventResult esc2 = e.processSensorEvent({SensorType::IR1, true, 700});
    CHECK(esc2.type == EventType::IR_ACTIVITY);
}

static void testRearmResetsEscape() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);

    e.processSensorEvent({SensorType::IR1, true, 100});
    e.processSensorEvent({SensorType::IR1, false, 200});
    e.processSensorEvent({SensorType::TRAP_SENSOR, true, 300});
    e.processSensorEvent({SensorType::IR2, true, 400});   // escape reported
    CHECK(e.trapState() == TrapState::NEEDS_REARMING);

    EventResult r = e.processSensorEvent({SensorType::TRAP_SENSOR, false, 500});
    CHECK(r.type == EventType::TRAP_REARMED);
    CHECK(r.trap_state == TrapState::ARMED);
    CHECK(e.trapState() == TrapState::ARMED);

    // After rearming, IR is normal activity again, not an escape.
    EventResult r2 = e.processSensorEvent({SensorType::IR1, true, 600});
    CHECK(r2.type == EventType::IR_ACTIVITY);
}

static void testPassThroughCancelledByTrigger() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);

    e.processSensorEvent({SensorType::IR1, true, 100});
    e.processSensorEvent({SensorType::IR1, false, 200});
    e.processSensorEvent({SensorType::IR2, true, 300});
    e.processSensorEvent({SensorType::IR2, false, 400});   // pass-through pending @900

    // Trap snaps inside the confirmation window: trigger wins.
    EventResult trig = e.processSensorEvent({SensorType::TRAP_SENSOR, true, 600});
    CHECK(trig.type == EventType::TRAP_TRIGGERED);

    EventResult late = e.poll(950);
    CHECK(late.type != EventType::PASSED_THROUGH);
    CHECK(late.type == EventType::NONE);
    CHECK(e.trapState() == TrapState::NEEDS_REARMING);
}

static void testNoiseAndDuplicates() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);

    CHECK(e.processSensorEvent({SensorType::IR1, true, 100}).type == EventType::IR_ACTIVITY);

    // Duplicate "blocked" while already blocked.
    CHECK(e.processSensorEvent({SensorType::IR1, true, 110}).type == EventType::NONE);

    CHECK(e.processSensorEvent({SensorType::IR1, false, 120}).type == EventType::NONE);

    // Rising edge within the 50 ms cooldown is ignored as noise.
    CHECK(e.processSensorEvent({SensorType::IR1, true, 125}).type == EventType::NONE);

    // After the cooldown a real edge is accepted again.
    CHECK(e.processSensorEvent({SensorType::IR1, true, 200}).type == EventType::IR_ACTIVITY);
    CHECK(e.processSensorEvent({SensorType::IR1, false, 210}).type == EventType::NONE);

    // Discard on timeout, trap untouched.
    CHECK(e.poll(210 + 30000 + 1).type == EventType::NONE);
    CHECK(e.trapState() == TrapState::ARMED);
}

static void testOutOfOrderIgnored() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);
    arm(e);
    CHECK(e.processSensorEvent({SensorType::IR1, true, 100}).type == EventType::IR_ACTIVITY);
    EventResult r = e.processSensorEvent({SensorType::IR1, false, 50});  // stale
    CHECK(r.type == EventType::NONE);
}

static void testSprungAtBootWithoutStoredState() {
    TrapManager tm;
    EventEngine e = makeEngine(tm);   // no arm: manager starts UNKNOWN
    EventResult r = e.processSensorEvent({SensorType::TRAP_SENSOR, true, 100});
    CHECK(r.type == EventType::NONE);            // no false "triggered" report
    CHECK(e.trapState() == TrapState::NEEDS_REARMING);
}

int main() {
    std::printf("== test_event_engine ==\n");
    testIr1AloneTimesOut();
    testIr2AloneTimesOut();
    testPassThroughIr1ThenIr2();
    testPassThroughIr2ThenIr1();
    testIrThenTrapActivation();
    testUnexpectedTrapActivationWithoutIr();
    testTriggerThenEscape();
    testRearmResetsEscape();
    testPassThroughCancelledByTrigger();
    testNoiseAndDuplicates();
    testOutOfOrderIgnored();
    testSprungAtBootWithoutStoredState();
    std::printf("  %d checks, %d failures\n", testfw::gChecks(), testfw::gFailures());
    return testfw::gFailures() == 0 ? 0 : 1;
}