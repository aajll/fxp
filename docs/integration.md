# Integration guide

This guide shows how to add `fxp` to a project, configure it, build it, and run its tests. The API is described in [api.md](api.md), and the arithmetic model in [design.md](design.md).

## Requirements

- A C11 compiler with `int16_t`, `int32_t` and `int_least64_t`. A 32-bit word also needs `int64_t`. The configuration headers use `_Static_assert`.
- Arithmetic right shift of negative signed values. Every common compiler provides it and C23 requires it; a static assertion rejects a compiler that does not.
- No floating point in the core. The optional conversion helpers use `double`.
- For the tests: Meson, Ninja, and Python 3.

## Copy-in

Copy-in is the recommended method for embedded targets. Copy the headers and the core source into your source tree:

```
include/fxp.h
include/fxp_conf.h
include/fxp_config.h
include/fxp_const.h
include/fxp_status.h
src/fxp.c
```

Add `include/fxp_convert.h` and `src/fxp_convert.c` only if you want the `double` helpers. Compile every source with the same configuration macros as every file that includes the header, then include it:

```c
#include "fxp.h"
```

`fxp.h` defines the version macros described in [api.md](api.md#version).

## Meson subproject

Put the repository in `subprojects/fxp`, as a checkout or through a wrap file, then declare the dependency:

```meson
fxp_dep = dependency('fxp', fallback: ['fxp', 'fxp_dep'])
```

The build calls `meson.override_dependency('fxp', ...)`, so other subprojects in the same build find `fxp` by name. Include the header as `"fxp.h"`. Tests are on by default; a consumer that only needs the library passes `-Dfxp:build_tests=false`.

| Option | Default | Effect |
| --- | --- | --- |
| `build_tests` | `true` | Builds the unit tests and the documentation example test. |
| `with_convert` | `true` | Builds `src/fxp_convert.c` and installs `fxp_convert.h`. Set `false` on a float-free target. |

## Installed package

```sh
meson setup build --buildtype=release -Dbuild_tests=false --prefix=/usr/local
meson compile -C build
meson install -C build
cc app.c $(pkg-config --cflags --libs fxp) -o app
```

The headers install to an `fxp/` subdirectory, and the `pkg-config` flags add that directory to the include path:

<!-- snippet: skip -->
```c
#include "fxp.h"
```

`fxp_version.h` is installed beside it as a deprecated header that includes `fxp.h`.

## Configuration

The format and rounding policy are macros with defaults in `fxp_conf.h`. Define them with `-D`, or before the first include of any `fxp` header, identically for the library sources and every file that includes `fxp.h`. The library is built for one format; to mix formats, convert at the boundary with `fxp_rescale`.

| Macro | Default | Effect |
| --- | --- | --- |
| `FXP_WORD_BITS` | `32` | Storage width, `16` or `32`. A 64-bit word is not supported because its products need a 128-bit intermediate. |
| `FXP_FRAC_BITS` | `16` | Fractional bits. Must satisfy `0 < FXP_FRAC_BITS < FXP_WORD_BITS - 1`, so that 1.0 is representable. |
| `FXP_ROUNDING` | `FXP_ROUND_NEAREST_EVEN` | Rounding on every narrowing path: `FXP_ROUND_NEAREST_EVEN`, `FXP_ROUND_NEAREST_UP` or `FXP_ROUND_TRUNCATE`. |

The defaults give signed Q16.16 with ties to even, and are part of the public contract. Each rounding mode is described in [design.md](design.md#rounding). An invalid combination fails the build at a static assertion.

A Meson build takes the macros through `c_args`, for example Q8.24 with truncation and no conversion helpers:

```sh
meson setup build "-Dc_args=-DFXP_FRAC_BITS=24 -DFXP_ROUNDING=FXP_ROUND_TRUNCATE" -Dwith_convert=false
```

## Build and test

```sh
# Library only
meson setup build --buildtype=release -Dbuild_tests=false
meson compile -C build

# Unit tests under sanitisers: the default build, the Q8.8 build, each
# rounding mode, the conversion oracle, and the documentation test
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

Coverage, formatting, and MISRA commands are in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Cross builds

The consuming firmware owns its cross file, linker command file, and memory placement. No cross file ships with this project.

CI compiles the library with warnings as errors using two releases of a vendor C11 compiler for a 16-bit-MAU DSP target, where `CHAR_BIT` is 16, under both of its object formats. This covers the source dialect and type assumptions only. Linking, execution, and the final map-file footprint remain checks for the consumer.

On a target without a floating-point unit, build with `-Dwith_convert=false`, or leave `src/fxp_convert.c` out of a copy-in build, so no `double` code is compiled.
