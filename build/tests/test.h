#ifndef M3DT_TEST_H
#define M3DT_TEST_H
#include <stdio.h>
#include <string.h>

extern int g_test_failures;

#define EXPECT(cond) do { \
    if (!(cond)) { g_test_failures++; \
      printf("FAIL %s:%d  EXPECT(%s)\n", __FILE__, __LINE__, #cond); } \
  } while (0)

#define EXPECT_EQ_INT(a,b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { g_test_failures++; \
      printf("FAIL %s:%d  %s (=%lld) != %s (=%lld)\n", __FILE__, __LINE__, #a, _a, #b, _b); } \
  } while (0)

#define EXPECT_STR(a,b) do { \
    if (strcmp((a),(b)) != 0) { g_test_failures++; \
      printf("FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); } \
  } while (0)

#endif
