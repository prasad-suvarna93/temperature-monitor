// tiny check macros for the host tests
#ifndef TEST_HPP
#define TEST_HPP

// no gtest so make test runs on a bare toolchain

#include <cstdio>
#include <cstring>

extern int g_checks;
extern int g_failures;

#define SECTION(name) std::printf("\n-- %s\n", (name))

// variadic so a braced initialiser still passes
#define CHECK(...)                                                           \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!(__VA_ARGS__)) {                                                    \
      ++g_failures;                                                          \
      std::printf("   FAIL  %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); \
    }                                                                        \
  } while (0)

// prints both values on failure
#define CHECK_EQ(actual, expected)                                                                              \
  do {                                                                                                          \
    const long a_ = static_cast<long>(actual);                                                                  \
    const long e_ = static_cast<long>(expected);                                                                \
    ++g_checks;                                                                                                 \
    if (a_ != e_) {                                                                                             \
      ++g_failures;                                                                                             \
      std::printf("   FAIL  %s:%d  %s\n         got %ld expected %ld\n", __FILE__, __LINE__, #actual, a_, e_); \
    }                                                                                                           \
  } while (0)

#define CHECK_STR(actual, expected)                                                                           \
  do {                                                                                                        \
    ++g_checks;                                                                                               \
    if (std::strcmp((actual), (expected)) != 0) {                                                             \
      ++g_failures;                                                                                           \
      std::printf("   FAIL  %s:%d  got \"%s\" expected \"%s\"\n", __FILE__, __LINE__, (actual), (expected)); \
    }                                                                                                         \
  } while (0)

#endif  // TEST_HPP
