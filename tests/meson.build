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
