# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-06-07

Initial release. A portable, integer-only fixed-point arithmetic kernel.

### Added

- Build-time format selection via `fxp_conf.h` (`FXP_WORD_BITS`, `FXP_FRAC_BITS`); default signed Q16.16.
- Single, build-wide rounding policy (`FXP_ROUNDING`): round to nearest, ties to even (default), nearest-up, or truncate — applied uniformly to every narrowing path.
- Saturating (`_sat`) and checked (`_chk`) `add`, `sub`, `mul`; checked `div` and fused `muldiv` with divide-by-zero reporting.
- Wide multiply-accumulate (`fxp_acc_t`, `fxp_acc_mac`, `fxp_acc_narrow_*`) for FIR/biquad/dot-product sums that narrow once, plus a checked `fxp_acc_mac_chk` that detects accumulator overflow and clamps.
- Checked narrow, clamp / clamp-with-saturation-report, saturating `neg`/`abs`, `lerp`, three-way `cmp`, and `rescale` between fractional-bit counts.
- Construction/readout: `fxp_from_int`, `fxp_from_ratio`, `fxp_to_int`, `fxp_floor`/`ceil`/`round`, and the compile-time literal macros `FXP_C`, `FXP_INT`, `FXP_RAW` (no floating point). Out-of-range constructor inputs saturate.
- `fxp_rescale` between fractional-bit counts, with argument-domain validation (`FXP_BAD_PARAM`) and overflow-safe up-scaling.
- Status codes (`enum fxp_status`): `FXP_OK`, `FXP_SATURATED`, `FXP_OVERFLOW`, `FXP_DIV_ZERO`, `FXP_INEXACT`, `FXP_BAD_PARAM`.
- Optional, separable `double` conversion helpers (`fxp_convert.c`, `-Dwith_convert`) kept out of the float-free core.
- Configuration invariants enforced with `_Static_assert` (including `FXP_FRAC_BITS < FXP_WORD_BITS-1` so `1.0` is always representable).
- Full Doxygen contract annotations (`@pre`/`@post`/`@retval`/`@warning`) across the public API, including the required-`out` / NULL-tolerant pointer policy.
- Unit-test suite (core, rounding consistency, edge cases, a 16-bit configuration build, and a double-oracle sweep) with ASan/UBSan, a coverage gate, and a float-free-core (nm) gate in CI.

[0.1.0]: https://github.com/aajll/fxp/releases/tag/v0.1.0
