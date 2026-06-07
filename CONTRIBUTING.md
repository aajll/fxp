# Contributing to fxp

fxp is a small, integer-only fixed-point arithmetic kernel for embedded C. It is designed to be safe to drop into firmware and audited environments, so correctness and determinism take priority over breadth of features.

## Getting started

The same commands CI runs, locally:

```sh
# Configure with tests + sanitisers (CI default)
meson setup build --buildtype=debug -Dbuild_tests=true \
                  -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# Coverage (CI gate is 80% line + 70% branch)
meson setup build_cov --buildtype=debug -Dbuild_tests=true \
                      -Db_coverage=true
meson compile -C build_cov && meson test -C build_cov
gcovr --root . --filter 'src/' --filter 'include/' --print-summary
```

## Source style

- `.clang-format` is mandatory. Run `clang-format -i` on every modified `.c` / `.h` file before submitting.
- 8-space indent, Linux brace style, 80-column limit. Match the existing conventions; do not reformat unrelated code.
- The Meson build system is the single source of truth. Update `meson.build` / `tests/meson.build` when adding or removing source files.
- No CMake, no Make, no other build systems.
- Markdown: write one logical line per paragraph and per list item; do not hard-wrap prose at a fixed column. Let the renderer handle wrapping. (Code blocks and tables keep their own line breaks.)

## C language rules

- C11 only (uses `_Static_assert`).
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>`.
- No heap allocation (`malloc`, `free`, VLAs). The library has no global state; keep all functions pure and reentrant.
- Pointer-argument policy: a required `out` result pointer on a `_chk` function is a documented precondition (`@pre out != NULL`) — not runtime-checked, so the hot path stays branch-free. Optional status/flag pointers (`st`, `clamped`, `overflow`) must be NULL-tolerant. Keep new APIs consistent with this split.
- **No floating point in the core.** Only `src/fxp_convert.c` may reference `double`. CI's nm-gate fails the build if any float ABI symbol appears in the core object. New float code belongs in the convert translation unit.
- Never left-shift a negative signed value (UB). Use multiplication by `FXP_ONE` and mask-based remainders, as the existing code does.

## Arithmetic correctness

This is a math kernel; the bar for arithmetic changes is high.

- Every narrowing path must route through the shared rounding helpers so the build-wide `FXP_ROUNDING` policy stays uniform. Do not introduce a bare `>>` or `/` that rounds inconsistently with the rest of the library.
- Multiplies and scales must use the `fxp_wide_t` intermediate and saturate before narrowing — no silent overflow.
- Special-case the most-negative value (`FXP_MIN`) in negate/abs/divide.

## Tests and coverage

- Add a test for every bug fix and every new operation.
- Prefer exhaustive or swept tests over spot checks where the domain allows; the double-oracle in `tests/test_convert.c` is the model.
- New rounding behaviour must be checked on negative values and tie points.
- CI enforces an 80% line / 70% branch coverage gate. New code without tests will fail it.
- Tests live in `tests/test_*.c` and must be registered with `test()` in `tests/meson.build`.

## API and format stability

- The public API in `fxp.h` is stable across the 0.x line on a best-effort basis and locked from 1.0.0.
- The default build format (Q16.16) and rounding policy (round-to-nearest, ties-to-even) are part of the contract: changing either shifts downstream numerical results and is a breaking change.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `doc: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` neither fixes a bug nor adds a feature

Keep the subject under ~70 characters. Use the body to explain _why_.

## Pull requests

- Open an issue first for non-trivial changes so the design can be agreed.
- Keep PRs focused: one feature or one fix per PR.
- All CI checks must pass: tests on Linux + macOS, ASan + UBSan, the nm-gate, the release build, and the coverage gate.
