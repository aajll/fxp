# SPDX-License-Identifier: MIT

# Unit-test suite. Each file is its own executable and is registered with
# test() so `meson test` runs them all.

test_core = executable(
  'test_fxp',
  files('test_fxp.c'),
  dependencies: [fxp_dep],
  include_directories: [public_headers, generated_headers],
)
test('core', test_core)

test_rounding = executable(
  'test_rounding',
  files('test_rounding.c'),
  dependencies: [fxp_dep],
  include_directories: [public_headers, generated_headers],
)
test('rounding', test_rounding)

test_hardening = executable(
  'test_hardening',
  files('test_hardening.c'),
  dependencies: [fxp_dep],
  include_directories: [public_headers, generated_headers],
)
test('hardening', test_hardening)

# The differential oracle needs the optional double<->fxp helpers.
if get_option('with_convert')
  test_convert = executable(
    'test_convert',
    files('test_convert.c'),
    dependencies: [fxp_dep],
    include_directories: [public_headers, generated_headers],
  )
  test('convert', test_convert)
endif

# A second build of the core in the 16-bit (Q8.8) configuration, exercised by
# its own test so the WORD=16 code paths (notably the constructors) are covered
# end to end rather than only through the default Q16.16 format.
fxp16_args = ['-DFXP_WORD_BITS=16', '-DFXP_FRAC_BITS=8']
fxp16_lib = static_library(
  'fxp16',
  files('../src/fxp.c'),
  include_directories: [public_headers, generated_headers],
  c_args: fxp16_args,
  install: false,
)
test_format16 = executable(
  'test_format16',
  files('test_format16.c'),
  link_with: fxp16_lib,
  include_directories: [public_headers, generated_headers],
  c_args: fxp16_args,
)
test('format16', test_format16)

# Every narrowing path under each rounding mode. The default library is built
# with FXP_ROUND_NEAREST_EVEN; the other modes get a core build of their own.
test_rounding_modes = executable(
  'test_rounding_modes',
  files('test_rounding_modes.c'),
  dependencies: [fxp_dep],
  include_directories: [public_headers, generated_headers],
)
test('rounding_modes_nearest_even', test_rounding_modes)

foreach mode : ['TRUNCATE', 'NEAREST_UP']
  mode_args = ['-DFXP_ROUNDING=FXP_ROUND_' + mode]
  mode_lib = static_library(
    'fxp_round_' + mode.to_lower(),
    files('../src/fxp.c'),
    include_directories: [public_headers, generated_headers],
    c_args: mode_args,
    install: false,
  )
  test(
    'rounding_modes_' + mode.to_lower(),
    executable(
      'test_rounding_modes_' + mode.to_lower(),
      files('test_rounding_modes.c'),
      link_with: mode_lib,
      include_directories: [public_headers, generated_headers],
      c_args: mode_args,
    ),
  )
endforeach

# Compiles every C block in README.md, CONTRIBUTING.md and docs/ against the
# public headers, so a documented signature cannot drift from the code.
test(
  'fxp docs examples',
  import('python').find_installation(),
  args: [
    files('docs/check_snippets.py'),
    '--root', meson.project_source_root(),
    '--lang', 'c',
    '--prelude', '#include "fxp.h"',
    '--prelude', '#include "fxp_convert.h"',
    '--',
  ] + meson.get_compiler('c').cmd_array() + [
    '-std=c11',
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    '-Werror',
    '-Wno-unused-function',
    '-Wno-unused-variable',
    '-I' + (meson.project_source_root() / 'include'),
    '-c', '{source}', '-o', '{object}',
  ],
  timeout: 60,
)
