#pragma once

#include <cstdio>

#include "rattrap/interfaces/Storage.h"

namespace rattrap {

// In-memory storage mock. Simulates persistence across a reboot: create the
// next node with the SAME MockStorage instance.
class MockStorage final : public Storage {
public:
    bool save(const PersistentState& state) override {
        ++save_count;
        if (fail) {
            return false;
        }
        stored = state;
        has_data = true;
        return true;
    }

    bool load(PersistentState& out) override {
        ++load_count;
        if (fail || !has_data) {
            return false;
        }
        out = stored;
        return true;
    }

    bool has_data = false;
    bool fail = false;
    size_t save_count = 0;
    size_t load_count = 0;
    PersistentState stored{};
};

}  // namespace rattrap