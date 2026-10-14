# fxp

[![CI](https://github.com/aajll/fxp/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/fxp/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**Integer-only fixed-point arithmetic for embedded C, with saturating and checked operations and one build-wide rounding policy.**

## Quick start

Copy `include/` and `src/fxp.c` into your project. For a Meson subproject or an installed `pkg-config` package, see the [integration guide](docs/integration.md).

```c
#include "fxp.h"

static const fxp_t kp = FXP_C(3, 2);  /* 1.5, built at compile time */
static const fxp_t hi = FXP_INT(100);

fxp_t
example(void)
{
        fxp_t out;
        enum fxp_status st;

        /* Saturating arithmetic for hot paths. */
        fxp_t y = fxp_mul_sat(kp, FXP_INT(10)); /* 15.0 */
        y = fxp_clamp(y, FXP_INT(0), hi);

        /* Checked arithmetic where the caller must react. */
        st = fxp_div_chk(FXP_INT(7), FXP_INT(2), &out); /* 3.5, FXP_OK */
        if (st == FXP_DIV_ZERO) {
                return y;
        }

        /* Sum products in a wide accumulator, then narrow once. */
        fxp_acc_t acc = fxp_acc_zero();
        acc = fxp_acc_mac(acc, FXP_C(1, 2), FXP_INT(4));
        acc = fxp_acc_mac(acc, FXP_C(1, 4), FXP_INT(8));
        return fxp_acc_narrow_sat(acc); /* 4.0 */
}
```

## Description

`fxp` holds a real number as a signed integer with an implied binary point. The format is fixed at build time and defaults to signed Q16.16: a 32-bit word with 16 fractional bits, a range of about ±32768 and a resolution of 2⁻¹⁶.

- Saturating (`_sat`) and checked (`_chk`) add, subtract, multiply, divide and fused multiply-divide
- A wide accumulator for FIR, biquad and dot-product sums, narrowed once
- Rescaling between fractional-bit counts, clamping, interpolation, rounding and comparison
- One rounding policy on every narrowing path: ties to even by default, or nearest-up or truncate
- No undefined behaviour for any input, no allocation, no global state, and no floating point in the core
- 16-bit and 32-bit words, and targets where `CHAR_BIT` is 16
- 100% line and branch coverage under sanitisers, and MISRA C:2023 analysis with [`misch`](https://pypi.org/project/misch/)

Optional `double` conversion helpers live in a separate translation unit that float-free builds leave out.

## Validated toolchains

| Toolchain               | Target                        | Validation                                   |
| ----------------------- | ----------------------------- | -------------------------------------------- |
| GCC, Clang              | Linux hosts                   | Unit and sanitiser tests in every rounding mode and in Q8.8 |
| DSP vendor C11 compiler | 16-bit MAU (`CHAR_BIT == 16`) | Library compile, two object formats          |

Target linking and execution belong to the consuming firmware. See [cross builds](docs/integration.md#cross-builds).

## Documentation

- [Integration guide](docs/integration.md) — copy-in, Meson subproject, installed package, configuration, and cross builds
- [API reference](docs/api.md) — every function, macro and status code, with filter and control-loop recipes
- [Design](docs/design.md) — the fixed-point model, rounding, narrowing, accumulator headroom, and verification
- [Changelog](CHANGELOG.md) — release history

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before you open a pull request. Report security issues as described in [SECURITY.md](.github/SECURITY.md).

## License

MIT. See [LICENSE](LICENSE).
