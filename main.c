#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <clang-c/CXString.h>
#include <clang-c/Index.h>

#include "huntc.h"

static void
clear_function(void* _association) {
  HuntcAssociation* association = _association;
  clang_disposeString(association->file_name);
  g_clear_pointer(&association->normalized_type_spelling.data, g_free);
  clang_disposeString(association->signature_spelling);
}

static int
compare_function(const void* _a, const void* _b, void* _normalized_query) {
  const HuntcAssociation* a = _a;
  const HuntcAssociation* b = _b;
  HuntcString normalized_query = *(HuntcString*)_normalized_query;

  size_t distance_a = huntc_distance(a->normalized_type_spelling, normalized_query);
  size_t distance_b = huntc_distance(b->normalized_type_spelling, normalized_query);
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
  char* format = NULL;
  GError* error = NULL;
  huntc_parse_arguments(&argc, &argv, &query, &libc, &format, &error);

  if (error != NULL) {
    g_fprintf(stderr, "%s\n", error->message);
    g_error_free(error);
    g_free(query);
    return EXIT_FAILURE;
  }

  GArray* associations = g_array_new(false, false, sizeof(HuntcAssociation));
  g_array_set_clear_func(associations, clear_function);

  {
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
        huntc_compute_associations(translation_unit, associations);
        clang_disposeTranslationUnit(translation_unit);
      }
    }

    for (int i = 1; i < argc; ++i) {
      CXTranslationUnit translation_unit = clang_parseTranslationUnit(
        index, argv[i], NULL, 0, NULL, 0, CXTranslationUnit_SkipFunctionBodies);

      if (translation_unit != NULL) {
        huntc_compute_associations(translation_unit, associations);
        clang_disposeTranslationUnit(translation_unit);
      }
    }

    clang_disposeIndex(index);
  }

  if (query != NULL) {
    size_t query_length = strlen(query);
    HuntcString normalized_query;

    if (huntc_normalize_spelling((HuntcConstString){query, query_length}, &normalized_query)) {
      g_free(query);
    } else {
      normalized_query.data = query;
      normalized_query.length = query_length;
    }

    g_array_sort_with_data(associations, compare_function, &normalized_query);
    free(normalized_query.data);
  }

  {
    const char* f = format != NULL ? format : "%1$s:%2$u:%3$u: %4$s";

    for (unsigned i = 0; i < associations->len; ++i) {
      const HuntcAssociation* a = &g_array_index(associations, HuntcAssociation, i);
      const char* file_name = clang_getCString(a->file_name);
      const char* signature_spelling = clang_getCString(a->signature_spelling);
      g_printf(f, file_name, a->line, a->column, signature_spelling);
      putchar('\n');
    }

    g_free(format);
  }

  g_array_unref(associations);
  return EXIT_SUCCESS;
}
