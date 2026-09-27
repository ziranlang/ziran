# UTF-8 scan benchmark

Fresh-process wall time; validated output for every sample. Not a language ranking.

| Phase | Case | Median ms | Range ms | Samples |
|---|---|---:|---:|---:|
| cached go build | hand Go | 114.701 | 104.011–155.249 | 3 |
| cached go build | ziran go (saved) | 114.866 | 105.158–115.668 | 3 |
| cached go build | ziran go (source) | 103.435 | 91.124–141.567 | 3 |
| comparison compile | hand C | 31.174 | 28.384–33.149 | 3 |
| comparison compile | hand C++ | 46.133 | 42.148–46.596 | 3 |
| comparison compile | hand Java | 356.179 | 342.233–362.277 | 3 |
| comparison compile | hand Rust | 227.132 | 202.823–231.469 | 3 |
| downstream compile | ziran c (saved) | 78.038 | 71.194–80.215 | 3 |
| downstream compile | ziran c (source) | 101.291 | 76.259–104.033 | 3 |
| downstream compile | ziran cpp (saved) | 109.089 | 105.641–135.311 | 3 |
| downstream compile | ziran cpp (source) | 112.486 | 105.181–120.257 | 3 |
| fake Plan 9 compile | ziran plan9-c (saved) | 78.253 | 65.074–89.686 | 3 |
| fake Plan 9 compile | ziran plan9-c (source) | 80.594 | 75.103–81.738 | 3 |
| run rounds=1000 | JavaScript | 22.307 | 17.478–22.723 | 5 |
| run rounds=1000 | Python | 10.551 | 9.212–11.430 | 5 |
| run rounds=1000 | hand C | 0.307 | 0.283–0.589 | 5 |
| run rounds=1000 | hand C++ | 0.520 | 0.270–0.563 | 5 |
| run rounds=1000 | hand Go | 1.105 | 0.861–1.265 | 5 |
| run rounds=1000 | hand Java | 33.557 | 25.653–33.981 | 5 |
| run rounds=1000 | hand Rust | 0.766 | 0.614–0.950 | 5 |
| run rounds=1000 | ziran VM (saved) | 39.746 | 38.293–41.205 | 5 |
| run rounds=1000 | ziran VM (source) | 40.430 | 37.449–40.948 | 5 |
| run rounds=1000 | ziran c (saved) | 0.371 | 0.340–0.719 | 5 |
| run rounds=1000 | ziran c (source) | 0.656 | 0.335–0.994 | 5 |
| run rounds=1000 | ziran cpp (saved) | 0.402 | 0.330–0.560 | 5 |
| run rounds=1000 | ziran cpp (source) | 0.535 | 0.336–0.791 | 5 |
| run rounds=1000 | ziran go (saved) | 1.188 | 0.963–1.605 | 5 |
| run rounds=1000 | ziran go (source) | 1.034 | 0.823–1.290 | 5 |
| run rounds=1000 | ziran plan9-c dialect (saved) | 0.365 | 0.290–0.709 | 5 |
| run rounds=1000 | ziran plan9-c dialect (source) | 0.507 | 0.362–0.864 | 5 |
| run rounds=2000000 | JavaScript | 56.853 | 54.406–64.165 | 5 |
| run rounds=2000000 | Python | 560.506 | 540.036–623.428 | 5 |
| run rounds=2000000 | hand C | 11.153 | 9.992–12.644 | 5 |
| run rounds=2000000 | hand C++ | 10.768 | 9.929–11.124 | 5 |
| run rounds=2000000 | hand Go | 13.661 | 13.496–15.116 | 5 |
| run rounds=2000000 | hand Java | 50.945 | 43.100–57.071 | 5 |
| run rounds=2000000 | hand Rust | 5.336 | 5.128–5.810 | 5 |
| run rounds=2000000 | ziran c (saved) | 63.116 | 62.107–63.823 | 5 |
| run rounds=2000000 | ziran c (source) | 63.536 | 62.330–64.744 | 5 |
| run rounds=2000000 | ziran cpp (saved) | 64.238 | 63.064–66.102 | 5 |
| run rounds=2000000 | ziran cpp (source) | 62.804 | 61.418–64.737 | 5 |
| run rounds=2000000 | ziran go (saved) | 300.845 | 290.416–326.958 | 5 |
| run rounds=2000000 | ziran go (source) | 309.862 | 300.952–323.924 | 5 |
| run rounds=2000000 | ziran plan9-c dialect (saved) | 42.653 | 42.086–43.765 | 5 |
| run rounds=2000000 | ziran plan9-c dialect (source) | 43.221 | 42.727–43.909 | 5 |
| ziran | saved IR check | 1.586 | 1.432–2.087 | 3 |
| ziran | saved to c | 4.143 | 4.124–4.275 | 3 |
| ziran | saved to cpp | 4.761 | 4.500–4.815 | 3 |
| ziran | saved to go | 5.103 | 4.353–5.276 | 3 |
| ziran | saved to plan9-c (direct) | 4.597 | 4.143–5.871 | 3 |
| ziran | saved to plan9-c (dispatcher) | 4.728 | 4.223–4.907 | 3 |
| ziran | saved to zib | 2.731 | 2.575–3.030 | 3 |
| ziran | source check | 2.819 | 2.696–3.153 | 3 |
| ziran | source to c | 8.452 | 6.835–9.335 | 3 |
| ziran | source to cpp | 5.150 | 4.592–7.500 | 3 |
| ziran | source to go | 5.340 | 5.158–7.012 | 3 |
| ziran | source to plan9-c (direct) | 7.113 | 5.953–7.796 | 3 |
| ziran | source to plan9-c (dispatcher) | 9.052 | 7.147–10.334 | 3 |
| ziran | source to saved IR | 2.867 | 2.755–3.305 | 3 |
| ziran | source to zib | 4.703 | 4.074–6.443 | 3 |

Expected checksums: {"1000": 148921000, "2000000": 297842000000}

Generated source equality: {"c": true, "cpp": true, "go": true, "plan9-c": true}

Source and saved-IR bundles are byte-identical.

Unavailable comparisons: Plan 9 native execution: 9c unavailable

- Fresh processes include startup, loading, printing, and shutdown.
- Go build uses a dedicated warm cache; it is not a forced recompilation.
- Ziran, C, and C++ use explicit UTF-8 decoders; Go, Rust, Java, JavaScript, and Python use their native codepoint iteration.
- No CPU isolation; medians are not regression thresholds.
- VM large run omitted because its instruction budget is too small.
- RSS from wait4 includes launcher accounting and is omitted from the table.
- Plan 9 C emission includes checking, target lowering, and the post-pass in one process; compare it with direct C emission rather than interpreting the difference as an isolated post-pass cost.
- Plan 9 dialect execution is compiled by host GCC against a minimal fake Plan 9 libc when no Plan 9 compiler is installed; it validates output and dialect shape but is not Plan 9 hardware performance.
