#include <stdbool.h>
#include <stdio.h>

#include <glib.h>

#include <clang-c/CXString.h>
#include <clang-c/Index.h>

typedef struct String {
  char* data;
  size_t length;
} String;

typedef struct ConstString {
  const char* data;
  size_t length;
} ConstString;

typedef struct Association {
  String normalized_type_spelling;
  CXString file_name;
  unsigned line;
  unsigned column;
  CXString signature_spelling;
} Association;

/// @brief Normalize a spelling
/// @return @c true on success, @c false if there was a parsing error
bool
huntc_normalize_spelling(ConstString spelling, String* result);

/// @brief Compute the associations between type and signature spellings in a given C/C++
/// @p file_name and append them to @p associations static void
void
huntc_compute_associations(CXTranslationUnit translation_unit, GArray* associations);

/// @brief Calculate the Levenshtein distance between two strings
size_t
huntc_distance(String a, String b);

/// @brief Parse the command line arguments, possibly printing an error message on errors
/// @details The index of the first remaining argument will be stored in @c optind
void
huntc_parse_arguments(
  int* argc, char*** argv, char** query, bool* libc, char** format, GError** error);
