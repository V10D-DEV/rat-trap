# Rat Trap Node

> Platform-independent core logic for a battery-powered IoT rat-trap node

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-v0.8-orange)

## Overview

**Rat Trap Node** is the brain of a distributed pest-control monitoring system. It provides platform-independent core logic for trap state management, event detection, and battery monitoring—designed to run on standard PCs today and embedded ESP32-C3 devices tomorrow.

This is **pure C++17** with zero dependencies on Arduino, PlatformIO, Wi-Fi stacks, GPIO libraries, or ESP-IDF. All timing and state management use caller-supplied timestamps, making the code deterministic and fully testable on any platform.

### Hardware Target (Future)

- **Microcontroller**: ESP32-C3 mini
- **Sensors**: 
  - Two IR break-beam sensors (NE555 emitter + LM393 comparator)
  - Reed switch / mechanical end-stop (trap arm position)
- **Power**: 3000 mAh Li-ion battery (ADC voltage divider monitoring)
- **Communication**: Wi-Fi event reporting with deep-sleep optimization

## Architecture

```
                ┌────────────────────────────────────────────┐
                │              TrapNode (facade)             │  ← ESP32-C3
                │   wires core + adapters together           │    adapter
                └───┬────────┬───────────┬──────────┬───────┘     plugs in
                    │        │           │          │
      handleSensorEvent   poll    updateBattery    requestSleep
                    │        │           │          │
       ┌────────────▼──┐  ┌──▼──────────┐  ┌───────▼──────┐  ┌──────────────┐
       │  EventEngine  │  │ TrapManager │  │ BatteryCalc  │  │   Adapters   │
       │  time/motion  │◄►│ persistent  │  │ pure function│  │ (interfaces) │
       │  state machine│  │ trap state  │  └──────┬───────┘  │ NetworkRepr. │
       └────────────┬──┘  └─────┬───────┘         │          │ Storage      │
                    │            │                 │          │ PowerManager │
                    ▼            ▼                 │          └──────────────┘
              ┌────────────────────────┐           │
              │        NodeState       │◄──────────┘
              │ counters, last event,  │
              │ battery, persist/restore│
              └────────────────────────┘
```

### Design Principles

#### 1. **Physical ≠ Logical State**
- `TrapState` (mechanical): `ARMED`, `TRIGGERED`, `NEEDS_REARMING`, `UNKNOWN`
- `EventType` (logical): `PASSED_THROUGH`, `TRAP_TRIGGERED`, `ESCAPED_AFTER_TRIGGER`, etc.
- `TRIGGERED` is transient—settles to `NEEDS_REARMING` in one cycle. Sprung traps never auto-arm.

#### 2. **Edge-Triggered, Time-Driven**
- Ignores duplicate samples and enforces per-sensor cooldown (`min_pulse_interval_ms`)
- Refuses out-of-order timestamps
- Timeouts (`episode_timeout_ms`, `pass_through_confirm_delay_ms`) evaluated in `poll(now_ms)`
- Caller supplies timestamps → identical code runs on PC and ESP32

#### 3. **Pass-Through Confirmation**
- Not assumed, but explicitly confirmed
- Requires both beams interrupted *and* cleared within confirmation window
- Trap activation inside window takes priority

#### 4. **Explicit Persistence**
- `PersistentState` owns counters, last event, and trap state
- `Storage` interface handles saves/loads
- Reboot restores state without double-reporting

## State Machine

The event engine interprets sensor streams into logical motion and trap events:

```
              ARMED
                │  IR beam blocked (rising edge)
                ▼
        MOTION_DETECTED  ← recurring beam activity refreshes timeout
          │                   │
          │ nothing for        │ trap activates
          │ 30 seconds         ▼
          ▼              TRAP_TRIGGERED → NEEDS_REARMING
       IDLE                      │
    (sequence              further IR activity (once)
     discarded,                  ▼
     trap stays           ESCAPED_AFTER_TRIGGER
     ARMED)                      │
                                 ▼
                           NEEDS_REARMING
                                 │ trap returns to ARMED_POSITION
                                 ▼
                               ARMED
```

Key timeouts:
- **`episode_timeout_ms`** (default 30s): No IR activity → discard sequence, stay ARMED
- **`pass_through_confirm_delay_ms`** (default 500ms): All-clear window; trap snap here wins
- **`min_pulse_interval_ms`** (default 50ms): Per-sensor debounce

## Project Structure

```
rat-trap-node/
├── CMakeLists.txt                      # Build configuration
├── README.md                           # This file
├── include/rattrap/
│   ├── Types.h                         # Shared vocabulary (enums, structs, identity)
│   ├── core/
│   │   ├── EventEngine.h               # Time/motion state machine
│   │   ├── TrapManager.h               # Persistent mechanical trap state
│   │   ├── NodeState.h                 # Counters, status, persist/restore
│   │   └── TrapNode.h                  # Facade & ESP32-C3 integration seam
│   ├── interfaces/
│   │   ├── NetworkReporter.h           # Abstract reporting interface (no Wi-Fi)
│   │   ├── Storage.h                   # Abstract persistence (no NVS/EEPROM)
│   │   └── PowerManager.h              # Abstract deep-sleep request
│   └── battery/
│       └── BatteryCalculator.h         # Configurable Li-ion curve (pure function)
├── src/
│   ├── core/
│   │   ├── EventEngine.cpp
│   │   ├── TrapManager.cpp
│   │   ├── NodeState.cpp
│   │   └── TrapNode.cpp
│   └── battery/
│       └── BatteryCalculator.cpp
├── tests/
│   ├── TestFramework.h                 # Simple unit test utilities
│   ├── test_event_engine.cpp           # EventEngine unit tests
│   ├── test_battery.cpp                # Battery calculation tests
│   ├── test_trap_state.cpp             # TrapManager tests
│   ├── mocks/
│   │   ├── MockNetworkReporter.h
│   │   ├── MockStorage.h
│   │   └── MockPowerManager.h
│   └── simulation/
│       └── main.cpp                    # Integration simulation
└── examples/                           # Coming soon
```

## Building and Testing

### Requirements
- CMake >= 3.16
- C++17 compiler (MSVC, MinGW, GCC, or Clang)

### Build Instructions

```bash
# Configure
cmake -S . -B build

# Compile (Release mode)
cmake --build build --config Release

# Run all tests
ctest --test-dir build --output-on-failure

# Or run individual test executables
./build/test_battery
./build/test_trap_state
./build/test_event_engine

# Run simulation
./build/rattrap_sim
```

### What Gets Built

| Target | Purpose |
|--------|---------|
| `rattrap_core` | Platform-independent static library |
| `test_battery` | Battery calculator unit tests |
| `test_trap_state` | TrapManager unit tests |
| `test_event_engine` | EventEngine and state machine tests |
| `rattrap_sim` | PC-based simulation with mock adapters |

## Example Event Sequences

The simulation reproduces canonical flows with full timestamps and logical event output:

### 1) Mouse passes through
```
IR1 blocked → IR2 blocked → IR1 cleared → IR2 cleared → confirm delay elapses
Result: PASSED_THROUGH
Trap state: ARMED
```

### 2) Mouse triggers trap
```
IR1 blocked → trap sensor triggers
Result: TRAP_TRIGGERED
Trap state: NEEDS_REARMING
```

### 3) Mouse triggers trap and escapes
```
IR1 blocked → trap sensor triggers → IR2 blocked
Result: ESCAPED_AFTER_TRIGGER
Trap state: NEEDS_REARMING
```

### 4) Trap manually re-armed
```
Trap sensor returns to ARMED_POSITION
Trap state: ARMED
```

The full simulation covers:
- Single-beam activity (IR1-only, IR2-only)
- Beam ordering (IR1→IR2 vs IR2→IR1)
- Trap activation with/without IR activity
- Timeout discarding incomplete sequences
- Duplicate/noisy sensor edges
- Invalid boot states (sprung without persisted state)
- Reboot recovery and state restoration
- Battery voltage sampling
- 24-hour status reporting deadlines
- Deep-sleep request to power manager

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| **Beam edges carry information** | Rising/falling edge indicates motion. Driver layer debounces; engine adds coarse per-sensor cooldown. |
| **Trap sensor polarity in software** | Decoding happens in the ESP32 adapter—`state == true` → sprung, `state == false` → armed. |
| **Pass-through requires both beams (default)** | Configurable; ensures actual traversal. Trap snap inside confirm window overrides pending pass-through. |
| **Trap activation always reported** | Except at boot (where sprung without history is adopted silently to avoid false reports). |
| **Caller supplies timestamps** | Zero sleeps, zero blocks. Identical code on PC and ESP32. Out-of-order samples ignored. |
| **Escape = IR activity while sprung** | De-duplicated once per trigger cycle. |
| **Battery = configurable Li-ion curve** | Voltage → %, linear interpolation, clamped. ADC scaling is hardware (ESP32 adapter). |
| **24h reporting = deadline, not wall-clock** | Passed to `PowerManager::sleep(wake_at_ms)` so deep-sleep timer can be programmed. |

## ESP32-C3 Integration (TODO)

The ESP32-C3 adapter layer implements **three interfaces** and supplies **two value streams** with no changes to `rattrap_core`:

### Interfaces to Implement

| Interface | Purpose | Example Implementation |
|-----------|---------|------------------------|
| `NetworkReporter` | Wi-Fi/HTTP(S)/MQTT event reporting | Send events, 24h status; keep radio off otherwise |
| `Storage` | Persistent state (NVS-backed) | Save/restore `PersistentState` across reboots |
| `PowerManager` | Deep sleep, RTC timer, wake triggers | `sleep(wake_at_ms)` with sensor wake-up capability |

### Data Flow

1. **GPIO ISRs** → Digitize LM393/LM555 IR conditioning + trap sensor
   - Polarity-decode to logical state (`beam_blocked`, `trap_sprung`)
   - Timestamp with `esp_timer_get_time() / 1000` (milliseconds)
   - Call `TrapNode::handleSensorEvent()`

2. **ADC sampling** → Read battery voltage
   - Apply voltage-divider factor and channel scaling to get real volts
   - Call `TrapNode::updateBattery(status)`

3. **Main loop / timer** → Call `TrapNode::poll(now_ms)` regularly
   - Check `dailyReportDue()` and issue `requestSleep()`
   - Integrate with deep-sleep management

### Integration Seams

Look for `TODO(esp32-c3)` markers in headers for exact seams and wiring points.
## References

- **State machine patterns**: Temporal edge-driven design for reactive embedded systems
- **Li-ion battery curves**: Standard 1S nominal discharge characteristics
- **Deep-sleep optimization**: Wake-on-interrupt with deadline-based scheduling
