# fxp Guide

> A deep dive into fixed-point arithmetic and the fxp library API.
>
> **Target Audience:** Embedded developers moving from floating-point to
> fixed-point for DSP, control loops, and hardware drivers.
>
> **Prerequisites:** Familiarity with C, basic bitwise operations, and
> the concept of signed integers.

---

## Table of Contents

1. [Introduction to Fixed-Point Arithmetic](#1-introduction-to-fixed-point-arithmetic)
2. [The fxp Library: Design Philosophy](#2-the-fxp-library-design-philosophy)
3. [Construction and Readout](#3-construction-and-readout)
4. [Compile-Time Constants](#4-compile-time-constants)
5. [Arithmetic Hot Paths](#5-arithmetic-hot-paths)
6. [Wide Accumulators: The DSP "Holy Grail"](#6-wide-accumulators-the-dsp-holy-grail)
7. [Format Conversion and Helpers](#7-format-conversion-and-helpers)
8. [The Checked (`_chk`) Interface](#8-the-checked-chk-interface)
9. [Cheat Sheet](#9-cheat-sheet)
10. [Appendices](#appendix-a-understanding-the-types)

---

## 1. Introduction to Fixed-Point Arithmetic

### 1.1 Why Fixed-Point?

Floating-point math (IEEE-754) is a wonderful tool for general-purpose
computing. But in embedded systems, it has several problems:

- **No hardware support.** Many microcontrollers (Cortex-M0, PIC, AVR) have
  no floating-point unit (FPU). Floating-point math is performed in software,
  which is orders of magnitude slower than integer math.
- **Non-determinism.** Floating-point results can differ between compilers,
  optimization levels, and even different CPU architectures. If you're building
  a motor controller or an audio filter, you need the _exact same result_
  every single time.
- **Memory footprint.** Linking a soft-float library can add tens of
  kilobytes to your binary.

Fixed-point arithmetic solves all of these. It is simply **integer math with
an implied decimal point**.

### 1.2 How It Works: The Implied Binary Point

In fixed-point, you choose a number of "fractional bits." The value of the
number is always:

```
real_value = raw_integer / 2^fractional_bits
```

For example, if you choose **16 fractional bits** (called **Q16.16** format):

| Raw Integer | Real Value          | Explanation                 |
| ----------: | ------------------- | --------------------------- |
|     `65536` | `1.0`               | `65536 / 65536`             |
|     `32768` | `0.5`               | `32768 / 65536`             |
|    `196608` | `3.0`               | `196608 / 65536`            |
|    `131072` | `2.0`               | `131072 / 65536`            |
|         `1` | `0.000015258789...` | `1 / 65536` (smallest step) |

The "binary point" is not actually stored anywhere. It's an agreement between
you and the library about how to interpret the bits.

### 1.3 Q-Format Notation

The standard notation is **Qm.n**, where:

- **m** = number of integer bits (including the sign bit)
- **n** = number of fractional bits
- **m + n** = total bits

For a 32-bit signed integer with 16 fractional bits:

```
Q16.16  →  1 sign bit + 15 integer bits + 16 fractional bits
```

This gives:

- **Range:** Approximately ±32,768 (since you have 15 bits for the integer part)
- **Resolution:** 2⁻¹⁶ ≈ 0.000015 (about 5 decimal places of precision)

**The trade-off:** If you increase fractional bits for more precision, you
reduce the integer range. If you increase integer bits for more range, you
lose precision. The `fxp` library lets you configure this at build time.

### 1.4 Why Not Just Use Floats?

Consider a simple DSP operation: a dot product of two coefficient arrays.
In floating-point:

```c
double dot_product(const double *a, const double *b, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}
```

In fixed-point, you have to worry about:

1. **Overflow** — multiplying two 32-bit numbers produces a 64-bit result.
2. **Rounding** — when you narrow back down, how do you handle the lost bits?
3. **Saturation** — what happens when the result exceeds the representable range?

The purpose of the `fxp` library is to handle all of this for you, with a
consistent policy applied across every operation.

---

## 2. The fxp Library: Design Philosophy

The `fxp` library is built around four core principles:

### 2.1 No Undefined Behavior

In C, signed integer overflow is undefined behavior. Left-shifting a negative
number is undefined behavior. The `fxp` library avoids these pitfalls by:

- Using **wide intermediates** (`fxp_wide_t`) for all multiplications.
- Using **masks** instead of left-shifts to extract fractional bits.
- **Saturating** (clamping) results instead of allowing them to wrap.

### 2.2 One Rounding Policy, Build-Wide

Every time a value is "narrowed" (e.g., a 64-bit product reduced back to a
32-bit fixed-point value), the same rounding logic is applied. The default is
**round-to-nearest, ties-to-even** (banker's rounding), which has zero
statistical bias — the same policy used by IEEE-754 floating-point.

This matters for DSP: if different operations use different rounding, your
filter coefficients will drift, and your audio will sound distorted.

### 2.3 Pure Functions, No State

Every `fxp` function is pure: same input → same output. No global state, no
allocations, no hidden side effects. This makes the library:

- **Thread-safe** — multiple threads can call `fxp` functions simultaneously.
- **Reentrant** — safe to call from interrupt handlers.
- **Deterministic** — bit-identical results across compilers and CPUs.

### 2.4 What Is "Narrowing"?

**Narrowing** is the process of reducing a wide intermediate value (e.g., a
64-bit product) back to the standard `fxp_t` type (e.g., 32-bit). It is the
single most important operation in the library because **every multiply,
divide, accumulate, and rescale ends with a narrow step**.

A bare C cast `(fxp_t)wide_value` would silently truncate or wrap on overflow.
Narrowing does three things:

1. **Rounds** — applies the build's rounding policy to the discarded low bits.
2. **Saturates** — clamps to `[FXP_MIN, FXP_MAX]` if the value is out of range.
3. **Shifts** — for product sums, shifts right by `FXP_FRAC_BITS` to restore
   the correct fractional scale.

The library provides several narrowing functions:

| Function                        | Input                 | Shifts?        | Saturates?   | Use Case                      |
| ------------------------------- | --------------------- | -------------- | ------------ | ----------------------------- |
| `fxp_acc_narrow_sat(acc)`       | Wide product sum      | Yes (`>>FRAC`) | Yes          | After `fxp_acc_mac` loop      |
| `fxp_acc_narrow_chk(acc, &out)` | Wide product sum      | Yes (`>>FRAC`) | Yes + status | Same, with overflow detection |
| `fxp_narrow_sat(wide)`          | Wide fxp-scaled value | No             | Yes          | Manual wide computation       |
| `fxp_narrow_chk(wide, &out)`    | Wide fxp-scaled value | No             | Yes + status | Same, with overflow detection |

**The key distinction:** `fxp_acc_narrow_*` shifts right by `FXP_FRAC_BITS`
because the accumulator holds _product sums_ with double the fractional bits.
`fxp_narrow_*` does _not_ shift because the input is already in fxp scale.

### 2.5 The `_sat` vs `_chk` Convention

Every arithmetic operation comes in two flavors:

| Flavor | Behavior                                      | Use Case                             |
| ------ | --------------------------------------------- | ------------------------------------ |
| `_sat` | Clamps on overflow, returns the value         | Hot paths, performance-critical code |
| `_chk` | Returns a status code, writes value to `*out` | Debugging, boundary validation       |

This lets you write fast code in production and switch to checked code during
development to catch saturation issues.

---

## 3. Construction and Readout

Construction functions bring values _into_ the fixed-point domain. Readout
functions bring values _out_.

### 3.1 `fxp_from_int` — Integer to Fixed-Point

```c
fxp_t fxp_from_int(int32_t whole);
```

Converts a whole integer into a fixed-point value by multiplying by the "scale
factor" (2^fractional_bits).

**Example:** Converting a sensor reading (e.g., a temperature of 25 degrees):

```c
fxp_t temp = fxp_from_int(25);
// In Q16.16: temp = 25 * 65536 = 1638400
// This represents the real value 25.0
```

**Why it matters:** The function saturates if the integer is too large to fit
in the fixed-point format. For Q16.16, the maximum whole integer is 32,767.
Passing 100,000 will return `FXP_MAX` (≈32,767.99998), not a wrapped value.

### 3.2 `fxp_from_ratio` — Numerator/Denominator to Fixed-Point

```c
fxp_t fxp_from_ratio(int32_t num, int32_t den, enum fxp_status *st);
```

Constructs a fixed-point value from a fraction. This is useful when you need
to represent a value that isn't a clean integer.

**Example:** Setting a PWM duty cycle of 3/4 (75%):

```c
fxp_t duty_cycle;
enum fxp_status st;

duty_cycle = fxp_from_ratio(3, 4, &st);
// duty_cycle represents 0.75 in Q16.16
// st == FXP_OK
```

**Example:** Handling division by zero:

```c
duty_cycle = fxp_from_ratio(1, 0, &st);
// duty_cycle == 0
// st == FXP_DIV_ZERO
```

**Why it matters:** In driver code, you might read a register value and a
divisor from hardware. This function ensures you never get undefined behavior
if the divisor happens to be zero.

### 3.3 `fxp_to_int` — Fixed-Point to Integer

```c
int32_t fxp_to_int(fxp_t a);
```

Converts a fixed-point value to the nearest integer, using the build's
rounding policy.

**Example:** Converting a fixed-point temperature back to an integer for display:

```c
fxp_t temp = fxp_from_int(25);
temp = fxp_add_sat(temp, FXP_C(1, 2));  // 25.5

int32_t display_temp = fxp_to_int(temp);
// display_temp == 26 (rounded to nearest, ties to even)
```

**Why it matters:** The rounding is consistent with every other narrowing
operation in the library. If you built with `FXP_ROUND_NEAREST_EVEN`, this
will round 25.5 to 26 (even), but 24.5 to 24 (even).

### 3.4 Rounding Helpers: `fxp_floor`, `fxp_ceil`, `fxp_round`

These functions round a fixed-point value to the nearest integer _and return
it as a fixed-point value_ (not an `int32_t`).

```c
fxp_t fxp_floor(fxp_t a);   // Largest integer <= a (toward -inf)
fxp_t fxp_ceil(fxp_t a);    // Smallest integer >= a (toward +inf)
fxp_t fxp_round(fxp_t a);   // Nearest integer, ties to even
```

**Example:** DSP sample rate conversion:

```c
// You have a fixed-point sample rate of 44100.5 Hz
fxp_t sample_rate = fxp_from_ratio(88201, 2);

// Floor: 44100 Hz
fxp_t rate_floor = fxp_floor(sample_rate);

// Ceil: 44101 Hz
fxp_t rate_ceil = fxp_ceil(sample_rate);

// Round: 44100 Hz (ties to even)
fxp_t rate_round = fxp_round(sample_rate);
```

**Why it matters:** In control loops, you might need to quantize a
continuous value to discrete steps without losing the fixed-point format.

---

## 4. Compile-Time Constants

The `fxp` library provides macros for creating fixed-point constants at
compile time. These are **integer constant expressions**; they don't require
any runtime computation or floating-point support.

### 4.1 `FXP_INT(whole)` — Exact Integer

```c
#define FXP_INT(whole)
```

Creates a fixed-point value for an exact integer.

```c
static const fxp_t max_speed = FXP_INT(100);    // 100.0
static const fxp_t min_voltage = FXP_INT(-5);   // -5.0
static const fxp_t zero = FXP_INT(0);           // 0.0
```

### 4.2 `FXP_C(num, den)` — Fractional Constant

```c
#define FXP_C(num, den)
```

Creates a fixed-point value for `num / den`. Rounds half away from zero at
compile time.

```c
// Common DSP coefficients
static const fxp_t pi = FXP_C(314159, 100000);    // ≈ 3.14159
static const fxp_t half = FXP_C(1, 2);            // 0.5
static const fxp_t quarter = FXP_C(1, 4);         // 0.25
static const fxp_t neg_three_half = FXP_C(-7, 2); // -3.5

// PID controller gains
static const fxp_t kp = FXP_C(3, 2);              // 1.5
static const fxp_t ki = FXP_C(1, 10);             // 0.1
static const fxp_t kd = FXP_C(7, 100);            // 0.07
```

### 4.3 `FXP_RAW(bits)` — Pre-Scaled Raw Value

```c
#define FXP_RAW(bits)
```

An escape hatch for values that are already in the correct fixed-point format.
Use this when importing pre-computed coefficients from a toolchain.

```c
// Suppose you exported a coefficient table from MATLAB
static const fxp_t coeff_table[] = {
    FXP_RAW(131072),   // 2.0
    FXP_RAW(98304),    // 1.5
    FXP_RAW(65536),    // 1.0
    FXP_RAW(32768),    // 0.5
};
```

> Using `FXP_C` and `FXP_INT` means these tables are pre-computed and don't
> require any runtime conversion.

---

## 5. Arithmetic Hot Paths

The core arithmetic operations: add, subtract, multiply, divide, and
multiply-divide. Each is available in `_sat` (saturating) and `_chk` (checked)
forms.

### 5.1 Addition and Subtraction

```c
fxp_t fxp_add_sat(fxp_t a, fxp_t b);
fxp_t fxp_sub_sat(fxp_t a, fxp_t b);
```

**Example:** Accumulating sensor readings:

```c
// Average of 4 temperature readings
fxp_t readings[] = { FXP_INT(20), FXP_INT(22), FXP_INT(21), FXP_INT(23) };

fxp_t sum = FXP_INT(0);
for (int i = 0; i < 4; i++) {
    sum = fxp_add_sat(sum, readings[i]);  // sum = 86.0
}

fxp_t average;
fxp_div_chk(sum, FXP_INT(4), &average);   // average = 21.5
```

**Why saturation matters:** If you add two large values that exceed `FXP_MAX`,
the result wraps in normal C integer arithmetic. With `fxp_add_sat`, the
result is clamped to `FXP_MAX` instead.

### 5.2 Multiplication

```c
fxp_t fxp_mul_sat(fxp_t a, fxp_t b);
```

**Example:** Applying a gain to an audio sample:

```c
// Apply 1.5x gain to a sample
fxp_t sample = FXP_C(1, 2);   // 0.5 (half-scale)
fxp_t gain = FXP_C(3, 2);     // 1.5

fxp_t amplified = fxp_mul_sat(sample, gain);
// amplified = 0.75
```

**What happens internally:**

1. The two 32-bit values are multiplied, producing a 64-bit result.
2. The 64-bit result has _double_ the fractional bits (Q32.32 in Q16.16 mode).
3. The result is rounded and shifted right by 16 bits to restore Q16.16.
4. If the result exceeds `FXP_MAX` or `FXP_MIN`, it is saturated.

**Why the wide intermediate matters:** If you multiplied two Q16.16 values
directly in 32-bit arithmetic, you would lose all the fractional bits. The
wide intermediate preserves precision.

### 5.3 Division

```c
enum fxp_status fxp_div_chk(fxp_t a, fxp_t b, fxp_t *out);
```

**Note:** Division is only available in the `_chk` form. Unlike add/sub/mul,
there is no `fxp_div_sat`. This is deliberate:

1. **Divide-by-zero must always be detectable.** A silent-saturate version
   would return `FXP_MAX` or `0` on division by zero, making it impossible to
   distinguish from a legitimate large result.
2. **The status check is cheap.** The `_chk` function returns the status in a
   register; ignoring it costs nothing.
3. **Division is rarely on a hot path.** In DSP, division is typically used for
   calibration or setup, not per-sample processing.

If you don't care about the status, simply ignore the return value:

```c
fxp_t result;
fxp_div_chk(numerator, denominator, &result);  // status ignored
```

**Example:** Computing a transfer function:

```c
fxp_t numerator = FXP_INT(100);
fxp_t denominator = FXP_INT(3);
fxp_t result;
enum fxp_status st;

st = fxp_div_chk(numerator, denominator, &result);
// result ≈ 33.33333 (33 + 1/3 in Q16.16)
// st == FXP_OK
```

**What happens internally:**

1. The numerator is scaled by `FXP_ONE` (shifted left by fractional bits).
2. The scaled numerator is divided by the denominator.
3. The result is rounded and saturated.

**Why scaling is needed:** In fixed-point, `a / b` must account for the
implied decimal point. If both `a` and `b` are in Q16.16, the raw division
would cancel out the fractional bits. The library multiplies the numerator by
`FXP_ONE` (which is `1 << FXP_FRAC_BITS`, i.e., the fixed-point representation
of `1.0`) to restore them.

**What is `FXP_ONE`?** It's the fixed-point representation of `1.0`. In Q16.16,
`FXP_ONE == 65536`. It's the "scale factor" that bridges the integer and
fractional domains. You'll see it used whenever you need to multiply or divide
by `1.0` in fixed-point:

### 5.4 Fused Multiply-Divide

```c
enum fxp_status fxp_muldiv_chk(fxp_t a, fxp_t b, fxp_t divisor, fxp_t *out);
```

Computes `(a * b) / divisor` in a single wide operation.

**Example:** Computing a weighted average:

```c
// (value1 * weight1 + value2 * weight2) / (weight1 + weight2)
fxp_t value1 = FXP_INT(100);
fxp_t value2 = FXP_INT(200);
fxp_t weight1 = FXP_C(3, 10);  // 0.3
fxp_t weight2 = FXP_C(7, 10);  // 0.7

fxp_t weighted1, weighted2, total_weight, result;

fxp_muldiv_chk(value1, weight1, FXP_ONE, &weighted1);  // 100 * 0.3
fxp_muldiv_chk(value2, weight2, FXP_ONE, &weighted2);  // 200 * 0.7

total_weight = fxp_add_sat(weight1, weight2);  // 1.0
fxp_div_chk(fxp_add_sat(weighted1, weighted2), total_weight, &result);
// result = 170.0
```

**Why fused matters:** Doing `a * b` and then `/ divisor` as separate
operations means you round twice, losing precision. The fused version
evaluates in the wide intermediate and narrows only once.

**Side-by-side comparison:**

```c
fxp_t a = FXP_C(1, 3);      // 0.33333...
fxp_t b = FXP_C(2, 3);      // 0.66666...
fxp_t divisor = FXP_C(1, 2); // 0.5

// TWO-STEP: Rounds twice, loses precision
fxp_t product;
fxp_mul_chk(a, b, &product);       // product ≈ 0.22222 (rounded to Q16.16)
fxp_t result1;
fxp_div_chk(product, divisor, &result1);  // result1 ≈ 0.44444 (rounded again)

// FUSED: Single narrow, preserves precision
fxp_t result2;
fxp_muldiv_chk(a, b, divisor, &result2);  // result2 ≈ 0.44444 (more precise)
```

In the two-step path, the intermediate `product` is narrowed to 32 bits after
the multiply, discarding low bits. The division then scales those truncated
bits back up, amplifying the error. The fused path keeps everything in 64 bits
until the final narrow.

**When to use it:** Any time you compute `(a * b) / c` where `c` is not a power
of two. If `c` is a power of two, you can use `fxp_mul_sat` followed by a
simple right shift (which the compiler may optimize).

---

## 6. Wide Accumulators: The DSP "Holy Grail"

The accumulator API is the most important tool for DSP applications. It allows
you to sum many products without rounding or saturating until the very end.

### 6.1 The Problem: Rounding Drift

Consider a simple FIR (Finite Impulse Response) filter:

```c
// BAD: Rounding after every multiply-add
fxp_t fir_bad(const fxp_t *coeff, const fxp_t *input, size_t n) {
    fxp_t sum = FXP_INT(0);
    for (size_t i = 0; i < n; i++) {
        // Each fxp_mul_sat rounds and saturates!
        // This accumulates rounding errors with every iteration.
        sum = fxp_add_sat(sum, fxp_mul_sat(coeff[i], input[i]));
    }
    return sum;
}
```

Every call to `fxp_mul_sat` rounds the 64-bit product back to 32 bits. If you
have 64 taps, you round 64 times, and the errors compound.

### 6.2 The Solution: Wide Accumulator

```c
// GOOD: Sum in wide, narrow once
fxp_t fir_good(const fxp_t *coeff, const fxp_t *input, size_t n) {
    fxp_acc_t acc = fxp_acc_zero();
    for (size_t i = 0; i < n; i++) {
        acc = fxp_acc_mac(acc, coeff[i], input[i]);
    }
    return fxp_acc_narrow_sat(acc);
}
```

**What happens internally:**

1. `fxp_acc_zero()` initializes a 64-bit accumulator to zero.
2. `fxp_acc_mac()` multiplies two 32-bit values (producing 64 bits) and adds
   the result to the 64-bit accumulator. **No rounding, no narrowing.**
3. `fxp_acc_narrow_sat()` performs a single rounding and saturation at the end.

### 6.3 The Accumulator API

```c
// Initialize
fxp_acc_t fxp_acc_zero(void);

// Fast multiply-accumulate (no overflow check)
fxp_acc_t fxp_acc_mac(fxp_acc_t acc, fxp_t a, fxp_t b);

// Checked multiply-accumulate (with overflow detection)
fxp_acc_t fxp_acc_mac_chk(fxp_acc_t acc, fxp_t a, fxp_t b, bool *overflow);

// Narrow to fxp_t
fxp_t fxp_acc_narrow_sat(fxp_acc_t acc);
enum fxp_status fxp_acc_narrow_chk(fxp_acc_t acc, fxp_t *out);
```

### 6.4 Overflow Detection

The fast `fxp_acc_mac` does not check for overflow. Use `fxp_acc_mac_chk` when
operand magnitudes are unknown:

```c
fxp_acc_t acc = fxp_acc_zero();
bool overflowed = false;

for (size_t i = 0; i < n; i++) {
    acc = fxp_acc_mac_chk(acc, coeff[i], input[i], &overflowed);
    if (overflowed) {
        // Clamp the accumulator and continue
        // (the function already clamps to FXP_WIDE_MAX/MIN)
        break;
    }
}
```

**How much headroom do you have?** In Q16.16:

- **Normalized inputs** (`|x| < 1`): Products are < 2³², leaving room for
  ~2³¹ terms before overflow.
- **Full-scale inputs** (`|x| ≈ 32768`): Products are ~2⁶², leaving room for
  only ~2 terms before overflow.

### 6.5 Real-World DSP Example: Biquad Filter (Direct-Form I)

A biquad filter (common in audio processing) uses both feedforward and
feedback coefficients. The difference equation is:

```
y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
```

Notice that the feedforward terms use _previous inputs_ (`x[n-1]`, `x[n-2]`)
while the feedback terms use _previous outputs_ (`y[n-1]`, `y[n-2]`). You need
**four** delay states, not two.

```c
typedef struct {
    fxp_t b0, b1, b2;  // Feedforward coefficients
    fxp_t a1, a2;      // Feedback coefficients
    fxp_t x1, x2;      // Previous inputs:  x[n-1], x[n-2]
    fxp_t y1, y2;      // Previous outputs: y[n-1], y[n-2]
} biquad_t;

fxp_t biquad_process(biquad_t *filter, fxp_t input) {
    // Accumulate feedforward terms: b0*x[n] + b1*x[n-1] + b2*x[n-2]
    fxp_acc_t acc = fxp_acc_zero();
    acc = fxp_acc_mac(acc, filter->b0, input);
    acc = fxp_acc_mac(acc, filter->b1, filter->x1);
    acc = fxp_acc_mac(acc, filter->b2, filter->x2);

    // Accumulate feedback terms: -a1*y[n-1] - a2*y[n-2]
    // Negate the coefficients so we can use fxp_acc_mac (which adds)
    fxp_t neg_a1 = fxp_neg_sat(filter->a1);
    fxp_t neg_a2 = fxp_neg_sat(filter->a2);
    acc = fxp_acc_mac(acc, neg_a1, filter->y1);
    acc = fxp_acc_mac(acc, neg_a2, filter->y2);

    // Narrow and saturate
    fxp_t output = fxp_acc_narrow_sat(acc);

    // Update delay lines: shift old values, store new ones
    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;

    return output;
}
```

**Why four states?** A common mistake is to reuse the same delay line for both
feedforward and feedback. This produces a different filter topology (direct-form
II) with different numerical properties. Direct-form I (shown above) is simpler
to reason about: the input and output histories are independent.

**Coefficient design:** The `b0`, `b1`, `b2`, `a1`, `a2` coefficients are
typically computed by a tool (MATLAB, Python's `scipy.signal`, or a web-based
biquad calculator) and stored as `FXP_C(...)` constants in flash.

---

## 7. Format Conversion and Helpers

### 7.1 `fxp_rescale` — Converting Between Q-Formats

```c
enum fxp_status fxp_rescale(fxp_t a, uint8_t from_frac, uint8_t to_frac,
                            fxp_t *out);
```

Converts a value from one fractional bit count to another. This is useful when
you need to interface with hardware or protocols that use a different Q-format.

**Important:** The library's build format is _fixed at compile time_. `fxp_rescale`
does _not_ change the library's internal format. Instead, it treats the raw bits
of the input `fxp_t` as if they had `from_frac` fractional bits, and produces a
new `fxp_t` in the _build_ format with `to_frac` fractional bits. Both input and
output are `fxp_t` — the function is a boundary converter for external data,
not an internal format switcher.

**Example:** Reading a hardware register in Q1.15:

```c
// A DAC expects Q1.15 values (1 integer bit + sign, 15 fractional bits)
// Your internal math uses Q16.16. Convert at the boundary.
fxp_t internal_value = FXP_C(3, 4);  // 0.75 in Q16.16

fxp_t dac_value;
enum fxp_status st;
st = fxp_rescale(internal_value, 16, 15, &dac_value);
// st == FXP_OK
// dac_value now has the correct raw bits for a Q1.15 DAC register
```

**Example:** Converting from Q16.16 to Q8.24:

**Example:** Converting from Q16.16 to Q8.24:

```c
// You have a Q16.16 value (16 fractional bits)
fxp_t value_q16 = FXP_C(314, 100);  // 3.14 in Q16.16

// Convert to Q8.24 (24 fractional bits)
fxp_t value_q24;
enum fxp_status st;

st = fxp_rescale(value_q16, 16, 24, &value_q24);
// st == FXP_OK (exact, because upscaling just adds zeros)
// value_q24 now represents 3.14 in Q8.24
```

**Example:** Converting from Q8.24 back to Q16.16:

```c
// Downscaling loses precision
st = fxp_rescale(value_q24, 24, 16, &value_q16);
// st == FXP_INEXACT (some low bits were discarded)
// value_q16 is rounded to Q16.16 precision
```

**Why it matters:** Many hardware peripherals use different Q-formats. A DAC
might expect Q1.15, while your internal math uses Q16.16. `fxp_rescale`
handles the conversion safely, reporting whether precision was lost.

### 7.2 `fxp_lerp` — Linear Interpolation

```c
fxp_t fxp_lerp(fxp_t a, fxp_t b, fxp_t t);
```

Computes `a + (b - a) * t`. This is fundamental for animation, PWM control,
and signal interpolation.

**Example:** Smooth PWM transition for a motor driver:

```c
// Current duty cycle: 0.2 (20%)
// Target duty cycle: 0.8 (80%)
// Blend factor: 0.5 (50% of the way there)

fxp_t current = FXP_C(1, 5);   // 0.2
fxp_t target = FXP_C(4, 5);    // 0.8
fxp_t blend = FXP_C(1, 2);     // 0.5

fxp_t new_duty = fxp_lerp(current, target, blend);
// new_duty = 0.5 (50% duty cycle)
```

**Example:** Interpolating between two filter coefficients:

```c
// Crossfade between a low-pass and high-pass filter
fxp_t lpf_coeff = FXP_C(1, 4);   // 0.25
fxp_t hpf_coeff = FXP_C(3, 4);   // 0.75
fxp_t crossfade = FXP_C(1, 3);   // 0.333...

fxp_t blended = fxp_lerp(lpf_coeff, hpf_coeff, crossfade);
// blended ≈ 0.4167
```

**Why wide intermediate matters:** The `(b - a)` term is evaluated in the wide
type to avoid overflow when `a` and `b` have opposite signs and large
magnitudes.

### 7.3 `fxp_clamp` and `fxp_clamp_wide`

```c
fxp_t fxp_clamp(fxp_t a, fxp_t lo, fxp_t hi);
fxp_t fxp_clamp_wide(fxp_wide_t a, fxp_t lo, fxp_t hi, bool *clamped);
```

Clamps a value to a range. `fxp_clamp_wide` operates on a _wide_ value,
clamps it, and reports whether clamping occurred. `fxp_clamp` operates on an
already-narrowed `fxp_t` and has no status output.

**Example:** Basic clamping with `fxp_clamp`:

```c
// Clamp a sensor reading to valid range
fxp_t reading = read_sensor_fixed();
reading = fxp_clamp(reading, FXP_INT(0), FXP_INT(100));
// reading is now guaranteed to be in [0, 100]
```

**Example:** Anti-windup with `fxp_clamp_wide`:

```c
// Clamp the WIDE accumulator BEFORE narrowing, so we can detect
// whether the value hit the actuator limits.
bool hit_limit = false;

fxp_t pid_output = fxp_clamp_wide(integral_acc,
                                  FXP_INT(-100), FXP_INT(100),
                                  &hit_limit);

if (hit_limit) {
    // Disable integral accumulation to prevent windup.
    // The wide value was clamped, so we know the actuator is saturated.
    integral_enabled = false;
}
```

**Why `fxp_clamp_wide` matters:** In control loops, "integral windup" occurs
when the integral term keeps growing even though the output is already
saturated. `fxp_clamp_wide` lets you clamp the wide accumulator _before_
narrowing and detect whether clamping occurred in a single step. This is more
efficient than narrowing first, then clamping, then checking — you avoid an
extra saturation pass and get the overflow flag for free.

**Key distinction:**

- `fxp_clamp(fxp_t, ...)` — operates on a narrowed value; no status output.
- `fxp_clamp_wide(fxp_wide_t, ..., bool *)` — operates on a wide value;
  returns a narrowed `fxp_t` and sets `*clamped` if the value was out of range.

### 7.4 `fxp_cmp` — Three-Way Comparison

```c
int fxp_cmp(fxp_t a, fxp_t b);
```

Returns `<0` if `a < b`, `0` if equal, `>0` if `a > b`.

**Example:** Comparing sensor readings:

```c
fxp_t threshold = FXP_C(3, 4);  // 0.75 (75%)
fxp_t reading = read_sensor_fixed();

if (fxp_cmp(reading, threshold) > 0) {
    // Reading exceeds threshold — trigger action
    activate_alarm();
}
```

**Why it matters:** You can't use `<` and `>` directly on fixed-point values
if you want to compare across different formats. `fxp_cmp` is defined only for
values in the _same_ format.

### 7.5 `fxp_narrow_sat` and `fxp_narrow_chk`

```c
fxp_t fxp_narrow_sat(fxp_wide_t wide);
enum fxp_status fxp_narrow_chk(fxp_wide_t wide, fxp_t *out);
```

These functions narrow a `fxp_wide_t` value to `fxp_t` with saturation. Unlike
`fxp_acc_narrow_sat`, these do **not** shift right by `FXP_FRAC_BITS` — they
assume the input is _already_ in fxp scale.

**When would you use these?** When you perform a custom wide computation that
stays in fxp scale (no multiplication, so no extra fractional bits), but you
need to detect overflow before narrowing.

**Example:** Scaling a value by a power of two in the wide domain:

```c
// Double a value, but detect if it overflows
fxp_t value = FXP_INT(20000);
fxp_wide_t doubled = (fxp_wide_t)value * 2;  // Still in fxp scale

fxp_t result;
enum fxp_status st = fxp_narrow_chk(doubled, &result);
// st == FXP_OK (40000 fits in Q16.16)

// Now try a value that overflows:
value = FXP_INT(30000);
doubled = (fxp_wide_t)value * 2;  // 60000, exceeds FXP_MAX
st = fxp_narrow_chk(doubled, &result);
// st == FXP_SATURATED, result == FXP_MAX
```

**Key distinction from `fxp_acc_narrow_*`:**

| Function                  | Input scale                 | Shifts?      | Use Case                 |
| ------------------------- | --------------------------- | ------------ | ------------------------ |
| `fxp_acc_narrow_sat(acc)` | Product sum (Q32.32)        | Yes (`>>16`) | After `fxp_acc_mac` loop |
| `fxp_narrow_sat(wide)`    | Already fxp-scaled (Q16.16) | No           | Custom wide computation  |

If you accidentally use `fxp_narrow_sat` on a product sum, the result will be
off by a factor of 2^16. Always use `fxp_acc_narrow_sat` for accumulator
results.

### 7.6 `fxp_neg_sat` and `fxp_abs_sat`

```c
fxp_t fxp_neg_sat(fxp_t a);
fxp_t fxp_abs_sat(fxp_t a);
```

**Example:** Computing error magnitude:

```c
fxp_t setpoint = FXP_INT(100);
fxp_t measured = FXP_INT(95);

fxp_t error = fxp_sub_sat(setpoint, measured);  // 5.0
fxp_t abs_error = fxp_abs_sat(error);            // 5.0

// Or if measured > setpoint:
measured = FXP_INT(105);
error = fxp_sub_sat(setpoint, measured);         // -5.0
abs_error = fxp_abs_sat(error);                  // 5.0
```

**Why saturation matters:** The most negative value (`FXP_MIN`) cannot be
negated because of **two's complement asymmetry**. In a 32-bit signed integer:

```
Range: -2,147,483,648 to +2,147,483,647
       (FXP_MIN)              (FXP_MAX)
```

The negative side has one more value than the positive side. Negating
`FXP_MIN` would produce `+2,147,483,648`, which doesn't fit in a signed
32-bit integer. In raw C, this is undefined behavior. `fxp_neg_sat` handles
this by saturating `FXP_MIN` to `FXP_MAX` instead.

---

## 8. The Checked (`_chk`) Interface

The `_chk` functions are your debugging and validation tools. They return a
status code that tells you exactly what happened during the operation.

### 8.1 Status Codes

```c
enum fxp_status {
    FXP_OK,        // Exact, in-range result
    FXP_SATURATED, // Result was clamped to [MIN, MAX]
    FXP_DIV_ZERO,  // Divisor was zero; result is 0
    FXP_INEXACT,   // Nonzero low bits were discarded
    FXP_BAD_PARAM, // Argument was out of valid domain
};
```

### 8.2 When to Use `_chk`

**Use `_chk` during development** to catch saturation issues:

```c
void debug_multiply(fxp_t a, fxp_t b) {
    fxp_t result;
    enum fxp_status st = fxp_mul_chk(a, b, &result);

    if (st == FXP_SATURATED) {
        // Log a warning — the product was too large
        log_warning("Multiplication saturated! a=%d, b=%d", a, b);
    }
}
```

**Use `_chk` in production** when you must react to errors:

```c
fxp_t compute_ratio(fxp_t numerator, fxp_t denominator) {
    fxp_t result;
    enum fxp_status st = fxp_div_chk(numerator, denominator, &result);

    if (st == FXP_DIV_ZERO) {
        // Handle division by zero (e.g., return a safe default)
        return FXP_INT(0);
    }

    if (st == FXP_SATURATED) {
        // Handle overflow (e.g., clamp to a known safe value)
        return FXP_MAX;
    }

    return result;
}
```

### 8.3 The `out` Parameter Convention

All `_chk` functions require a non-NULL `out` parameter. This is a deliberate
design choice:

- **No branch per call:** The function doesn't need to check if `out` is NULL.
- **Consistent interface:** You always write to `*out` and check the return
  value.

**Optional status/flag pointers** (like `bool *clamped` or `bool *overflow`)
are NULL-tolerant. Pass NULL if you don't care about the flag.

---

## 9. Cheat Sheet

### 9.1 Quick Reference

| Operation | Saturating          | Checked                         |
| --------- | ------------------- | ------------------------------- |
| Add       | `fxp_add_sat(a, b)` | `fxp_add_chk(a, b, &out)`       |
| Subtract  | `fxp_sub_sat(a, b)` | `fxp_sub_chk(a, b, &out)`       |
| Multiply  | `fxp_mul_sat(a, b)` | `fxp_mul_chk(a, b, &out)`       |
| Divide    | N/A                 | `fxp_div_chk(a, b, &out)`       |
| Mul-Div   | N/A                 | `fxp_muldiv_chk(a, b, d, &out)` |

### 9.2 Construction

| From     | To      | Function                        |
| -------- | ------- | ------------------------------- |
| Integer  | Fixed   | `fxp_from_int(whole)`           |
| Fraction | Fixed   | `fxp_from_ratio(num, den, &st)` |
| Fixed    | Integer | `fxp_to_int(a)`                 |

### 9.3 Compile-Time Constants

| Macro         | Example          | Value        |
| ------------- | ---------------- | ------------ |
| `FXP_INT(n)`  | `FXP_INT(42)`    | 42.0         |
| `FXP_C(n, d)` | `FXP_C(1, 2)`    | 0.5          |
| `FXP_RAW(v)`  | `FXP_RAW(65536)` | 1.0 (Q16.16) |

### 9.3.1 The `FXP_ONE` Constant

`FXP_ONE` is the fixed-point representation of `1.0`. In Q16.16, it equals
`65536` (`1 << 16`). It's the "scale factor" used throughout the library:

- **Division:** `fxp_div_chk` multiplies the numerator by `FXP_ONE` to restore
  fractional bits.
- **Identity multiplication:** `fxp_mul_sat(a, FXP_ONE)` returns `a` (useful
  in accumulator loops when you need to add a value without scaling).
- **Ratio construction:** `fxp_from_ratio(n, d)` internally computes
  `(n * FXP_ONE) / d`.

### 9.4 Accumulator Pattern

```c
// The canonical DSP accumulate-and-narrow pattern:
fxp_acc_t acc = fxp_acc_zero();
for (size_t i = 0; i < n; i++) {
    acc = fxp_acc_mac(acc, a[i], b[i]);
}
fxp_t result = fxp_acc_narrow_sat(acc);
```

### 9.5 Common Patterns

**PID Controller:**

```c
fxp_t pid(fxp_t error, pid_state_t *s) {
    s->integral = fxp_add_sat(s->integral,
        fxp_mul_sat(error, s->ki));
    s->integral = fxp_clamp(s->integral, s->i_min, s->i_max);

    fxp_t derivative = fxp_mul_sat(fxp_sub_sat(error, s->prev_error), s->kd);
    s->prev_error = error;

    fxp_acc_t acc = fxp_acc_zero();
    acc = fxp_acc_mac(acc, error, s->kp);
    acc = fxp_acc_mac(acc, s->integral, FXP_ONE);
    acc = fxp_acc_mac(acc, derivative, FXP_ONE);

    return fxp_clamp(fxp_acc_narrow_sat(acc), s->out_min, s->out_max);
}
```

**Moving Average Filter:**

```c
fxp_t moving_average(const fxp_t *window, size_t n) {
    fxp_acc_t acc = fxp_acc_zero();
    for (size_t i = 0; i < n; i++) {
        acc = fxp_acc_mac(acc, window[i], FXP_ONE);
    }
    fxp_t sum = fxp_acc_narrow_sat(acc);
    fxp_t avg;
    fxp_div_chk(sum, fxp_from_int((int32_t)n), &avg);
    return avg;
}
```

**PWM Duty Cycle from Sensor:**

```c
// Map sensor reading [0, 4095] to PWM duty [0%, 100%]
fxp_t sensor_to_duty(uint16_t raw_reading) {
    fxp_t reading = fxp_from_ratio((int32_t)raw_reading, 4095);
    // reading is now in [0, 1]
    return fxp_clamp(reading, FXP_INT(0), FXP_ONE);
}
```

### 9.6 Configuration

| Macro           | Default                  | Description                      |
| --------------- | ------------------------ | -------------------------------- |
| `FXP_WORD_BITS` | 32                       | Storage width (16 or 32)         |
| `FXP_FRAC_BITS` | 16                       | Fractional bits (Q16.16 default) |
| `FXP_ROUNDING`  | `FXP_ROUND_NEAREST_EVEN` | Rounding policy                  |

Set via compiler flags:

```sh
meson setup build -Dc_args='-DFXP_FRAC_BITS=24 -DFXP_ROUNDING=FXP_ROUND_TRUNCATE'
```

### 9.7 Status Codes

| Code            | Meaning                            |
| --------------- | ---------------------------------- |
| `FXP_OK`        | Exact, in-range result             |
| `FXP_SATURATED` | Result clamped to [MIN, MAX]       |
| `FXP_DIV_ZERO`  | Divisor was zero                   |
| `FXP_INEXACT`   | Low bits discarded during rounding |
| `FXP_BAD_PARAM` | Argument out of valid domain       |

---

## Appendix A: Understanding the Types

| Type         | Size (Q16.16) | Purpose                                 |
| ------------ | ------------- | --------------------------------------- |
| `fxp_t`      | 32-bit        | Main fixed-point storage                |
| `fxp_wide_t` | 64-bit        | Wide intermediate for multiply/scale    |
| `fxp_acc_t`  | 64-bit        | Accumulator (alias for `fxp_wide_t`)    |
| `fxp_calc_t` | 64-bit        | Always-64-bit for construction/division |

## Appendix B: Rounding Policies

| Policy                   | Behavior              | Bias                    |
| ------------------------ | --------------------- | ----------------------- |
| `FXP_ROUND_TRUNCATE`     | Floor (toward -inf)   | Negative bias           |
| `FXP_ROUND_NEAREST_UP`   | Nearest, ties up      | Tiny positive bias      |
| `FXP_ROUND_NEAREST_EVEN` | Nearest, ties to even | Zero bias (recommended) |

## Appendix C: Q16.16 Quick Reference

| Concept    | Value                        |
| ---------- | ---------------------------- |
| `FXP_ONE`  | `65536` (represents 1.0)     |
| `FXP_MAX`  | `2147483647` (≈ 32767.99998) |
| `FXP_MIN`  | `-2147483648` (≈ -32768.0)   |
| Resolution | `1/65536` ≈ 0.000015         |
| Range      | ≈ ±32,768                    |

## Appendix D: Common Pitfalls and How to Avoid Them

### D.1 Never Mix Formats Without Rescaling

A common mistake is comparing or adding values from different Q-formats:

```c
// WRONG: Comparing Q16.16 with Q8.24
fxp_t value_a = FXP_C(1, 2);  // 0.5 in Q16.16
fxp_t value_b = rescale_to_q24(value_a);  // 0.5 in Q8.24

if (fxp_cmp(value_a, value_b) == 0) {  // FALSE! Different raw values!
    // This will never trigger because the raw integers differ
}

// CORRECT: Rescale before comparing
fxp_t normalized;
fxp_rescale(value_b, 24, 16, &normalized);
if (fxp_cmp(value_a, normalized) == 0) {  // TRUE!
    // Now they're in the same format
}
```

### D.2 Don't Forget to Check Division Status

Division always returns a status. Ignoring it can hide bugs:

```c
// WRONG: Ignoring divide-by-zero
fxp_t result;
fxp_div_chk(numerator, denominator, &result);
// If denominator is 0, result is 0 (not an error!)

// CORRECT: Check the status
enum fxp_status st = fxp_div_chk(numerator, denominator, &result);
if (st == FXP_DIV_ZERO) {
    handle_error();
}
```

### D.3 Accumulator Overflow with Full-Scale Inputs

If your inputs are near `FXP_MAX`, the accumulator will overflow quickly:

```c
// DANGEROUS: Full-scale inputs overflow the accumulator
fxp_t large = FXP_MAX;  // ≈ 32768
fxp_acc_t acc = fxp_acc_zero();
acc = fxp_acc_mac(acc, large, large);  // Product is ≈ 2^62
acc = fxp_acc_mac(acc, large, large);  // OVERFLOW! (exceeds 2^63)

// CORRECT: Use checked MAC
bool overflow = false;
acc = fxp_acc_mac_chk(acc, large, large, &overflow);
if (overflow) {
    // Handle overflow (e.g., scale down inputs)
}
```

### D.4 Compile-Time Constants Don't Check Range

`FXP_C` and `FXP_INT` are macros that don't check for overflow:

```c
// DANGEROUS: Silently wraps
static const fxp_t too_large = FXP_INT(100000);
// In Q16.16, this wraps because 100000 > 32767

// CORRECT: Use runtime construction for validation
fxp_t safe = fxp_from_int(100000);  // Saturates to FXP_MAX
```

### D.5 Floating-Point Helpers Are Optional

The `fxp_convert.h` helpers use `double` and should not be used in the core:

```c
// ONLY for host-side tooling, testing, or bring-up:
#include "fxp_convert.h"
fxp_t value = fxp_from_double(3.14159, NULL);

// NEVER in embedded code:
// This links soft-float, which you're trying to avoid!
```

## Appendix E: Complete DSP Example — FIR Low-Pass Filter

Here's a complete FIR filter implementation using `fxp`:

```c
#include "fxp.h"

#define FIR_TAPS 16

typedef struct {
    fxp_t coefficients[FIR_TAPS];
    fxp_t delay_line[FIR_TAPS];
    uint8_t write_index;
} fir_filter_t;

// Initialize filter with pre-computed coefficients
void fir_init(fir_filter_t *filter, const fxp_t *coeffs) {
    for (int i = 0; i < FIR_TAPS; i++) {
        filter->coefficients[i] = coeffs[i];
        filter->delay_line[i] = FXP_INT(0);
    }
    filter->write_index = 0;
}

// Process a single sample
fxp_t fir_process(fir_filter_t *filter, fxp_t input) {
    // Update delay line
    filter->delay_line[filter->write_index] = input;
    filter->write_index = (filter->write_index + 1) % FIR_TAPS;

    // Compute dot product using wide accumulator
    fxp_acc_t acc = fxp_acc_zero();
    for (int i = 0; i < FIR_TAPS; i++) {
        acc = fxp_acc_mac(acc, filter->coefficients[i],
                          filter->delay_line[i]);
    }

    // Narrow and saturate
    return fxp_acc_narrow_sat(acc);
}

// Example: Low-pass filter coefficients (normalized)
static const fxp_t lpf_coefficients[FIR_TAPS] = {
    FXP_C(-17, 10000), FXP_C(6, 10000),  FXP_C(31, 10000), FXP_C(-84, 10000),
    FXP_C(-164, 10000), FXP_C(242, 10000), FXP_C(1212, 10000), FXP_C(2772, 10000),
    FXP_C(3660, 10000), FXP_C(2772, 10000), FXP_C(1212, 10000), FXP_C(242, 10000),
    FXP_C(-164, 10000), FXP_C(-84, 10000), FXP_C(31, 10000), FXP_C(6, 10000),
};

// Usage in a DMA interrupt handler:
void dma_complete_handler(void) {
    static fir_filter_t filter;
    static bool initialized = false;

    if (!initialized) {
        fir_init(&filter, lpf_coefficients);
        initialized = true;
    }

    // Process each sample in the DMA buffer
    for (int i = 0; i < BUFFER_SIZE; i++) {
        fxp_t sample = adc_to_fxp(adc_buffer[i]);
        filtered_buffer[i] = fxp_to_adc(fir_process(&filter, sample));
    }
}
```

## Appendix F: Complete Driver Example — ADC to Engineering Units

Here's how a complete sensor driver might use `fxp`:

```c
#include "fxp.h"

typedef struct {
    fxp_t raw_value;       // ADC reading in fixed-point
    fxp_t voltage_mv;      // Voltage in millivolts
    fxp_t temperature_c;   // Temperature in °C
    enum fxp_status status;
} sensor_reading_t;

// ADC configuration
#define ADC_RESOLUTION 4096    // 12-bit ADC
#define ADC_REF_VOLTAGE_MV 3300  // 3.3V reference

// Thermistor configuration (Steinhart-Hart simplified)
#define THERMISTOR_B_CONSTANT FXP_C(3950, 1)
#define THERMISTOR_R_NOMINAL  FXP_C(10000, 1)  // 10kΩ nominal
#define THERMISTOR_T_NOMINAL  FXP_C(29815, 100) // 298.15K (25°C)
#define SERIES_RESISTOR       FXP_C(10000, 1)  // 10kΩ series

// Convert ADC raw value to voltage (mV)
fxp_t adc_to_voltage(uint16_t raw) {
    fxp_t raw_fxp = fxp_from_int((int32_t)raw);
    fxp_t resolution = fxp_from_int(ADC_RESOLUTION);

    // voltage = (raw / resolution) * ref_voltage
    fxp_t voltage;
    fxp_muldiv_chk(raw_fxp, fxp_from_int(ADC_REF_VOLTAGE_MV),
                   resolution, &voltage);
    return voltage;
}

// Convert voltage to temperature using thermistor equation
fxp_t voltage_to_temperature(fxp_t voltage_mv) {
    // R_therm = R_series * (V_sense / (V_ref - V_sense))
    fxp_t v_ref = fxp_from_int(ADC_REF_VOLTAGE_MV);
    fxp_t v_diff = fxp_sub_sat(v_ref, voltage_mv);

    fxp_t resistance;
    enum fxp_status st = fxp_div_chk(voltage_mv, v_diff, &resistance);
    if (st != FXP_OK) {
        return FXP_INT(0);  // Error: division failed
    }
    resistance = fxp_mul_sat(resistance, SERIES_RESISTOR);

    // Simplified Steinhart-Hart: T = 1 / (1/T_nom + (1/B) * ln(R/R_nom))
    // For simplicity, we use a linear approximation here
    fxp_t r_ratio;
    fxp_div_chk(resistance, THERMISTOR_R_NOMINAL, &r_ratio);

    // Natural log approximation (simplified for example)
    // In practice, use a lookup table or polynomial
    fxp_t ln_ratio = fxp_sub_sat(r_ratio, FXP_ONE);  // ln(x) ≈ x - 1 for x ≈ 1

    fxp_t inv_b = fxp_div_chk(FXP_ONE, THERMISTOR_B_CONSTANT, NULL);
    fxp_t temp_kelvin = fxp_add_sat(THERMISTOR_T_NOMINAL,
        fxp_mul_sat(inv_b, ln_ratio));

    // Convert to Celsius
    return fxp_sub_sat(temp_kelvin, FXP_C(27315, 100));  // -273.15
}

// Main sensor read function
sensor_reading_t sensor_read(uint16_t adc_raw) {
    sensor_reading_t reading;

    reading.raw_value = fxp_from_int((int32_t)adc_raw);
    reading.voltage_mv = adc_to_voltage(adc_raw);
    reading.temperature_c = voltage_to_temperature(reading.voltage_mv);
    reading.status = FXP_OK;

    // Clamp temperature to valid range
    reading.temperature_c = fxp_clamp(reading.temperature_c,
        FXP_INT(-40), FXP_INT(125));

    return reading;
}
```
