# fxp

[![CI](https://github.com/aajll/fxp/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/fxp/actions/workflows/ci.yml)

A portable, integer-only fixed-point arithmetic kernel for embedded C.

fxp provides the deterministic math that fixed-point code otherwise hand-rolls: saturating and checked add/sub/mul/div, fused multiply-divide, a wide multiply-accumulate, format rescaling, clamping, interpolation, and conversion. One audited rounding policy is applied everywhere.

For a detailed guide on the design philosophy, arithmetic hot paths, and common patterns (like dot products/FIR filters), see the [FXP Guide](docs/FXP_GUIDE.md).

## Features

- **No undefined behaviour** — multiplies and scales use a `2*WORD` intermediate and clamp before narrowing; no negative-value left shifts.
- **No allocation, no global state** — values are plain integers; every function is pure and reentrant.
- **Float-free core** — `fxp.c` links no soft-float ABI symbols (CI enforces it). Optional `double` helpers live in a separable translation unit.
- **One build-wide rounding policy** — round to nearest, ties to even (default), applied uniformly to every narrowing path. No floor-vs-truncate surprises.
- **Saturating _and_ checked flavours** — `_sat` clamps and returns; `_chk` reports `FXP_SATURATED` / `FXP_DIV_ZERO` / `FXP_INEXACT` through a status code.
- **Build-time format** — default signed Q16.16, overridable via `fxp_conf.h`.
- **Deterministic** — bit-identical results across compilers and optimisation levels; all loops bounded; config invariants enforced with `_Static_assert`.

## Requirements

- A C11-compatible toolchain (uses `_Static_assert`).
- Conformant `<stdint.h>` / `<stdbool.h>`.
- An arithmetic right shift for signed integers (universal; required by C23).
- No floating point is required by the core; the optional convert helpers use `double` and can be disabled with `-Dwith_convert=false`.

## Installation

### As a Meson subproject

```meson
fxp_dep = dependency('fxp', fallback: ['fxp', 'fxp_dep'])
```

The project exports `meson.override_dependency('fxp', ...)`, so downstream Meson builds resolve the subproject by name. Include the public header directly:

```c
#include "fxp.h"
```

### As an installed dependency

The project installs a static library, the public headers, the generated version header, and a pkg-config file. After install:

```c
#include <fxp/fxp.h>
```

## Quick start

```c
#include "fxp.h"

/* Build-time constants, no floating point. */
static const fxp_t kp = FXP_C(3, 2);  /* 1.5  */
static const fxp_t hi = FXP_INT(100); /* 100  */

void example(void)
{
        fxp_t out;
        enum fxp_status st;

        /* Saturating arithmetic for hot paths. */
        fxp_t y = fxp_mul_sat(kp, FXP_INT(10)); /* 15.0 */
        y = fxp_clamp(y, FXP_INT(0), hi);

        /* Checked arithmetic where you must react to saturation / errors. */
        st = fxp_div_chk(FXP_INT(7), FXP_INT(2), &out); /* out = 3.5 */
        if (st == FXP_DIV_ZERO) {
                /* handle */
        }

        /* Wide multiply-accumulate: sum many products, narrow once. */
        fxp_acc_t acc = fxp_acc_zero();
        acc = fxp_acc_mac(acc, FXP_C(1, 2), FXP_INT(4)); /* 0.5 * 4 */
        acc = fxp_acc_mac(acc, FXP_C(1, 4), FXP_INT(8)); /* 0.25 * 8 */
        out = fxp_acc_narrow_sat(acc);                   /* 4.0 */
}
```

## API overview

Everything is declared in `fxp.h`. The `_sat` variants clamp and return the value; the `_chk` variants write the result through `*out` and return an `enum fxp_status` (`out` is required, status/flag pointers are optional).

### Construction and readout

```c
fxp_t   fxp_from_int(int32_t whole);
fxp_t   fxp_from_ratio(int32_t num, int32_t den, enum fxp_status *st);
int32_t fxp_to_int(fxp_t a);
fxp_t   fxp_floor(fxp_t a);
fxp_t   fxp_ceil(fxp_t a);
fxp_t   fxp_round(fxp_t a);
```

Compile-time literals (integer constant expressions, no floating point):

```c
FXP_C(num, den)   /* num / den, e.g. FXP_C(314, 100) ~= 3.14, FXP_C(-7, 2) == -3.5 */
FXP_INT(whole)    /* exact integer */
FXP_RAW(bits)     /* a pre-scaled raw fixed-point value */
```

### Arithmetic

```c
fxp_t           fxp_add_sat(fxp_t a, fxp_t b);
enum fxp_status fxp_add_chk(fxp_t a, fxp_t b, fxp_t *out);
fxp_t           fxp_sub_sat(fxp_t a, fxp_t b);
enum fxp_status fxp_sub_chk(fxp_t a, fxp_t b, fxp_t *out);
fxp_t           fxp_mul_sat(fxp_t a, fxp_t b);
enum fxp_status fxp_mul_chk(fxp_t a, fxp_t b, fxp_t *out);
enum fxp_status fxp_div_chk(fxp_t a, fxp_t b, fxp_t *out);
enum fxp_status fxp_muldiv_chk(fxp_t a, fxp_t b, fxp_t divisor, fxp_t *out);
```

`fxp_muldiv_chk` evaluates `a * b / divisor` in the wide intermediate and narrows once, avoiding the overflow and precision loss of a separate multiply then divide. `fxp_div_chk` and `fxp_muldiv_chk` report `FXP_DIV_ZERO` on a zero divisor.

### Wide accumulator

```c
fxp_acc_t       fxp_acc_zero(void);
fxp_acc_t       fxp_acc_mac(fxp_acc_t acc, fxp_t a, fxp_t b);
fxp_acc_t       fxp_acc_mac_chk(fxp_acc_t acc, fxp_t a, fxp_t b, bool *overflow);
fxp_t           fxp_acc_narrow_sat(fxp_acc_t acc);
enum fxp_status fxp_acc_narrow_chk(fxp_acc_t acc, fxp_t *out);
```

Accumulate many products, then narrow once. `fxp_acc_mac` is the fast path; `fxp_acc_mac_chk` detects accumulator overflow when operand magnitudes are not known to be bounded.

### Narrow and clamp

```c
fxp_t           fxp_narrow_sat(fxp_wide_t wide);
enum fxp_status fxp_narrow_chk(fxp_wide_t wide, fxp_t *out);
fxp_t           fxp_clamp(fxp_t a, fxp_t lo, fxp_t hi);
fxp_t           fxp_clamp_wide(fxp_wide_t a, fxp_t lo, fxp_t hi, bool *clamped);
```

### Helpers

```c
fxp_t fxp_neg_sat(fxp_t a);
fxp_t fxp_abs_sat(fxp_t a);
fxp_t fxp_lerp(fxp_t a, fxp_t b, fxp_t t);
int   fxp_cmp(fxp_t a, fxp_t b);
```

`fxp_cmp` returns `<0`, `0`, or `>0`, and is defined only for two values in the same format.

### Format conversion

```c
enum fxp_status fxp_rescale(fxp_t a, uint8_t from_frac, uint8_t to_frac, fxp_t *out);
```

### Floating-point conversion (optional)

Declared in `fxp_convert.h` and built only when `-Dwith_convert=true` (the default); these are the only functions that touch `double`.

```c
fxp_t  fxp_from_double(double x, enum fxp_status *st);
double fxp_to_double(fxp_t a);
```

## Configuration

Override before including any fxp header, or on the command line:

| Macro           | Default                  | Meaning                                                         |
| --------------- | ------------------------ | --------------------------------------------------------------- |
| `FXP_WORD_BITS` | `32`                     | Storage width: 16 or 32 (64 unsupported — no portable 128-bit). |
| `FXP_FRAC_BITS` | `16`                     | Fractional bits; `0 < FRAC < WORD`. Defaults give Q16.16.       |
| `FXP_ROUNDING`  | `FXP_ROUND_NEAREST_EVEN` | Also `FXP_ROUND_NEAREST_UP`, `FXP_ROUND_TRUNCATE`.              |

```sh
# Example: Q8.24, truncating, no convert helpers
meson setup build -Dc_args='-DFXP_FRAC_BITS=24 -DFXP_ROUNDING=FXP_ROUND_TRUNCATE' \
                  -Dwith_convert=false
```

## Building

```sh
# Library only (release)
meson setup build --buildtype=release -Dbuild_tests=false
meson compile -C build

# With unit tests + sanitisers
meson setup build --buildtype=debug -Dbuild_tests=true \
                  -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

## Design notes

| Topic               | Note                                                                                                                                               |
| ------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Rounding**        | One policy, build-wide, on every narrowing path. The default (ties-to-even) is zero-bias and IEEE-aligned — important for integrators and filters. |
| **No NaN/Inf**      | Out-of-range is reported by saturation (`_sat`) or status (`_chk`), never a sentinel value — which is why the checked variants exist.              |
| **Mixing formats**  | The type is a bare integer; same-format compare is valid, cross-format is not. Convert at boundaries with `fxp_rescale`.                           |
| **Float-free core** | All `double` lives in `fxp_convert.c`; the core builds clean under `-mfloat-abi=soft`.                                                             |
