#include <stddef.h>

#include <glib.h>

#include "huntc.h"

#define STRING(s) ((HuntcString){s, sizeof(s) - 1})
#define huntc_distance(a, b, fuzzy) huntc_distance(STRING(a), STRING(b), fuzzy)

static void
test_huntc_distance(void) {
  g_assert_cmpint(huntc_distance("", "", false), ==, 0);
  g_assert_cmpint(huntc_distance("", "abc", false), ==, 3);
  g_assert_cmpint(huntc_distance("abc", "", false), ==, 3);
  g_assert_cmpint(huntc_distance("abc", "axc", false), ==, 1);
  g_assert_cmpint(huntc_distance("axc", "abc", false), ==, 1);
  g_assert_cmpint(huntc_distance("abc", "xyz", false), ==, 3);
  g_assert_cmpint(huntc_distance("xyz", "abc", false), ==, 3);
  g_assert_cmpint(huntc_distance("abc", "abcdef", false), ==, 3);
  g_assert_cmpint(huntc_distance("abcdef", "abc", false), ==, 3);
  g_assert_cmpint(huntc_distance("bcde", "abcdef", false), ==, 2);
  g_assert_cmpint(huntc_distance("abcdef", "bcde", false), ==, 2);
  g_assert_cmpint(huntc_distance("kitten", "sitting", false), ==, 3);
  g_assert_cmpint(huntc_distance("sitting", "kitten", false), ==, 3);
}

int
main(int argc, char** argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/huntc_distance/test", test_huntc_distance);
  return g_test_run();
}
