# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- CI runs MISRA C:2023 analysis with `misch` and fails on any finding outside the baseline recorded in `analysis/baseline/`.
- `scripts/format.sh`, which pins the clang-format release and runs it with `uvx`. CI checks formatting through it.
- The test suite builds the core under each rounding mode (`FXP_ROUND_TRUNCATE`, `FXP_ROUND_NEAREST_UP` and `FXP_ROUND_NEAREST_EVEN`) and checks every narrowing path against the expected result for ties, non-ties and exact values of both signs, with positive and negative divisors.

### Changed

- `fxp.h` defines `FXP_VERSION_MAJOR`, `_MINOR`, `_PATCH`, `FXP_VERSION`, `FXP_VERSION_STRING` and a new `FXP_VERSION_NUMBER` (`long`, `MAJOR * 10000 + MINOR * 100 + PATCH`), so a copied-in header carries its version. Meson fails to configure when the header and `project(version:)` disagree, and the build needs Meson 0.57 or later.
- The README is a short landing page. `docs/integration.md`, `docs/api.md` and `docs/design.md` replace `docs/FXP_GUIDE.md`, and a test compiles every C example in the documentation.
- CI installs Meson, Ninja and gcovr with `uv` instead of `pip` and `apt`.
- CI gates line and branch coverage at 100 %, up from 80 % line and 70 % branch.
- The zero-distance check in the internal shift-rounding helper is removed: every caller passes a positive distance. Results are unchanged.


### Removed

- `fxp_version.h`. `fxp.h` defines the version macros.
- The `extern "C"` guards in `fxp.h` and `fxp_convert.h`. The library is C11 only and its headers make no C++ claim.

### Fixed

- `fxp_rescale` takes its fractional-bit counts as `uint_least8_t` instead of `uint8_t`, so the library compiles on targets with `CHAR_BIT == 16`, where C11 does not provide `uint8_t`. Where `uint8_t` exists the two are the same type, so callers need no change.

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
