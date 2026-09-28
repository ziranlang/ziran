# Record-call benchmark

Fresh-process wall time; validated output for every sample. Not a language ranking.

| Phase | Case | Median ms | Range ms | Samples |
|---|---|---:|---:|---:|
| cached go build | hand Go | 107.707 | 105.500–117.871 | 3 |
| cached go build | ziran go (saved) | 112.037 | 102.918–124.970 | 3 |
| cached go build | ziran go (source) | 112.195 | 83.712–116.457 | 3 |
| comparison compile | hand C | 36.285 | 36.189–36.498 | 3 |
| comparison compile | hand C++ | 47.899 | 47.531–48.422 | 3 |
| comparison compile | hand Java | 374.316 | 368.614–385.652 | 3 |
| comparison compile | hand Rust | 246.789 | 244.371–253.325 | 3 |
| downstream compile | ziran c (saved) | 72.736 | 72.607–73.571 | 3 |
| downstream compile | ziran c (source) | 74.954 | 73.036–75.671 | 3 |
| downstream compile | ziran cpp (saved) | 91.937 | 91.712–92.380 | 3 |
| downstream compile | ziran cpp (source) | 94.823 | 92.724–98.449 | 3 |
| run n=1000 | JavaScript | 25.984 | 25.697–27.086 | 5 |
| run n=1000 | Python | 33.099 | 31.649–33.487 | 5 |
| run n=1000 | hand C | 0.718 | 0.604–0.741 | 5 |
| run n=1000 | hand C++ | 0.545 | 0.435–0.763 | 5 |
| run n=1000 | hand Go | 1.026 | 0.930–1.183 | 5 |
| run n=1000 | hand Java | 28.852 | 27.007–32.670 | 5 |
| run n=1000 | hand Rust | 0.902 | 0.538–0.918 | 5 |
| run n=1000 | ziran VM (saved) | 221.883 | 221.425–224.924 | 5 |
| run n=1000 | ziran VM (source) | 224.098 | 222.516–232.464 | 5 |
| run n=1000 | ziran c (saved) | 0.732 | 0.643–0.952 | 5 |
| run n=1000 | ziran c (source) | 1.002 | 0.820–1.096 | 5 |
| run n=1000 | ziran cpp (saved) | 0.986 | 0.646–1.166 | 5 |
| run n=1000 | ziran cpp (source) | 0.857 | 0.711–1.110 | 5 |
| run n=1000 | ziran go (saved) | 1.249 | 1.198–1.587 | 5 |
| run n=1000 | ziran go (source) | 1.526 | 1.395–1.738 | 5 |
| run n=500000 | JavaScript | 263.526 | 263.201–285.927 | 5 |
| run n=500000 | Python | 10819.586 | 10589.643–11367.342 | 5 |
| run n=500000 | hand C | 24.736 | 24.402–24.930 | 5 |
| run n=500000 | hand C++ | 25.259 | 24.467–25.888 | 5 |
| run n=500000 | hand Go | 29.107 | 23.004–30.379 | 5 |
| run n=500000 | hand Java | 140.135 | 138.009–159.099 | 5 |
| run n=500000 | hand Rust | 25.177 | 24.988–26.928 | 5 |
| run n=500000 | ziran c (saved) | 166.550 | 165.184–172.721 | 5 |
| run n=500000 | ziran c (source) | 170.947 | 165.172–176.943 | 5 |
| run n=500000 | ziran cpp (saved) | 166.189 | 162.964–168.990 | 5 |
| run n=500000 | ziran cpp (source) | 170.981 | 163.043–172.654 | 5 |
| run n=500000 | ziran go (saved) | 220.346 | 214.270–240.211 | 5 |
| run n=500000 | ziran go (source) | 227.709 | 219.597–228.837 | 5 |
| ziran | saved IR check | 0.925 | 0.901–0.967 | 3 |
| ziran | saved to c | 2.210 | 2.109–2.629 | 3 |
| ziran | saved to cpp | 2.038 | 1.894–2.386 | 3 |
| ziran | saved to go | 2.654 | 2.457–3.092 | 3 |
| ziran | saved to zib | 1.620 | 1.609–1.661 | 3 |
| ziran | source check | 1.861 | 1.831–1.992 | 3 |
| ziran | source to c | 2.731 | 2.719–3.436 | 3 |
| ziran | source to cpp | 2.856 | 2.852–3.214 | 3 |
| ziran | source to go | 2.960 | 2.933–2.983 | 3 |
| ziran | source to saved IR | 1.894 | 1.882–1.928 | 3 |
| ziran | source to zib | 2.765 | 2.502–2.981 | 3 |

Expected checksums: {"1000": 33798843034, "500000": -2238818866076}

Generated source equality: {"c": true, "cpp": true, "go": true}

Source and saved-IR bundles are byte-identical.

Unavailable comparisons: none

- Fresh processes include startup, loading, printing, and shutdown.
- Go build uses a dedicated warm cache; it is not a forced recompilation.
- Each implementation updates a fixed 32-record table with wrapping s32 arithmetic.
- JavaScript uses BigInt for the sum to preserve exact checksums.
- No CPU isolation; medians are not regression thresholds.
- VM large run omitted because record/call execution exceeds its instruction budget.
- RSS from wait4 includes launcher accounting and is omitted from the table.
