#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clang-c/Index.h>

typedef struct Association {
  char* normalized_type_spelling;
  CXString file_name;
  unsigned line;
  unsigned column;
  CXString signature_spelling;
} Association;

/// @brief Normalize a spelling
/// @return The normalized spelling, or @c NULL on failure
static char* normalize_spelling(const char* spelling) {
  CXIndex index = clang_createIndex(FALSE, FALSE);

  struct CXUnsavedFile unsaved_files[] = {
    {
      .Filename = "main.c",
      .Contents = spelling,
      .Length = strlen(spelling),
    },
  };

  CXTranslationUnit translation_unit = clang_parseTranslationUnit(
    index,
    "main.c",
    NULL,
    0,
    unsaved_files,
    G_N_ELEMENTS(unsaved_files),
    CXTranslationUnit_None
  );

  if (translation_unit == NULL)
    return NULL;

  CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);
  CXSourceRange range = clang_getCursorExtent(cursor);
  CXToken* tokens;
  unsigned token_count;
  clang_tokenize(translation_unit, range, &tokens, &token_count);

  GString* builder = g_string_new(NULL);

  if (token_count > 0) {
    for (unsigned i = 0; i < token_count - 1; ++i) {
      CXString spelling = clang_getTokenSpelling(translation_unit, tokens[i]);
      g_string_append_printf(builder, "%s ", clang_getCString(spelling));
      clang_disposeString(spelling);
    }

    CXString spelling = clang_getTokenSpelling(translation_unit, tokens[token_count - 1]);
    g_string_append(builder, clang_getCString(spelling));
    clang_disposeString(spelling);
  }

  char* result = g_string_free_and_steal(builder);

  clang_disposeTokens(translation_unit, tokens, token_count);
  clang_disposeTranslationUnit(translation_unit);
  clang_disposeIndex(index);

  return result;
}

static enum CXChildVisitResult compute_associations_visitor(
  CXCursor cursor,
  CXCursor parent,
  CXClientData _associations
) {
  (void)parent;
  GArray* associations = (GArray*)_associations;

  if (clang_getCursorKind(cursor) != CXCursor_FunctionDecl)
    return CXChildVisit_Continue;

  cursor = clang_getCanonicalCursor(cursor);

  Association a;

  CXFile file;
  clang_getFileLocation(clang_getCursorLocation(cursor), &file, &a.line, &a.column, NULL);

  a.file_name = clang_getFileName(file);
  const char* file_name = clang_getCString(a.file_name);

  for (unsigned i = 0; i < associations->len; ++i) {
    const Association* b = &g_array_index(associations, Association, i);

    if (
      b->line == a.line && b->column == a.column
      && strcmp(clang_getCString(b->file_name), file_name) == 0
    ) {
      clang_disposeString(a.file_name);
      return CXChildVisit_Continue;
    }
  }

  CXString type_spelling = clang_getTypeSpelling(clang_getCursorType(cursor));
  a.normalized_type_spelling = normalize_spelling(clang_getCString(type_spelling));
  clang_disposeString(type_spelling);

  CXPrintingPolicy printing_policy = clang_getCursorPrintingPolicy(cursor);
  clang_PrintingPolicy_setProperty(printing_policy, CXPrintingPolicy_PolishForDeclaration, TRUE);
  a.signature_spelling = clang_getCursorPrettyPrinted(cursor, printing_policy);
  clang_PrintingPolicy_dispose(printing_policy);

  g_array_append_val(associations, a);

  return CXChildVisit_Continue;
}

/// @brief Compute the associations between type and signature spellings in a given C/C++
/// @p file_name and append them to @p associations
static void compute_associations(CXIndex index, const char* file_name, GArray* associations) {
  CXTranslationUnit translation_unit = clang_parseTranslationUnit(
    index,
    file_name,
    NULL,
    0,
    NULL,
    0,
    CXTranslationUnit_SkipFunctionBodies
  );

  if (translation_unit == NULL)
    return;

  CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);
  clang_visitChildren(cursor, compute_associations_visitor, associations);
  clang_disposeTranslationUnit(translation_unit);
}

/// @brief Calculate the Levenshtein distance between two strings
static size_t distance(const char* a, const char* b) {
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);

  size_t* ds = g_new(size_t, (b_len + 1) * (a_len + 1));

  for (size_t r = 0; r <= a_len; ++r) {
    ds[(b_len + 1) * r] = r;
  }

  for (size_t c = 1; c <= b_len; ++c) {
    ds[c] = c;
  }

  for (size_t r = 1; r <= a_len; ++r) {
    for (size_t c = 1; c <= b_len; ++c) {
      size_t d1 = ds[(b_len + 1) * (r - 1) + c] + 1;
      size_t d2 = ds[(b_len + 1) * r + (c - 1)] + 1;
      size_t d3 = ds[(b_len + 1) * (r - 1) + (c - 1)] + (a[r - 1] == b[c - 1] ? 0 : 1);
      ds[(b_len + 1) * r + c] = MIN(MIN(d1, d2), d3);
    }
  }

  size_t result = ds[(b_len + 1) * a_len + b_len];
  g_free(ds);
  return result;
}

/// @brief Parse the command line arguments, possibly printing an error message on errors
/// @details The index of the first remaining argument will be stored in @c optind
static void parse_arguments(int* argc, char*** argv, char** query, GError** error) {
  const GOptionEntry entries[] = {
    {
      .long_name = "query",
      .short_name = 'q',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_STRING,
      .arg_data = query,
      .description = NULL,
      .arg_description = "QUERY",
    },
    {
      .long_name = NULL,
      .short_name = 0,
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_NONE,
      .arg_data = NULL,
      .description = NULL,
      .arg_description = NULL,
    },
  };

  GOptionContext* context = g_option_context_new(NULL);
  // TODO: g_option_context_set_summary(context, ...);
  // TODO: g_option_context_set_description(context, ...);
  g_option_context_add_main_entries(context, entries, NULL);

  g_option_context_parse(context, argc, argv, error);

  g_option_context_free(context);
}

static void main_clear_func(void* _association) {
  Association* association = _association;
  clang_disposeString(association->file_name);
  g_free(&association->normalized_type_spelling);
  clang_disposeString(association->signature_spelling);
}

static int main_compare_func(const void* _a, const void* _b, void* _normalized_query) {
  const Association* a = _a;
  const Association* b = _b;
  const char* normalized_query = _normalized_query;

  size_t distance_a = distance(a->normalized_type_spelling, normalized_query);
  size_t distance_b = distance(b->normalized_type_spelling, normalized_query);
  return distance_a == distance_b ? 0 : (distance_a < distance_b ? -1 : +1);
}

int main(int argc, char** argv) {
  char* query = NULL;
  GError* error = NULL;
  parse_arguments(&argc, &argv, &query, &error);

  if (error != NULL) {
    puts(error->message);
    g_error_free(error);
    g_free(query);
    return EXIT_FAILURE;
  }

  GArray* associations = g_array_new(FALSE, FALSE, sizeof(Association));
  g_array_set_clear_func(associations, main_clear_func);

  {
    CXIndex index = clang_createIndex(FALSE, FALSE);

    for (int i = 0; i < argc; ++i) {
      compute_associations(index, argv[i], associations);
    }

    clang_disposeIndex(index);
  }

  if (query != NULL) {
    char* normalized_query = normalize_spelling(query);

    if (normalized_query != NULL) {
      g_free(query);
    } else {
      normalized_query = query;
    }

    g_array_sort_with_data(associations, main_compare_func, normalized_query);
    g_free(normalized_query);
  }

  for (unsigned i = 0; i < associations->len; ++i) {
    const Association* a = &g_array_index(associations, Association, i);
    const char* file_name = clang_getCString(a->file_name);
    const char* signature_spelling = clang_getCString(a->signature_spelling);
    printf("%s:%u:%u: %s\n", file_name, a->line, a->column, signature_spelling);
  }

  g_array_unref(associations);

  return EXIT_SUCCESS;
}
