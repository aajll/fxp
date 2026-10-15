# Design

`fxp` is a C11 fixed-point arithmetic library. It holds real numbers as signed integers with an implied binary point, in one format fixed at build time, and performs every operation in integer arithmetic with a single rounding policy. It allocates nothing and keeps no state.

## Goals and non-goals

Goals:

- Arithmetic without floating point, for targets with no floating-point unit or where a soft-float library is too large or too slow.
- Results that are bit-identical across compilers, targets and optimisation levels.
- No undefined behaviour for any input value.
- One rounding rule for the whole build, so filters and integrators carry no hidden bias from mixed rounding.

Non-goals:

- Several formats in one build, or a format carried with each value. A consumer that exchanges data in another format converts at the boundary.
- A 64-bit word. Its products need a 128-bit intermediate, which C11 does not provide portably.
- Transcendental functions, square roots and lookup tables.
- Detecting misuse that the type system cannot see, such as mixing two formats or narrowing a product sum with the wrong function.

## Fixed-point model

A value is `raw / 2^F`, where `raw` is the stored integer and `F` is `FXP_FRAC_BITS`. The binary point is not stored; it is an agreement between the code that writes a value and the code that reads it. In Qm.n notation, m counts the integer bits including the sign and n the fractional bits, and m + n is the word width.

| Format | Word | Range | Resolution |
| --- | --- | --- | --- |
| Q16.16 (default) | 32 bits | -32768 to 32767.99998 | 2⁻¹⁶, about 0.000015 |
| Q8.24 | 32 bits | -128 to 127.99999994 | 2⁻²⁴ |
| Q8.8 | 16 bits | -128 to 127.996 | 2⁻⁸, about 0.0039 |

More fractional bits give finer resolution and a smaller range. `FXP_FRAC_BITS` must leave at least one integer bit beyond the sign, so that 1.0 is representable; the library and the constant macros depend on `FXP_ONE`. Pure-fraction formats such as Q0.15 are therefore not supported.

The format is a build-time choice because a runtime tag would cost a branch or a table lookup on every operation, and fixed-point code is usually written for one known range. Values in another format are rescaled at the boundary with `fxp_rescale`. The type is a bare integer, so the compiler cannot reject a mix of formats; it is the caller's responsibility.

## Types

`fxp_t` is a 16-bit or 32-bit signed integer. `fxp_wide_t` is twice that width and holds the exact product of two `fxp_t` values, with `2F` fractional bits. Every multiply, divide and scale forms its intermediate in `fxp_wide_t`, so no partial result overflows.

`fxp_acc_t` is `fxp_wide_t` under a separate name, for sums of products. `fxp_calc_t` is at least 64 bits in every format: the constructors take `int32_t` arguments, and in a 16-bit build `fxp_wide_t` is only 32 bits, too narrow to scale them before saturating.

## Rounding

Every operation that discards low bits applies `FXP_ROUNDING`: multiply, divide, fused multiply-divide, accumulator narrowing, rescaling to fewer fractional bits, `fxp_from_ratio` and `fxp_to_int`. All of them go through two internal helpers, one for shifts and one for divides, so the rule cannot differ between paths.

| Mode | Rule | Bias |
| --- | --- | --- |
| `FXP_ROUND_NEAREST_EVEN` (default) | nearest, ties to the even neighbour | none on average |
| `FXP_ROUND_NEAREST_UP` | nearest, ties toward positive infinity | small positive bias on exact halves |
| `FXP_ROUND_TRUNCATE` | toward negative infinity | about half an LSB negative on average |

Ties to even matches the IEEE 754 default and has no statistical bias, which matters for integrators and recursive filters, where a biased rounding accumulates into a steady offset. Truncation is the cheapest and suits code that has already budgeted for its bias.

Both helpers measure the discarded remainder from the floor, which an arithmetic right shift gives directly, so the remainder always lies in [0, 2^s) and a single comparison against one half selects the result for either sign. The divide helper normalises to a positive divisor so the same logic applies.

Four conversions round by a fixed rule, independent of `FXP_ROUNDING`. `FXP_C` rounds half away from zero, because it is an integer constant expression evaluated by the compiler. `fxp_round` rounds ties to even, because it returns an integral value rather than narrowing. `fxp_floor` and `fxp_ceil` round in the direction their names give. `fxp_from_double` rounds half away from zero, which needs no `<math.h>`.

## Narrowing and saturation

Narrowing reduces a wide intermediate to `fxp_t`. It rounds the discarded bits, shifts a product sum back from `2F` to `F` fractional bits, and clamps to [`FXP_MIN`, `FXP_MAX`]. A plain cast would instead keep the low bits, which is an implementation-defined conversion and in practice a wrap-around. The accumulator narrowing functions shift; `fxp_narrow_sat` and `fxp_narrow_chk` do not, because their input is already in the build format.

Fixed-point has no NaN or infinity to mark an invalid result, so out-of-range results are clamped and reported. Each operation that can saturate comes as `_sat`, which returns the clamped value, and `_chk`, which also returns a status. Both clamp identically, so a caller can move between them without changing results. Division and fused multiply-divide have only a `_chk` form: their zero-divisor result is 0, and a `_sat` form would make that indistinguishable from a real zero.

The `out` pointer of a `_chk` function is a precondition rather than a runtime check, so the arithmetic carries no extra branch. Reporting pointers that a caller may not want (`st`, `overflow`, `clamped`) accept `NULL`.

## Accumulator headroom

An accumulator sums raw products, so its headroom is the gap between the largest product and the accumulator limit. In Q16.16 the accumulator is 64 bits.

| Operand magnitude | Largest product | Terms before overflow |
| --- | --- | --- |
| below 1.0 | below 2³² | about 2³¹ |
| full scale, about 32768 | about 2⁶² | 2 |

Normalised signals and coefficients, as in most filters, leave room for any practical number of taps. Full-scale operands do not, and `fxp_acc_mac` does not check, because a check on every tap would cost the hot path its purpose. `fxp_acc_mac_chk` checks each addition before it is made and clamps the accumulator to its limit, so the final narrow saturates in the right direction instead of wrapping.

## Defined behaviour

The library avoids every source of undefined or implementation-defined behaviour that fixed-point code usually relies on:

- Products and scaled values are formed in a type wide enough to hold them, then clamped before the final conversion to `fxp_t`.
- No negative value is shifted left. Scaling multiplies by `FXP_ONE`, and remainders are taken with a mask.
- Right shifts of negative values must be arithmetic. C23 requires it, every common compiler does it, and a static assertion fails the build on a compiler that does not.
- `FXP_MIN` has no positive counterpart, so negation and absolute value map it to `FXP_MAX` explicitly.
- `fxp_rescale` rejects shift counts of `FXP_WIDE_BITS` or more, which would be undefined, and handles shifts of the full word width without forming the shift.
- `fxp_acc_mac` is the one exception: it leaves accumulator overflow to the caller, as described above.

## Float-free core

`src/fxp.c` contains no floating-point code, so linking it pulls no soft-float routines into a target image. The `double` helpers live in `src/fxp_convert.c`, which a Meson build leaves out with `-Dwith_convert=false` and a copy-in build simply does not copy. Compile-time constants come from integer macros, so coefficient tables also need no floating point.

## Portability

- C11 with `<stdint.h>` and `<stdbool.h>`. No other dependencies, and `<stddef.h>` only for `NULL`.
- 16-bit and 32-bit words. A 16-bit build uses a 32-bit wide type and the 64-bit `fxp_calc_t` for construction.
- No exact 8-bit types. `fxp_rescale` takes `uint_least8_t`, which exists where `CHAR_BIT` is 16 and `uint8_t` does not.
- Static assertions reject an unsupported word width, a fractional-bit count outside its range, an unknown rounding mode, and a non-arithmetic right shift.

## Verification

The test suite runs against the default Q16.16 build and covers:

- construction, readout, arithmetic, the accumulator, narrowing, helpers, comparison and rescaling, including the saturation and status of each;
- the out-of-domain and overflowing `fxp_rescale` arguments and accumulator overflow detection;
- a differential oracle that compares the integer arithmetic with `double` over a swept grid of operands, and the conversion helpers themselves;
- a separate core build in Q8.8 with 16-bit words, so the 16-bit paths and constructor saturation run end to end;
- every narrowing path in each rounding mode, against a core build per mode, for ties, non-ties and exact values of both signs, with positive and negative divisors.

CI runs the suite under AddressSanitizer and UndefinedBehaviorSanitizer, gates `src/` and `include/` at 100 percent line and branch coverage, builds the core without the conversion unit and scans its object for soft-float helper symbols, and builds a release configuration. A documentation test compiles every C example in the README and `docs/` against the headers.

CI compiles the library with warnings as errors under TI C2000 Code Generation Tools 22.6.2.LTS and 25.11.1.LTS in C11 C28x mode, under both the COFF and EABI ABIs. This validates the source dialect and type assumptions only. Linking, target execution, and final map-file footprint remain consumer-owned checks.

MISRA C:2023 analysis with `misch` runs in CI over `src/` and `include/`, and fails on any finding not recorded in the baseline in `analysis/baseline/`. The version macros in `fxp.h` carry an inline deviation of Rule 2.5 (unused macro), because they exist for consumers and the library does not use them.
