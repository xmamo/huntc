#include <stddef.h>

#include <glib.h>

#include "huntc.h"

#define STRING(s) ((HuntcString){s, sizeof(s) - 1})
#define huntc_distance(a, b) huntc_distance(STRING(a), STRING(b))

static void
test_huntc_distance(void) {
  g_assert_cmpint(huntc_distance("", ""), ==, 0);
  g_assert_cmpint(huntc_distance("", "abc"), ==, 3);
  g_assert_cmpint(huntc_distance("abc", ""), ==, 3);
  g_assert_cmpint(huntc_distance("abc", "axc"), ==, 1);
  g_assert_cmpint(huntc_distance("axc", "abc"), ==, 1);
  g_assert_cmpint(huntc_distance("abc", "xyz"), ==, 3);
  g_assert_cmpint(huntc_distance("xyz", "abc"), ==, 3);
  g_assert_cmpint(huntc_distance("abc", "abcdef"), ==, 3);
  g_assert_cmpint(huntc_distance("abcdef", "abc"), ==, 3);
  g_assert_cmpint(huntc_distance("bcde", "abcdef"), ==, 2);
  g_assert_cmpint(huntc_distance("abcdef", "bcde"), ==, 2);
  g_assert_cmpint(huntc_distance("kitten", "sitting"), ==, 3);
  g_assert_cmpint(huntc_distance("sitting", "kitten"), ==, 3);
}

int
main(int argc, char** argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/huntc_distance/test", test_huntc_distance);
  return g_test_run();
}
