cmake_minimum_required(VERSION 3.16)
project(rattrap_node LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# MinGW (UCRT builds in particular) may reference API-set DLLs that are not
# present on some Windows images; statically link the runtime so the produced
# executables run anywhere without extra DLLs on PATH.
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

# ---- Platform-independent core library -------------------------------------
add_library(rattrap_core STATIC
    src/core/TrapManager.cpp
    src/core/EventEngine.cpp
    src/core/NodeState.cpp
    src/core/TrapNode.cpp
    src/battery/BatteryCalculator.cpp
)
target_include_directories(rattrap_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(rattrap_core PRIVATE /W4)
else()
    target_compile_options(rattrap_core PRIVATE -Wall -Wextra)
endif()

# ---- Tests / simulation ----------------------------------------------------
set(TESTS_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(rattrap_sim tests/simulation/main.cpp)
target_link_libraries(rattrap_sim PRIVATE rattrap_core)
target_include_directories(rattrap_sim PRIVATE ${TESTS_INCLUDE})

add_executable(test_battery tests/test_battery.cpp)
target_link_libraries(test_battery PRIVATE rattrap_core)
target_include_directories(test_battery PRIVATE ${TESTS_INCLUDE})

add_executable(test_trap_state tests/test_trap_state.cpp)
target_link_libraries(test_trap_state PRIVATE rattrap_core)
target_include_directories(test_trap_state PRIVATE ${TESTS_INCLUDE})

add_executable(test_event_engine tests/test_event_engine.cpp)
target_link_libraries(test_event_engine PRIVATE rattrap_core)
target_include_directories(test_event_engine PRIVATE ${TESTS_INCLUDE})

# ---- CTest -----------------------------------------------------------------
enable_testing()
add_test(NAME battery COMMAND test_battery)
add_test(NAME trap_state COMMAND test_trap_state)
add_test(NAME event_engine COMMAND test_event_engine)