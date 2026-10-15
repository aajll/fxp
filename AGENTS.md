# AGENTS.md

---

## 1) Project-specific instructions

**Project:** `fxp`
**Primary goal:** A C11 integer-only fixed-point arithmetic library for embedded systems, with saturating and checked operations, one build-wide rounding policy, no allocation and no state.

### 1.1 Essential commands

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

#### MISRA C:2023 (reads `build/compile_commands.json`)

```sh
env -u MISRA_RULE_TEXTS misch run --baseline
```

#### Formatting (pinned clang-format through `uvx`)

```sh
scripts/format.sh          # check, as CI does
scripts/format.sh --fix    # rewrite in place
```

### 1.2 Notes

- `include/fxp.h` defines the version macros, and `meson setup` fails when they differ from `project(version:)`, so a release updates both. `include/fxp_version.h` is a deprecated header that includes `fxp.h`.
- The format and rounding are macros (`FXP_WORD_BITS`, `FXP_FRAC_BITS`, `FXP_ROUNDING`) with defaults in `fxp_conf.h`. The Meson options are `build_tests` and `with_convert`.
- `tests/meson.build` builds extra cores: Q8.8 for `test_format16`, and one per non-default rounding mode for `test_rounding_modes`.

---

## 2) Non-negotiable design invariants

- **No floating point in the core.** Only `src/fxp_convert.c` may use `double`. CI builds the core without it and fails if the object references a soft-float routine.
- **One rounding policy, build-wide.** Every narrowing path (multiply, divide, multiply-divide, accumulator narrow, rescale down, `fxp_from_ratio`, `fxp_to_int`) goes through `fxp_shr_round` or `fxp_div_round`. Never add a bare `>>` or `/` that rounds differently.
- **No undefined behaviour.** Never left-shift a negative signed value; multiply by `FXP_ONE` and take remainders with a mask. Form products in `fxp_wide_t` and saturate before narrowing. Special-case `FXP_MIN` in negate, absolute value and divide.
- **No exact 8-bit types.** Use `uint_least8_t` or `int_least8_t`; `uint8_t` does not exist where `CHAR_BIT` is 16.
- **Public contract.** The default format (Q16.16) and rounding (ties to even) are part of the API; changing either is a breaking change.
- No heap, no recursion, no global state.

---

## 3) CI / source of truth

- CI definitions live in `.github/workflows/ci.yml`.
- Prefer running the same commands locally as CI runs (see §1.1 above).
- Install Python tools (Meson, Ninja, gcovr, misch) with `uv tool install`, never `pip`; CI does the same through `astral-sh/setup-uv`.
- Regenerate the MISRA baseline only with `MISRA_RULE_TEXTS` unset (`env -u MISRA_RULE_TEXTS misch baseline`), because the rule texts change the recorded messages.
- The CI jobs are: tests under ASan/UBSan, the float-free core check, a release build, a TI C2000 library build (CGT 22.6.2 and 25.11.1, COFF and EABI, warnings as errors), `clang-format`, a 100 percent line and branch coverage gate, and `misch` against the baseline.

---

## 4) Docs / commit conventions

### Documentation layout

- `README.md` is a short landing page (about 70 lines). Put detail in `docs/`, never in the README; link to the owning file instead.
- `docs/integration.md` owns requirements, install methods, configuration, building, and cross builds.
- `docs/api.md` owns the API reference and recipes. Change it together with the headers; keep signatures in fenced `c` blocks and describe them in prose, not function tables. Check every value an example states against a build.
- `docs/design.md` is the single authoritative design record.
- `CONTRIBUTING.md` documents the contributor workflow; `CHANGELOG.md` holds release history; `.github/SECURITY.md` covers vulnerability reporting.
- Code blocks in `README.md`, `CONTRIBUTING.md`, and `docs/` are compiled by the `fxp docs examples` test (`tests/docs/check_snippets.py`). Each document is compiled as one source file: a `c` fence is file-level code, `<!-- snippet: body -->` wraps statements, and `<!-- snippet: skip -->` excludes pseudo-code. Do not use `skip` to silence a broken example.
- Prose uses British spelling. Do not hard-wrap Markdown: write one line per paragraph. The 80-column limit applies to C source, not Markdown.

### Commits

- Use **Conventional Commits** format when asked to commit.
- Keep commits focused; explain *why* in the message body.

---

## 5) C style expectations

### Build & configuration

- Use the Meson build system. Do not introduce CMake, Make, or other systems.
- Update `meson.build` and `tests/meson.build` when adding or removing files.

### Formatting

- `.clang-format` is present and **mandatory**. Run `scripts/format.sh --fix` before committing. It runs the clang-format release pinned in the script through `uvx`. Do not use a system `clang-format`: another release can format the same code differently.
- Do not reformat unrelated code.
- Key settings: 8-space indent, `BreakBeforeBraces: Linux`, column limit 80.

### Style & correctness

- Match conventions in the existing files (indentation, braces, naming).
- Keep public headers minimal and stable.
- A `_chk` function's `out` pointer is a precondition, not runtime-checked; optional reporting pointers accept `NULL`.
- Configuration goes in `fxp_conf.h` behind `#ifndef` guards, with invariants enforced by `_Static_assert` in `fxp_config.h`.

### Error handling

- Out-of-range results saturate; `_chk` variants report through `enum fxp_status`.
- No `errno`; no exceptions.

### Comment placement (Doxygen)

- Inline trailing annotations (`/**< ... */`) on `enum`/`struct` members are allowed only when the resulting line fits the 80-column limit.
- If any member's annotation would overrun, move **all** of that aggregate's member docs into a single structured Doxygen block above the type, as an `@details` list of `- ::SYMBOL  description` entries.
- Never mix inline and block forms within one aggregate, and never leave a trailing comment that clang-format would wrap onto a second line.
- After editing, run `scripts/format.sh` and an 80-column scan.

### Testing

- Run `meson test -C build` after every change.
- Add a test case for each bug fix.
- Check rounding changes on negative values and ties in every rounding mode, and width-dependent code in the Q8.8 build.
- Keep `src/` and `include/` at the coverage gate (100 percent line and branch).
