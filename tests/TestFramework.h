#pragma once

#include <cstdio>
#include <string>

namespace testfw {

inline int& gChecks() {
    static int c = 0;
    return c;
}
inline int& gFailures() {
    static int f = 0;
    return f;
}

}  // namespace testfw

#define CHECK(cond)                                                             \
    do {                                                                        \
        ++testfw::gChecks();                                                    \
        if (!(cond)) {                                                          \
            ++testfw::gFailures();                                              \
            std::printf("  FAIL %s:%d : %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                       \
    } while (0)

#define CHECK_EQ(lhs, rhs)                                                      \
    do {                                                                        \
        ++testfw::gChecks();                                                    \
        if (!((lhs) == (rhs))) {                                                \
            ++testfw::gFailures();                                              \
            std::printf("  FAIL %s:%d : %s == %s\n", __FILE__, __LINE__, #lhs,  \
                        #rhs);                                                  \
        }                                                                       \
    } while (0)
