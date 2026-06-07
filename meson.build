project(
  'fxp',
  'c',
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'warning_level=3',
  ],
  license: 'MIT',
)

project_description = 'Portable fixed-point arithmetic kernel for embedded C'

ver = meson.project_version().split('.')
major = ver[0].to_int()
minor = ver[1].to_int()
patch = ver[2].to_int()

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set('MAJOR', major)
conf.set('MINOR', minor)
conf.set('PATCH', patch)

generated_version_header = configure_file(
  input: 'config' / 'fxp_version.h.in',
  output: 'fxp_version.h',
  configuration: conf,
  install: true,
  install_dir: get_option('includedir') / 'fxp',
)

public_headers = include_directories('include')
generated_headers = include_directories('.')

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
  include_directories: [public_headers, generated_headers],
  install: true,
)

install_headers(
  public_header_list,
  subdir: 'fxp'
)

fxp_dep = declare_dependency(
  include_directories: [public_headers, generated_headers],
  link_with: fxp_lib,
  sources: generated_version_header,
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
