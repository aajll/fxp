# Security Policy

## Reporting a Vulnerability

Use GitHub's Security Advisories feature to report security concerns privately.

Expect a response within 7 days. If the issue is confirmed, a fix will be released as a patch version.

## Scope

fxp is a small, integer-only fixed-point arithmetic library with no network stack, no external dependencies, and no dynamic memory allocation. The primary attack surface is integer overflow / undefined behaviour in the arithmetic and narrowing paths.

The core (`fxp.c`) links no floating-point ABI symbols; the optional `double` conversion helpers (`fxp_convert.c`) are the only component that touches floating point and can be excluded with `-Dwith_convert=false`.
