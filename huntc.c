#include "huntc.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include <clang-c/Index.h>

bool
huntc_normalize_spelling(HuntcConstString spelling, HuntcString* result) {
  CXIndex index = clang_createIndex(FALSE, FALSE);

  struct CXUnsavedFile main_c_file = {
    .Filename = "main.c",
    .Contents = spelling.data,
    .Length = spelling.length,
  };

  CXTranslationUnit translation_unit =
    clang_parseTranslationUnit(index, "main.c", NULL, 0, &main_c_file, 1, CXTranslationUnit_None);

  if (translation_unit == NULL) {
    clang_disposeIndex(index);
    return false;
  }

  CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);
  CXSourceRange range = clang_getCursorExtent(cursor);
  CXToken* tokens;
  unsigned token_count;
  clang_tokenize(translation_unit, range, &tokens, &token_count);

  {
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

    result->length = builder->len;
    result->data = g_string_free(builder, FALSE);
  }

  clang_disposeTokens(translation_unit, tokens, token_count);
  clang_disposeTranslationUnit(translation_unit);
  clang_disposeIndex(index);
  return true;
}

static enum CXChildVisitResult
compute_associations_visitor(CXCursor cursor, CXCursor parent, CXClientData _associations) {
  (void)parent;
  GArray* associations = (GArray*)_associations;

  if (clang_getCursorKind(cursor) != CXCursor_FunctionDecl)
    return CXChildVisit_Continue;

  HuntcAssociation a;
  cursor = clang_getCanonicalCursor(cursor);

  {
    CXFile file;
    clang_getFileLocation(clang_getCursorLocation(cursor), &file, &a.line, &a.column, NULL);

    a.file_name = clang_getFileName(file);
    const char* file_name = clang_getCString(a.file_name);

    for (unsigned i = 0; i < associations->len; ++i) {
      const HuntcAssociation* b = &g_array_index(associations, HuntcAssociation, i);

      if (b->line == a.line && b->column == a.column
        && strcmp(clang_getCString(b->file_name), file_name) == 0) {
        clang_disposeString(a.file_name);
        return CXChildVisit_Continue;
      }
    }
  }

  {
    CXString type_spelling = clang_getTypeSpelling(clang_getCursorType(cursor));

    const char* type_spelling_data = clang_getCString(type_spelling);
    size_t type_spelling_length = strlen(type_spelling_data);

    if (!huntc_normalize_spelling((HuntcConstString){type_spelling_data, type_spelling_length},
          &a.normalized_type_spelling)) {
      a.normalized_type_spelling.data = strndup(type_spelling_data, type_spelling_length);
      a.normalized_type_spelling.length = type_spelling_length;
    }

    clang_disposeString(type_spelling);
  }

  {
    CXPrintingPolicy printing_policy = clang_getCursorPrintingPolicy(cursor);
    clang_PrintingPolicy_setProperty(printing_policy, CXPrintingPolicy_PolishForDeclaration, TRUE);
    a.signature_spelling = clang_getCursorPrettyPrinted(cursor, printing_policy);
    clang_PrintingPolicy_dispose(printing_policy);
  }

  g_array_append_val(associations, a);
  return CXChildVisit_Continue;
}

void
huntc_compute_associations(CXTranslationUnit translation_unit, GArray* associations) {
  CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);
  clang_visitChildren(cursor, compute_associations_visitor, associations);
}

size_t
huntc_distance(HuntcString a, HuntcString b, bool fuzzy) {
  if (b.length < a.length) {
    HuntcString t = a;
    a = b;
    b = t;
  }

  if (a.length == 0)
    return b.length;

  size_t* rows = g_new(size_t, (b.length + 1) * 2);
  size_t* row0 = rows + (b.length + 1) * 0;
  size_t* row1 = rows + (b.length + 1) * 1;

  if (fuzzy) {
    for (size_t ci = 0; ci <= b.length; ++ci) {
      row0[ci] = 0;
    }
  } else {
    for (size_t ci = 0; ci <= b.length; ++ci) {
      row0[ci] = ci;
    }
  }

  for (size_t ri = 1;; ++ri) {
    row1[0] = ri;

    for (size_t ci = 1; ci <= b.length; ++ci) {
      size_t deletion_cost = row0[ci] + 1;
      size_t insertion_cost = row1[ci - 1] + 1;
      size_t substitution_cost = a.data[ri - 1] == b.data[ci - 1] ? row0[ci - 1] : row0[ci - 1] + 1;
      row1[ci] = MIN(MIN(deletion_cost, insertion_cost), substitution_cost);
    }

    if (ri >= a.length)
      break;

    size_t* t = row0;
    row0 = row1;
    row1 = t;
  }

  size_t result = row1[b.length];
  g_free(rows);
  return result;
}

void
huntc_parse_arguments(
  int* argc, char*** argv, char** query, bool* libc, bool* fuzzy, char** format, GError** error) {
  gboolean c = *libc;
  gboolean f = *fuzzy;

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
      .long_name = "libc",
      .short_name = 'c',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_NONE,
      .arg_data = &c,
      .description = NULL,
      .arg_description = NULL,
    },
    {
      .long_name = "fuzzy",
      .short_name = 'f',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_NONE,
      .arg_data = &f,
      .description = NULL,
      .arg_description = NULL,
    },
    {
      .long_name = "format",
      .short_name = 'F',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_STRING,
      .arg_data = format,
      .description = NULL,
      .arg_description = NULL,
    },
    G_OPTION_ENTRY_NULL,
  };

  GOptionContext* context = g_option_context_new(NULL);
  g_option_context_add_main_entries(context, entries, NULL);

  GError* e = NULL;
  g_option_context_parse(context, argc, argv, &e);

  *libc = c;
  *fuzzy = f;

  if (e != NULL) {
    g_propagate_error(error, e);
  }

  g_option_context_free(context);
}
