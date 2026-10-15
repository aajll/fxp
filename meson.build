# SPDX-License-Identifier: MIT

project(
  'fxp',
  'c',
  version: '0.1.0',
  meson_version: '>=0.57.0',
  default_options: [
    'c_std=c11',
    'warning_level=3',
  ],
  license: 'MIT',
)

project_description = 'Portable fixed-point arithmetic kernel for embedded C'

# === Version ==================================================================
# include/fxp.h defines the version macros, so a copy-in build has them.
# Configuration fails when they differ from the project version.

fs = import('fs')
version_parts = meson.project_version().split('.')
version_macros = {
  'FXP_VERSION_MAJOR': version_parts[0],
  'FXP_VERSION_MINOR': version_parts[1],
  'FXP_VERSION_PATCH': version_parts[2],
  'FXP_VERSION': '"' + meson.project_version() + '"',
}
header_lines = []
foreach line : fs.read('include' / 'fxp.h').split('\n')
  header_lines += ' '.join(line.split())
endforeach
foreach name, value : version_macros
  if '#define @0@ @1@'.format(name, value) not in header_lines
    error('include/fxp.h must define @0@ as @1@'.format(name, value))
  endif
endforeach

public_headers = include_directories('include')

# Core is strictly integer-only (no floating-point ABI). The optional
# double<->fxp conversion helpers live in a separate translation unit so they
# can be excluded on float-free targets via -Dwith_convert=false.
library_sources = files('src/fxp.c')

public_header_list = [
  'include/fxp.h',
  'include/fxp_conf.h',
  'include/fxp_config.h',
  'include/fxp_const.h',
  'include/fxp_status.h',
]

if get_option('with_convert')
  library_sources += files('src/fxp_convert.c')
  public_header_list += 'include/fxp_convert.h'
endif

fxp_lib = static_library(
  'fxp',
  library_sources,
  include_directories: [public_headers],
  install: true,
)

install_headers(
  public_header_list,
  subdir: 'fxp'
)

fxp_dep = declare_dependency(
  include_directories: [public_headers],
  link_with: fxp_lib,
)

meson.override_dependency('fxp', fxp_dep)

pkg = import('pkgconfig')
pkg.generate(
  fxp_lib,
  description: project_description,
  filebase: 'fxp',
  name: 'fxp',
  subdirs: 'fxp',
)

if get_option('build_tests')
  subdir('tests')
endif
