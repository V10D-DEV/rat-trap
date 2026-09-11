// Unit tests for BatteryCalculator: lithium-ion curve + low-battery logic.
#include <cstdio>

#include "rattrap/battery/BatteryCalculator.h"
#include "tests/TestFramework.h"

using namespace rattrap;

static void testDefaultCurve() {
    const BatteryConfig cfg = defaultLiIon1SConfig();

    CHECK_EQ(BatteryCalculator::evaluate(4.20, cfg).percentage, 100);
    CHECK_EQ(BatteryCalculator::evaluate(4.50, cfg).percentage, 100);  // clamp high
    CHECK_EQ(BatteryCalculator::evaluate(4.00, cfg).percentage, 85);
    CHECK_EQ(BatteryCalculator::evaluate(3.70, cfg).percentage, 52);
    CHECK_EQ(BatteryCalculator::evaluate(3.05, cfg).percentage, 0);
    CHECK_EQ(BatteryCalculator::evaluate(3.00, cfg).percentage, 0);   // clamp low
    CHECK_EQ(BatteryCalculator::evaluate(2.90, cfg).percentage, 0);
}

static void testInterpolation() {
    const BatteryConfig cfg = defaultLiIon1SConfig();
    CHECK_EQ(BatteryCalculator::evaluate(3.75, cfg).percentage, 57);  // sense check
}

static void testLowBatteryThreshold() {
    const BatteryConfig cfg = defaultLiIon1SConfig();
    CHECK(BatteryCalculator::evaluate(3.29, cfg).low_battery);
    CHECK(BatteryCalculator::isLow(3.29, cfg));
    CHECK(!BatteryCalculator::evaluate(3.30, cfg).low_battery);
    CHECK(!BatteryCalculator::evaluate(4.10, cfg).low_battery);
}

static void testCustomCurve() {
    BatteryConfig cfg;
    cfg.low_battery_voltage_v = 3.20;
    cfg.curve = {{4.2, 100}, {3.0, 0}};
    CHECK_EQ(BatteryCalculator::evaluate(3.6, cfg).percentage, 50);
    CHECK_EQ(BatteryCalculator::evaluate(4.2, cfg).percentage, 100);
}

static void testEmptyCurve() {
    BatteryConfig cfg;
    BatteryStatus b = BatteryCalculator::evaluate(4.0, cfg);
    CHECK_EQ(b.percentage, -1);
    CHECK(!b.low_battery);
}

static void testVoltagePassthrough() {
    const BatteryConfig cfg = defaultLiIon1SConfig();
    BatteryStatus b = BatteryCalculator::evaluate(3.95, cfg);
    CHECK_EQ(b.voltage, 3.95);
}

int main() {
    std::printf("== test_battery ==\n");
    testDefaultCurve();
    testInterpolation();
    testLowBatteryThreshold();
    testCustomCurve();
    testEmptyCurve();
    testVoltagePassthrough();
    std::printf("  %d checks, %d failures\n", testfw::gChecks(), testfw::gFailures());
    return testfw::gFailures() == 0 ? 0 : 1;
}