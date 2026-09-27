# Plan 9 multilanguage UTF-8 scan matrix

This run measures a fixed `aé中🙂` workload (10 UTF-8 bytes, 4 Unicode scalar
values) from Ziran source and saved IR to C, C++, Go, experimental Plan 9 C,
and portable `.zib`, with handwritten C, C++, Go, Rust, Java, JavaScript, and
Python comparisons. It contains **244 validated samples across 60 phase/case
combinations**. Every runtime sample returned `148921 * rounds`, including
`297842000000` at two million rounds for both Plan 9 dialect executables.
Generated C, C++, Go, and Plan 9 C were identical for source and saved IR;
direct and dispatcher Plan 9 C emission was identical; and the two `.zib`
bundles were byte-identical.

At two million rounds, median fresh-process times were: Rust 5.336 ms,
handwritten C++ 10.768 ms, handwritten C 11.153 ms, handwritten Go 13.661 ms,
generated Plan 9 C dialect 42.653–43.221 ms, Java 50.945 ms, JavaScript
56.853 ms, generated C 63.116–63.536 ms, generated C++ 62.804–64.238 ms,
generated Go 300.845–309.862 ms, and Python 560.506 ms. The explicit
Ziran/C/C++ decoder validates multibyte structure on every scalar, while Go,
Rust, Java, JavaScript, and Python use their native codepoint iterators.
These whole-process numbers are not a general language ranking.

The run used commit `1eda3d61cf74d370874c763eebe20158e6c08765`. No Plan 9
compiler was installed, so dialect execution was compiled by host GCC against
a minimal fake Plan 9 libc; it validates output and dialect shape but is not
Plan 9 hardware performance. Raw commands, timings, tool versions, fixture
hashes, generated-output hashes, and caveats are retained alongside this
summary.

Reproduce it with:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY python3 bench/text_scan.py --out /tmp/ziran-text-scan-plan9
```
