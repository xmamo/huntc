project('huntc', 'c', 'cpp',
  version: '0.3.0',
  default_options: ['warning_level=3'])

c_compiler = meson.get_compiler('c')

llvm_dep = dependency('LLVM',
  method: 'cmake')

clang_dep = c_compiler.find_library('clang',
  dirs: [llvm_dep.get_variable('LLVM_LIBRARY_DIR')])

clang_dep = declare_dependency(
  dependencies: [clang_dep],
  include_directories: [llvm_dep.get_variable('LLVM_INCLUDE_DIR')])

huntc_lib = static_library('huntc', 'huntc.c',
  dependencies: [dependency('glib-2.0'), clang_dep])

huntc_exe = executable('huntc', 'main.c',
  dependencies: [dependency('glib-2.0'), clang_dep],
  link_with: [huntc_lib])

huntc_test_exe = executable('huntc_test', 'test.c',
  dependencies: [dependency('glib-2.0'), clang_dep],
  link_with: [huntc_lib])

test('huntc_test', huntc_test_exe)
