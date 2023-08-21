#include <getopt.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <clang-c/Index.h>

struct Association {
  std::string normalized_type;
  std::string file_name;
  unsigned line;
  unsigned column;
  std::string signature;
};

/// @brief Normalize a type spelling; appends the result to @p normalized
/// @return @c true on success, @c false if there was a parsing error
bool normalize_type(std::string_view type, std::string& normalized) {
  CXIndex index = clang_createIndex(false, false);

  CXUnsavedFile unsaved_file;
  unsaved_file.Filename = "main.c";
  unsaved_file.Contents = type.data();
  unsaved_file.Length = type.length();

  CXTranslationUnit translation_unit = clang_parseTranslationUnit(
    index, "main.c", NULL, 0, &unsaved_file, 1, CXTranslationUnit_SkipFunctionBodies
  );

  if (translation_unit == NULL)
    return false;

  CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);
  CXSourceRange range = clang_getCursorExtent(cursor);
  CXToken* tokens;
  unsigned token_count;
  clang_tokenize(translation_unit, range, &tokens, &token_count);

  if (token_count > 0) {
    for (unsigned i = 0; i < token_count - 1; ++i) {
      CXString spelling = clang_getTokenSpelling(translation_unit, tokens[i]);
      normalized += clang_getCString(spelling);
      normalized += ' ';
      clang_disposeString(spelling);
    }

    CXString spelling = clang_getTokenSpelling(translation_unit, tokens[token_count - 1]);
    normalized += clang_getCString(spelling);
    clang_disposeString(spelling);
  }

  clang_disposeTokens(translation_unit, tokens, token_count);
  clang_disposeTranslationUnit(translation_unit);
  clang_disposeIndex(index);
  return true;
}

/// @brief Compute the associations between type and signature spellings in a given C/C++
/// @p file_name and append them to @p associations
void compute_associations(CXIndex index, const char* file_name, std::vector<Association>& associations) {
  CXTranslationUnit translation_unit = clang_parseTranslationUnit(
    index, file_name, NULL, 0, NULL, 0, CXTranslationUnit_SkipFunctionBodies
  );

  if (translation_unit == NULL)
    return;

  auto visitor = [](CXCursor cursor, CXCursor, CXClientData client_data) -> CXChildVisitResult {
    if (clang_getCursorKind(cursor) != CXCursor_FunctionDecl)
      return CXChildVisit_Continue;

    Association a;

    CXString type_spelling = clang_getTypeSpelling(clang_getCursorType(cursor));
    normalize_type(clang_getCString(type_spelling), a.normalized_type);
    clang_disposeString(type_spelling);

    CXFile file_name;
    clang_getFileLocation(clang_getCursorLocation(cursor), &file_name, &a.line, &a.column, NULL);

    CXString file_name = clang_getFileName(file_name);
    a.file_name = clang_getCString(file_name);
    clang_disposeString(file_name);

    CXPrintingPolicy printing_policy = clang_getCursorPrintingPolicy(cursor);
    clang_PrintingPolicy_setProperty(printing_policy, CXPrintingPolicy_PolishForDeclaration, true);
    CXString signature = clang_getCursorPrettyPrinted(cursor, printing_policy);
    a.signature = clang_getCString(signature);
    clang_disposeString(signature);
    clang_PrintingPolicy_dispose(printing_policy);

    std::vector<Association>& associations = *static_cast<std::vector<Association>*>(client_data);
    associations.push_back(a);

    return CXChildVisit_Continue;
  };

  clang_visitChildren(clang_getTranslationUnitCursor(translation_unit), visitor, &associations);
  clang_disposeTranslationUnit(translation_unit);
}

/// @brief Calculate the Levenshtein distance between two strings
std::size_t distance(std::string_view a, std::string_view b) {
  std::unique_ptr<std::size_t[]> distances =
    std::make_unique<std::size_t[]>((b.size() + 1) * (a.size() + 1));

  for (std::size_t r = 0; r <= a.size(); ++r) {
    distances[(b.size() + 1) * r] = r;
  }

  for (std::size_t c = 1; c <= b.size(); ++c) {
    distances[c] = c;
  }

  for (std::size_t r = 1; r <= a.size(); ++r) {
    for (std::size_t c = 1; c <= b.size(); ++c) {
      distances[(b.size() + 1) * r + c] = std::min({
        distances[(b.size() + 1) * (r - 1) + c] + 1,
        distances[(b.size() + 1) * r + (c - 1)] + 1,
        distances[(b.size() + 1) * (r - 1) + (c - 1)] + (a[r - 1] == b[c - 1] ? 0 : 1),
      });
    }
  }

  return distances[(b.size() + 1) * a.size() + b.size()];
}

/// @brief Parse the command line arguments, possibly printing an error message on errors
/// @details The index of the first remaining argument will be stored in @c optind
/// @return @c true on success @c false otherwise
bool parse_arguments(int argc, char** argv, const char*& query) {
  static const struct option options[] = {
    option{"query", required_argument, NULL, 'q'},
    option{NULL, 0, NULL, 0},
  };

  int i = 0;
  int c;

  while ((c = getopt_long(argc, argv, "q:", options, &i)) != -1) {
    switch (c) {
      case 'q':
        query = optarg;
        break;

      case '?':
        return false;
    }
  }

  return true;
}

int main(int argc, char** argv) {
  const char* query = nullptr;

  if (!parse_arguments(argc, argv, query) || query == nullptr)
    return EXIT_FAILURE;

  std::string normalized_query;

  if (!normalize_type(query, normalized_query))
    return EXIT_FAILURE;

  std::vector<Association> associations;

  {
    CXIndex index = clang_createIndex(false, false);

    for (int i = optind; i < argc; ++i) {
      compute_associations(index, argv[i], associations);
    }

    clang_disposeIndex(index);
  }

  auto compare = [&](const Association& a, const Association& b) -> bool {
    std::size_t distance_a = distance(a.normalized_type, normalized_query);
    std::size_t distance_b = distance(b.normalized_type, normalized_query);
    return distance_a < distance_b;
  };

  std::sort(associations.begin(), associations.end(), compare);

  for (const Association& a : associations) {
    std::cout << a.file_name << ':' << a.line << ':' << a.column << ": " << a.signature << std::endl;
  }

  return EXIT_SUCCESS;
}
