#pragma once

#include <vector>

#include "rattrap/Types.h"

namespace rattrap {

// One point of the configurable Li-ion voltage->percentage curve.
struct BatteryPoint {
    double voltage_v = 0.0;
    int percent = 0;
};

struct BatteryConfig {
    std::vector<BatteryPoint> curve;   // sorted descending by voltage, 0..100
    double low_battery_voltage_v = 3.30;
};

// Reasonable default for a single-cell (1S) 3.7 V Li-ion pack.
BatteryConfig defaultLiIon1SConfig();

// Pure battery math. No ADC, no hardware.
//
// The ESP32-C3 ADC layer is responsible for converting raw ADC samples
// (including the voltage-divider scale factor) into a real voltage in volts
// and calling evaluate() with it.
// TODO(esp32-c3): measure the divider ratio and add it to the adapter that
// fills BatteryStatus.
class BatteryCalculator {
public:
    static BatteryStatus evaluate(double voltage_v, const BatteryConfig& cfg);
    static int voltageToPercent(double voltage_v, const BatteryConfig& cfg);
    static bool isLow(double voltage_v, const BatteryConfig& cfg);

private:
    BatteryCalculator() = delete;
};

}  // namespace rattrap