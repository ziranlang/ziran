# Record-call benchmark

Fresh-process wall time; validated output for every sample. Not a language ranking.

| Phase | Case | Median ms | Range ms | Samples |
|---|---|---:|---:|---:|
| cached go build | hand Go | 70.870 | 64.990–73.822 | 3 |
| cached go build | ziran go (saved) | 67.063 | 63.342–97.682 | 3 |
| cached go build | ziran go (source) | 117.158 | 67.253–121.038 | 3 |
| comparison compile | hand C | 22.891 | 21.764–23.968 | 3 |
| comparison compile | hand C++ | 30.386 | 30.141–32.580 | 3 |
| comparison compile | hand Java | 223.009 | 215.997–225.202 | 3 |
| comparison compile | hand Rust | 169.498 | 161.732–171.725 | 3 |
| downstream compile | ziran c (saved) | 44.550 | 44.078–47.203 | 3 |
| downstream compile | ziran c (source) | 44.606 | 43.279–46.724 | 3 |
| downstream compile | ziran cpp (saved) | 56.442 | 56.190–56.877 | 3 |
| downstream compile | ziran cpp (source) | 56.262 | 55.270–58.174 | 3 |
| downstream compile | ziran rust (saved) | 8.649 | 8.153–9.467 | 3 |
| downstream compile | ziran rust (source) | 8.959 | 8.144–8.998 | 3 |
| run n=1000 | JavaScript | 15.527 | 14.380–18.794 | 5 |
| run n=1000 | Python | 18.800 | 18.355–19.136 | 5 |
| run n=1000 | hand C | 0.389 | 0.323–0.518 | 5 |
| run n=1000 | hand C++ | 0.312 | 0.233–0.644 | 5 |
| run n=1000 | hand Go | 0.628 | 0.605–0.925 | 5 |
| run n=1000 | hand Java | 18.874 | 17.575–20.078 | 5 |
| run n=1000 | hand Rust | 0.493 | 0.308–0.631 | 5 |
| run n=1000 | ziran VM (saved) | 128.131 | 124.520–130.090 | 5 |
| run n=1000 | ziran VM (source) | 127.748 | 126.182–129.895 | 5 |
| run n=1000 | ziran c (saved) | 0.531 | 0.517–0.685 | 5 |
| run n=1000 | ziran c (source) | 0.783 | 0.593–0.832 | 5 |
| run n=1000 | ziran cpp (saved) | 0.570 | 0.475–1.003 | 5 |
| run n=1000 | ziran cpp (source) | 0.575 | 0.501–0.672 | 5 |
| run n=1000 | ziran go (saved) | 0.894 | 0.716–1.039 | 5 |
| run n=1000 | ziran go (source) | 0.787 | 0.728–1.013 | 5 |
| run n=1000 | ziran rust (saved) | 0.429 | 0.365–0.716 | 5 |
| run n=1000 | ziran rust (source) | 0.648 | 0.352–0.678 | 5 |
| run n=500000 | JavaScript | 141.106 | 135.665–143.081 | 5 |
| run n=500000 | Python | 5795.591 | 5650.491–5956.598 | 5 |
| run n=500000 | hand C | 12.573 | 12.373–13.173 | 5 |
| run n=500000 | hand C++ | 12.459 | 11.912–13.069 | 5 |
| run n=500000 | hand Go | 13.733 | 13.584–14.197 | 5 |
| run n=500000 | hand Java | 85.433 | 82.654–87.318 | 5 |
| run n=500000 | hand Rust | 13.376 | 12.845–13.922 | 5 |
| run n=500000 | ziran c (saved) | 122.451 | 117.954–124.004 | 5 |
| run n=500000 | ziran c (source) | 120.980 | 119.205–126.286 | 5 |
| run n=500000 | ziran cpp (saved) | 118.952 | 116.770–126.199 | 5 |
| run n=500000 | ziran cpp (source) | 121.684 | 120.571–124.343 | 5 |
| run n=500000 | ziran go (saved) | 84.382 | 81.048–85.198 | 5 |
| run n=500000 | ziran go (source) | 83.324 | 81.006–85.682 | 5 |
| run n=500000 | ziran rust (saved) | 16.700 | 14.570–18.286 | 5 |
| run n=500000 | ziran rust (source) | 17.578 | 14.480–18.236 | 5 |
| ziran | saved IR check | 0.577 | 0.569–0.657 | 3 |
| ziran | saved to c | 1.478 | 1.418–1.726 | 3 |
| ziran | saved to cpp | 1.070 | 1.016–1.256 | 3 |
| ziran | saved to go | 1.910 | 1.542–3.587 | 3 |
| ziran | saved to rust | 1.253 | 1.105–1.262 | 3 |
| ziran | saved to zib | 1.361 | 1.321–1.828 | 3 |
| ziran | source check | 1.521 | 1.507–1.543 | 3 |
| ziran | source to c | 1.724 | 1.638–1.785 | 3 |
| ziran | source to cpp | 1.894 | 1.726–1.955 | 3 |
| ziran | source to go | 2.429 | 2.342–6.064 | 3 |
| ziran | source to rust | 1.587 | 1.571–1.607 | 3 |
| ziran | source to saved IR | 1.085 | 1.071–1.143 | 3 |
| ziran | source to zib | 1.829 | 1.447–1.834 | 3 |

Expected checksums: {"1000": 33798843034, "500000": -2238818866076}

Generated source equality: {"c": true, "cpp": true, "go": true, "rust": true}

Source and saved-IR bundles are byte-identical.

Unavailable comparisons: none

- Fresh processes include startup, loading, printing, and shutdown.
- Go build uses a dedicated warm cache; it is not a forced recompilation.
- Each implementation updates a fixed 32-record table with wrapping s32 arithmetic.
- JavaScript uses BigInt for the sum to preserve exact checksums.
- No CPU isolation; medians are not regression thresholds.
- VM large run omitted because record/call execution exceeds its instruction budget.
- RSS from wait4 includes launcher accounting and is omitted from the table.
