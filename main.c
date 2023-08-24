#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clang-c/CXFile.h>
#include <clang-c/CXSourceLocation.h>
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
static bool
normalize_spelling(ConstString spelling, String* result) {
  CXIndex index = clang_createIndex(false, false);

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
    result->data = g_string_free_and_steal(builder);
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

  Association a;
  cursor = clang_getCanonicalCursor(cursor);

  {
    CXFile file;
    clang_getFileLocation(clang_getCursorLocation(cursor), &file, &a.line, &a.column, NULL);

    a.file_name = clang_getFileName(file);
    const char* file_name = clang_getCString(a.file_name);

    for (unsigned i = 0; i < associations->len; ++i) {
      const Association* b = &g_array_index(associations, Association, i);

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

    if (!normalize_spelling(
          (ConstString){type_spelling_data, type_spelling_length}, &a.normalized_type_spelling)) {
      a.normalized_type_spelling.data = strdup(type_spelling_data);
      a.normalized_type_spelling.length = type_spelling_length;
    }

    clang_disposeString(type_spelling);
  }

  {
    CXPrintingPolicy printing_policy = clang_getCursorPrintingPolicy(cursor);
    clang_PrintingPolicy_setProperty(printing_policy, CXPrintingPolicy_PolishForDeclaration, true);
    a.signature_spelling = clang_getCursorPrettyPrinted(cursor, printing_policy);
    clang_PrintingPolicy_dispose(printing_policy);
  }

  g_array_append_val(associations, a);
  return CXChildVisit_Continue;
}

/// @brief Compute the associations between type and signature spellings in a given C/C++
/// @p file_name and append them to @p associations static void
static void
compute_associations(CXTranslationUnit translation_unit, GArray* associations) {
  CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);
  clang_visitChildren(cursor, compute_associations_visitor, associations);
}

/// @brief Calculate the Levenshtein distance between two strings
static size_t
distance(String a, String b) {
  if (a.length < b.length) {
    {
      String t = a;
      a = b;
      b = t;
    }
  }

  size_t* rows = g_new(size_t, (b.length + 1) * 2);
  size_t* row0 = rows + (b.length + 1) * 0;
  size_t* row1 = rows + (b.length + 1) * 1;

  for (size_t ci = 0; ci <= b.length; ++ci) {
    row0[ci] = ci;
  }

  for (size_t ri = 1; ri <= b.length; ++ri) {
    row1[0] = ri;

    for (size_t ci = 1; ci <= b.length; ++ci) {
      size_t deletion_cost = row0[ci] + 1;
      size_t insertion_cost = row1[ci - 1] + 1;
      size_t substitution_cost = a.data[ri - 1] == b.data[ci - 1] ? row0[ci - 1] : row0[ci - 1] + 1;
      row1[ci] = MIN(MIN(deletion_cost, insertion_cost), substitution_cost);
    }

    {
      size_t* t = row0;
      row0 = row1;
      row1 = t;
    }
  }

  size_t result = row0[b.length];
  g_free(rows);
  return result;
}

/// @brief Parse the command line arguments, possibly printing an error message on errors
/// @details The index of the first remaining argument will be stored in @c optind
static void
parse_arguments(int* argc, char*** argv, char** query, bool* libc, GError** error) {
  int _libc = *libc;

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
      .arg_data = &_libc,
      .description = NULL,
      .arg_description = NULL,
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
  *libc = _libc;

  g_option_context_free(context);
}

static void
main_clear_func(void* _association) {
  Association* association = _association;
  clang_disposeString(association->file_name);
  g_free(&association->normalized_type_spelling);
  clang_disposeString(association->signature_spelling);
}

static int
main_compare_func(const void* _a, const void* _b, void* _normalized_query) {
  const Association* a = _a;
  const Association* b = _b;
  String normalized_query = *(String*)_normalized_query;

  size_t distance_a = distance(a->normalized_type_spelling, normalized_query);
  size_t distance_b = distance(b->normalized_type_spelling, normalized_query);
  return distance_a == distance_b ? 0 : (distance_a < distance_b ? -1 : +1);
}

#define C_H                            \
  "#include <aio.h>\n"                 \
  "#include <argp.h>\n"                \
  "#include <argz.h>\n"                \
  "#include <arpa/inet.h>\n"           \
  "#include <assert.h>\n"              \
  "#include <complex.h>\n"             \
  "#include <crypt.h>\n"               \
  "#include <ctype.h>\n"               \
  "#include <dirent.h>\n"              \
  "#include <dlfcn.h>\n"               \
  "#include <envz.h>\n"                \
  "#include <err.h>\n"                 \
  "#include <errno.h>\n"               \
  "#include <error.h>\n"               \
  "#include <execinfo.h>\n"            \
  "#include <fcntl.h>\n"               \
  "#include <fenv.h>\n"                \
  "#include <float.h>\n"               \
  "#include <fmtmsg.h>\n"              \
  "#include <fnmatch.h>\n"             \
  "#include <fstab.h>\n"               \
  "#include <ftw.h>\n"                 \
  "#include <gconv.h>\n"               \
  "#include <getopt.h>\n"              \
  "#include <glob.h>\n"                \
  "#include <grp.h>\n"                 \
  "#include <iconv.h>\n"               \
  "#include <inttypes.h>\n"            \
  "#include <langinfo.h>\n"            \
  "#include <libgen.h>\n"              \
  "#include <libintl.h>\n"             \
  "#include <limits.h>\n"              \
  "#include <locale.h>\n"              \
  "#include <malloc.h>\n"              \
  "#include <math.h>\n"                \
  "#include <mcheck.h>\n"              \
  "#include <mntent.h>\n"              \
  "#include <net/if.h>\n"              \
  "#include <netdb.h>\n"               \
  "#include <netinet/in.h>\n"          \
  "#include <nl_types.h>\n"            \
  "#include <obstack.h>\n"             \
  "#include <printf.h>\n"              \
  "#include <pthread.h>\n"             \
  "#include <pty.h>\n"                 \
  "#include <pwd.h>\n"                 \
  "#include <regex.h>\n"               \
  "#include <sched.h>\n"               \
  "#include <search.h>\n"              \
  "#include <setjmp.h>\n"              \
  "#include <sgtty.h>\n"               \
  "#include <signal.h>\n"              \
  "#include <stdarg.h>\n"              \
  "#include <stddef.h>\n"              \
  "#include <stdint.h>\n"              \
  "#include <stdio_ext.h>\n"           \
  "#include <stdio.h>\n"               \
  "#include <stdlib.h>\n"              \
  "#include <string.h>\n"              \
  "#include <sys/auxv.h>\n"            \
  "#include <sys/file.h>\n"            \
  "#include <sys/ioctl.h>\n"           \
  "#include <sys/mman.h>\n"            \
  "#include <sys/mount.h>\n"           \
  "#include <sys/param.h>\n"           \
  "#include <sys/random.h>\n"          \
  "#include <sys/resource.h>\n"        \
  "#include <sys/rseq.h>\n"            \
  "#include <sys/single_threaded.h>\n" \
  "#include <sys/socket.h>\n"          \
  "#include <sys/stat.h>\n"            \
  "#include <sys/sysinfo.h>\n"         \
  "#include <sys/time.h>\n"            \
  "#include <sys/times.h>\n"           \
  "#include <sys/timex.h>\n"           \
  "#include <sys/types.h>\n"           \
  "#include <sys/uio.h>\n"             \
  "#include <sys/un.h>\n"              \
  "#include <sys/utsname.h>\n"         \
  "#include <sys/vlimit.h>\n"          \
  "#include <sys/wait.h>\n"            \
  "#include <syslog.h>\n"              \
  "#include <termios.h>\n"             \
  "#include <threads.h>\n"             \
  "#include <time.h>\n"                \
  "#include <ucontext.h>\n"            \
  "#include <ulimit.h>\n"              \
  "#include <unistd.h>\n"              \
  "#include <utime.h>\n"               \
  "#include <utmp.h>\n"                \
  "#include <utmpx.h>\n"               \
  "#include <wchar.h>\n"               \
  "#include <wctype.h>\n"              \
  "#include <wordexp.h>\n"

int
main(int argc, char** argv) {
  char* query = NULL;
  bool libc = false;
  GError* error = NULL;
  parse_arguments(&argc, &argv, &query, &libc, &error);

  if (error != NULL) {
    puts(error->message);
    g_error_free(error);
    g_free(query);
    return EXIT_FAILURE;
  }

  GArray* associations = g_array_new(false, false, sizeof(Association));
  g_array_set_clear_func(associations, main_clear_func);

  CXIndex index = clang_createIndex(false, false);

  if (libc) {
    struct CXUnsavedFile c_h_file = {
      .Filename = "c.h",
      .Contents = C_H,
      .Length = sizeof(C_H) - 1,
    };

    CXTranslationUnit translation_unit = clang_parseTranslationUnit(
      index, "c.h", NULL, 0, &c_h_file, 1, CXTranslationUnit_SkipFunctionBodies);

    if (translation_unit != NULL) {
      compute_associations(translation_unit, associations);
      clang_disposeTranslationUnit(translation_unit);
    }
  }

  for (int i = 0; i < argc; ++i) {
    CXTranslationUnit translation_unit = clang_parseTranslationUnit(
      index, argv[i], NULL, 0, NULL, 0, CXTranslationUnit_SkipFunctionBodies);

    if (translation_unit != NULL) {
      compute_associations(translation_unit, associations);
      clang_disposeTranslationUnit(translation_unit);
    }
  }

  clang_disposeIndex(index);

  if (query != NULL) {
    size_t query_length = strlen(query);
    String normalized_query;

    if (normalize_spelling((ConstString){query, query_length}, &normalized_query)) {
      g_free(query);
    } else {
      normalized_query.data = query;
      normalized_query.length = query_length;
    }

    g_array_sort_with_data(associations, main_compare_func, &normalized_query);
    free(normalized_query.data);
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
