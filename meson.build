project('huntc', 'c', 'cpp',
  version: '0.3.0',
  default_options: ['warning_level=3']
)

c_compiler = meson.get_compiler('c')

llvm_dep = dependency('llvm')

clang_dep = declare_dependency(
  dependencies: [c_compiler.find_library('clang', dirs: [llvm_dep.get_variable('libdir')])],
  include_directories: [llvm_dep.get_variable('includedir')]
)

huntc_exe = executable('huntc', 'huntc.c', 'main.c',
  dependencies: [dependency('glib-2.0'), clang_dep]
)

huntc_test_exe = executable('huntc_test', 'huntc.c', 'test.c',
  dependencies: [dependency('glib-2.0'), clang_dep]
)

test('huntc_test', huntc_test_exe)
