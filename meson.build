project('bcrush', 'c',
  meson_version : '>=0.45.0',
  default_options : [
    'b_ndebug=if-release',
    'buildtype=release',
    'c_std=c99',
    'default_library=static',
    'warning_level=3',
  ],
  version : '0.2.1',
  license : 'Zlib'
)

lib = library('crush', 'crush.c', 'crush_depack.c', 'crush_depack_file.c')

crush_dep = declare_dependency(
  include_directories : include_directories('.'),
  link_with : lib,
  version : meson.project_version()
)

executable('bcrush', 'bcrush.c', 'parg.c', dependencies : crush_dep)
