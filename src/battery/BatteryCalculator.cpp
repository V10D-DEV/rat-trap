#include "rattrap/battery/BatteryCalculator.h"

#include <cmath>

namespace rattrap {

BatteryConfig defaultLiIon1SConfig() {
    BatteryConfig cfg;
    cfg.low_battery_voltage_v = 3.30;
    cfg.curve = {
        {4.20, 100},
        {4.00, 85},
        {3.90, 74},
        {3.80, 63},
        {3.70, 52},
        {3.60, 38},
        {3.50, 24},
        {3.40, 10},
        {3.30, 4},
        {3.05, 0},
    };
    return cfg;
}

int BatteryCalculator::voltageToPercent(double voltage_v, const BatteryConfig& cfg) {
    if (cfg.curve.empty()) {
        return -1;
    }
    if (voltage_v >= cfg.curve.front().voltage_v) {
        return 100;
    }
    if (voltage_v <= cfg.curve.back().voltage_v) {
        return 0;
    }
    for (std::size_t i = 0; i + 1 < cfg.curve.size(); ++i) {
        const double hi = cfg.curve[i].voltage_v;
        const double lo = cfg.curve[i + 1].voltage_v;
        if (voltage_v <= hi && voltage_v >= lo) {
            const int ph = cfg.curve[i].percent;
            const int pl = cfg.curve[i + 1].percent;
            const double t = (hi - voltage_v) / (hi - lo);
            return ph + static_cast<int>(std::llround(t * (pl - ph)));
        }
    }
    return -1;
}

bool BatteryCalculator::isLow(double voltage_v, const BatteryConfig& cfg) {
    return voltage_v < cfg.low_battery_voltage_v;
}

BatteryStatus BatteryCalculator::evaluate(double voltage_v, const BatteryConfig& cfg) {
    BatteryStatus b;
    b.voltage = voltage_v;
    b.percentage = voltageToPercent(voltage_v, cfg);
    b.low_battery = isLow(voltage_v, cfg);
    return b;
}

}  // namespace rattrap