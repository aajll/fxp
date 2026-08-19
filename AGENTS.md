# AGENTS.md

## Project-specific instructions

**Project:** `fxp` **Primary goal:** Portable fixed-point arithmetic kernel for embedded C

### Essential commands

#### Configure and build (library only)

```sh
meson setup build --wipe --buildtype=release -Dbuild_tests=false
meson compile -C build
```

#### Configure, build, and run unit tests

```sh
meson setup build --wipe --buildtype=debug -Dbuild_tests=true
meson compile -C build
meson test -C build --verbose
```

### CI / source of truth

- CI definitions live in `.github/workflows/ci.yml`.
- Prefer running the same commands locally as CI runs.
- If `pre-commit` is configured later, run it before committing.

## Docs / commit conventions

- Use Conventional Commits when asked to commit.
- Keep commits focused and explain why the change exists.

## C style expectations

### Build and configuration

- Use the Meson build system; do not introduce another build system.
- Update `meson.build` when adding or removing source files.

### Formatting

- `.clang-format` is present and should be used on modified `.c` and `.h` files.
- Do not reformat unrelated code.
- Key settings: 8-space indent, `BreakBeforeBraces: Linux`, column limit 80.

### Style and correctness

- Match the conventions in the existing files.
- Keep public headers minimal and stable.
- Prefer explicit fixed-width integer types when ABI or serialization matters.
- Use `fxp_conf.h` for compile-time configuration options. This header is automatically included by `fxp.h` and can be overridden before including the main header.

### fxp-specific invariants (do not break these)

- **No floating point in the core.** Only `src/fxp_convert.c` may use `double`. CI's nm-gate fails if any float ABI symbol appears in `fxp.c`'s object.
- **One rounding policy, build-wide.** Every narrowing (`mul`, `div`, `muldiv`, accumulator narrow, rescale-down, `to_int`) must route through the shared `fxp_shr_round` / `fxp_div_round` helpers. Never add a bare `>>` or `/` that rounds inconsistently.
- **Never left-shift a negative signed value** (UB). Multiply by `FXP_ONE` and take remainders with a mask, as the existing code does.
- **Use `fxp_wide_t` intermediates** for multiply/scale and saturate before narrowing; special-case `FXP_MIN` in negate/abs/divide.
- The default format (Q16.16) and rounding (ties-to-even) are part of the public contract — changing them is a breaking change.

### Comment placement (Doxygen)

- Inline trailing annotations (`/**< ... */`) on `enum`/`struct` members are allowed only when the resulting line fits the 80-column limit.
- If any member's annotation would overrun, move **all** of that aggregate's member docs into a single structured Doxygen block above the type, as an `@details` list of `- ::SYMBOL  description` entries.
- Never mix inline and block forms within one aggregate, and never leave a trailing comment that clang-format would wrap onto a second line.
- After editing, verify with a `clang-format --style=file` no-reformat diff and an 80-column scan.

### Testing

- Run `meson test -C build` after changes.
- Add a test case for each bug fix.
- Keep tests in `tests/test_*.c`.
