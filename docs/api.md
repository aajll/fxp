# API reference

Every declaration lives in [`include/fxp.h`](../include/fxp.h) and the headers it includes, which carry the full Doxygen contract. Configuration macros are listed in the [integration guide](integration.md#configuration), and the reasons behind these rules are in [design.md](design.md). Values in the examples assume the default Q16.16 format with ties to even.

## Conventions

- A `_sat` function clamps an out-of-range result to [`FXP_MIN`, `FXP_MAX`] and returns it.
- A `_chk` function writes its result through `out` and returns an `enum fxp_status`. On saturation `*out` holds the clamped value, so a `_chk` call never yields less than its `_sat` counterpart.
- `out` is required: it must not be `NULL`, and it is not checked at run time. Optional reporting pointers (`st`, `overflow`, `clamped`) accept `NULL`.
- Division and fused multiply-divide exist only as `_chk`, so a zero divisor is always distinguishable from a large result.
- Every function is pure: no allocation, no global state, and calls are reentrant and safe from interrupt handlers.

## Types and limits

<!-- snippet: skip -->
```c
typedef int32_t fxp_t;          /* int16_t when FXP_WORD_BITS is 16 */
typedef int64_t fxp_wide_t;     /* int32_t when FXP_WORD_BITS is 16 */
typedef fxp_wide_t fxp_acc_t;
typedef int_least64_t fxp_calc_t;
```

`fxp_t` holds a value as `raw / 2^FXP_FRAC_BITS`. `fxp_wide_t` is twice the word width and holds a full product. `fxp_acc_t` is the accumulator type. `fxp_calc_t` is at least 64 bits in every format, and the constructors use it so that their `int32_t` arguments scale without overflow in a 16-bit build.

| Macro | Meaning | Q16.16 |
| --- | --- | --- |
| `FXP_ONE` | 1.0, `1 << FXP_FRAC_BITS` | `65536` |
| `FXP_MAX` | largest value, `2^(WORD-1) - 1` raw | `2147483647`, about 32767.99998 |
| `FXP_MIN` | most negative value, `-2^(WORD-1)` raw | `-2147483648`, -32768.0 |
| `FXP_FRAC_MASK` | mask of the fractional bits | `0xFFFF` |
| `FXP_WIDE_BITS` | bit width of `fxp_wide_t` | `64` |
| `FXP_WIDE_MAX`, `FXP_WIDE_MIN` | limits of `fxp_wide_t` | `INT64_MAX`, `INT64_MIN` |

## Status codes

<!-- snippet: skip -->
```c
enum fxp_status {
        FXP_OK = 0,
        FXP_SATURATED,
        FXP_OVERFLOW,
        FXP_DIV_ZERO,
        FXP_INEXACT,
        FXP_BAD_PARAM,
};
```

`FXP_OK` means an exact, in-range result. `FXP_SATURATED` means the result was out of range and clamped. `FXP_OVERFLOW` is returned only by `fxp_from_double`, when the input cannot be represented. `FXP_DIV_ZERO` means the divisor was zero and the result is 0. `FXP_INEXACT` means `fxp_rescale` discarded non-zero low bits. `FXP_BAD_PARAM` means an argument was outside its domain.

## Compile-time constants

<!-- snippet: skip -->
```c
#define FXP_INT(whole)
#define FXP_C(num, den)
#define FXP_RAW(bits)
```

Each macro is an integer constant expression in the build format, so it can initialise a `static const` table without floating point. `FXP_INT(whole)` is the exact integer. `FXP_C(num, den)` is `num / den`, rounded half away from zero; this compile-time rounding is independent of `FXP_ROUNDING`. `FXP_RAW(bits)` wraps an already-scaled raw value, such as a coefficient exported by a design tool.

```c
static const fxp_t gains[] = {
        FXP_INT(-5),              /* -5.0 */
        FXP_C(1, 10),             /* 0.1, raw 6554 */
        FXP_C(-7, 2),             /* -3.5 */
        FXP_C(314159, 100000),    /* 3.141586, raw 205887 */
        FXP_RAW(98304),           /* 1.5 */
};
```

The macros perform no range check and may evaluate their arguments more than once. A value outside the format, such as `FXP_INT(100000)` in Q16.16, is converted to `fxp_t` out of range, which is implementation-defined. Pass only constant, side-effect-free arguments, and use `fxp_from_int` or `fxp_from_ratio` for values that are not known to fit.

## Construction and readout

<!-- snippet: skip -->
```c
fxp_t fxp_from_int(int32_t whole);
fxp_t fxp_from_ratio(int32_t num, int32_t den, enum fxp_status *st);
int32_t fxp_to_int(fxp_t a);
fxp_t fxp_floor(fxp_t a);
fxp_t fxp_ceil(fxp_t a);
fxp_t fxp_round(fxp_t a);
```

`fxp_from_int` returns `whole` as a fixed-point value, saturated to the format: `fxp_from_int(100000)` is `FXP_MAX` in Q16.16. `fxp_from_ratio` returns `num / den` rounded per `FXP_ROUNDING`; it returns 0 with `FXP_DIV_ZERO` when `den` is 0, and saturates with `FXP_SATURATED` when the quotient is out of range. `st` may be `NULL`.

`fxp_to_int` rounds to the nearest integer per `FXP_ROUNDING`. Rounding can carry one past the largest whole value: `fxp_to_int(FXP_MAX)` is 32768 in Q16.16.

`fxp_floor`, `fxp_ceil` and `fxp_round` return an integral value as an `fxp_t`. `fxp_floor` rounds toward negative infinity and `fxp_ceil` toward positive infinity, saturating to `FXP_MAX` when the ceiling is not representable. `fxp_round` always rounds ties to even, whatever `FXP_ROUNDING` is.

<!-- snippet: body -->
```c
enum fxp_status st;
fxp_t duty = fxp_from_ratio(3, 4, &st);         /* 0.75, FXP_OK */
fxp_t bad = fxp_from_ratio(1, 0, &st);          /* 0, FXP_DIV_ZERO */

int32_t up = fxp_to_int(FXP_C(51, 2));          /* 25.5 -> 26 */
int32_t down = fxp_to_int(FXP_C(49, 2));        /* 24.5 -> 24 */

fxp_t f = fxp_floor(FXP_C(-5, 2));              /* -3.0 */
fxp_t c = fxp_ceil(FXP_C(-5, 2));               /* -2.0 */
fxp_t r = fxp_round(FXP_C(7, 2));               /* 3.5 -> 4.0 */
(void)duty; (void)bad; (void)up; (void)down; (void)f; (void)c; (void)r;
```

## Arithmetic

<!-- snippet: skip -->
```c
fxp_t fxp_add_sat(fxp_t a, fxp_t b);
enum fxp_status fxp_add_chk(fxp_t a, fxp_t b, fxp_t *out);
fxp_t fxp_sub_sat(fxp_t a, fxp_t b);
enum fxp_status fxp_sub_chk(fxp_t a, fxp_t b, fxp_t *out);
fxp_t fxp_mul_sat(fxp_t a, fxp_t b);
enum fxp_status fxp_mul_chk(fxp_t a, fxp_t b, fxp_t *out);
enum fxp_status fxp_div_chk(fxp_t a, fxp_t b, fxp_t *out);
enum fxp_status fxp_muldiv_chk(fxp_t a, fxp_t b, fxp_t divisor, fxp_t *out);
```

Add and subtract saturate instead of wrapping. Multiply forms the full product in `fxp_wide_t`, narrows it once with the build rounding, and saturates. The `_chk` forms return `FXP_OK` or `FXP_SATURATED`.

`fxp_div_chk` returns `a / b`, rounded and saturated. When `b` is 0 it writes 0 and returns `FXP_DIV_ZERO`; a caller that ignores the status cannot tell that result from a real zero.

`fxp_muldiv_chk` returns `a * b / divisor` from the full product, narrowed once, with the same zero-divisor rule. A separate multiply and divide round the product first and can saturate it, so the fused form is the one to use for any `(a * b) / c`.

<!-- snippet: body -->
```c
fxp_t q;
enum fxp_status st = fxp_div_chk(FXP_INT(100), FXP_INT(3), &q);  /* 33.33333, FXP_OK */

/* 300 * 200 saturates in Q16.16, but the fused result fits. */
fxp_t p;
fxp_t two_step;
fxp_t fused;
st = fxp_mul_chk(FXP_INT(300), FXP_INT(200), &p);                 /* FXP_SATURATED */
st = fxp_div_chk(p, FXP_INT(400), &two_step);                      /* 81.92 */
st = fxp_muldiv_chk(FXP_INT(300), FXP_INT(200), FXP_INT(400), &fused); /* 150.0, FXP_OK */
(void)q; (void)st; (void)two_step; (void)fused;
```

## Wide accumulator

<!-- snippet: skip -->
```c
fxp_acc_t fxp_acc_zero(void);
fxp_acc_t fxp_acc_mac(fxp_acc_t acc, fxp_t a, fxp_t b);
fxp_acc_t fxp_acc_mac_chk(fxp_acc_t acc, fxp_t a, fxp_t b, bool *overflow);
fxp_t fxp_acc_narrow_sat(fxp_acc_t acc);
enum fxp_status fxp_acc_narrow_chk(fxp_acc_t acc, fxp_t *out);
```

An accumulator holds a sum of full products, each with twice the fractional bits, so no term is rounded or clamped before the end. `fxp_acc_narrow_sat` and `fxp_acc_narrow_chk` shift the sum back to the build format with the build rounding, and saturate.

`fxp_acc_mac` does not check the accumulator for overflow, and signed overflow is undefined behaviour. Use it only when the operand magnitudes bound the sum; the headroom is worked out in [design.md](design.md#accumulator-headroom). `fxp_acc_mac_chk` sets `*overflow` to whether this addition would have overflowed and, if so, clamps the accumulator to `FXP_WIDE_MAX` or `FXP_WIDE_MIN`, so the later narrow saturates. The flag describes only the current call; to know whether any term overflowed, combine the flags.

<!-- snippet: body -->
```c
fxp_acc_t acc = fxp_acc_zero();
bool overflow = false;
bool any = false;

/* Two full-scale products fit in 64 bits; the third does not. */
for (int i = 0; i < 3; i++) {
        acc = fxp_acc_mac_chk(acc, FXP_MAX, FXP_MAX, &overflow);
        any = any || overflow;
}
fxp_t sum = fxp_acc_narrow_sat(acc);  /* FXP_MAX, any == true */
(void)sum;
```

## Narrowing

<!-- snippet: skip -->
```c
fxp_t fxp_narrow_sat(fxp_wide_t wide);
enum fxp_status fxp_narrow_chk(fxp_wide_t wide, fxp_t *out);
```

These saturate a wide value that is already in the build format to `fxp_t`. They do not shift, so they are for wide sums and scaled values, not for product sums: an accumulator narrowed this way is off by `2^FXP_FRAC_BITS`.

<!-- snippet: body -->
```c
fxp_t out;
enum fxp_status st = fxp_narrow_chk((fxp_wide_t)FXP_INT(10000) * 2, &out);  /* 20000.0, FXP_OK */
st = fxp_narrow_chk((fxp_wide_t)FXP_INT(20000) * 2, &out);  /* FXP_MAX, FXP_SATURATED */
(void)st;
```

## Helpers

<!-- snippet: skip -->
```c
fxp_t fxp_neg_sat(fxp_t a);
fxp_t fxp_abs_sat(fxp_t a);
fxp_t fxp_clamp(fxp_t a, fxp_t lo, fxp_t hi);
fxp_t fxp_clamp_wide(fxp_wide_t a, fxp_t lo, fxp_t hi, bool *clamped);
fxp_t fxp_lerp(fxp_t a, fxp_t b, fxp_t t);
int fxp_cmp(fxp_t a, fxp_t b);
```

`fxp_neg_sat` and `fxp_abs_sat` map `FXP_MIN`, which has no positive counterpart in two's complement, to `FXP_MAX`.

`fxp_clamp` limits `a` to [`lo`, `hi`], which requires `lo <= hi`. `fxp_clamp_wide` does the same for a wide value in the build format and sets `*clamped` to whether it was outside the range, so a control loop can clamp and detect saturation in one step. `clamped` may be `NULL`.

`fxp_lerp` returns `a + (b - a) * t`, with `b - a` formed in the wide type so opposite-signed extremes do not overflow, and saturates the result. A `t` in [0, 1] interpolates and a `t` outside it extrapolates: `fxp_lerp(FXP_C(1, 5), FXP_C(4, 5), FXP_C(1, 2))` is 0.5.

`fxp_cmp` returns a negative value, 0 or a positive value as `a` is less than, equal to or greater than `b`. It gives the same order as `<` on two values in the same format; values with different fractional-bit counts must be rescaled to one format before any comparison.

## Rescaling

<!-- snippet: skip -->
```c
enum fxp_status fxp_rescale(fxp_t a, uint_least8_t from_frac, uint_least8_t to_frac, fxp_t *out);
```

`fxp_rescale` converts a raw value with `from_frac` fractional bits to a raw value with `to_frac` fractional bits. It is for data in another Q format, such as a peripheral register, and does not change the build format. Increasing the fractional bits is exact unless the value no longer fits, in which case `*out` saturates and the status is `FXP_SATURATED`. Decreasing them rounds per `FXP_ROUNDING` and returns `FXP_INEXACT` when non-zero bits are discarded. A count of `FXP_WIDE_BITS` or more returns `FXP_BAD_PARAM` and writes 0.

<!-- snippet: body -->
```c
fxp_t q15;
fxp_t q24;
fxp_t q16;
fxp_t q8;
enum fxp_status st;

st = fxp_rescale(FXP_C(3, 4), 16, 15, &q15);   /* raw 24576, FXP_OK */
st = fxp_rescale(FXP_C(314, 100), 16, 24, &q24); /* raw 52680448, FXP_OK */
st = fxp_rescale(q24, 24, 16, &q16);           /* back to raw 205783, FXP_OK */
st = fxp_rescale(FXP_C(1, 3), 16, 8, &q8);     /* raw 85, FXP_INEXACT */
st = fxp_rescale(FXP_INT(200), 16, 24, &q24);  /* FXP_MAX, FXP_SATURATED */
(void)st;
```

## Floating-point conversion

<!-- snippet: skip -->
```c
#include "fxp_convert.h"  /* built only with -Dwith_convert=true */

fxp_t fxp_from_double(double x, enum fxp_status *st);
double fxp_to_double(fxp_t a);
```

These are the only functions that use `double`, and they live in `src/fxp_convert.c` so a float-free build can leave them out. `fxp_from_double` rounds to nearest with ties away from zero and saturates, reporting `FXP_OVERFLOW` when `x` is out of range; `st` may be `NULL`. `fxp_to_double` is exact for every `fxp_t`. They suit host tools, tests and bring-up rather than target hot paths.

<!-- snippet: body -->
```c
enum fxp_status st;
fxp_t pi = fxp_from_double(3.14159, &st);      /* raw 205887, FXP_OK */
fxp_t big = fxp_from_double(1e9, &st);         /* FXP_MAX, FXP_OVERFLOW */
double back = fxp_to_double(FXP_C(-7, 2));     /* -3.5 */
(void)pi; (void)big; (void)back;
```

## Version

<!-- snippet: skip -->
```c
#include "fxp_version.h"  /* Meson builds only */
```

The generated header defines `FXP_VERSION` and `FXP_VERSION_STRING` as the version string, and `FXP_VERSION_MAJOR`, `FXP_VERSION_MINOR` and `FXP_VERSION_PATCH` as integers.

## Recipes

### FIR filter

Each output is the dot product of the coefficients with the most recent inputs, accumulated wide and narrowed once. Coefficient `k` multiplies the input from `k` samples ago. With coefficients whose magnitudes sum to at most 1, the sum cannot overflow, so the fast `fxp_acc_mac` is safe.

```c
#define FIR_TAPS 4u

struct fir {
        fxp_t coeff[FIR_TAPS];
        fxp_t history[FIR_TAPS];
        unsigned int newest;
};

static fxp_t
fir_step(struct fir *f, fxp_t x)
{
        fxp_acc_t acc = fxp_acc_zero();
        unsigned int idx;

        f->newest = (f->newest + 1u) % FIR_TAPS;
        f->history[f->newest] = x;

        idx = f->newest;
        for (unsigned int k = 0u; k < FIR_TAPS; k++) {
                acc = fxp_acc_mac(acc, f->coeff[k], f->history[idx]);
                idx = (idx == 0u) ? (FIR_TAPS - 1u) : (idx - 1u);
        }
        return fxp_acc_narrow_sat(acc);
}
```

With the averaging coefficients `{0.25, 0.25, 0.25, 0.25}`, a unit step input gives 0.25, 0.5, 0.75 and then 1.0.

### Biquad filter

A direct-form I biquad keeps separate input and output histories and computes `y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2` in one accumulator. The feedback terms are added with negated coefficients, because the accumulator only adds.

```c
struct biquad {
        fxp_t b0, b1, b2;
        fxp_t a1, a2;
        fxp_t x1, x2;
        fxp_t y1, y2;
};

static fxp_t
biquad_step(struct biquad *q, fxp_t x)
{
        fxp_acc_t acc = fxp_acc_zero();
        fxp_t y;

        acc = fxp_acc_mac(acc, q->b0, x);
        acc = fxp_acc_mac(acc, q->b1, q->x1);
        acc = fxp_acc_mac(acc, q->b2, q->x2);
        acc = fxp_acc_mac(acc, fxp_neg_sat(q->a1), q->y1);
        acc = fxp_acc_mac(acc, fxp_neg_sat(q->a2), q->y2);
        y = fxp_acc_narrow_sat(acc);

        q->x2 = q->x1;
        q->x1 = x;
        q->y2 = q->y1;
        q->y1 = y;
        return y;
}
```

Coefficients usually come from a filter design tool and are stored with `FXP_C` or `FXP_RAW`. A filter with large feedback gain can grow its sum beyond the format; use `fxp_acc_mac_chk` there.

### Integrator with anti-windup

An integrator held in a wide variable can exceed the output limits without wrapping. `fxp_clamp_wide` limits it and reports the clamp, and writing the clamped value back stops the integrator winding up while the output is saturated.

```c
struct integrator {
        fxp_wide_t sum;
        fxp_t ki;
        fxp_t lo;
        fxp_t hi;
};

static fxp_t
integrator_step(struct integrator *it, fxp_t error, bool *limited)
{
        fxp_t out;

        it->sum += fxp_mul_sat(it->ki, error);
        out = fxp_clamp_wide(it->sum, it->lo, it->hi, limited);
        it->sum = out;
        return out;
}
```

With `ki` of 0.5, limits of ±100 and a constant error of 150, the output is 75 and then 100 with `*limited` set, and it leaves the limit as soon as the error changes sign.
