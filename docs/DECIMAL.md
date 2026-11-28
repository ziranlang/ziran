# Native decimal arithmetic

`std/decimal_linux` provides exact decimal construction and context-controlled
arithmetic through the optional, 64-bit Linux `libmpdec.so.4` ABI. It does not
use Python, launch a worker or change the process's decimal trap handler.
Programs that do not import it do not acquire this dependency. The compiler
and the ordinary compiler checks do not require mpdecimal.

```zi
D :: #import "std/decimal_linux"

main :: () -> s32 {
    arena: D.Arena
    defer D.Close(*arena)
    a := D.Parse(*arena, "0.1")
    b := D.Parse(*arena, "0.2")
    result := D.Binary(*arena, .Add, a, b)
    if !result.valid { return 1 }
    text := D.ToText(result)
    print("%\n", TextView(text)) // 0.3
    return 0
}
```

An arena owns every native coefficient allocated through it. A `Decimal` is a
copyable reference borrowing that arena, including the cached `valid`, `finite`,
`negative`, `zero` and `status` properties. Keep the arena alive until all uses
of its references have ended. Do not copy the arena, close it during an
operation, use a reference after closing it or share a mutable arena between
threads. `Close` frees its coefficients and vector; repeated close is safe.
Each operation allocates a new coefficient rather than mutating its inputs.
For long-running work, use an arena per request or bounded batch and close it
when the result has been copied into owned application data.

`Parse` copies exact ASCII decimal syntax, including signed zero, infinities,
quiet/signaling NaNs and NaN payloads. It does not round to the default
precision. Its native 64-bit limits are 999999999999999999 digits of precision
and exponent magnitude, subject to available memory and mpdecimal's adjusted
exponent rules. There is no smaller coefficient buffer or exponent clamp.
Unicode digits, underscores, surrounding whitespace and application-specific
coercion belong in the caller. Embedded NUL is rejected, rather than truncating
input at the C boundary. `FromFloat` constructs the exact IEEE binary64 value,
including negative zero and subnormals; it does not parse a short float string.

`Default()` selects precision 28, Emin -999999, Emax 999999, half-even rounding,
and traps for invalid operations, division by zero and overflow. Change a
`Context` value explicitly for another precision, exponent range, rounding,
clamp or trap policy. Quiet native operations return status flags without
raising a signal. `valid` is false when a selected trap fires, allocation fails,
an input was invalid or the library is unavailable. `status` retains the native
mpdecimal flags, including details of invalid operations. A trapped result
can still have a native handle for diagnostics; check `valid` before using it
in subsequent arithmetic. Applications that require finite amounts must also
check `finite`, since successfully parsed NaN and infinity are valid decimals.

`Binary` offers add, subtract, multiply, divide, power, quantize, scale-by-ten,
integer division, remainder, minimum and maximum. `Unary` offers context plus,
minus and normalize, and integral-value floor, ceiling and truncate; the last
three retain infinities and do not signal inexact rounding. `Compare` returns
an order and a separate success flag; NaNs have no order. `ToText` copies the
exact scientific decimal representation into an owned `Text`. `Format` uses
mpdecimal's decimal format grammar and returns owned text plus status. These
texts remain valid after closing the coefficient's arena.

## Deployment and verification

Install a 64-bit mpdecimal 4 runtime or bundle its shared library and BSD
license notice with the application. The module calls `dlopen("libmpdec.so.4",
RTLD_NOW)` once and retains the immutable library/function table until process
exit. Standard loader search paths, including `LD_LIBRARY_PATH` set before
launch, apply. A missing library or symbol makes `Available()` false and
operations return `UNAVAILABLE`; no fallback arithmetic or Python process is
started. `Version()` returns the loaded library's version, or empty text.

The verified dependency was Debian trixie `libmpdec4` **4.0.1-5**, built from
[mpdecimal 4.0.1](https://www.bytereef.org/mpdecimal/download.html), licensed
BSD-2-Clause (Copyright 2008–2025 Stefan Krah). The amd64 runtime package has
SHA-256 `4b431fb6617a664540dd65331f3fea703f756d8631cd613602f0bb8f0cc51bd2`;
its `libmpdec.so.4.0.1` has SHA-256
`70502e4225fa79a009a93ad6d20dc2f25137e84292b57d9d7714e4a2708960be`.
The shared library needs libc and libm; `libmpdec-dev` is unnecessary at
application runtime. This binding requires no mpdecimal headers or strong
`-lmpdec` link. Consult the upstream [context](https://www.bytereef.org/mpdecimal/doc/libmpdec/context.html)
and [arithmetic](https://www.bytereef.org/mpdecimal/doc/libmpdec/arithmetic.html)
references for detailed native limits and status semantics.

Run the explicit native integration check with the runtime on the loader path:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY build/bin/ziran run --target=c --root tests tests/decimal_linux_check.zi
```

It compares frozen CPython 3.13.5/libmpdec 2.5.1 results for exact construction,
large coefficients and exponent limits, every binary/unary operation, rounding,
traps, status flags, formatting, and 1,613 binary64 bit patterns. All retained
test control is Ziran; Python is not needed to run the check. It also checks
reference copies and repeated arena cleanup. On a machine without the optional
library, test the explicit unavailable result with the same command followed
by `-- --expect-unavailable`. The full integration check fails if its library
is missing.
