# Contributing to fxp

`fxp` is a small C11 fixed-point arithmetic library for embedded systems. Downstream filters and control loops depend on its exact results, so two contracts carry most of the design weight: every narrowing path applies the one build-wide rounding policy, and no input, however extreme, causes undefined behaviour. Keep both in mind when you change anything.

## Getting started

Install a C11 compiler and install the Python tools with [uv](https://docs.astral.sh/uv/), as CI does. The MISRA analysis also needs a current `cppcheck`; CI installs it from Homebrew because the 2.13 release in Ubuntu 24.04 reports different findings. The same commands CI runs, locally:

```sh
# Python tools
uv tool install meson
uv tool install ninja
uv tool install gcovr
uv tool install misch

# Configure with tests and sanitisers (CI default). The suite builds the
# default core, a Q8.8 core and a core per rounding mode.
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# Coverage (CI gate is 100% line and branch for src/ and include/)
meson setup build_cov --buildtype=debug -Dbuild_tests=true -Db_coverage=true
meson compile -C build_cov && meson test -C build_cov
gcovr --root . --filter 'src/' --filter 'include/' --merge-mode-functions=separate --print-summary

# MISRA C:2023 against the baseline (reads build/compile_commands.json)
env -u MISRA_RULE_TEXTS misch run --baseline

# Formatting (pinned clang-format through uvx)
scripts/format.sh --fix
```

## Source style

- Formatting is mandatory. Run `scripts/format.sh --fix` before submitting, or `scripts/format.sh` to check without writing, as CI does. The script pins the clang-format release and runs it through `uvx`, so an editor formatter gives the same result only when it runs the same release.
- 8-space indent, Linux brace style, 80-column limit. Match the existing conventions and do not reformat unrelated code.
- The Meson build system is the single source of truth. Update `meson.build` and `tests/meson.build` when you add or remove files. No CMake, no Make, and no other build systems.

## C language rules

- C11 only.
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>`. For small counts use `uint_least8_t` or `int_least8_t`, never `uint8_t` or `int8_t`: C11 does not provide the exact 8-bit types where `CHAR_BIT` is 16, and the CI cross build fails on them.
- No heap allocation (`malloc`, `free`, VLAs), no recursion, and no global state. Every function stays pure and reentrant.
- A `_chk` function's `out` pointer is a documented precondition (`@pre out != NULL`) and is not checked at run time. Optional reporting pointers (`st`, `clamped`, `overflow`) must accept `NULL`. Keep new functions consistent with this split.
- No floating point in `src/fxp.c`. Only `src/fxp_convert.c` may use `double`, and CI fails if the core object references a soft-float routine.
- Never left-shift a negative signed value. Scale by multiplying by `FXP_ONE`, and take remainders with a mask.

## Arithmetic correctness

- Every narrowing path goes through the shared rounding helpers, `fxp_shr_round` and `fxp_div_round`, so `FXP_ROUNDING` applies uniformly. Do not add a bare `>>` or `/` that rounds differently.
- Form multiplies and scales in `fxp_wide_t`, or `fxp_calc_t` for the `int32_t` constructors, and saturate before converting to `fxp_t`.
- Handle `FXP_MIN` explicitly in negation, absolute value and division.
- The default format (Q16.16) and rounding (ties to even) are part of the public contract. Changing either changes downstream results and is a breaking change.

## MISRA C:2023

The library is analysed with `misch` (cppcheck-backed MISRA C:2023 analysis), configured by `misra.toml`, over `include/` and `src/`. It reads the compilation database from a Meson `build/` directory, so configure one first. The baseline in `analysis/baseline/` records the existing findings, and CI fails on any finding that is not in it.

The baseline is recorded without rule texts, and the texts change the messages a finding is matched on. If you have `MISRA_RULE_TEXTS` set, unset it for `misch run --baseline` as well, as CI does.

A change must not add findings. If it removes one, regenerate the baseline with `MISRA_RULE_TEXTS` unset:

```sh
env -u MISRA_RULE_TEXTS misch baseline
```

Full tool-driven compliance beyond this analysis needs a certified static analyser, which this repository does not vendor.

## Tests and coverage

- Add a test for every bug fix and every new operation.
- Prefer swept or exhaustive tests over spot checks where the domain allows. The `double` oracle in `tests/test_convert.c` is the model.
- Check new rounding behaviour on negative values and tie points in every rounding mode. `tests/test_rounding_modes.c` runs against a core build per mode; add each new narrowing path to its table.
- Code that depends on the word width needs a case in `tests/test_format16.c`, which runs against a Q8.8 core.
- Tests live in `tests/test_*.c`, each its own executable registered with `test()` in `tests/meson.build`.
- CI enforces 100 percent line and branch coverage of `src/` and `include/`. Every new branch needs a test that fails when the branch is broken.
- The `fxp docs examples` test compiles every C code block in `README.md`, `CONTRIBUTING.md`, and `docs/` against the public headers, through `tests/docs/check_snippets.py`. A block compiles at file scope by default. On the line directly above the fence, put `<!-- snippet: body -->` for a block of statements, or `<!-- snippet: skip -->` for pseudo-code or a copy of a header definition.

## Documentation

| File | Purpose |
| --- | --- |
| `README.md` | Landing page: quick start, description, validated toolchains, and links. Keep it short. |
| `docs/integration.md` | Installation, configuration, building, and cross builds. |
| `docs/api.md` | Reference for every function, macro and status code, with recipes. |
| `docs/design.md` | The fixed-point model, rounding, narrowing, accumulator headroom, defined behaviour, and verification. |

Update `docs/api.md` and the header Doxygen together when an interface changes, and check every value an example states against a build. Update `docs/design.md` when behaviour changes. Keep it standalone: describe how the library works, not how it was built or which other projects use it. Write prose in British English, one line per paragraph.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `docs: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` a change that neither fixes a bug nor adds a feature

Keep the subject under about 70 characters. Use the body to explain why the change is needed.

## Pull requests

- Open an issue first for non-trivial changes so the design can be agreed before implementation.
- Keep pull requests focused: one feature or one fix.
- All CI checks must pass.
- Flag any change to a numerical result in the description.

## When in doubt

Open an issue. A change to a rounded result shifts every filter and control loop built on it.
