#pragma once

#include "rattrap/Types.h"

namespace rattrap {

// Abstract persistent storage for counters, last event and trap state.
// No ESP32-specific storage (Preferences/EEPROM/NVS/SPIFFS/LittleFS) here.
//
// TODO(esp32-c3): implement with NVS (non-volatile storage) on the ESP32-C3.
// This is one of the three adapters the ESP32-C3 layer will provide.
class Storage {
public:
    virtual ~Storage() = default;

    virtual bool save(const PersistentState& state) = 0;
    virtual bool load(PersistentState& out) = 0;
};

}  // namespace rattrap