project('coogle', 'c', 'cpp',
  version: '0.1.0',
  default_options: ['warning_level=3'])

c_compiler = meson.get_compiler('c')

llvm_dep = dependency('LLVM',
  method: 'cmake')

clang_dep = c_compiler.find_library('clang',
  dirs: [llvm_dep.get_variable('LLVM_LIBRARY_DIR')])

clang_dep = declare_dependency(
  dependencies: [clang_dep],
  include_directories: [llvm_dep.get_variable('LLVM_INCLUDE_DIR')])

executable('coogle', 'main.c',
  dependencies: [dependency('glib-2.0'), clang_dep])
