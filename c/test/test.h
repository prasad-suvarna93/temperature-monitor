// Tiny test framework on purpose
#ifndef TEST_H
#define TEST_H

// no external framework so make test runs on a bare toolchain
#include <stdint.h>
#include <stdio.h>

extern int g_checks;
extern int g_failures;

#define SECTION(name) printf("\n-- %s\n", (name))

#define CHECK(cond)                                              \
  do {                                                           \
    g_checks++;                                                  \
    if (!(cond)) {                                               \
      g_failures++;                                              \
      printf("   FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                            \
  } while (0)

// prints both values on failure
#define CHECK_EQ(actual, expected)                                                                         \
  do {                                                                                                     \
    const long _a = (long)(actual);                                                                        \
    const long _e = (long)(expected);                                                                      \
    g_checks++;                                                                                            \
    if (_a != _e) {                                                                                        \
      g_failures++;                                                                                        \
      printf("   FAIL  %s:%d  %s\n         got %ld, expected %ld\n", __FILE__, __LINE__, #actual, _a, _e); \
    }                                                                                                      \
  } while (0)

#define CHECK_STR(actual, expected)                                                                      \
  do {                                                                                                   \
    g_checks++;                                                                                          \
    if (strcmp((actual), (expected)) != 0) {                                                             \
      g_failures++;                                                                                      \
      printf("   FAIL  %s:%d  got \"%s\", expected \"%s\"\n", __FILE__, __LINE__, (actual), (expected)); \
    }                                                                                                    \
  } while (0)

#endif
