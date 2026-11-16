# C teacher: native assembly comparison

Base revision: 96c23c7de6bc622225a916dfce18bff04486ba2b
Host: alpha
build/bin/zi2c SHA-256: 2e21cbdd222519dc2f54993721c5168719b0340fc00d4271c4ca101dd8300152
build/bin/zi2zx SHA-256: eb47203a4eef36585fcca75340e026e86b2fd34895b22c8dda342e127f956c00
build/bin/zx2native SHA-256: c1cf0932e45b017e8fbc90bb935966c4e67dbd4e700e22ecd52e0346597e1433
Learning off. Generated C compiled with -O2; native compiled from verified ZX. Function extents come from the native map; C symbols match module_name or exported name exactly. Absent symbols are not evidence of inlining. Metrics: instructions/loop-union/back-edges/stack-reads/stack-writes/memory/indexed/branches/forward-branches/calls/vector-register-uses/mul/div/shifts/loop-immediate-or-PC-relative candidates. Stack traffic includes locals and saves. Vector-register uses include scalar floating point. Loop constants are LICM candidates, not proven invariants. Back edges describe loop bodies, not execution frequency. SIGPROF uses a 1ms CPU timer, at most 30 runs; zero samples means unknown time share. Sampling includes process startup and runtime. Ranking weights relative instruction excess by observed function samples; it predicts neither cycles nor speedup.

## Sieve / cc
Native SHA-256: f8190c4ddf6088aee7d4a38e89b3ff80094393c754d79f0d40778ab31dc29a96
C SHA-256: 5f026028c7bbd24e984aec96b546a19feadd55679daf9ab28dcc4ea766fe0784
ZX SHA-256: df125c0d48de6389dcf2561e1a12b084d6ed84392cbd9fb826c24c44013e7a50
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 166/3

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 31/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 54..85: 8 instructions, 0 samples |
| wasm_tier_Sieve | 77/42/7/2/2/24/7/13/6/5/6/1/0/0/12 | 46/23/5/1/2/9/3/8/3/2/0/1/0/0/9 | 166 | 373..443: 16 instructions, 157 samples |

## Sieve / clang
Native SHA-256: f8190c4ddf6088aee7d4a38e89b3ff80094393c754d79f0d40778ab31dc29a96
C SHA-256: 910a60d1644991bdfc3aa15ca5c9251b2bfc6660cacea207f8d76363c4e6f65d
ZX SHA-256: df125c0d48de6389dcf2561e1a12b084d6ed84392cbd9fb826c24c44013e7a50
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 163/3

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 31/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 54..85: 8 instructions, 0 samples |
| wasm_tier_Sieve | 77/42/7/2/2/24/7/13/6/5/6/1/0/0/12 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 | 163 | 373..443: 16 instructions, 157 samples |

## Records / cc
Native SHA-256: a88af3ac1583d28eaad25518d40d5206599879c472b5e37e969ba0b3ff354e24
C SHA-256: 8679ca26bed0141e970c6c51c5cf0fcef1903c12458bd12eef4095e12b5d15eb
ZX SHA-256: ef16b8ef699b421e58e69b410074367ab582b3a091d1e4002cea5ae34acf1e28
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 159/4

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 31/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 54..85: 8 instructions, 0 samples |
| wasm_tier_Records | 904/117/4/260/520/806/4/25/21/5/0/5/2/0/25 | 80/59/4/1/1/20/3/7/3/1/12/2/0/2/12 | 159 | 5751..5922: 39 instructions, 155 samples |

## Records / clang
Native SHA-256: a88af3ac1583d28eaad25518d40d5206599879c472b5e37e969ba0b3ff354e24
C SHA-256: 0920384a04220eea8446363dc12b6c03ac25b5bca3ad579a022ad26a408745da
ZX SHA-256: ef16b8ef699b421e58e69b410074367ab582b3a091d1e4002cea5ae34acf1e28
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 158/4

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 31/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 54..85: 8 instructions, 0 samples |
| wasm_tier_Records | 904/117/4/260/520/806/4/25/21/5/0/5/2/0/25 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 | 158 | 5751..5922: 39 instructions, 152 samples |

## Hash / cc
Native SHA-256: c42e0ed1756d1c65012c4745b14e96cf9300d9946f64239a7dbd02c212e41454
C SHA-256: d4c655704cbf4473cd9abc03baabf51e3e94a93680372868e3d9ca77d78d5374
ZX SHA-256: 57b64bac7cd36c69c44d765fbafb41d44c55a5807f89fa282957f99f887bcdb0
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 165/3

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 32/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Hash | 50/46/4/0/0/3/2/9/5/3/0/1/1/0/12 | 26/12/2/0/0/4/1/2/0/0/0/4/0/1/4 | 165 | 166..258: 22 instructions, 158 samples |

## Hash / clang
Native SHA-256: c42e0ed1756d1c65012c4745b14e96cf9300d9946f64239a7dbd02c212e41454
C SHA-256: dddfab1cdc51eef0df1c8151992699e7c2dbe8a2b3a2f370537177c1e74d271b
ZX SHA-256: 57b64bac7cd36c69c44d765fbafb41d44c55a5807f89fa282957f99f887bcdb0
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 162/3

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 32/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Hash | 50/46/4/0/0/3/2/9/5/3/0/1/1/0/12 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 | 162 | 166..258: 22 instructions, 155 samples |

## Vectors / cc
Native SHA-256: 2a6499df0c82ef502200b0ee7cdb4fceb862d17d415c46e53f0ab77ecfaf83f5
C SHA-256: e78a12e2cd342cd1d4e154306f4d450e0af3ea9b11141e25b21605e0c125b035
ZX SHA-256: 196b5ccacd46bc2512922f4ff460ba3d5b880f5a0ace4bda93bda40502aadfda
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 195/4

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 32/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Vectors | 104/93/5/11/11/24/2/14/9/3/0/2/0/0/13 | 109/97/8/7/8/27/6/18/10/2/7/0/0/0/10 | 177 | 171..470: 79 instructions, 152 samples |

## Vectors / clang
Native SHA-256: 2a6499df0c82ef502200b0ee7cdb4fceb862d17d415c46e53f0ab77ecfaf83f5
C SHA-256: b1d2f674b3e19e1606cbcab0df96e862cf143b0a859f16fb962ed6908268f315
ZX SHA-256: 196b5ccacd46bc2512922f4ff460ba3d5b880f5a0ace4bda93bda40502aadfda
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 193/4

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 32/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Vectors | 104/93/5/11/11/24/2/14/9/3/0/2/0/0/13 | 123/101/10/3/3/22/15/17/7/2/26/1/0/1/13 | 164 | 171..470: 79 instructions, 145 samples |

## Calls / cc
Native SHA-256: 3b845219fe850756935b5034d51dc71eaa52a571adc606e138b02382b618fb02
C SHA-256: 922a9620bc815c5ac8291dc11ef01dedb4ba756529d458fd9a62019ded351d4c
ZX SHA-256: c27d356fbe90f7ceae6c72a8be3e9254579dd29bb57bcff5ece449e6eaae34e7
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Calls_main | 22/7/1/0/0/0/0/3/2/1/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 23..53: 7 instructions, 0 samples |
| wasm_tier_Calls | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 17/6/1/0/0/0/0/1/0/1/0/0/0/0/2 | 0 | none |
| wasm_tier_Fib | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 230/213/10/27/24/75/1/20/10/1/0/0/0/0/25 | 0 | none |

## Calls / clang
Native SHA-256: 3b845219fe850756935b5034d51dc71eaa52a571adc606e138b02382b618fb02
C SHA-256: cce08cbe37f03bd186e3ee19cab1b79fcebe7fd7f573a59fc4f573a22bd401fd
ZX SHA-256: c27d356fbe90f7ceae6c72a8be3e9254579dd29bb57bcff5ece449e6eaae34e7
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Calls_main | 22/7/1/0/0/0/0/3/2/1/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 23..53: 7 instructions, 0 samples |
| wasm_tier_Calls | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| wasm_tier_Fib | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 21/7/1/0/0/2/0/2/1/1/0/0/0/0/1 | 0 | none |

## Printing / cc
Native SHA-256: af25055dcf9896ba346d9a709503a100baa050dac53d7abdb6e9cfc3a262d8db
C SHA-256: f2d5a4ffb813abaa72568fa45a2da9c83de9f9f8689bea1f43ff301efba8e209
ZX SHA-256: 18a5c85c96a698300434e0ab76dee00954720f58a9e3fe3fb39026b8af07a97d
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 100/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Printing_main | 15/5/1/1/1/2/0/2/1/1/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 29..51: 5 instructions, 0 samples |
| wasm_tier_Printing | 59/44/1/5/5/12/0/5/4/7/0/1/1/0/9 | 32/15/1/0/0/4/1/1/0/1/0/2/0/0/5 | 39 | 96..260: 44 instructions, 39 samples |

## Printing / clang
Native SHA-256: af25055dcf9896ba346d9a709503a100baa050dac53d7abdb6e9cfc3a262d8db
C SHA-256: 0a9bc9d89962d93db0b50f31fcaf9a0c02d3d30801b715eeb4225576bd40e44f
ZX SHA-256: 18a5c85c96a698300434e0ab76dee00954720f58a9e3fe3fb39026b8af07a97d
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 99/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Printing_main | 15/5/1/1/1/2/0/2/1/1/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 29..51: 5 instructions, 0 samples |
| wasm_tier_Printing | 59/44/1/5/5/12/0/5/4/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 | 34 | 96..260: 44 instructions, 34 samples |

## Button / cc
Native SHA-256: d10f951da3d737c1de31b633d41a04ea6acf2a1085efdc8d9296354de9f981b2
C SHA-256: ec85279fb49515de632e0e7dac87b57e461b4af308943da2fb5c83557c326f02
ZX SHA-256: 8e2995b9f3256b2991b13ae1fca2db342b6d982648166b61d8d3d3439a0ad604
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 160/10

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 72..104: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 157/142/10/3/9/41/1/19/9/13/8/0/0/0/36 | 3 | 1154..1176: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 11/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 | 1 | none |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 14/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 1 | 4338..4360: 4 instructions, 1 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | 4797..4816: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 258/199/14/30/15/124/40/31/17/10/12/18/0/1/52 | 2 | none |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 | 0 | 7580..9139: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 9336..9355: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | 9598..9752: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 2 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 | 0 | 9827..9937: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 14 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 10843..10886: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 | 0 | 11880..14331: 524 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 | 1 | 16026..16285: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 | 0 | none |
| button_SubmitButton | 622/601/18/167/120/309/20/59/41/56/52/0/0/0/60 | 447/414/9/119/78/217/3/21/10/36/134/0/0/1/35 | 2 | 16464..19288: 586 instructions, 2 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 1 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 | 1 | none |
| paint_queue_PaintSnapshotEmit | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 | 0 | 20564..22612: 346 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 | 2 | 23621..24021: 76 instructions, 2 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 | 2 | 24801..24823: 4 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 201/158/14/35/13/71/4/27/13/13/41/2/0/1/16 | 0 | 27781..28842: 229 instructions, 0 samples |
| tree_input_TreeTextFocused | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 46/8/1/0/0/10/2/7/6/1/0/2/0/0/2 | 0 | 29426..29520: 22 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 8/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 | 0 | 30664..30680: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 31/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 | 0 | 30909..31228: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 93/18/2/4/2/23/8/12/10/2/0/4/0/2/2 | 0 | 31625..31717: 25 instructions, 0 samples |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 32088..32346: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 | 0 | none |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 | 0 | 32870..33136: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/1/0/0/4 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 | 0 | 34544..35176: 148 instructions, 0 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 | 5 | 36429..39372: 622 instructions, 1 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 59/24/2/1/1/11/1/8/6/3/0/2/0/0/5 | 0 | 39722..39777: 17 instructions, 0 samples |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 28/0/0/0/0/5/2/1/1/2/0/3/0/0/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/39/4/3/3/15/4/8/4/3/0/4/0/0/7 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 274/176/4/90/72/183/2/10/6/16/144/0/0/0/7 | 2 | 40803..40825: 4 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 41/0/0/10/1/11/0/4/4/2/7/2/0/2/0 | 2 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 263/223/22/59/30/100/3/41/19/5/145/0/0/0/13 | 2 | 42423..43721: 305 instructions, 1 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 1 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 377/294/13/41/19/86/4/61/48/25/89/0/0/0/41 | 203/183/15/35/11/67/5/31/14/1/113/0/0/0/10 | 0 | 44235..45541: 282 instructions, 0 samples |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/8/0/6/6/2/0/2/0/1/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 1 | none |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 916/893/51/266/138/459/11/100/43/43/383/2/0/8/84 | 6 | 48394..55037: 1343 instructions, 6 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 27/12/1/0/0/1/0/2/1/2/0/0/0/0/1 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 | 1 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 | 3 | 56238..56534: 58 instructions, 2 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 | 0 | 58784..59859: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 | 0 | 60842..62076: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 | 0 | 62544..62566: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 | 3 | 64344..64366: 4 instructions, 2 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 25/0/0/0/0/1/1/5/5/0/14/0/0/0/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 32/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 | 0 | 65429..66217: 211 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 421/373/45/30/47/105/9/89/44/1/2/0/0/10/98 | 0 | 66641..67872: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 | 3 | 68634..68885: 62 instructions, 3 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 78/18/2/2/2/19/8/5/3/2/0/4/0/2/2 | 2 | 69435..69618: 42 instructions, 1 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 | 0 | 69884..70213: 77 instructions, 0 samples |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 84/63/7/10/5/24/2/12/5/7/1/1/0/0/6 | 0 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 46/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 1 | 71066..71232: 33 instructions, 1 samples |
| style_sheet_ResolveInstalledStyleRules | 267/236/11/58/44/144/24/25/14/11/14/6/0/0/37 | 189/151/5/31/50/120/13/11/6/4/76/0/0/5/15 | 0 | 71614..72584: 184 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 227/179/12/38/30/96/7/27/15/16/39/0/0/0/25 | 3 | 73081..74149: 201 instructions, 3 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 46/31/1/8/1/10/0/3/2/4/0/0/0/0/3 | 0 | 74194..74351: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 35/16/2/0/0/5/3/5/3/0/0/0/0/2/5 | 2 | 74478..74640: 47 instructions, 2 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 11/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 19/0/0/0/0/0/0/1/1/0/16/0/0/0/0 | 0 | 74963..75141: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 176/152/5/4/13/19/0/28/23/9/58/0/0/0/14 | 74/38/3/0/0/5/2/9/6/0/45/0/0/0/0 | 1 | 75262..75884: 148 instructions, 1 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 27/0/0/0/0/3/2/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 37/23/3/5/2/14/1/7/4/2/8/0/0/0/3 | 0 | 76224..76246: 4 instructions, 0 samples |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/9/0/0/13 | 142/95/6/5/8/36/14/20/14/3/0/6/0/6/20 | 0 | 76699..76978: 69 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 | 1 | 77378..77688: 70 instructions, 1 samples |
| tree_TreeStoreLabel | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 | 0 | 77866..79471: 389 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 9/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 | 0 | 80989..81011: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 48/0/0/1/13/18/0/1/1/2/4/0/0/0/0 | 0 | 82203..82225: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 45/32/1/12/2/29/0/3/2/2/20/0/0/0/1 | 0 | 82723..82745: 4 instructions, 0 samples |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 45/22/2/2/1/10/2/6/2/0/28/0/0/0/2 | 0 | 82884..83130: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 28/12/1/1/1/6/1/3/2/2/0/0/0/0/2 | 0 | 83676..83698: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 16/0/0/0/0/1/0/2/2/0/9/0/0/0/0 | 0 | 83910..84068: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 99/53/4/0/0/0/0/11/7/7/34/0/0/0/7 | 23/15/1/0/0/6/1/3/2/0/16/0/0/0/0 | 0 | 84296..84468: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 240/176/5/37/33/79/0/26/21/9/108/0/0/0/12 | 124/89/8/2/0/14/3/17/9/1/77/0/0/0/1 | 1 | 84779..85519: 171 instructions, 1 samples |
| svg_icon_behavior_PaintSvgIcon | 371/316/4/71/71/157/3/27/23/22/113/4/1/0/36 | 225/195/7/30/25/84/9/17/8/8/91/1/0/4/21 | 0 | 85772..87194: 312 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 58/0/0/19/15/39/0/5/5/2/28/0/0/0/0 | 0 | 87463..87485: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 76/36/1/15/3/27/1/8/7/1/33/0/0/1/5 | 0 | 88127..88149: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 33/0/0/6/5/11/0/1/1/2/4/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 45/0/0/1/10/15/0/1/1/2/2/0/0/0/0 | 0 | 89554..89576: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 | 3 | 90765..90787: 4 instructions, 1 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 91554..91564: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 | 2 | 91748..91767: 4 instructions, 2 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 | 1 | 92345..92673: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 278/259/112/0/0/136/9/116/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 36/0/0/0/0/5/0/0/0/0/0/0/0/5/0 | 0 | 93566..93872: 79 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 85/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 148/0/0/59/14/82/0/15/15/0/70/0/0/0/0 | 0 | 95560..95582: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 | 4 | 95691..95710: 4 instructions, 4 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 55/0/0/0/0/1/1/1/1/0/0/10/0/12/0 | 0 | 95984..96645: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 115/84/6/20/13/54/2/13/7/5/32/0/0/0/10 | 0 | 97236..97255: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 24/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 69/52/5/10/4/30/6/9/4/2/27/0/0/0/3 | 0 | 98005..98164: 43 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 38/29/1/8/5/14/1/3/2/3/0/0/0/0/3 | 0 | 98232..98311: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/2/15/6/3/3/1/0/2/0/0/0 | 0 | none |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 1 | none |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 114/100/2/23/5/33/2/11/8/2/53/0/0/2/12 | 1 | 98835..98857: 4 instructions, 1 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 | 0 | 99459..99481: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 | 6 | 99665..103155: 715 instructions, 6 samples |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 119/99/2/24/6/35/0/11/8/2/59/0/0/2/11 | 0 | 103230..103252: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 4/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 123/104/6/4/12/41/10/10/4/3/4/1/0/1/23 | 0 | 103812..104256: 99 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 78/29/2/0/0/0/0/11/9/6/23/0/0/0/5 | 35/10/1/0/0/5/0/6/3/0/16/0/0/0/0 | 0 | 104806..104921: 28 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/93/12/13/9/3/0/16/1/0/22 | 125/61/5/6/4/55/20/11/6/2/5/2/0/3/10 | 0 | 105103..105314: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 9/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 105/77/2/20/34/60/0/4/2/3/26/0/0/0/8 | 0 | 106759..106781: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 83/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 108003..108598: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 | 2 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 | 0 | 109810..109832: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 | 25 | 122161..122183: 4 instructions, 2 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 21/0/0/3/0/7/0/0/0/0/18/0/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 | 0 | 123756..124445: 157 instructions, 0 samples |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 | 0 | 124595..125673: 254 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 | 0 | 126301..126320: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 | 0 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 | 0 | 127627..127886: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 19/6/1/0/0/5/2/2/1/0/0/1/0/0/1 | 0 | 127994..128052: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/27/2/3/4/24/5/4/2/1/5/0/0/1/8 | 0 | none |
| svg_shape_ShapeRaster | 261/220/9/32/17/69/6/47/38/15/17/2/0/0/40 | 127/89/5/5/4/35/7/17/12/4/9/1/0/0/18 | 0 | 128594..129413: 199 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 332/300/20/30/35/98/9/48/28/9/0/10/1/0/41 | 330/291/25/30/20/108/27/52/27/1/26/2/0/8/51 | 0 | 129905..130712: 211 instructions, 0 samples |
| image_behavior_ImageFitBounds | 204/184/8/18/9/31/0/26/18/13/53/0/0/0/18 | 92/77/6/6/2/15/0/16/10/0/56/0/0/0/7 | 0 | 130795..131489: 175 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 103/59/1/9/1/12/1/16/15/7/16/0/0/0/4 | 35/14/1/0/0/8/2/8/7/0/12/0/0/0/0 | 0 | 131596..131841: 59 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 131963..131985: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 17/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 132394..132416: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 1 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 | 0 | 133708..133786: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 134377..134396: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 5 | 134846..134865: 4 instructions, 3 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 5 | 135787..135806: 4 instructions, 2 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 | 18 | 137568..137587: 4 instructions, 5 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 | 2 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 5 | 138360..138376: 4 instructions, 3 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 22/0/0/2/0/6/0/0/0/0/18/0/0/0/0 | 1 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 | 0 | 138755..138946: 43 instructions, 0 samples |
| paint_queue_FillShapeRows | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 | 0 | 139023..140019: 233 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 | 0 | 140153..140530: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 194/165/10/21/26/76/11/29/19/5/21/4/0/1/22 | 0 | 141558..142113: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 349/310/21/56/16/91/15/62/41/19/60/1/0/0/48 | 162/151/12/0/0/48/26/32/20/0/21/0/0/0/24 | 0 | 142412..143577: 276 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 614/592/44/147/107/273/7/91/47/35/160/0/0/0/51 | 447/416/39/36/21/126/22/74/35/14/90/0/0/0/87 | 0 | 143844..146127: 506 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 334/315/14/23/20/72/15/49/35/14/64/3/0/0/51 | 222/201/15/15/13/79/21/37/21/2/47/0/0/1/30 | 0 | 146899..147912: 246 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 20/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 40/0/0/4/3/11/0/2/0/1/16/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/0/0/11/0/0/1/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 | 1 | 149275..149320: 12 instructions, 1 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/3/0/0/0/0/5/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 | 1 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 1 | 149922..150011: 21 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 0 | 150229..150318: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 | 0 | 150546..150635: 21 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 8/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 15/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 139/123/3/0/0/16/4/16/13/11/40/0/0/0/22 | 110/13/2/0/0/21/11/13/7/0/42/0/0/2/2 | 0 | 151153..151630: 122 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 151752..151870: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 | 3 | 151981..153943: 411 instructions, 3 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 154075..154122: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 | 0 | 154417..155305: 188 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 155437..155484: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 | 0 | 155779..156667: 188 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 156799..156846: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 614/531/11/90/49/179/20/57/46/38/279/0/3/0/62 | 367/340/21/19/10/94/24/47/25/7/185/0/3/0/38 | 0 | 157289..159629: 506 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 6/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | 159811..159934: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 71/67/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/19/7/7/4/0/14/0/0/0/4 | 0 | 159972..160203: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 53/35/3/0/0/13/5/8/5/0/19/0/0/1/10 | 0 | 160288..160494: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 21/6/1/0/0/4/0/2/1/0/12/0/0/0/1 | 0 | 160617..160735: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 11/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 10/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 9/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 20/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 161546..161562: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 161921..161937: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 102/87/3/4/5/27/12/13/10/4/27/0/2/0/14 | 95/68/5/0/0/27/19/15/10/0/23/0/2/2/13 | 0 | 162121..162319: 47 instructions, 0 samples |

## Button / clang
Native SHA-256: d10f951da3d737c1de31b633d41a04ea6acf2a1085efdc8d9296354de9f981b2
C SHA-256: 9484f192b9f17b2d12cbe771349c638d9eb8f3747d02d162729d052651eb055f
ZX SHA-256: 8e2995b9f3256b2991b13ae1fca2db342b6d982648166b61d8d3d3439a0ad604
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 160/10

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 72..104: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 141/0/0/3/5/37/0/12/12/13/8/0/0/0/0 | 2 | 1154..1176: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 | 0 | none |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 4338..4360: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 4797..4816: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 210/110/2/8/1/65/22/28/26/7/18/6/0/4/23 | 0 | none |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 | 0 | 7580..9139: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 9336..9355: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 9598..9752: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 1 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 9827..9937: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 13 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 10843..10886: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 373/232/17/41/14/130/43/58/41/21/26/5/0/4/33 | 0 | 11880..14331: 524 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 2 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 16026..16285: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| button_SubmitButton | 622/601/18/167/120/309/20/59/41/56/52/0/0/0/60 | 376/10/1/57/34/129/0/17/16/38/33/2/0/3/1 | 5 | 16464..19288: 586 instructions, 5 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 1 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 | 0 | 20564..22612: 346 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 148/83/4/6/2/51/14/12/8/12/6/1/0/6/13 | 3 | 23621..24021: 76 instructions, 2 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 1 | 24801..24823: 4 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 0 | 27781..28842: 229 instructions, 0 samples |
| tree_input_TreeTextFocused | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 0 | 29426..29520: 22 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 1 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 30664..30680: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 118/103/2/0/0/11/3/22/20/0/0/0/0/3/32 | 0 | 30909..31228: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 | 0 | 31625..31717: 25 instructions, 0 samples |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 32088..32346: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 1 | none |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 32870..33136: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 | 1 | 34358..34615: 62 instructions, 1 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 | 1 | 36429..39372: 622 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 0 | 39722..39777: 17 instructions, 0 samples |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 1 | 40853..40872: 4 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 1 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/3/0/8/0/4/4/2/5/2/0/1/0 | 2 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 | 1 | none |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 190/0/0/19/10/49/0/23/23/2/120/0/0/0/0 | 3 | 42423..43721: 305 instructions, 3 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 377/294/13/41/19/86/4/61/48/25/89/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/1 | 0 | 44235..45541: 282 instructions, 0 samples |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 2 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 793/0/0/161/116/384/0/54/54/41/331/4/0/7/0 | 5 | 48394..55037: 1343 instructions, 5 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 1 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 0 | 56238..56534: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 58784..59859: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 60842..62076: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 62544..62566: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 7 | 64344..64366: 4 instructions, 4 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 65429..66217: 211 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 | 0 | 66641..67872: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 | 4 | 68634..68885: 62 instructions, 4 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 69435..69618: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 | 2 | 69884..70213: 77 instructions, 1 samples |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 81/12/1/6/0/13/3/12/11/6/5/1/0/2/3 | 2 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 71066..71232: 33 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 267/236/11/58/44/144/24/25/14/11/14/6/0/0/37 | 253/196/17/22/19/107/14/33/16/4/0/0/0/12/36 | 0 | 71614..72584: 184 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 5 | 73081..74149: 201 instructions, 3 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 74194..74351: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 | 2 | 74478..74640: 47 instructions, 2 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 74963..75141: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 176/152/5/4/13/19/0/28/23/9/58/0/0/0/14 | 44/0/0/0/0/1/0/8/8/0/29/0/0/0/0 | 3 | 75262..75884: 148 instructions, 3 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 1 | 76224..76246: 4 instructions, 1 samples |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/9/0/0/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 0 | 76699..76978: 69 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 0 | 77378..77688: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 | 1 | 77866..79471: 389 instructions, 1 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 3 | 80989..81011: 4 instructions, 1 samples |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 0 | 82203..82225: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 0 | 82723..82745: 4 instructions, 0 samples |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 27/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 82884..83130: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 83676..83698: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 1 | 83910..84068: 42 instructions, 1 samples |
| button_behavior_ButtonLabeledPictureSize | 99/53/4/0/0/0/0/11/7/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 84296..84468: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 240/176/5/37/33/79/0/26/21/9/108/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/87/0/0/0/0 | 0 | 84779..85519: 171 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 371/316/4/71/71/157/3/27/23/22/113/4/1/0/36 | 246/35/1/23/20/69/6/17/16/6/128/1/0/1/3 | 0 | 85772..87194: 312 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 87463..87485: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 88127..88149: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 89554..89576: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 0 | 90765..90787: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 91554..91564: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 2 | 91694..91713: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 1 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 1 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 1 | 92345..92673: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 93566..93872: 79 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 95560..95582: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 2 | 95691..95710: 4 instructions, 1 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 95984..96645: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 73/0/0/6/0/29/1/9/8/3/12/0/0/0/0 | 0 | 97236..97255: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 98005..98164: 43 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 98232..98311: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/0/14/7/3/3/2/0/1/0/0/0 | 0 | none |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 1 | none |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 1 | 98835..98857: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 99459..99481: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 | 3 | 99665..103155: 715 instructions, 3 samples |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 103230..103252: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 164/41/4/6/7/61/19/20/16/3/10/5/0/0/4 | 0 | 103812..104256: 99 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 78/29/2/0/0/0/0/11/9/6/23/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 104806..104921: 28 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/93/12/13/9/3/0/16/1/0/22 | 192/156/12/7/3/67/28/24/12/1/9/2/0/10/39 | 0 | 105103..105314: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 106759..106781: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 108003..108598: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 2 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 109810..109832: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 | 25 | 121838..121857: 4 instructions, 2 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 | 0 | 123756..124445: 157 instructions, 0 samples |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 | 0 | 124595..125673: 254 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 126301..126320: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 2 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 | 1 | 127512..127531: 4 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 47/22/2/0/0/6/5/6/4/0/0/5/0/0/1 | 0 | 127994..128052: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 261/220/9/32/17/69/6/47/38/15/17/2/0/0/40 | 129/113/4/3/3/27/9/17/13/4/8/1/0/0/21 | 0 | 128594..129413: 199 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 332/300/20/30/35/98/9/48/28/9/0/10/1/0/41 | 372/321/22/23/18/117/47/58/36/1/32/4/0/4/51 | 0 | 129905..130712: 211 instructions, 0 samples |
| image_behavior_ImageFitBounds | 204/184/8/18/9/31/0/26/18/13/53/0/0/0/18 | 43/0/0/0/0/3/0/9/9/0/30/0/0/0/0 | 0 | 130795..131489: 175 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 103/59/1/9/1/12/1/16/15/7/16/0/0/0/4 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 131596..131841: 59 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 131963..131985: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 132394..132416: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 133708..133786: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 134377..134396: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 11 | 134846..134865: 4 instructions, 3 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 5 | 135787..135806: 4 instructions, 1 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 12 | 137568..137587: 4 instructions, 1 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 3 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 3 | 138360..138376: 4 instructions, 2 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 138755..138946: 43 instructions, 0 samples |
| paint_queue_FillShapeRows | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 324/262/6/37/24/70/2/22/16/2/285/0/0/0/2 | 0 | 139023..140019: 233 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 140153..140530: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 141558..142113: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 349/310/21/56/16/91/15/62/41/19/60/1/0/0/48 | 179/142/8/0/0/31/14/38/30/0/33/0/0/0/25 | 0 | 142412..143577: 276 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 614/592/44/147/107/273/7/91/47/35/160/0/0/0/51 | 659/625/33/85/27/229/45/103/69/15/134/0/0/3/82 | 0 | 143844..146127: 506 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 334/315/14/23/20/72/15/49/35/14/64/3/0/0/51 | 192/158/16/10/9/58/22/28/12/3/45/0/0/3/12 | 0 | 146899..147912: 246 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 1 | 149275..149320: 12 instructions, 1 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 0 | 149922..150011: 21 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 0 | 150229..150318: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 106/18/2/0/0/18/7/14/12/4/0/0/0/4/1 | 0 | 150546..150635: 21 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 139/123/3/0/0/16/4/16/13/11/40/0/0/0/22 | 91/16/2/0/0/22/17/14/12/1/39/0/0/0/1 | 0 | 151153..151630: 122 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 151752..151870: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 4 | 151981..153943: 411 instructions, 4 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 154075..154122: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 154417..155305: 188 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 155437..155484: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 155779..156667: 188 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 156799..156846: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 614/531/11/90/49/179/20/57/46/38/279/0/3/0/62 | 575/417/24/43/25/189/49/61/37/8/319/0/3/10/72 | 0 | 157289..159629: 506 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 61/27/3/0/0/26/17/10/7/0/18/0/0/2/5 | 0 | 159811..159934: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 71/67/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 159972..160203: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 160288..160494: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 160617..160735: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 161546..161562: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 161921..161937: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 102/87/3/4/5/27/12/13/10/4/27/0/2/0/14 | 133/55/6/0/0/47/34/24/18/0/46/0/0/2/8 | 0 | 162121..162319: 47 instructions, 0 samples |

## Text / cc
Native SHA-256: 93bdf4a11da853a4aeaa057aa9f64f76816e2c5415efeb17847cc6f719fc13e5
C SHA-256: 3a7ef319b774a9e97fad5e99c8231dc96788d5ad7596b3f0801d747ef467ed2e
ZX SHA-256: 08137dc7ebf665c46aee733d2a81195d11bc08eaa9fe27c54aab1a5d1061ba47
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 157/9

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 52..84: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 279/155/5/34/82/140/3/25/20/16/47/0/0/0/27 | 143/127/10/15/19/63/2/17/7/8/26/0/0/1/29 | 3 | 895..1535: 149 instructions, 2 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 11/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 | 2 | none |
| text___zi_spec_6a19f7c98e79a734 | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 6/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 1 | 3077..3099: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | 3551..3570: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 | 0 | 4373..5932: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 6129..6148: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | 6391..6545: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 | 0 | 6620..6730: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 13 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 7636..7679: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 | 0 | 8673..11124: 524 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 | 1 | 12819..13078: 61 instructions, 1 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 | 0 | none |
| text_DrawSession | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 301/266/10/103/83/191/2/16/6/18/155/0/0/0/15 | 0 | 14388..15013: 121 instructions, 0 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 | 0 | none |
| paint_queue_PaintSnapshotEmit | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 | 0 | 15998..18046: 346 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 | 2 | 19055..19455: 76 instructions, 2 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 | 1 | 20235..20257: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 31/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 | 0 | 22977..23296: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 92/18/2/4/2/22/7/12/10/2/0/4/0/2/2 | 0 | 23693..23785: 25 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 | 0 | none |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 24536..24794: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 | 0 | 25669..25685: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 | 0 | 25980..26246: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/0/0/1/4 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 | 3 | 27654..28286: 148 instructions, 1 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 | 1 | 29539..32482: 622 instructions, 0 samples |
| std_math_Finite32 | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 12/0/0/0/0/3/0/2/2/0/4/0/0/0/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 70/56/1/12/15/33/0/3/2/3/14/0/0/2/13 | 0 | 33058..33080: 4 instructions, 0 samples |
| text_PrepareText | 264/82/2/62/65/169/0/13/11/15/16/0/0/0/2 | 167/85/3/36/22/65/2/7/4/14/32/0/0/3/4 | 4 | 34238..34438: 47 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 1 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 46/0/0/3/3/12/3/4/3/1/4/2/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 200/103/6/48/72/124/1/11/5/6/91/0/0/0/6 | 88/29/1/9/14/27/0/2/1/3/59/0/0/0/4 | 5 | 35153..35451: 58 instructions, 2 samples |
| text_TextOrigin | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 5/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 68/0/0/2/2/13/1/8/8/3/0/1/0/1/0 | 0 | 37047..37336: 74 instructions, 0 samples |
| text_TextHiddenLines | 119/40/2/11/2/22/0/11/9/10/22/0/0/0/4 | 76/8/1/13/5/24/3/7/2/2/22/0/0/0/2 | 1 | 37716..37809: 28 instructions, 1 samples |
| tree_TreeSetHiddenLines | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 | 0 | none |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 49/0/0/16/16/36/0/1/1/1/30/0/0/0/0 | 0 | 38228..38250: 4 instructions, 0 samples |
| text_PaintTextLines | 695/613/7/184/149/360/10/49/42/45/86/0/0/0/42 | 556/471/12/192/123/338/6/35/13/15/207/0/0/6/34 | 1 | 38750..41951: 610 instructions, 1 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 1 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 | 1 | 42995..43291: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 | 1 | 45541..46616: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 | 0 | 47599..48833: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 | 3 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 | 1 | 49301..49323: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 | 3 | 49823..49845: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 | 4 | 51599..52387: 211 instructions, 4 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 421/373/45/30/47/105/9/89/44/1/2/0/0/10/98 | 0 | 52811..54042: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 3 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 | 0 | 54984..55474: 123 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 75/18/2/2/2/16/6/5/3/2/0/4/0/2/2 | 0 | 55605..55788: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 | 1 | 56054..56383: 77 instructions, 0 samples |
| text_DefaultTextFace | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 14/0/0/0/0/7/0/0/0/1/0/0/0/0/0 | 1 | 56762..56781: 4 instructions, 0 samples |
| text_TextFaceFor | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 51/0/0/11/8/23/0/1/1/2/10/0/0/2/0 | 1 | 57104..57123: 4 instructions, 1 samples |
| text_behavior_ResolveTextStyle | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 78/56/6/8/3/19/2/14/6/1/11/0/0/0/3 | 0 | 57405..57792: 100 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 15/0/0/0/0/4/0/3/1/0/7/0/0/0/0 | 1 | 58815..58920: 27 instructions, 1 samples |
| text_behavior_TextWrapPolicy | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 1 | none |
| text_layout_NextTextLine | 670/608/18/130/245/440/16/51/33/27/19/12/1/0/57 | 265/228/27/12/8/98/40/43/15/3/11/2/0/16/37 | 0 | 59371..62539: 573 instructions, 0 samples |
| text_behavior_TextExtent | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 14/0/0/0/0/0/0/2/2/0/0/2/0/0/0 | 0 | 62735..62805: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| geometry_WorldToScreen | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 39/0/0/9/4/13/0/0/0/2/33/0/0/0/0 | 0 | none |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 | 2 | 63608..63918: 70 instructions, 2 samples |
| tree_TreeStoreLabel | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 | 3 | 64096..65701: 389 instructions, 2 samples |
| text_behavior_TextAlignmentOffset | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 16/0/0/0/0/2/1/2/2/0/7/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 27/8/1/0/0/1/1/4/3/1/0/0/0/0/1 | 0 | 65919..65978: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2 | none |
| text_TextEllipsisStart | 95/84/5/23/14/42/1/10/5/4/0/0/0/0/11 | 60/41/4/2/2/11/5/9/5/1/0/0/0/0/6 | 0 | 66179..66504: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 16/0/0/0/0/1/1/1/1/0/0/0/0/2/0 | 1 | none |
| text_PaintTextLineRuns | 339/321/6/109/109/237/12/13/7/13/36/0/0/0/14 | 277/229/5/89/51/149/4/9/4/11/97/2/0/4/22 | 3 | 66713..68457: 303 instructions, 3 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 | 4 | 69285..69307: 4 instructions, 0 samples |
| text_PaintTextEmphasis | 224/205/10/69/59/145/7/19/9/11/7/0/0/0/13 | 236/186/5/58/38/108/7/15/10/6/37/2/0/7/21 | 0 | 69967..70863: 169 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 25/0/0/0/0/2/0/0/0/0/10/0/0/2/0 | 0 | 71016..71084: 17 instructions, 0 samples |
| text_PaintTextRule | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 134/0/0/28/29/58/0/4/2/6/68/0/0/3/0 | 0 | 71255..71277: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 72500..72510: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 | 3 | 72640..72659: 4 instructions, 2 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 | 0 | 73291..73619: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 278/259/112/0/0/136/9/116/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 32/12/1/6/4/16/1/3/2/2/8/0/0/0/1 | 0 | 74468..74490: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 45/32/1/12/2/29/0/3/2/2/20/0/0/0/1 | 0 | 74753..74775: 4 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 309/277/8/40/17/70/5/28/20/6/12/0/0/13/28 | 6 | 75381..76506: 280 instructions, 6 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 16/6/1/0/0/2/0/2/1/0/0/1/0/0/1 | 0 | 76545..76597: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 24/10/4/1/0/10/1/7/3/0/1/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 246/138/4/0/0/0/0/21/17/12/99/0/0/0/27 | 95/49/3/0/0/28/1/15/10/0/55/0/0/0/0 | 4 | 77339..77954: 135 instructions, 0 samples |
| std_math_CosineDegrees | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/1/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/2/15/6/3/3/1/0/2/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/3/1/5/3/0/0/0/0/2/5 | 22 | 78447..78609: 47 instructions, 22 samples |
| text_TextLinkRunAt | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 84/58/2/4/0/10/6/10/8/0/0/0/0/3/7 | 0 | 78704..78832: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 22/0/0/0/0/1/0/0/0/3/0/2/0/4/0 | 0 | none |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 | 1 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 | 0 | 80443..80465: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 | 4 | 80730..84220: 715 instructions, 4 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 34/10/2/0/0/3/2/3/1/0/0/0/0/2/3 | 0 | none |
| paint_queue_PaintClippedEmphasis | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 52/0/0/5/17/25/0/1/1/2/6/0/0/0/0 | 0 | 85306..85328: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 | 0 | 86523..86545: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 43/0/0/0/7/10/0/0/0/1/17/0/0/0/0 | 0 | 88043..88065: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 | 1 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 | 1 | 98746..98765: 4 instructions, 1 samples |
| text_layout_TextTokenNext | 300/277/12/43/22/80/6/60/48/27/0/0/0/0/45 | 253/234/27/2/3/43/10/61/34/6/0/0/0/4/54 | 5 | 99072..100166: 269 instructions, 5 samples |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 100359..100515: 42 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 | 0 | 101763..101841: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 | 0 | 102189..103267: 254 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 | 0 | 103932..103951: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 | 2 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 | 1 | 105071..105090: 4 instructions, 1 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 114/100/2/23/5/33/2/11/8/2/53/0/0/2/12 | 0 | 105562..105584: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 | 0 | 106186..106208: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 31/0/0/0/4/11/0/0/0/1/0/0/0/0/0 | 0 | 107021..107043: 4 instructions, 0 samples |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 | 6 | 108944..108966: 4 instructions, 2 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 110/97/14/0/0/21/5/28/14/5/0/0/0/0/20 | 1 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 | 1 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 | 0 | 110581..110626: 12 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 191/162/10/21/26/74/8/29/19/5/21/4/0/1/22 | 0 | 111641..112196: 127 instructions, 0 samples |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 | 0 | 112948..113637: 157 instructions, 0 samples |
| shape_paint_EmitShape | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 28/0/0/4/0/8/0/11/7/0/3/0/0/0/0 | 0 | 113851..113873: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 12/0/0/4/0/5/0/0/0/1/0/0/0/0/0 | 0 | 114296..114318: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 114855..114874: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 2 | 115324..115343: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 1 | 116265..116284: 4 instructions, 1 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 | 2 | 118046..118065: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 | 1 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 1 | 118838..118854: 4 instructions, 0 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 25/8/2/0/0/6/0/4/2/0/0/0/0/0/2 | 6 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 115/105/18/0/0/13/2/28/10/0/0/0/0/0/37 | 0 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 37/19/2/0/0/4/1/7/5/0/0/0/0/0/6 | 1 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 2 | 121807..121896: 21 instructions, 2 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 0 | 122114..122203: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 | 0 | 122431..122520: 21 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 | 0 | 122852..123043: 43 instructions, 0 samples |
| paint_queue_FillShapeRows | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 | 0 | 123120..124116: 233 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 | 0 | 124250..124627: 96 instructions, 0 samples |
| shape_paint_EmitLineShape | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 110/47/2/20/1/45/7/7/5/6/44/0/0/2/5 | 0 | 124712..124734: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 223/80/4/50/41/93/6/18/14/17/61/0/0/0/10 | 185/132/10/28/16/70/2/21/11/8/60/0/0/3/15 | 0 | 126026..126369: 72 instructions, 0 samples |
| shape_paint_EmitRingShape | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 111/84/9/20/6/42/1/14/5/8/45/0/0/0/9 | 0 | 127259..127437: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 38/12/1/7/0/19/1/3/2/2/9/0/0/0/3 | 0 | 127484..127506: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 | 4 | 127966..129928: 411 instructions, 4 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 130060..130107: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 | 0 | 130402..131290: 188 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 131422..131469: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 | 0 | 131764..132652: 188 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 132784..132831: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 | 0 | none |
| shape_paint_ShapeAt | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 6/0/0/1/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 89/0/0/11/9/25/0/5/5/3/65/0/0/0/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 8/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/11/2/0/0/6/0/4/2/0/15/0/0/0/1 | 0 | none |
| shape_paint_PointsBounds | 128/113/5/20/18/46/1/11/6/5/49/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 | 0 | 134794..135148: 89 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 376/47/6/82/69/202/16/34/28/28/74/0/0/0/8 | 234/186/9/42/39/91/4/18/8/13/120/0/0/2/11 | 0 | 136531..136656: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 20/0/0/0/0/7/1/2/2/1/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcDirections | 68/28/1/10/12/30/3/4/3/3/25/0/0/0/2 | 51/17/1/3/4/14/3/3/2/2/20/0/0/0/1 | 0 | 137249..137373: 28 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 74/14/1/1/1/4/1/2/1/2/3/2/0/9/3 | 0 | 137541..137982: 116 instructions, 0 samples |
| paint_queue_GradientColorAt | 71/53/2/13/10/24/0/5/3/6/17/0/0/0/4 | 153/122/12/0/0/24/9/29/13/0/73/0/0/6/14 | 0 | 138104..138308: 51 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 42/29/1/9/8/19/0/2/1/3/5/0/0/0/2 | 36/15/1/3/3/11/2/2/1/1/17/0/0/0/1 | 0 | 138350..138466: 29 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 66/53/1/17/14/35/0/2/1/6/9/0/0/0/3 | 59/33/1/7/5/17/2/2/1/2/35/0/0/0/1 | 0 | 138509..138726: 53 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 138853..138971: 32 instructions, 0 samples |
| std_math_Sqrt | 161/101/4/7/3/11/0/20/16/10/47/0/2/0/16 | 102/80/5/0/0/26/4/17/11/0/57/0/0/1/9 | 0 | 139210..139604: 97 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 105/68/1/11/6/41/5/9/8/6/12/0/0/0/12 | 84/39/3/8/8/32/6/8/5/2/44/0/0/0/6 | 0 | 139791..140045: 68 instructions, 0 samples |
| shape_paint_ShapeClipSide | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 285/259/23/14/8/69/14/61/38/1/117/0/0/0/22 | 0 | 140384..140699: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 140990..140999: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/2/0/0/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 34/0/0/0/0/7/3/4/3/0/17/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcPoint | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 12/0/0/0/0/2/1/0/0/0/8/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| std_math_Finite64 | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 12/0/0/0/0/3/0/2/2/0/4/0/0/0/0 | 0 | none |
| std_math_MathSplit | 151/142/9/3/5/10/0/18/9/7/36/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 141759..142330: 127 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 106/96/7/9/11/20/0/19/12/6/34/0/0/0/8 | 75/48/7/0/0/15/5/17/10/0/21/0/0/0/6 | 0 | 142432..142779: 85 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/15/0/0/0/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 23/0/0/0/0/3/2/3/3/0/5/0/0/0/0 | 0 | none |
| shape_paint_ShapeCrossing | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 44/22/1/0/0/3/1/6/5/0/31/0/0/0/1 | 0 | none |
| widget_benchmark_host_RasterTriangles | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/1/0/6/0/0/1/2 | 0 | 143502..143637: 34 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 143684..143786: 24 instructions, 0 samples |

## Text / clang
Native SHA-256: 93bdf4a11da853a4aeaa057aa9f64f76816e2c5415efeb17847cc6f719fc13e5
C SHA-256: 83c05852115ae97404c04e04863107bd567438b264aa9639cd94058a515d3932
ZX SHA-256: 08137dc7ebf665c46aee733d2a81195d11bc08eaa9fe27c54aab1a5d1061ba47
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 155/9

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 52..84: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 279/155/5/34/82/140/3/25/20/16/47/0/0/0/27 | 167/53/3/6/15/56/0/18/15/12/37/0/0/1/14 | 2 | 895..1535: 149 instructions, 2 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 | 0 | none |
| text___zi_spec_6a19f7c98e79a734 | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | 3077..3099: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3551..3570: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 | 0 | 4373..5932: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 6129..6148: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 6391..6545: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 6620..6730: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 15 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 7636..7679: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 374/232/17/41/14/130/42/58/41/21/26/4/0/5/33 | 0 | 8673..11124: 524 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 12819..13078: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| text_DrawSession | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 300/20/2/37/35/102/1/25/23/20/115/1/0/2/2 | 2 | 13567..13586: 4 instructions, 1 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 | 0 | 15998..18046: 346 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 148/83/4/6/2/51/14/12/8/12/6/1/0/6/13 | 3 | 19055..19455: 76 instructions, 3 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 1 | 20235..20257: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 118/103/2/0/0/11/3/22/20/0/0/0/0/3/32 | 0 | 22977..23296: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 | 0 | 23693..23785: 25 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 24536..24794: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 25669..25685: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 25980..26246: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 | 1 | 27654..28286: 148 instructions, 1 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 | 3 | 29539..32482: 622 instructions, 2 samples |
| std_math_Finite32 | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 | 0 | none |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 48/0/0/12/9/28/0/2/2/2/12/0/0/1/0 | 1 | 33058..33080: 4 instructions, 1 samples |
| text_PrepareText | 264/82/2/62/65/169/0/13/11/15/16/0/0/0/2 | 151/22/1/19/7/52/1/6/5/14/18/0/0/3/3 | 1 | 34238..34438: 47 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 200/103/6/48/72/124/1/11/5/6/91/0/0/0/6 | 116/0/0/28/17/47/0/0/0/10/103/0/0/0/0 | 2 | 35278..35659: 72 instructions, 1 samples |
| text_TextOrigin | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 1 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 1 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 1 | none |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 0 | 37047..37336: 74 instructions, 0 samples |
| text_TextHiddenLines | 119/40/2/11/2/22/0/11/9/10/22/0/0/0/4 | 116/20/2/2/1/18/1/11/9/3/55/1/0/0/2 | 0 | 37716..37809: 28 instructions, 0 samples |
| tree_TreeSetHiddenLines | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 | 0 | 38228..38250: 4 instructions, 0 samples |
| text_PaintTextLines | 695/613/7/184/149/360/10/49/42/45/86/0/0/0/42 | 519/378/9/116/83/259/7/34/25/23/188/0/0/10/38 | 1 | 38750..41951: 610 instructions, 1 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 1 | 42995..43291: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 45541..46616: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 47599..48833: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 1 | 49447..49469: 4 instructions, 1 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 6 | 49720..49742: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 9 | 51599..52387: 211 instructions, 6 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 | 0 | 52811..54042: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 1 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 | 1 | 54804..55055: 62 instructions, 1 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 55605..55788: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 | 0 | 56054..56383: 77 instructions, 0 samples |
| text_DefaultTextFace | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 | 0 | 56762..56781: 4 instructions, 0 samples |
| text_TextFaceFor | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 | 1 | 57104..57123: 4 instructions, 1 samples |
| text_behavior_ResolveTextStyle | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 56/0/0/5/0/8/0/3/3/2/7/0/0/2/0 | 0 | 57405..57792: 100 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 1 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 1 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 1 | 58815..58920: 27 instructions, 1 samples |
| text_behavior_TextWrapPolicy | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 670/608/18/130/245/440/16/51/33/27/19/12/1/0/57 | 348/109/17/13/6/102/41/57/40/4/17/10/0/13/13 | 2 | 59371..62539: 573 instructions, 0 samples |
| text_behavior_TextExtent | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 | 0 | 62735..62805: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| geometry_WorldToScreen | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 | 3 | none |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 0 | 63608..63918: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 | 1 | 64096..65701: 389 instructions, 1 samples |
| text_behavior_TextAlignmentOffset | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 | 0 | 65919..65978: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_TextEllipsisStart | 95/84/5/23/14/42/1/10/5/4/0/0/0/0/11 | 53/21/3/2/2/10/3/7/4/1/0/0/0/0/2 | 0 | 66179..66504: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 | 0 | none |
| text_PaintTextLineRuns | 339/321/6/109/109/237/12/13/7/13/36/0/0/0/14 | 312/241/14/78/50/161/9/25/11/16/93/2/0/2/18 | 0 | 66713..68457: 303 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 4 | 69285..69307: 4 instructions, 1 samples |
| text_PaintTextEmphasis | 224/205/10/69/59/145/7/19/9/11/7/0/0/0/13 | 224/156/17/35/33/97/13/30/13/5/14/2/0/3/17 | 0 | 69967..70863: 169 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 | 0 | 71016..71084: 17 instructions, 0 samples |
| text_PaintTextRule | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 102/0/0/26/21/52/0/4/2/6/53/0/0/0/0 | 0 | 71255..71277: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 72500..72510: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 2 | 72640..72659: 4 instructions, 2 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 1 | 73291..73619: 82 instructions, 1 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 | 0 | 74468..74490: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 0 | 74753..74775: 4 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 1 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 | 4 | 75381..76506: 280 instructions, 4 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 45/22/2/0/0/7/5/6/4/0/0/5/0/0/1 | 0 | 76545..76597: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 246/138/4/0/0/0/0/21/17/12/99/0/0/0/27 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 | 7 | 77339..77954: 135 instructions, 0 samples |
| std_math_CosineDegrees | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 80/0/0/0/0/24/0/12/12/1/63/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/0/14/7/3/3/2/0/1/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 | 21 | 78347..78515: 43 instructions, 21 samples |
| text_TextLinkRunAt | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 51/23/7/2/0/9/4/14/7/0/0/0/0/0/2 | 0 | 78704..78832: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 | 0 | none |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 2 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 80443..80465: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 | 4 | 80730..84220: 715 instructions, 4 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 25/0/0/0/0/2/2/3/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_PaintClippedEmphasis | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 | 0 | 85306..85328: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 86523..86545: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 | 0 | 88043..88065: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 1 | 98746..98765: 4 instructions, 1 samples |
| text_layout_TextTokenNext | 300/277/12/43/22/80/6/60/48/27/0/0/0/0/45 | 209/116/20/4/2/22/7/55/35/3/0/0/0/3/34 | 9 | 99072..100166: 269 instructions, 8 samples |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 100359..100515: 42 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 101763..101841: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 2 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 | 0 | 102189..103267: 254 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 103932..103951: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 1 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 | 0 | 105258..105517: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 105562..105584: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 106186..106208: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 | 0 | 107021..107043: 4 instructions, 0 samples |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 | 6 | 108865..108887: 4 instructions, 1 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 1 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 82/46/12/0/0/8/0/24/12/1/0/0/0/0/16 | 1 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 110581..110626: 12 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 111641..112196: 127 instructions, 0 samples |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 | 0 | 112948..113637: 157 instructions, 0 samples |
| shape_paint_EmitShape | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 | 0 | 113851..113873: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 | 0 | 114296..114318: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 114855..114874: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 5 | 115324..115343: 4 instructions, 4 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 0 | 116265..116284: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 4 | 118046..118065: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 118838..118854: 4 instructions, 0 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 7 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/7/4/0/0/0/0/1/7 | 0 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 3 | 121807..121896: 21 instructions, 3 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 0 | 122114..122203: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 106/18/2/0/0/18/7/14/12/4/0/0/0/4/1 | 0 | 122431..122520: 21 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 122852..123043: 43 instructions, 0 samples |
| paint_queue_FillShapeRows | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 324/262/6/37/24/70/2/22/16/2/285/0/0/0/2 | 0 | 123120..124116: 233 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 124250..124627: 96 instructions, 0 samples |
| shape_paint_EmitLineShape | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 | 0 | 124712..124734: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 223/80/4/50/41/93/6/18/14/17/61/0/0/0/10 | 279/234/11/37/20/102/11/19/8/10/112/2/0/8/25 | 0 | 126026..126369: 72 instructions, 0 samples |
| shape_paint_EmitRingShape | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 174/55/3/27/13/76/9/19/16/9/97/0/0/0/0 | 0 | 127259..127437: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 | 0 | 127484..127506: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 4 | 127966..129928: 411 instructions, 4 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 130060..130107: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 130402..131290: 188 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 131422..131469: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 131764..132652: 188 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 132784..132831: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| shape_paint_ShapeAt | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 | 0 | none |
| shape_paint_PointsBounds | 128/113/5/20/18/46/1/11/6/5/49/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/5 | 0 | 134794..135148: 89 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 376/47/6/82/69/202/16/34/28/28/74/0/0/0/8 | 566/480/21/63/91/185/30/86/64/10/306/0/0/1/48 | 0 | 136531..136656: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcDirections | 68/28/1/10/12/30/3/4/3/3/25/0/0/0/2 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 | 0 | 137249..137373: 28 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 | 0 | 137541..137982: 116 instructions, 0 samples |
| paint_queue_GradientColorAt | 71/53/2/13/10/24/0/5/3/6/17/0/0/0/4 | 143/0/0/0/0/17/0/16/16/1/72/0/0/13/0 | 0 | 138104..138308: 51 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 42/29/1/9/8/19/0/2/1/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 | 0 | 138350..138466: 29 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 66/53/1/17/14/35/0/2/1/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 | 0 | 138509..138726: 53 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 138853..138971: 32 instructions, 0 samples |
| std_math_Sqrt | 161/101/4/7/3/11/0/20/16/10/47/0/2/0/16 | 226/41/7/0/0/40/6/32/25/0/139/0/0/2/6 | 0 | 139210..139604: 97 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 105/68/1/11/6/41/5/9/8/6/12/0/0/0/12 | 62/0/0/6/4/21/3/6/6/2/28/0/0/0/0 | 0 | 139791..140045: 68 instructions, 0 samples |
| shape_paint_ShapeClipSide | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 | 0 | 140384..140699: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 140990..140999: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 | 0 | none |
| shape_paint_ShapeArcPoint | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 | 0 | none |
| std_math_Finite64 | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| std_math_MathSplit | 151/142/9/3/5/10/0/18/9/7/36/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 141759..142330: 127 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 106/96/7/9/11/20/0/19/12/6/34/0/0/0/8 | 86/33/5/0/0/14/3/22/17/0/34/0/0/1/4 | 0 | 142432..142779: 85 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 | 0 | none |
| shape_paint_ShapeCrossing | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTriangles | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 | 0 | 143502..143637: 34 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 143684..143786: 24 instructions, 0 samples |

## Input / cc
Native SHA-256: a86bdea0340e51aadb0b3a0b267259e3fc32304e25f8dd94b2dfe50a66ef7ee2
C SHA-256: 2f57319d07a5c639b60d0c15b1ff0e821a92a3ea235000a36cba404a77fa242a
ZX SHA-256: 7f6c46c85d7b7414390cb902c41e91a566c51a3a3db6492791a868eb07138cb1
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 170/7

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 77..109: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 164/148/15/11/14/58/2/18/3/12/8/0/0/0/42 | 1 | 951..973: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 11/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 | 0 | none |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 204/140/7/49/18/83/3/19/12/10/16/0/0/3/19 | 0 | 4506..4528: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 | 0 | none |
| tree_input_TreePointerUpdate | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 258/199/14/30/15/124/40/31/17/10/12/18/0/1/52 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | 7840..7859: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 | 0 | 8662..10221: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 10418..10437: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | 10680..10834: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 | 0 | 10909..11019: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 20 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 11925..11968: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 | 0 | 12962..15413: 524 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 | 2 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 | 0 | 17108..17367: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 | 0 | none |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/0/0/0/0/0/0/0/0 | 0 | 17593..17731: 38 instructions, 0 samples |
| text_context_menu_TextMenuTakeCommand | 147/106/4/23/10/47/0/27/23/14/0/0/0/0/18 | 76/59/7/9/7/27/2/14/7/4/0/0/0/0/7 | 1 | 17997..18342: 90 instructions, 1 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 4/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 64/21/1/6/5/13/2/4/3/4/11/0/0/1/1 | 0 | none |
| textfield_TextFieldCore | 3941/3769/51/856/1156/2333/28/399/348/251/280/1/1/0/265 | 2455/2369/103/737/417/1258/21/253/135/152/372/2/0/16/325 | 11 | 19619..39175: 3688 instructions, 8 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 83/0/0/17/17/53/4/3/3/6/0/4/0/0/0 | 54/21/3/1/1/21/8/5/2/3/0/0/0/0/3 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 23/5/1/0/0/5/0/4/3/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 18/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 | 1 | none |
| paint_queue_PaintSnapshotEmit | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 | 0 | 41214..43262: 346 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 | 0 | 44271..44671: 76 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 201/158/14/35/13/71/4/27/13/13/41/2/0/1/16 | 1 | 45165..46226: 229 instructions, 0 samples |
| tree_input_TreeTextFocused | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 46/8/1/0/0/10/2/7/6/1/0/2/0/0/2 | 0 | 46810..46904: 22 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 8/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 | 2 | 48112..48128: 4 instructions, 1 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 | 1 | 48817..48839: 4 instructions, 0 samples |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 31/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 | 0 | 51624..51943: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 93/18/2/4/2/23/8/12/10/2/0/4/0/2/2 | 0 | 52340..52432: 25 instructions, 0 samples |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | 6/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | 52803..53061: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 | 0 | none |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 | 0 | 53585..53851: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/1/0/0/4 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 | 1 | 54977..55068: 20 instructions, 1 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 | 0 | 57144..60087: 622 instructions, 0 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/0/0/0/0/0/0/0/0 | 1 | none |
| system_clipboard_SystemClipboardText | 161/139/12/22/8/37/3/31/19/14/0/0/0/0/25 | 73/45/8/3/2/12/5/14/6/2/0/0/0/0/6 | 0 | 60310..60829: 132 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 25/0/0/0/0/1/1/5/5/0/14/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenAt | 85/34/1/14/9/44/0/8/7/7/2/0/0/0/2 | 70/12/2/5/1/19/2/8/6/3/2/0/0/0/2 | 0 | 61329..61482: 34 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 59/24/2/1/1/11/1/8/6/3/0/2/0/0/5 | 0 | 61855..61910: 17 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakeDragAt | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 172/147/13/11/5/53/2/28/15/7/21/7/0/0/20 | 0 | none |
| tree_TreePreviousWidgetState | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 46/0/0/0/5/15/2/4/4/1/2/2/0/0/0 | 1 | none |
| tree_input_TreeHasFocus | 96/68/2/11/8/35/6/10/8/8/0/3/0/0/14 | 71/44/3/5/4/20/6/9/6/3/0/1/0/0/7 | 0 | 64151..64308: 44 instructions, 0 samples |
| tree_input_TreeBlurAt | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 35/7/1/0/0/8/3/5/4/1/0/2/0/0/2 | 0 | none |
| textfield_TextFieldFaceFor | 178/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 129/78/1/31/16/65/1/3/2/11/52/0/0/0/5 | 1 | 64622..65592: 166 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 15/0/0/0/0/2/0/3/1/0/7/0/0/0/0 | 0 | 65981..66116: 38 instructions, 0 samples |
| textfield_TextFieldSecureDisplay | 60/34/2/16/5/27/1/7/5/3/0/0/0/0/5 | 49/20/2/0/0/4/1/5/3/2/0/0/0/0/3 | 0 | 66211..66329: 34 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 53/0/0/0/0/5/2/4/0/0/7/0/0/2/0 | 0 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 68/39/2/7/7/16/1/3/1/3/0/0/0/0/1 | 0 | 66785..67044: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 36/0/0/0/0/6/1/4/1/0/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 35/27/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/7/1/0/0/5/1/2/1/1/0/0/0/0/3 | 0 | 67355..67419: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 40/21/2/0/0/2/2/5/3/1/0/0/0/0/2 | 0 | 67565..67708: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 158/125/11/16/29/60/8/28/17/0/23/0/0/0/13 | 1 | 68586..69093: 113 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 16/11/1/2/3/6/1/2/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 22/8/1/0/0/5/3/5/4/0/0/0/0/0/3 | 0 | 69341..69418: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 135/96/8/14/3/35/9/21/13/4/0/0/0/1/9 | 0 | 69633..69778: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/93/9/5/17/29/3/20/11/0/0/0/0/5/14 | 0 | 70202..70863: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 109/77/9/16/14/37/1/14/5/5/12/0/0/0/6 | 0 | 71072..71094: 4 instructions, 0 samples |
| text_request_TextRequestTake | 198/0/0/47/28/118/10/16/16/11/0/9/0/0/0 | 85/0/0/5/4/27/3/9/9/5/0/1/0/0/0 | 1 | none |
| text_request_TextRequestChangeFor | 285/267/12/53/38/116/5/38/26/13/0/0/0/0/24 | 170/141/13/13/5/56/13/24/11/6/3/0/0/0/10 | 0 | 72705..73603: 240 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 68/50/5/0/0/7/2/8/3/4/0/0/0/0/5 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 35/0/0/0/0/5/1/2/2/1/0/0/0/0/0 | 1 | 73804..74030: 59 instructions, 1 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 26/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 74065..74232: 51 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 46/0/0/3/3/12/3/4/3/1/4/2/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetCaptureTab | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 52/38/5/1/1/8/0/8/3/4/0/1/0/1/5 | 0 | none |
| tree_input_TreeFocusSubmittedAt | 227/4/1/39/26/96/10/28/27/14/10/7/0/0/1 | 161/143/18/12/20/70/4/24/6/6/25/5/0/2/17 | 0 | 75674..75693: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 186/96/1/37/14/101/12/15/14/7/0/13/0/0/18 | 153/105/7/16/5/51/17/26/19/3/8/5/0/1/15 | 0 | 76875..77266: 96 instructions, 0 samples |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 68/0/0/2/2/13/1/8/8/3/0/1/0/1/0 | 0 | 77472..77761: 74 instructions, 0 samples |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 1 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetTextValue | 285/227/4/47/34/166/24/19/15/10/0/23/0/0/38 | 173/68/6/12/9/59/8/28/22/4/0/3/0/1/8 | 1 | 79615..80348: 178 instructions, 1 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 33/0/0/5/0/9/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 | 1 | 81195..81217: 4 instructions, 0 samples |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 48/0/0/1/13/18/0/1/1/2/4/0/0/0/0 | 2 | 82766..82788: 4 instructions, 0 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 179/0/0/22/20/53/0/13/13/12/47/1/1/0/0 | 108/0/0/2/0/13/2/8/0/0/37/0/0/3/0 | 1 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 18/0/0/0/0/4/1/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 18/0/0/0/0/4/1/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 28/0/0/0/0/4/2/4/2/0/6/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 | 2 | 84885..84907: 4 instructions, 1 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 29/0/0/0/0/4/1/4/2/0/7/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 7/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 85502..85519: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 85549..85603: 18 instructions, 0 samples |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 45/0/0/1/10/15/0/1/1/2/2/0/0/0/0 | 1 | 86331..86353: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | 86851..86870: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 14/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 86949..86971: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 21/5/1/0/0/7/3/3/2/0/0/0/0/0/1 | 0 | 87004..87049: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 90/0/0/18/17/57/6/3/3/5/0/6/0/0/0 | 52/17/2/1/1/16/6/4/2/3/0/0/0/0/3 | 0 | none |
| text_input_platform_TextInputFocused | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/25/4/0/0/7/2/5/1/3/0/0/0/0/1 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 12/6/1/0/0/1/0/3/1/0/0/0/0/0/0 | 1 | none |
| tree_TreeDefer | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 51/6/1/1/2/12/7/3/2/1/2/0/0/2/4 | 0 | 87895..87968: 17 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 162/147/6/15/38/92/2/14/8/5/21/0/0/2/33 | 0 | 88341..90842: 460 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 | 1 | 91869..92165: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 | 0 | 94415..95490: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 | 0 | 96473..97707: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 | 1 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 | 2 | 98321..98343: 4 instructions, 2 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 | 2 | 98594..98616: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 32/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 | 0 | 100761..101549: 211 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 421/373/45/30/47/105/9/89/44/1/2/0/0/10/98 | 2 | 101973..103204: 302 instructions, 2 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 | 2 | 103966..104217: 62 instructions, 2 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 77/18/2/2/2/18/8/5/3/2/0/4/0/2/2 | 2 | 104767..104950: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 | 1 | 105216..105545: 77 instructions, 1 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 12/0/0/0/0/3/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 84/63/7/10/5/24/2/12/5/7/1/1/0/0/6 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 21/0/0/3/0/7/0/0/0/0/18/0/0/0/0 | 1 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 65/0/0/9/12/33/0/0/0/6/24/0/0/0/0 | 0 | 106924..106943: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 267/236/11/58/44/144/24/25/14/11/14/6/0/0/37 | 189/151/5/31/50/120/13/11/6/4/76/0/0/5/15 | 1 | 107351..108321: 184 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/3/1/5/3/0/0/0/0/2/5 | 3 | 108443..108605: 47 instructions, 3 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/2/0/5/4/4/5/1/0/0/0 | 18/0/0/0/0/2/0/2/0/0/4/0/0/0/0 | 0 | 108751..108760: 5 instructions, 0 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 10/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 108852..108932: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 26/15/2/0/0/2/2/6/4/0/0/0/0/0/3 | 0 | 108985..109049: 17 instructions, 0 samples |
| std_grapheme_Previous | 59/44/2/10/4/14/0/7/5/5/0/0/0/0/4 | 33/9/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | 109143..109285: 42 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/9/0/0/13 | 142/95/6/5/8/36/14/20/14/3/0/6/0/6/20 | 1 | 109626..109905: 69 instructions, 0 samples |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 | 0 | 110034..110362: 82 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 | 3 | 110762..111072: 70 instructions, 2 samples |
| tree_TreeStoreLabel | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 | 0 | 111250..112855: 389 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 114/100/2/23/5/33/2/11/8/2/53/0/0/2/12 | 0 | 112969..112991: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 | 0 | 113593..113615: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 | 6 | 113799..117289: 715 instructions, 5 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 119/99/2/24/6/35/0/11/8/2/59/0/0/2/11 | 0 | 117520..117542: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 27/0/0/0/0/2/0/2/0/0/4/0/0/0/0 | 0 | 118099..118305: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 45/32/1/12/2/29/0/3/2/2/20/0/0/0/1 | 1 | 118420..118442: 4 instructions, 1 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 | 1 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 | 0 | 119642..119664: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 83/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 120393..120988: 157 instructions, 0 samples |
| button_SubmitButton | 622/601/18/167/120/309/20/59/41/56/52/0/0/0/60 | 447/414/9/119/78/217/3/21/10/36/134/0/0/1/35 | 0 | 121092..123916: 586 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 1020/985/24/257/265/610/28/81/57/58/87/16/0/0/95 | 542/515/31/88/88/248/32/67/36/19/103/9/0/4/70 | 0 | 124195..129010: 960 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 129407..129417: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 | 3 | 129601..129620: 4 instructions, 2 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 278/259/112/0/0/136/9/116/4/0/0/1/0/3/122 | 1 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 22/0/0/2/0/6/0/0/0/0/18/0/0/0/0 | 1 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 1 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 1 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 36/0/0/0/0/5/0/0/0/0/0/0/0/5/0 | 0 | 131277..131583: 79 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 85/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 148/0/0/59/14/82/0/15/15/0/70/0/0/0/0 | 0 | 133271..133293: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/2/15/6/3/3/1/0/2/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 | 0 | 134400..135089: 157 instructions, 0 samples |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 | 0 | 135239..136317: 254 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 | 0 | 136945..136964: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 | 4 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 | 1 | 138084..138103: 4 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 | 7 | 138648..138667: 4 instructions, 6 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 | 0 | 140114..140192: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 28/0/0/0/0/5/2/1/1/2/0/3/0/0/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/39/4/3/3/15/4/8/4/3/0/4/0/0/7 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 274/176/4/90/72/183/2/10/6/16/144/0/0/0/7 | 0 | 141402..141424: 4 instructions, 0 samples |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 41/0/0/10/1/11/0/4/4/2/7/2/0/2/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 263/223/22/59/30/100/3/41/19/5/145/0/0/0/13 | 0 | 142678..143976: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 377/294/13/41/19/86/4/61/48/25/89/0/0/0/41 | 203/183/15/35/11/67/5/31/14/1/113/0/0/0/10 | 0 | 144490..145796: 282 instructions, 0 samples |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/8/0/6/6/2/0/2/0/1/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 916/893/51/266/138/459/11/100/43/43/383/2/0/8/84 | 0 | 146802..153445: 1343 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 27/12/1/0/0/1/0/2/1/2/0/0/0/0/1 | 0 | none |
| tree_TreeNextKey | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/6/1/1/1/12/4/5/4/2/0/7/0/2/1 | 0 | 153849..153911: 15 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 42/8/1/2/2/9/0/4/3/2/0/2/0/0/2 | 0 | none |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/4/3/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 23/7/2/0/0/6/1/6/4/0/0/1/0/0/2 | 0 | 154296..154384: 21 instructions, 0 samples |
| contextmenu_ContextMenuFind | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/1/5/3/0/0/0/0/0/3 | 0 | 154449..154542: 23 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 115/43/1/32/26/64/1/3/2/6/46/1/0/0/1 | 83/0/0/10/8/19/1/2/1/4/46/1/0/0/0 | 0 | 154981..155158: 43 instructions, 0 samples |
| overlay_Draw | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 259/229/10/44/27/80/4/24/14/20/33/0/0/5/32 | 0 | 155493..156795: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1459/1242/10/375/368/800/10/72/62/142/339/1/0/0/114 | 1062/861/26/302/118/483/14/69/31/103/299/1/0/3/119 | 0 | 157885..164012: 1131 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/3/0/0/0/0/5/0/0/0/0 | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 | 0 | 175458..175649: 43 instructions, 0 samples |
| paint_queue_FillShapeRows | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 | 0 | 175726..176722: 233 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 | 0 | 176856..177233: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 | 1 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 194/165/10/21/26/76/11/29/19/5/21/4/0/1/22 | 0 | 178261..178816: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 | 25 | 181176..181198: 4 instructions, 4 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 | 1 | 181941..181986: 12 instructions, 1 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/0/0/11/0/0/1/0 | 0 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 46/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 0 | 182079..182245: 33 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 227/179/12/38/30/96/7/27/15/16/39/0/0/0/25 | 0 | 182742..183810: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 46/31/1/8/1/10/0/3/2/4/0/0/0/0/3 | 0 | 183855..184012: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 11/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 19/0/0/0/0/0/0/1/1/0/16/0/0/0/0 | 0 | 184326..184504: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 176/152/5/4/13/19/0/28/23/9/58/0/0/0/14 | 74/38/3/0/0/5/2/9/6/0/45/0/0/0/0 | 0 | 184625..185247: 148 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 27/0/0/0/0/3/2/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 37/23/3/5/2/14/1/7/4/2/8/0/0/0/3 | 0 | 185587..185609: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 9/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 45/22/2/2/1/10/2/6/2/0/28/0/0/0/2 | 0 | 185972..186218: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 28/12/1/1/1/6/1/3/2/2/0/0/0/0/2 | 0 | 186764..186786: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 16/0/0/0/0/1/0/2/2/0/9/0/0/0/0 | 0 | 186998..187156: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 99/53/4/0/0/0/0/11/7/7/34/0/0/0/7 | 23/15/1/0/0/6/1/3/2/0/16/0/0/0/0 | 0 | 187384..187556: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 240/176/5/37/33/79/0/26/21/9/108/0/0/0/12 | 124/89/8/2/0/14/3/17/9/1/77/0/0/0/1 | 0 | 187867..188607: 171 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 371/316/4/71/71/157/3/27/23/22/113/4/1/0/36 | 225/195/7/30/25/84/9/17/8/8/91/1/0/4/21 | 0 | 188860..190282: 312 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 58/0/0/19/15/39/0/5/5/2/28/0/0/0/0 | 0 | 190551..190573: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 76/36/1/15/3/27/1/8/7/1/33/0/0/1/5 | 0 | 191215..191237: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 33/0/0/6/5/11/0/1/1/2/4/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 460/424/20/93/66/194/11/52/32/33/61/12/0/0/41 | 257/196/11/37/28/99/5/30/19/19/56/2/0/1/32 | 0 | 192627..193953: 292 instructions, 0 samples |
| dpi_ScalePixels | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 43/0/0/0/0/7/0/4/1/0/22/0/0/0/0 | 0 | 194081..194376: 75 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 101/83/5/20/9/37/4/13/8/9/25/4/0/0/12 | 86/54/7/3/7/23/5/14/7/2/24/1/0/0/11 | 0 | 194452..194720: 59 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 20/14/1/0/0/1/1/2/1/0/0/0/0/2/4 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/25/2/4/3/14/3/7/5/5/0/0/0/0/4 | 0 | none |
| tree_TreeSetPopup | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 36/0/0/0/0/7/0/5/5/2/0/2/0/0/0 | 0 | none |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 195826..195982: 42 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| tree_TreePushScope | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 81/63/5/0/0/12/10/9/4/8/0/8/0/0/12 | 0 | none |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/2/2/4/0/1/1/2/2/0/0/0/0 | 0 | 196883..196896: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 28/13/4/0/0/8/4/7/3/0/0/0/0/0/4 | 0 | 196911..197077: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 35/0/0/2/0/4/0/2/2/4/4/0/0/0/0 | 0 | 197327..197336: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 23/6/1/0/0/6/1/3/2/0/0/2/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 648/600/8/74/331/442/14/31/23/21/84/0/0/0/35 | 191/96/5/19/43/82/13/12/7/5/77/0/0/1/19 | 0 | 197888..200773: 564 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 1 | 201309..201398: 21 instructions, 1 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 0 | 201616..201705: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 | 0 | 201933..202022: 21 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 202491..202510: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 8 | 202960..202979: 4 instructions, 3 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 3 | 203901..203920: 4 instructions, 1 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 | 16 | 205682..205701: 4 instructions, 1 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 | 3 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 4 | 206474..206490: 4 instructions, 4 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 55/0/0/0/0/1/1/1/1/0/0/10/0/12/0 | 0 | 206558..207219: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 115/84/6/20/13/54/2/13/7/5/32/0/0/0/10 | 0 | 207810..207829: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 24/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 69/52/5/10/4/30/6/9/4/2/27/0/0/0/3 | 0 | 208535..208694: 43 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 38/29/1/8/5/14/1/3/2/3/0/0/0/0/3 | 0 | 208754..208833: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 4/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 123/104/6/4/12/41/10/10/4/3/4/1/0/1/23 | 0 | 208949..209393: 99 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 78/29/2/0/0/0/0/11/9/6/23/0/0/0/5 | 35/10/1/0/0/5/0/6/3/0/16/0/0/0/0 | 0 | 209943..210058: 28 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/93/12/13/9/3/0/16/1/0/22 | 125/61/5/6/4/55/20/11/6/2/5/2/0/3/10 | 0 | 210240..210451: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 9/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 105/77/2/20/34/60/0/4/2/3/26/0/0/0/8 | 0 | 211896..211918: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 7/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 232/196/7/10/16/40/7/37/30/13/11/0/6/0/35 | 121/97/6/2/7/36/13/14/6/1/6/4/0/6/25 | 0 | 212749..213405: 195 instructions, 0 samples |
| tree_TreePopScope | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 54/26/1/3/2/7/6/4/3/4/0/4/0/0/5 | 0 | none |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 213909..214027: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 | 2 | 214138..216100: 411 instructions, 2 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 216232..216279: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 | 0 | 216574..217462: 188 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 217594..217641: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 | 0 | 217936..218824: 188 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 218956..219003: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 19/6/1/0/0/5/2/2/1/0/0/1/0/0/1 | 0 | 219191..219249: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/27/2/3/4/24/5/4/2/1/5/0/0/1/8 | 0 | none |
| svg_shape_ShapeRaster | 261/220/9/32/17/69/6/47/38/15/17/2/0/0/40 | 127/89/5/5/4/35/7/17/12/4/9/1/0/0/18 | 0 | 219791..220610: 199 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 332/300/20/30/35/98/9/48/28/9/0/10/1/0/41 | 330/291/25/30/20/108/27/52/27/1/26/2/0/8/51 | 0 | 221102..221909: 211 instructions, 0 samples |
| image_behavior_ImageFitBounds | 204/184/8/18/9/31/0/26/18/13/53/0/0/0/18 | 92/77/6/6/2/15/0/16/10/0/56/0/0/0/7 | 0 | 221992..222686: 175 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 103/59/1/9/1/12/1/16/15/7/16/0/0/0/4 | 35/14/1/0/0/8/2/8/7/0/12/0/0/0/0 | 0 | 222793..223038: 59 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 223160..223182: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 17/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 223591..223613: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 349/310/21/56/16/91/15/62/41/19/60/1/0/0/48 | 162/151/12/0/0/48/26/32/20/0/21/0/0/0/24 | 0 | 223799..224964: 276 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 614/592/44/147/107/273/7/91/47/35/160/0/0/0/51 | 447/416/39/36/21/126/22/74/35/14/90/0/0/0/87 | 0 | 225231..227514: 506 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 334/315/14/23/20/72/15/49/35/14/64/3/0/0/51 | 222/201/15/15/13/79/21/37/21/2/47/0/0/1/30 | 0 | 228286..229299: 246 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 20/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 40/0/0/4/3/11/0/2/0/1/16/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 8/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 15/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 139/123/3/0/0/16/4/16/13/11/40/0/0/0/22 | 110/13/2/0/0/21/11/13/7/0/42/0/0/2/2 | 0 | 230330..230807: 122 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 614/531/11/90/49/179/20/57/46/38/279/0/3/0/62 | 367/340/21/19/10/94/24/47/25/7/185/0/3/0/38 | 0 | 231178..233518: 506 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 6/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | 233700..233823: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 71/67/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/19/7/7/4/0/14/0/0/0/4 | 0 | 233861..234092: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 53/35/3/0/0/13/5/8/5/0/19/0/0/1/10 | 0 | 234177..234383: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 21/6/1/0/0/4/0/2/1/0/12/0/0/0/1 | 0 | 234506..234624: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 11/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 10/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 9/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 20/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 235435..235451: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 235810..235826: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 102/87/3/4/5/27/12/13/10/4/27/0/2/0/14 | 95/68/5/0/0/27/19/15/10/0/23/0/2/2/13 | 0 | 236010..236208: 47 instructions, 0 samples |

## Input / clang
Native SHA-256: a86bdea0340e51aadb0b3a0b267259e3fc32304e25f8dd94b2dfe50a66ef7ee2
C SHA-256: 95a389acb00b04cbeea8f464bff6ff620803c10829e91e4f7ea138139ba44bcb
ZX SHA-256: 7f6c46c85d7b7414390cb902c41e91a566c51a3a3db6492791a868eb07138cb1
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 167/7

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 77..109: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 157/94/13/11/24/69/0/19/6/11/23/0/0/0/24 | 2 | 951..973: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 | 0 | none |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 121/0/0/12/6/37/2/9/9/9/2/0/0/0/0 | 0 | 4506..4528: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_input_TreePointerUpdate | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 210/110/2/8/1/65/22/28/26/7/18/6/0/4/23 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 7840..7859: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 | 0 | 8662..10221: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 10418..10437: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 10680..10834: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 1 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 10909..11019: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 20 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 1 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | 11925..11968: 18 instructions, 1 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 373/232/17/41/14/130/43/58/41/21/26/5/0/4/33 | 0 | 12962..15413: 524 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 4 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 1 | 17108..17367: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 3 | 17593..17731: 38 instructions, 3 samples |
| text_context_menu_TextMenuTakeCommand | 147/106/4/23/10/47/0/27/23/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 | 0 | 17997..18342: 90 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 47/0/0/4/4/8/0/4/4/4/10/0/0/0/0 | 0 | none |
| textfield_TextFieldCore | 3941/3769/51/856/1156/2333/28/399/348/251/280/1/1/0/265 | 2086/536/26/384/149/876/24/235/209/148/294/1/0/13/54 | 3 | 19619..39175: 3688 instructions, 3 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 83/0/0/17/17/53/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 | 0 | 41214..43262: 346 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 148/83/4/6/2/51/14/12/8/12/6/1/0/6/13 | 3 | 44271..44671: 76 instructions, 3 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 0 | 45165..46226: 229 instructions, 0 samples |
| tree_input_TreeTextFocused | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 0 | 46810..46904: 22 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 48048..48064: 4 instructions, 0 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 0 | 48817..48839: 4 instructions, 0 samples |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 118/103/2/0/0/11/3/22/20/0/0/0/0/3/32 | 0 | 51624..51943: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 | 0 | 52340..52432: 25 instructions, 0 samples |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 1 | 52803..53061: 72 instructions, 1 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 53585..53851: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 2 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 | 0 | 55259..55891: 148 instructions, 0 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 | 2 | 57144..60087: 622 instructions, 1 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 1 | none |
| system_clipboard_SystemClipboardText | 161/139/12/22/8/37/3/31/19/14/0/0/0/0/25 | 63/15/2/0/0/5/1/9/7/2/0/0/0/0/1 | 0 | 60310..60829: 132 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenAt | 85/34/1/14/9/44/0/8/7/7/2/0/0/0/2 | 64/12/2/5/0/19/1/6/4/4/2/0/0/0/1 | 0 | 61329..61482: 34 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 0 | 61855..61910: 17 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 1 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakeDragAt | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 149/69/5/9/1/47/11/25/20/6/8/6/0/0/15 | 1 | none |
| tree_TreePreviousWidgetState | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 | 0 | none |
| tree_input_TreeHasFocus | 96/68/2/11/8/35/6/10/8/8/0/3/0/0/14 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 | 0 | 64151..64308: 44 instructions, 0 samples |
| tree_input_TreeBlurAt | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 | 0 | none |
| textfield_TextFieldFaceFor | 178/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 | 2 | 64622..65592: 166 instructions, 2 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 2 | 65981..66116: 38 instructions, 2 samples |
| textfield_TextFieldSecureDisplay | 60/34/2/16/5/27/1/7/5/3/0/0/0/0/5 | 53/8/1/0/0/2/2/5/4/2/0/0/0/0/1 | 0 | 66211..66329: 34 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 | 0 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 | 0 | 66785..67044: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 35/27/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 | 0 | 67355..67419: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | 67565..67708: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 104/92/3/0/0/37/5/14/11/0/19/0/0/0/11 | 1 | 68586..69093: 113 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 32/9/1/0/0/1/0/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 24/11/1/0/0/3/2/4/3/0/0/0/0/0/3 | 0 | 69341..69418: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 | 0 | 69633..69778: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 91/0/0/2/0/2/0/13/13/0/0/0/0/6/0 | 0 | 70202..70863: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 | 0 | 71072..71094: 4 instructions, 0 samples |
| text_request_TextRequestTake | 198/0/0/47/28/118/10/16/16/11/0/9/0/0/0 | 67/0/0/1/0/17/1/7/7/4/3/1/0/0/0 | 0 | none |
| text_request_TextRequestChangeFor | 285/267/12/53/38/116/5/38/26/13/0/0/0/0/24 | 147/55/6/6/12/41/8/22/16/5/0/0/0/2/0 | 0 | 72705..73603: 240 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 63/17/2/0/0/3/3/8/6/4/0/0/0/0/1 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 31/0/0/1/3/8/0/2/2/1/2/0/0/0/0 | 0 | 73804..74030: 59 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 74065..74232: 51 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetCaptureTab | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 | 0 | none |
| tree_input_TreeFocusSubmittedAt | 227/4/1/39/26/96/10/28/27/14/10/7/0/0/1 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 | 0 | 75674..75693: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 186/96/1/37/14/101/12/15/14/7/0/13/0/0/18 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 | 0 | 76875..77266: 96 instructions, 0 samples |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 0 | 77472..77761: 74 instructions, 0 samples |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetTextValue | 285/227/4/47/34/166/24/19/15/10/0/23/0/0/38 | 138/86/3/8/4/48/13/22/19/5/0/1/0/2/6 | 0 | 79615..80348: 178 instructions, 0 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 32/0/0/5/0/8/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 2 | 81195..81217: 4 instructions, 0 samples |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 1 | 82766..82788: 4 instructions, 0 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 179/0/0/22/20/53/0/13/13/12/47/1/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 | 2 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 19/0/0/0/0/3/0/2/2/1/11/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 1 | 85020..85042: 4 instructions, 1 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 85502..85519: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 85549..85603: 18 instructions, 0 samples |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 86331..86353: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 86851..86870: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 86949..86971: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 77/28/4/0/0/22/20/15/11/0/4/0/0/0/3 | 0 | 87004..87049: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 90/0/0/18/17/57/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocused | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 | 0 | 87895..87968: 17 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 143/0/0/6/36/78/0/12/12/5/18/0/0/2/0 | 0 | 88341..90842: 460 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 0 | 91869..92165: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 1 | 94415..95490: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 96473..97707: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 1 | 98321..98343: 4 instructions, 1 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 5 | 99975..99997: 4 instructions, 2 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 3 | 100761..101549: 211 instructions, 2 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 | 1 | 101973..103204: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 | 2 | 103966..104217: 62 instructions, 1 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 104767..104950: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 | 3 | 105216..105545: 77 instructions, 1 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 81/12/1/6/0/13/3/12/11/6/5/1/0/2/3 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 | 0 | 106924..106943: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 267/236/11/58/44/144/24/25/14/11/14/6/0/0/37 | 253/196/17/22/19/107/14/33/16/4/0/0/0/12/36 | 0 | 107351..108321: 184 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 | 2 | 108443..108605: 47 instructions, 2 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/2/0/5/4/4/5/1/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 0 | 108751..108760: 5 instructions, 0 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 108852..108932: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 25/7/1/0/0/3/2/3/2/0/0/0/0/0/2 | 0 | 108985..109049: 17 instructions, 0 samples |
| std_grapheme_Previous | 59/44/2/10/4/14/0/7/5/5/0/0/0/0/4 | 65/38/2/6/2/11/2/8/6/2/0/0/0/1/4 | 0 | 109143..109285: 42 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/9/0/0/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 0 | 109626..109905: 69 instructions, 0 samples |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 0 | 110034..110362: 82 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 1 | 110762..111072: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 | 0 | 111250..112855: 389 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 1 | 113089..113111: 4 instructions, 1 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 113593..113615: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 | 7 | 113799..117289: 715 instructions, 7 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 2 | 117640..117662: 4 instructions, 1 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 0 | 118099..118305: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 3 | 118378..118400: 4 instructions, 2 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 119642..119664: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 120393..120988: 157 instructions, 0 samples |
| button_SubmitButton | 622/601/18/167/120/309/20/59/41/56/52/0/0/0/60 | 376/10/1/57/34/129/0/17/16/38/33/2/0/3/1 | 0 | 121092..123916: 586 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 1020/985/24/257/265/610/28/81/57/58/87/16/0/0/95 | 553/476/26/87/96/234/31/71/45/19/102/14/0/3/59 | 0 | 124195..129010: 960 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 129407..129417: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 4 | 129601..129620: 4 instructions, 3 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 1 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 1 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 1 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 131277..131583: 79 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 133271..133293: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/0/14/7/3/3/2/0/1/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 1 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 | 0 | 134400..135089: 157 instructions, 0 samples |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 | 0 | 135239..136317: 254 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 136945..136964: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 2 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 | 0 | 138271..138530: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 7 | 138648..138667: 4 instructions, 6 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 140114..140192: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 0 | 141402..141424: 4 instructions, 0 samples |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/3/0/8/0/4/4/2/5/2/0/1/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 | 0 | none |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 190/0/0/19/10/49/0/23/23/2/120/0/0/0/0 | 0 | 142678..143976: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 377/294/13/41/19/86/4/61/48/25/89/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/1 | 0 | 144490..145796: 282 instructions, 0 samples |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 793/0/0/161/116/384/0/54/54/41/331/4/0/7/0 | 0 | 146802..153445: 1343 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_TreeNextKey | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 | 0 | 153849..153911: 15 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 | 0 | none |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 | 0 | 154296..154384: 21 instructions, 0 samples |
| contextmenu_ContextMenuFind | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 | 0 | 154449..154542: 23 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 115/43/1/32/26/64/1/3/2/6/46/1/0/0/1 | 122/32/8/25/13/45/3/12/3/5/50/2/0/0/9 | 0 | 154981..155158: 43 instructions, 0 samples |
| overlay_Draw | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 201/82/1/16/5/38/0/16/15/19/35/0/0/1/9 | 0 | 155493..156795: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1459/1242/10/375/368/800/10/72/62/142/339/1/0/0/114 | 939/718/13/213/93/370/25/55/42/101/293/2/0/1/93 | 0 | 157885..164012: 1131 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 1 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 175458..175649: 43 instructions, 0 samples |
| paint_queue_FillShapeRows | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 324/262/6/37/24/70/2/22/16/2/285/0/0/0/2 | 0 | 175726..176722: 233 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 176856..177233: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 178261..178816: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 | 18 | 181176..181198: 4 instructions, 4 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 181941..181986: 12 instructions, 0 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 182079..182245: 33 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 0 | 182742..183810: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 183855..184012: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 184326..184504: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 176/152/5/4/13/19/0/28/23/9/58/0/0/0/14 | 44/0/0/0/0/1/0/8/8/0/29/0/0/0/0 | 0 | 184625..185247: 148 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 0 | 185587..185609: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 27/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 185972..186218: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 186764..186786: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 186998..187156: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 99/53/4/0/0/0/0/11/7/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 187384..187556: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 240/176/5/37/33/79/0/26/21/9/108/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/87/0/0/0/0 | 0 | 187867..188607: 171 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 371/316/4/71/71/157/3/27/23/22/113/4/1/0/36 | 246/35/1/23/20/69/6/17/16/6/128/1/0/1/3 | 0 | 188860..190282: 312 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 190551..190573: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 191215..191237: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 460/424/20/93/66/194/11/52/32/33/61/12/0/0/41 | 273/193/8/42/16/91/6/30/22/22/45/1/0/0/32 | 0 | 192627..193953: 292 instructions, 0 samples |
| dpi_ScalePixels | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 | 0 | 194081..194376: 75 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 101/83/5/20/9/37/4/13/8/9/25/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 | 0 | 194452..194720: 59 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 21/0/0/0/0/0/0/0/0/0/0/0/0/3/0 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 | 0 | none |
| tree_TreeSetPopup | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 | 0 | none |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 195826..195982: 42 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_TreePushScope | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 | 0 | none |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 | 0 | 196883..196896: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 25/14/1/0/0/5/0/3/2/0/0/0/0/0/4 | 0 | 196911..197077: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 | 0 | 197327..197336: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 648/600/8/74/331/442/14/31/23/21/84/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 | 0 | 197888..200773: 564 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 42/0/0/0/0/6/4/5/5/4/0/4/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 0 | 201309..201398: 21 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 0 | 201616..201705: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 106/18/2/0/0/18/7/14/12/4/0/0/0/4/1 | 0 | 201933..202022: 21 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 202491..202510: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 7 | 202960..202979: 4 instructions, 3 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 6 | 203901..203920: 4 instructions, 2 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 14 | 205682..205701: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 4 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 6 | 206474..206490: 4 instructions, 5 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 206558..207219: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 73/0/0/6/0/29/1/9/8/3/12/0/0/0/0 | 0 | 207810..207829: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 208535..208694: 43 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 208754..208833: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 164/41/4/6/7/61/19/20/16/3/10/5/0/0/4 | 0 | 208949..209393: 99 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 78/29/2/0/0/0/0/11/9/6/23/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 209943..210058: 28 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/93/12/13/9/3/0/16/1/0/22 | 192/156/12/7/3/67/28/24/12/1/9/2/0/10/39 | 0 | 210240..210451: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 211896..211918: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 232/196/7/10/16/40/7/37/30/13/11/0/6/0/35 | 95/25/2/1/4/27/12/9/7/1/6/4/0/4/4 | 0 | 212749..213405: 195 instructions, 0 samples |
| tree_TreePopScope | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 42/0/0/0/0/6/4/5/5/4/0/4/0/0/0 | 0 | none |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 213909..214027: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 4 | 214138..216100: 411 instructions, 4 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 216232..216279: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 216574..217462: 188 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 217594..217641: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 217936..218824: 188 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/25/0/8/7/2/0/1/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 218956..219003: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 47/22/2/0/0/6/5/6/4/0/0/5/0/0/1 | 0 | 219191..219249: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 261/220/9/32/17/69/6/47/38/15/17/2/0/0/40 | 129/113/4/3/3/27/9/17/13/4/8/1/0/0/21 | 0 | 219791..220610: 199 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 332/300/20/30/35/98/9/48/28/9/0/10/1/0/41 | 372/321/22/23/18/117/47/58/36/1/32/4/0/4/51 | 0 | 221102..221909: 211 instructions, 0 samples |
| image_behavior_ImageFitBounds | 204/184/8/18/9/31/0/26/18/13/53/0/0/0/18 | 43/0/0/0/0/3/0/9/9/0/30/0/0/0/0 | 0 | 221992..222686: 175 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 103/59/1/9/1/12/1/16/15/7/16/0/0/0/4 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 222793..223038: 59 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 223160..223182: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 223591..223613: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 349/310/21/56/16/91/15/62/41/19/60/1/0/0/48 | 179/142/8/0/0/31/14/38/30/0/33/0/0/0/25 | 0 | 223799..224964: 276 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 614/592/44/147/107/273/7/91/47/35/160/0/0/0/51 | 659/625/33/85/27/229/45/103/69/15/134/0/0/3/82 | 0 | 225231..227514: 506 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 334/315/14/23/20/72/15/49/35/14/64/3/0/0/51 | 192/158/16/10/9/58/22/28/12/3/45/0/0/3/12 | 0 | 228286..229299: 246 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 139/123/3/0/0/16/4/16/13/11/40/0/0/0/22 | 91/16/2/0/0/22/17/14/12/1/39/0/0/0/1 | 0 | 230330..230807: 122 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 614/531/11/90/49/179/20/57/46/38/279/0/3/0/62 | 575/417/24/43/25/189/49/61/37/8/319/0/3/10/72 | 0 | 231178..233518: 506 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 61/27/3/0/0/26/17/10/7/0/18/0/0/2/5 | 0 | 233700..233823: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 71/67/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 233861..234092: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 234177..234383: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 234506..234624: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 235435..235451: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 235810..235826: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 102/87/3/4/5/27/12/13/10/4/27/0/2/0/14 | 133/55/6/0/0/47/34/24/18/0/46/0/0/2/8 | 0 | 236010..236208: 47 instructions, 0 samples |

## Hello / cc
Native SHA-256: 66e96c7832b03971230d94006661eb128c4a363afe6b77456ff5b7c0316ca9c0
C SHA-256: e70b3cc394a59ee620f5e3ed19d3caf074750a50123a3467a5f59c9989fde55a
ZX SHA-256: da9d8570dcaa74c0357ddf325c39fa1c6827428e86dfc6e41a1dee275cd52d27
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| hello_main | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 5/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |

## Hello / clang
Native SHA-256: 66e96c7832b03971230d94006661eb128c4a363afe6b77456ff5b7c0316ca9c0
C SHA-256: 0771743211a2860bf00059d57870c41591076da2a7b5ed54e0ebb8199a37e5df
ZX SHA-256: da9d8570dcaa74c0357ddf325c39fa1c6827428e86dfc6e41a1dee275cd52d27
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| hello_main | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |

## Ranked C wins

| Workload/compiler/function | Weighted excess | Function samples/total (estimated CPU share) | Patterns to inspect | Native | C |
| --- | ---: | ---: | --- | --- | --- |
| Records/clang/wasm_tier_Records | 91924 | 158/158 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 904/117/4/260/520/806/4/25/21/5/0/5/2/0/25 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 |
| Records/cc/wasm_tier_Records | 91150 | 159/159 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 904/117/4/260/520/806/4/25/21/5/0/5/2/0/25 | 80/59/4/1/1/20/3/7/3/1/12/2/0/2/12 |
| Sieve/clang/wasm_tier_Sieve | 53246 | 163/163 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 77/42/7/2/2/24/7/13/6/5/6/1/0/0/12 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 |
| Hash/cc/wasm_tier_Hash | 48000 | 165/165 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 50/46/4/0/0/3/2/9/5/3/0/1/1/0/12 | 26/12/2/0/0/4/1/2/0/0/0/4/0/1/4 |
| Sieve/cc/wasm_tier_Sieve | 40259 | 166/166 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 77/42/7/2/2/24/7/13/6/5/6/1/0/0/12 | 46/23/5/1/2/9/3/8/3/2/0/1/0/0/9 |
| Hash/clang/wasm_tier_Hash | 22000 | 162/162 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 50/46/4/0/0/3/2/9/5/3/0/1/1/0/12 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 |
| Printing/cc/wasm_tier_Printing | 17847 | 39/100 39% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 59/44/1/5/5/12/0/5/4/7/0/1/1/0/9 | 32/15/1/0/0/4/1/1/0/1/0/2/0/0/5 |
| Button/cc/theme_DefaultShadows | 9486 | 18/160 11% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 |
| Printing/clang/wasm_tier_Printing | 9313 | 34/99 34% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 59/44/1/5/5/12/0/5/4/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 |
| Input/clang/session_SessionIndex | 9195 | 20/167 11% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Input/cc/theme_DefaultShadows | 7936 | 16/170 9% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 |
| Input/cc/session_SessionIndex | 7773 | 20/170 11% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphWidth | 7743 | 22/157 14% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/3/1/5/3/0/0/0/0/2/5 |
| Text/clang/session_SessionIndex | 7430 | 15/155 9% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Button/cc/theme_DefaultTheme | 7403 | 25/160 15% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 |
| Input/clang/theme_DefaultShadows | 7331 | 14/167 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Input/cc/theme_DefaultTheme | 6968 | 25/170 14% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 |
| Button/clang/theme_DefaultTheme | 6887 | 25/160 15% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 |
| Button/clang/theme_DefaultShadows | 6559 | 12/160 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Button/clang/session_SessionIndex | 6238 | 13/160 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Button/cc/session_SessionIndex | 5781 | 14/160 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 |
| Button/clang/theme_LightPalette | 5582 | 11/160 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/session_SessionIndex | 5470 | 13/157 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 |
| Input/clang/theme_DefaultTheme | 4750 | 18/167 10% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 |
| Input/cc/theme_LightPalette | 3780 | 8/170 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang/std_utf8_Decode | 3700 | 9/155 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Text/clang/text_layout_TextIdeographic | 3422 | 7/155 4% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Input/clang/theme_LightPalette | 3403 | 7/167 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang/theme_DefaultMetrics | 3151 | 6/167 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Text/clang/std_math_SineDegrees | 3065 | 7/155 4% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 246/138/4/0/0/0/0/21/17/12/99/0/0/0/27 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 |
| Input/clang/theme_CurrentTheme | 2994 | 7/167 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Input/clang/theme_DefaultFonts | 2898 | 6/167 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/clang/paint_queue_PendingPaintAt | 2823 | 7/160 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Text/cc/text_layout_TextIdeographic | 2815 | 6/157 3% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 25/8/2/0/0/6/0/4/2/0/0/0/0/0/2 |
| Button/clang/theme_DefaultMetrics | 2741 | 5/160 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Button/cc/theme_DefaultMetrics | 2722 | 5/160 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Text/clang/theme_LightPalette | 2619 | 5/155 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/theme_LightPalette | 2510 | 5/160 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang/paint_queue_PendingPaintAt | 2498 | 6/155 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Button/cc/theme_DefaultFonts | 2485 | 5/160 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldCore | 2439 | 11/170 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3941/3769/51/856/1156/2333/28/399/348/251/280/1/1/0/265 | 2455/2369/103/737/417/1258/21/253/135/152/372/2/0/16/325 |
| Input/cc/theme_CurrentTheme | 2426 | 7/170 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 |
| Text/clang/theme_DefaultShadows | 2256 | 4/155 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Text/clang/widget_benchmark_host_MeasureGlyphWidth | 2139 | 21/155 13% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Input/clang/theme_DefaultMotion | 1978 | 4/167 2% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Text/cc/paint_queue_PaintClippedLabel | 1977 | 4/157 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 |
| Input/clang/paint_queue_PendingPaintAt | 1932 | 5/167 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Text/clang/paint_queue_PaintClippedLabel | 1912 | 4/155 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Input/clang/paint_queue_PaintClear | 1911 | 4/167 2% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Input/cc/theme_DefaultFonts | 1871 | 4/170 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/cc/theme_DefaultTheme | 1810 | 6/157 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 |
| Text/clang/paint_queue___zi_spec_9308732530dd822f | 1800 | 4/155 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/cc/geometry_RotatedRectangleBounds | 1783 | 5/157 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 200/103/6/48/72/124/1/11/5/6/91/0/0/0/6 | 88/29/1/9/14/27/0/2/1/3/59/0/0/0/4 |
| Text/clang/text_layout_TextTokenNext | 1761 | 9/155 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 300/277/12/43/22/80/6/60/48/27/0/0/0/0/45 | 209/116/20/4/2/22/7/55/35/3/0/0/0/3/34 |
| Button/clang/paint_queue___zi_spec_9308732530dd822f | 1743 | 4/160 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang/theme_DefaultTheme | 1706 | 6/155 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 |
| Text/cc/paint_queue___zi_spec_9308732530dd822f | 1673 | 4/157 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 |
| Input/clang/paint_queue___zi_spec_9308732530dd822f | 1670 | 4/167 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang/paint_queue_QueuePaint | 1664 | 7/167 4% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 |
| Text/cc/std_utf8_Decode | 1615 | 4/157 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 |
| Text/cc/std_math_SineDegrees | 1563 | 4/157 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 246/138/4/0/0/0/0/21/17/12/99/0/0/0/27 | 95/49/3/0/0/28/1/15/10/0/55/0/0/0/0 |
| Button/clang/theme_DefaultMotion | 1548 | 3/160 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Input/cc/theme_DefaultMetrics | 1537 | 3/170 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Button/cc/paint_queue_QueuePaint | 1513 | 6/160 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 |
| Button/clang/theme_DefaultFonts | 1512 | 3/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/cc/theme_CurrentTheme | 1473 | 4/160 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 |
| Input/cc/tree_TreeSubmittedGeometryAt | 1458 | 4/170 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 |
| Button/cc/paint_queue_PaintClippedLabel | 1455 | 3/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 |
| Button/clang/paint_queue_PaintRoundedFill | 1429 | 3/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Input/cc/paint_queue_QueuePaint | 1424 | 6/170 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 |
| Button/clang/button_behavior_MeasureContent | 1406 | 3/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 176/152/5/4/13/19/0/28/23/9/58/0/0/0/14 | 44/0/0/0/0/1/0/8/8/0/29/0/0/0/0 |
| Button/clang/button_PaintButtonForNode | 1352 | 5/160 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 793/0/0/161/116/384/0/54/54/41/331/4/0/7/0 |
| Button/cc/button_PaintButtonForNode | 1292 | 6/160 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 916/893/51/266/138/459/11/100/43/43/383/2/0/8/84 |
| Text/clang/text_layout_NextTextLineUncached | 1286 | 4/155 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 |
| Input/cc/theme_DefaultMotion | 1278 | 3/170 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 |
| Button/clang/button_ButtonDefaultFace | 1277 | 5/160 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Button/cc/widget_benchmarks_ButtonCase | 1257 | 3/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 157/142/10/3/9/41/1/19/9/13/8/0/0/0/36 |
| Text/cc/text_layout_TextIsSpace | 1249 | 3/157 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/button_SubmitButton | 1235 | 5/160 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 622/601/18/167/120/309/20/59/41/56/52/0/0/0/60 | 376/10/1/57/34/129/0/17/16/38/33/2/0/3/1 |
| Button/cc/paint_queue___zi_spec_9308732530dd822f | 1231 | 3/160 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 |
| Input/clang/paint_queue_EmitPaintDirect | 1157 | 4/167 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Input/clang/std_utf8_Decode | 1144 | 3/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Button/cc/tree_TreeFinish | 1109 | 3/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 |
| Text/cc/font_metrics_GlyphWidth | 1091 | 2/157 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/text_input_TextSelectionRangeForLength | 1091 | 3/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang/raster_text_PaintTextClipped | 1087 | 2/155 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/cc/theme_DefaultShadows | 1074 | 2/157 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 |
| Text/cc/paint_queue_QueuePaint | 1028 | 4/157 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 |
| Text/clang/paint_queue_QueuePaint | 1024 | 4/155 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 |
| Text/cc/theme_LightPalette | 1023 | 2/157 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang/tree_TreeSubmit | 1012 | 3/155 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 |
| Button/clang/paint_queue_PaintClear | 997 | 2/160 1% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Text/cc/tree_TreeIdentityHash | 984 | 3/157 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/widget_benchmark_host_MeasureGlyphWidth | 975 | 3/170 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/3/1/5/3/0/0/0/0/2/5 |
| Text/cc/paint_queue_EmitPaintDirect | 955 | 3/157 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 |
| Text/cc/text_PrepareText | 936 | 4/157 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/82/2/62/65/169/0/13/11/15/16/0/0/0/2 | 167/85/3/36/22/65/2/7/4/14/32/0/0/3/4 |
| Text/cc/widget_benchmarks_TextCase | 931 | 3/157 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 279/155/5/34/82/140/3/25/20/16/47/0/0/0/27 | 143/127/10/15/19/63/2/17/7/8/26/0/0/1/29 |
| Input/cc/paint_queue_PaintClear | 927 | 2/170 1% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 |
| Input/cc/paint_queue_PaintRoundedOutline | 925 | 2/170 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 48/0/0/1/13/18/0/1/1/2/4/0/0/0/0 |
| Input/clang/tree_TreePointerActivatable | 919 | 2/167 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Input/cc/paint_queue_PaintClippedLabel | 913 | 2/170 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 |
| Input/clang/paint_queue_PaintRoundedFill | 912 | 2/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Button/cc/theme_DefaultMotion | 905 | 2/160 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 |
| Button/clang/theme_CurrentTheme | 892 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Input/cc/paint_queue_EmitPaintDirect | 882 | 3/170 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 |
| Button/clang/widget_benchmarks_ButtonCase | 880 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 141/0/0/3/5/37/0/12/12/13/8/0/0/0/0 |
| Text/cc/paint_queue_PendingPaintAt | 875 | 3/157 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 |
| Button/cc/paint_queue_PendingPaintAt | 859 | 3/160 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 |
| Input/clang/textfield_TextFieldCore | 845 | 3/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3941/3769/51/856/1156/2333/28/399/348/251/280/1/1/0/265 | 2086/536/26/384/149/876/24/235/209/148/294/1/0/13/54 |
| Button/clang/button_behavior_MeasureBounds | 836 | 3/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 190/0/0/19/10/49/0/23/23/2/120/0/0/0/0 |
| Button/cc/tree_TreeSubmit | 833 | 5/160 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 |
| Input/clang/widget_benchmarks_InputCase | 831 | 2/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 157/94/13/11/24/69/0/19/6/11/23/0/0/0/24 |
| Button/clang/tree_TreeSubmittedGeometryAt | 809 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Text/cc/tree_TreeSubmittedGeometryAt | 789 | 2/157 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 |
| Button/cc/button_ButtonDetailSize | 784 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 41/0/0/10/1/11/0/4/4/2/7/2/0/2/0 |
| Input/clang/tree_TreeSubmittedGeometryAt | 775 | 2/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Input/cc/paint_queue___zi_spec_9308732530dd822f | 772 | 2/170 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 443/414/5/25/277/332/8/21/16/7/4/5/0/0/25 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 |
| Input/clang/text_input_TextInputScaledMetric | 754 | 2/167 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Button/clang/paint_queue_QueuePaint | 744 | 3/160 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/6/0/0/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 |
| Button/clang/button_ButtonDetailSize | 738 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/3/0/8/0/4/4/2/5/2/0/1/0 |
| Input/clang/text_input_TextFieldPaintFor | 709 | 2/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 179/0/0/22/20/53/0/13/13/12/47/1/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 |
| Text/cc/text_layout_NextTextLineUncached | 689 | 6/157 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 309/277/8/40/17/70/5/28/20/6/12/0/0/13/28 |
| Text/clang/geometry_WorldToScreen | 685 | 3/155 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 |
| Button/cc/widget_benchmark_host_MeasureGlyphWidth | 674 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 35/16/2/0/0/5/3/5/3/0/0/0/0/2/5 |
| Text/clang/paint_queue_EmitLabel | 663 | 2/155 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Button/clang/paint_queue_EmitLabel | 642 | 2/160 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Input/clang/tree_TreeSubmit | 626 | 2/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 |
| Button/cc/paint_queue_EmitPaintDirect | 625 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 |
| Text/clang/paint_queue_EmitPaintDirect | 623 | 2/155 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Text/clang/text_layout_NextTextLine | 620 | 2/155 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 670/608/18/130/245/440/16/51/33/27/19/12/1/0/57 | 348/109/17/13/6/102/41/57/40/4/17/10/0/13/13 |
| Button/clang/paint_queue_EmitPaintDirect | 604 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Button/clang/tree_input_TreeTakePointerActivationAt | 575 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 81/12/1/6/0/13/3/12/11/6/5/1/0/2/3 |
| Text/cc/theme_DefaultMetrics | 554 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Text/clang/surface_Unit | 552 | 1/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Input/clang/paint_queue_EmitRoundedOutline | 548 | 2/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Input/clang/textfield_TextFieldFaceFor | 544 | 2/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 178/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 |
| Text/clang/geometry_RotatedRectangleBounds | 541 | 2/155 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 200/103/6/48/72/124/1/11/5/6/91/0/0/0/6 | 116/0/0/28/17/47/0/0/0/10/103/0/0/0/0 |
| Text/clang/text_TextOrigin | 541 | 1/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Input/cc/paint_queue_PendingPaintAt | 539 | 2/170 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 |
| Button/clang/surface_Unit | 535 | 1/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Button/clang/button_behavior_ButtonPictureSize | 529 | 1/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Text/clang/tree_TreeOverlayLayerAt | 527 | 1/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_DefaultTextFace | 524 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 14/0/0/0/0/7/0/0/0/1/0/0/0/0/0 |
| Button/clang/tree_TreeBuildNewIndex | 517 | 4/160 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 |
| Text/clang/widget_benchmarks_TextCase | 517 | 2/155 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 279/155/5/34/82/140/3/25/20/16/47/0/0/0/27 | 167/53/3/6/15/56/0/18/15/12/37/0/0/1/14 |
| Text/cc/theme_DefaultFonts | 506 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeInputAt | 505 | 2/170 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 |
| Button/cc/paint_queue_EmitLabel | 500 | 2/160 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 |
| Text/cc/tree_TreeBuildOldIndex | 500 | 3/157 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 |
| Text/cc/text_layout_TextTokenNext | 498 | 5/157 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 300/277/12/43/22/80/6/60/48/27/0/0/0/0/45 | 253/234/27/2/3/43/10/61/34/6/0/0/0/4/54 |
| Input/clang/session_SessionFlagSet | 497 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_input_TreePointerUpdate | 489 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 258/199/14/30/15/124/40/31/17/10/12/18/0/1/52 |
| Text/cc/text___zi_spec_6a19f7c98e79a734 | 484 | 1/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 6/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/style_sheet_StyleDefaultFacts | 484 | 1/167 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Button/clang/tree_TreeSetLabel | 482 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/cc/text_typeface_TextTypefaceUse | 479 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Text/clang/text_layout_TextIsSpace | 471 | 1/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/cc/paint_queue_PaintLine | 466 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 45/0/0/1/10/15/0/1/1/2/2/0/0/0/0 |
| Input/cc/paint_queue_PaintRoundedFill | 464 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 |
| Text/cc/text_behavior_TextWrapPolicy | 463 | 1/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/tree_TreeStoreLabel | 461 | 3/157 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 |
| Text/cc/theme_DefaultMotion | 461 | 1/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 |
| Text/clang/theme_CurrentTheme | 460 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Input/clang/tree_TreeFindOld | 460 | 3/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 |
| Input/cc/paint_queue_KeepPaintBytes | 454 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 |
| Input/clang/paint_queue_PaintRoundedOutline | 449 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Button/clang/style_StyleFontValue | 446 | 1/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Input/clang/paint_queue_PaintClippedLabel | 443 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Input/clang/tree_TreeStart | 441 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Input/clang/std_grapheme_data_Property | 438 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Button/cc/button_ButtonDefaultFace | 437 | 3/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 227/179/12/38/30/96/7/27/15/16/39/0/0/0/25 |
| Text/cc/text_layout_TextDefaultLineGap | 433 | 1/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 15/0/0/0/0/4/0/3/1/0/7/0/0/0/0 |
| Input/clang/text_input_TextSelectionRangeFor | 433 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang/style_ResolveFont | 430 | 1/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Text/cc/tree_TreeStart | 423 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 |
| Input/clang/paint_queue_ShapeVisible | 422 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Text/clang/tree_TreeSubmittedGeometryAt | 417 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Button/cc/style_StyleMinHeightValue | 416 | 1/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Text/clang/text_TextFaceFor | 416 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 |
| Button/cc/tree_TreeStart | 415 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 |
| Text/clang/text_layout_TextDefaultLineGap | 411 | 1/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Text/cc/text_layout_TextLineXFor | 410 | 1/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 16/0/0/0/0/1/1/1/1/0/0/0/0/2/0 |
| Button/clang/tree_TreeCurrentParent | 402 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Button/cc/surface_Unit | 401 | 1/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Input/cc/widget_benchmarks_InputCase | 400 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 164/148/15/11/14/58/2/18/3/12/8/0/0/0/42 |
| Input/clang/text_input_CompositionTransitionFor | 399 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 104/92/3/0/0/37/5/14/11/0/19/0/0/0/11 |
| Text/cc/text_layout_TextNoBreakAfter | 391 | 1/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 37/19/2/0/0/4/1/7/5/0/0/0/0/0/6 |
| Input/clang/style_sheet_StyleControlFacts | 389 | 1/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Input/clang/tree_TreeCurrentParent | 385 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Text/cc/tree_draw_BeginFrame | 378 | 2/157 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 |
| Text/cc/tree_TreeFinish | 377 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 |
| Text/cc/theme_CurrentTheme | 375 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 |
| Text/cc/paint_queue_PaintFlush | 365 | 2/157 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 |
| Text/clang/tree_TreeRead | 363 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Button/cc/button_behavior_MeasureContent | 362 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 176/152/5/4/13/19/0/28/23/9/58/0/0/0/14 | 74/38/3/0/0/5/2/9/6/0/45/0/0/0/0 |
| Button/cc/widget_benchmark_host_RecordBounds | 361 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 |
| Button/cc/paint_queue_PaintFlush | 358 | 2/160 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 |
| Input/clang/tree_input_PointerSample | 357 | 1/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Button/clang/paint_queue_ResolvePaintText | 355 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Button/clang/tree_TreeRead | 352 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Button/cc/button_SubmitButton | 351 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 622/601/18/167/120/309/20/59/41/56/52/0/0/0/60 | 447/414/9/119/78/217/3/21/10/36/134/0/0/1/35 |
| Text/clang/tree_TreeIdentityHash | 351 | 1/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/text_PaintTextLineRuns | 349 | 3/157 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 339/321/6/109/109/237/12/13/7/13/36/0/0/0/14 | 277/229/5/89/51/149/4/9/4/11/97/2/0/4/22 |
| Input/cc/tree_TreeFinish | 348 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 |
| Input/cc/tree_TreePreviousWidgetState | 337 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 46/0/0/0/5/15/2/4/4/1/2/2/0/0/0 |
| Input/cc/text_request_TextRequestTake | 335 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 198/0/0/47/28/118/10/16/16/11/0/9/0/0/0 | 85/0/0/5/4/27/3/9/9/5/0/1/0/0/0 |
| Text/cc/paint_queue_PaintPointCount | 332 | 3/157 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 |
| Button/clang/tree_TreeRootBounds | 329 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 |
| Button/clang/tree_TreeSubmit | 327 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 |
| Text/clang/tree_TreeFinish | 327 | 1/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Input/cc/text_input_TextSelectionRangeFor | 324 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/0/0/0/0/0/0/0/0 |
| Button/clang/tree_TreeFindOld | 320 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 |
| Input/clang/std_grapheme_Push | 318 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 |
| Input/cc/text_input_platform_TextInputOwnerMatches | 316 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 12/6/1/0/0/1/0/3/1/0/0/0/0/0/0 |
| Button/clang/tree_TreeModalLayer | 312 | 1/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeCurrentParent | 305 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Button/cc/style_sheet_StyleControlFacts | 305 | 1/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 22/0/0/2/0/6/0/0/0/0/18/0/0/0/0 |
| Button/cc/button_behavior_ContentLayout | 302 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 240/176/5/37/33/79/0/26/21/9/108/0/0/0/12 | 124/89/8/2/0/14/3/17/9/1/77/0/0/0/1 |
| Input/cc/font_metrics_GlyphCaret | 302 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 35/0/0/0/0/5/1/2/2/1/0/0/0/0/0 |
| Input/cc/tree_input_TreeHitAt | 301 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 201/158/14/35/13/71/4/27/13/13/41/2/0/1/16 |
| Button/clang/paint_queue_KeepPaintMesh | 296 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 |
| Button/cc/button_behavior_MeasureBounds | 291 | 2/160 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 263/223/22/59/30/100/3/41/19/5/145/0/0/0/13 |
| Input/cc/text_input_CompositionTransitionFor | 291 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 158/125/11/16/29/60/8/28/17/0/23/0/0/0/13 |
| Input/clang/tree_input_TreeTakeDragAt | 290 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 149/69/5/9/1/47/11/25/20/6/8/6/0/0/15 |
| Button/cc/tree_TreeBuildNewIndex | 289 | 3/160 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 |
| Input/cc/style_sheet_StyleControlFacts | 287 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 22/0/0/2/0/6/0/0/0/0/18/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuTakeCommand | 284 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 147/106/4/23/10/47/0/27/23/14/0/0/0/0/18 | 76/59/7/9/7/27/2/14/7/4/0/0/0/0/7 |
| Text/cc/tree_TreeIsBuilding | 278 | 1/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/paint_queue_PaintOverflow | 278 | 1/157 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/clang/paint_queue_PaintOrder | 277 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Text/clang/text_PrepareText | 276 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/82/2/62/65/169/0/13/11/15/16/0/0/0/2 | 151/22/1/19/7/52/1/6/5/14/18/0/0/3/3 |
| Text/cc/paint_queue_KeepPaintMesh | 269 | 1/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 |
| Button/clang/theme_ControlHeightFor | 259 | 1/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Text/cc/paint_queue_EmitLabel | 254 | 1/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 |
| Text/clang/tree_TreeSetLabel | 249 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Text/clang/tree_TreeSetStyleId | 249 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/cc/paint_queue_KeepPaintMesh | 248 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 |
| Input/clang/tree_TreeBuildNewIndex | 247 | 2/167 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 |
| Button/clang/tree_input_TreeFocusAt | 246 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Button/cc/tree_TreeExplicitExists | 244 | 2/160 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 78/18/2/2/2/19/8/5/3/2/0/4/0/2/2 |
| Input/cc/tree_TreeExplicitExists | 242 | 2/170 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 77/18/2/2/2/18/8/5/3/2/0/4/0/2/2 |
| Text/clang/theme_CurrentText | 241 | 1/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_ShapeSmall | 239 | 1/167 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Text/cc/paint_queue_PaintOrder | 235 | 1/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 |
| Input/cc/paint_queue_EmitLabel | 235 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 |
| Input/cc/text_input_TextFieldPaintFor | 233 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 179/0/0/22/20/53/0/13/13/12/47/1/1/0/0 | 108/0/0/2/0/13/2/8/0/0/37/0/0/3/0 |
| Input/cc/tree_TreeSetTextValue | 231 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 285/227/4/47/34/166/24/19/15/10/0/23/0/0/38 | 173/68/6/12/9/59/8/28/22/4/0/3/0/1/8 |
| Text/cc/text_TextHiddenLines | 230 | 1/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 119/40/2/11/2/22/0/11/9/10/22/0/0/0/4 | 76/8/1/13/5/24/3/7/2/2/22/0/0/0/2 |
| Text/cc/tree_TreeFindOld | 230 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 |
| Button/cc/tree_TreeSetStyleId | 225 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Input/clang/tree_TreeEnsureSemantic | 225 | 1/167 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Input/cc/tree_TreeSetLabel | 215 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeRead | 213 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 |
| Input/cc/tree_TreeFindOld | 213 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 |
| Button/cc/button_Draw | 208 | 1/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 14/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Text/clang/tree_TreeLayerAt | 207 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Button/clang/tree_TreeLayerAt | 200 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Text/cc/paint_queue_PendingTextValue | 198 | 1/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 |
| Button/clang/widget_benchmark_host_MeasureGlyphWidth | 197 | 2/160 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Button/clang/tree_draw_FrameInputCommit | 192 | 1/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Input/clang/widget_benchmark_host_MeasureGlyphWidth | 189 | 2/167 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Button/cc/tree_draw_BeginFrame | 185 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 |
| Text/clang/tree_TreeBuildOldIndex | 183 | 1/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 |
| Input/cc/tree_TreeBuildNewIndex | 181 | 2/170 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 |
| Button/clang/tree_TreeBuildOldIndex | 178 | 1/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 |
| Input/cc/style_sheet_StyleControlRoleFacts | 176 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 21/0/0/3/0/7/0/0/0/0/18/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterText | 173 | 1/160 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 |
| Input/cc/style_sheet_ResolveInstalledStyleRules | 171 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 267/236/11/58/44/144/24/25/14/11/14/6/0/0/37 | 189/151/5/31/50/120/13/11/6/4/76/0/0/5/15 |
| Text/cc/tree_TreeSubmit | 169 | 1/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 |
| Text/clang/text_DrawSession | 165 | 2/155 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 300/20/2/37/35/102/1/25/23/20/115/1/0/2/2 |
| Text/clang/text_PaintTextLines | 163 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 695/613/7/184/149/360/10/49/42/45/86/0/0/0/42 | 519/378/9/116/83/259/7/34/25/23/188/0/0/10/38 |
| Input/cc/widget_benchmark_host_RasterText | 163 | 1/170 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 |
| Button/clang/tree_input_TreeFocusedAt | 162 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Input/cc/textfield_TextFieldFaceFor | 161 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 178/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 129/78/1/31/16/65/1/3/2/11/52/0/0/0/5 |
| Button/cc/tree_TreeLayerAt | 160 | 1/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 |
| Input/cc/tree_TreeBuildOldIndex | 153 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 |
| Text/cc/text_TextFaceFor | 137 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 51/0/0/11/8/23/0/1/1/2/10/0/0/2/0 |
| Text/clang/tree_TreeBuildNewIndex | 133 | 1/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 |
| Text/cc/text_PaintTextLines | 127 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 695/613/7/184/149/360/10/49/42/45/86/0/0/0/42 | 556/471/12/192/123/338/6/35/13/15/207/0/0/6/34 |
| Text/cc/tree_TreeRead | 108 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 |
| Text/clang/paint_queue_EmitPaint | 105 | 1/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Input/cc/paint_queue_PaintPointCount | 102 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 |
| Input/cc/tree_TreeRead | 100 | 1/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 |
| Input/cc/tree_TreeEnsureSemantic | 98 | 3/170 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 |
| Input/clang/paint_queue_EmitPaint | 97 | 1/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Button/cc/tree_draw_FrameInputCommit | 96 | 1/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 |
| Button/cc/tree_draw_CommitFrame | 76 | 1/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 |
| Input/cc/tree_draw_CommitFrame | 71 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 |
| Text/cc/tree_TreeEnsureSemantic | 70 | 2/157 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 |
| Text/cc/widget_benchmark_host_RasterTextClipped | 67 | 1/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 |
| Input/cc/paint_queue_EmitPaint | 64 | 2/170 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 |
| Text/cc/paint_queue_KeepShortPaintBytes | 50 | 2/157 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Button/cc/tree_TreeEnsureSemantic | 34 | 1/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 |
| Text/cc/paint_queue_EmitPaint | 34 | 1/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 |
| Text/clang/tree_TreeStoreLabel | 31 | 1/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 |
| Button/clang/tree_TreeStoreLabel | 30 | 1/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 |
| Input/cc/tree_TreeEnsureInteraction | 27 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/9/0/0/13 | 142/95/6/5/8/36/14/20/14/3/0/6/0/6/20 |
| Button/cc/paint_queue_KeepShortPaintBytes | 24 | 1/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Input/cc/paint_queue_KeepShortPaintBytes | 23 | 1/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Calls/clang/wasm_tier_Fib | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 21/7/1/0/0/2/0/2/1/1/0/0/0/0/1 |
| Button/cc/widget_benchmarks_BeginCase | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Button/cc/tree_draw_EndFrame | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 |
| Button/cc/tree_TreeCount | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeNodeAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 |
| Button/cc/text_request_TextRequestSend | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 |
| Button/cc/text_request_TextRequestClear | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Button/cc/std_grapheme_Boundary | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 |
| Button/cc/session_SessionOpen | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 |
| Button/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/160 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_PointerSample | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 |
| Button/cc/tree_input_SecondaryPointerSample | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 |
| Button/cc/session_SessionFlagSet | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Button/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 |
| Button/cc/tree_input_TreeKeyboardRoute | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 |
| Button/cc/text_editing_TextEditingFrameBegin | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_PaintClear | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 |
| Button/cc/tree_TreeFrameStatus | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 |
| Button/cc/session_SessionFlagGet | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_PaintSnapshotEmit | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 |
| Button/cc/tree_TreeIsBuilding | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_input_TreeHitAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 201/158/14/35/13/71/4/27/13/13/41/2/0/1/16 |
| Button/cc/tree_input_TreeTextFocused | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 46/8/1/0/0/10/2/7/6/1/0/2/0/0/2 |
| Button/cc/tree_input_TreeTextKind | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 8/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_TreeFocusAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 |
| Button/cc/tree_TreeInputAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 |
| Button/cc/std_utf8_Valid | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 |
| Button/cc/tree_TreeTextRange | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 93/18/2/4/2/23/8/12/10/2/0/4/0/2/2 |
| Button/cc/tree_input_TreeFocusedAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 |
| Button/cc/geometry_RectangleIntersection | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 |
| Button/cc/tree_TreePointerActivatable | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 |
| Button/cc/tree_input_TreeActivateAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/1/0/0/4 |
| Button/cc/cursor_CursorHoverFrameStart | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 |
| Button/cc/tree_TreeBuildOldIndex | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 |
| Button/cc/tree_TreePrevious | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 59/24/2/1/1/11/1/8/6/3/0/2/0/0/5 |
| Button/cc/style_ResolveFlags | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Button/cc/style_ResolveState | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Button/cc/tree_input_TreePressedAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 28/0/0/0/0/5/2/1/1/2/0/3/0/0/0 |
| Button/cc/tree_input_TreeHoveredAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/39/4/3/3/15/4/8/4/3/0/4/0/0/7 |
| Button/cc/style_ResolveFont | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Button/cc/style_StyleFontValue | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 |
| Button/cc/font_metrics_GlyphWidth | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/font_metrics_GlyphLineHeight | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_TreeRootBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Button/cc/button_ButtonMinimumHeight | 0 | 0/160 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/button_ButtonFitContentFace | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 377/294/13/41/19/86/4/61/48/25/89/0/0/0/41 | 203/183/15/35/11/67/5/31/14/1/113/0/0/0/10 |
| Button/cc/tree_TreeSetInteractive | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Button/cc/tree_TreeSetCircleHit | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/8/0/6/6/2/0/2/0/1/0 |
| Button/cc/tree_TreeSetSelected | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 |
| Button/cc/tree_TreeSetLabel | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeSetSemanticDetail | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeSetStyleClass | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Button/cc/paint_queue_PaintOverflow | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/cc/tree_TreeCancel | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Button/cc/paint_queue_PaintOrder | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 |
| Button/cc/paint_queue_SnapshotTextRead | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_SnapshotTextJoin | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 |
| Button/cc/paint_queue_PaintPointCount | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 |
| Button/cc/paint_queue_EmitPaint | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 |
| Button/cc/input_protocol_PointInBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 25/0/0/0/0/1/1/5/5/0/14/0/0/0/0 |
| Button/cc/tree_input_TreeTopModalLayer | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/cc/tree_TreeButtonLike | 0 | 0/160 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_PointInCircleBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 32/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/cc/std_utf8_Decode | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 |
| Button/cc/tree_TreeParentGeneration | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 |
| Button/cc/tree_TreeIdentityHash | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/tree_TreeFindOld | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 |
| Button/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 84/63/7/10/5/24/2/12/5/7/1/1/0/0/6 |
| Button/cc/style_sheet_ResolveInstalledStyleRules | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 267/236/11/58/44/144/24/25/14/11/14/6/0/0/37 | 189/151/5/31/50/120/13/11/6/4/76/0/0/5/15 |
| Button/cc/button_ButtonFaceFacts | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 46/31/1/8/1/10/0/3/2/4/0/0/0/0/3 |
| Button/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/160 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/button_ButtonDetailFont | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 11/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Button/cc/style_StyleLineHeightValue | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/cc/style_FitHeight | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 19/0/0/0/0/0/0/1/1/0/16/0/0/0/0 |
| Button/cc/button_behavior_ShapeWidth | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 27/0/0/0/0/3/2/4/4/0/12/0/0/0/0 |
| Button/cc/theme_ControlHeightFor | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 37/23/3/5/2/14/1/7/4/2/8/0/0/0/3 |
| Button/cc/tree_TreeEnsureInteraction | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/9/0/0/13 | 142/95/6/5/8/36/14/20/14/3/0/6/0/6/20 |
| Button/cc/tree_TreeStoreLabel | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 |
| Button/cc/style_StyleOpacityValue | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 |
| Button/cc/button_behavior_ButtonCornerRadius | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 9/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Button/cc/drawing_props_ColorFromPacked | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/surface_Opacity | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Button/cc/paint_queue_PaintRoundedFill | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 |
| Button/cc/paint_queue_PaintRoundedOutline | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 48/0/0/1/13/18/0/1/1/2/4/0/0/0/0 |
| Button/cc/focus_FocusDefaultOutlineBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 45/22/2/2/1/10/2/6/2/0/28/0/0/0/2 |
| Button/cc/image_lookup_IconLookupImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 28/12/1/1/1/6/1/3/2/2/0/0/0/0/2 |
| Button/cc/button_behavior_ButtonPictureSize | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 16/0/0/0/0/1/0/2/2/0/9/0/0/0/0 |
| Button/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 99/53/4/0/0/0/0/11/7/7/34/0/0/0/7 | 23/15/1/0/0/6/1/3/2/0/16/0/0/0/0 |
| Button/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 371/316/4/71/71/157/3/27/23/22/113/4/1/0/36 | 225/195/7/30/25/84/9/17/8/8/91/1/0/4/21 |
| Button/cc/image_lookup_ImageLookupTexture | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 58/0/0/19/15/39/0/5/5/2/28/0/0/0/0 |
| Button/cc/image_behavior_ImageDrawPlanFor | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 76/36/1/15/3/27/1/8/7/1/33/0/0/1/5 |
| Button/cc/paint_queue_PaintImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 33/0/0/6/5/11/0/1/1/2/4/0/0/0/0 |
| Button/cc/paint_queue_PaintLine | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 45/0/0/1/10/15/0/1/1/2/2/0/0/0/0 |
| Button/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/cc/cursor_CursorHoverFrameCommit | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Button/cc/paint_queue_PaintMeshPointCount | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Button/cc/paint_queue_ResolvePaintText | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 |
| Button/cc/tree_TreeModalLayer | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/std_utf8_Continuation | 0 | 0/160 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/std_grapheme_IsControl | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/session_WidgetPartKey | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/style_sheet_StyleBucket | 0 | 0/160 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleFirstBit | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 36/0/0/0/0/5/0/0/0/0/0/0/0/5/0 |
| Button/cc/style_sheet_StyleRuleMatchesState | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleSelectorMatches | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 85/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Button/cc/style_MergeValues | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 148/0/0/59/14/82/0/15/15/0/70/0/0/0/0 |
| Button/cc/theme_StateLayer | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 55/0/0/0/0/1/1/1/1/0/0/10/0/12/0 |
| Button/cc/card_behavior_CardDefaultFace | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 115/84/6/20/13/54/2/13/7/5/32/0/0/0/10 |
| Button/cc/theme_FontForRole | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 24/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Button/cc/theme_MotionDuration | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 69/52/5/10/4/30/6/9/4/2/27/0/0/0/3 |
| Button/cc/tree_TreeLabelSpanAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Button/cc/paint_queue_ClipShape | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 |
| Button/cc/drawing_props_PaintPixelScale | 0 | 0/160 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 4/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 123/104/6/4/12/41/10/10/4/3/4/1/0/1/23 |
| Button/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 78/29/2/0/0/0/0/11/9/6/23/0/0/0/5 | 35/10/1/0/0/5/0/6/3/0/16/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/93/12/13/9/3/0/16/1/0/22 | 125/61/5/6/4/55/20/11/6/2/5/2/0/3/10 |
| Button/cc/image_behavior_ImageFitRect | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 9/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Button/cc/paint_queue_PaintImageWithPolygon | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 105/77/2/20/34/60/0/4/2/3/26/0/0/0/8 |
| Button/cc/paint_queue_EmitLine | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 83/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Button/cc/paint_queue_ClipLabel | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 |
| Button/cc/paint_queue_PendingTextValue | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 |
| Button/cc/style_sheet_StyleMatchesOptional | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleControlRoleFacts | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 21/0/0/3/0/7/0/0/0/0/18/0/0/0/0 |
| Button/cc/paint_queue_ShapeVisible | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 |
| Button/cc/paint_queue_ShapeWhole | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 |
| Button/cc/raster_shape_FillRoundedRectangle | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_FillShapeClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 |
| Button/cc/paint_queue_EmitPaintNow | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 |
| Button/cc/tree_TreeCurrentPaintGeometry | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 |
| Button/cc/tree_TreeSubmittedGeometryAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 |
| Button/cc/paint_queue_KeepPaintMesh | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 |
| Button/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconHash | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 19/6/1/0/0/5/2/2/1/0/0/1/0/0/1 |
| Button/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/27/2/3/4/24/5/4/2/1/5/0/0/1/8 |
| Button/cc/svg_shape_ShapeRaster | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/220/9/32/17/69/6/47/38/15/17/2/0/0/40 | 127/89/5/5/4/35/7/17/12/4/9/1/0/0/18 |
| Button/cc/svg_icon_behavior_SvgIconCells | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses) | 332/300/20/30/35/98/9/48/28/9/0/10/1/0/41 | 330/291/25/30/20/108/27/52/27/1/26/2/0/8/51 |
| Button/cc/image_behavior_ImageFitBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/184/8/18/9/31/0/26/18/13/53/0/0/0/18 | 92/77/6/6/2/15/0/16/10/0/56/0/0/0/7 |
| Button/cc/image_behavior_ImagePolygonValid | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 103/59/1/9/1/12/1/16/15/7/16/0/0/0/4 | 35/14/1/0/0/8/2/8/7/0/12/0/0/0/0 |
| Button/cc/paint_queue_EmitImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Button/cc/paint_queue_ClipImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 17/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Button/cc/raster_StrokeLine | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/raster_text_PaintTextRotated | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 |
| Button/cc/raster_text_PaintTextItalicClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 |
| Button/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 |
| Button/cc/raster_text_PaintTextClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 |
| Button/cc/raster_text_PaintText | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/theme_DarkPalette | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 |
| Button/cc/paint_queue_ShapeRadius | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 |
| Button/cc/paint_queue_FillShapeRows | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 |
| Button/cc/paint_queue_FillClippedSpan | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 |
| Button/cc/paint_queue_KeepPaintBytes | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 |
| Button/cc/paint_queue_KeepMeshPoints | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 194/165/10/21/26/76/11/29/19/5/21/4/0/1/22 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Button/cc/svg_shape_ShapeNumber | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 349/310/21/56/16/91/15/62/41/19/60/1/0/0/48 | 162/151/12/0/0/48/26/32/20/0/21/0/0/0/24 |
| Button/cc/svg_shape_ShapeBuildPath | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 614/592/44/147/107/273/7/91/47/35/160/0/0/0/51 | 447/416/39/36/21/126/22/74/35/14/90/0/0/0/87 |
| Button/cc/svg_shape_ShapeCoverEdges | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 334/315/14/23/20/72/15/49/35/14/64/3/0/0/51 | 222/201/15/15/13/79/21/37/21/2/47/0/0/1/30 |
| Button/cc/paint_queue_EmitImageBegin | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 20/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 40/0/0/4/3/11/0/2/0/1/16/0/0/1/0 |
| Button/cc/paint_queue_EmitImageEnd | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 |
| Button/cc/text_emphasis_RunTextItalicRaster | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterTextClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 |
| Button/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Button/cc/style_sheet_StyleDefaultFacts | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/3/0/0/0/0/5/0/0/0/0 |
| Button/cc/paint_queue_ShapeCornerInset | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 |
| Button/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Button/cc/paint_queue_KeepLongPaintBytes | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 |
| Button/cc/svg_shape_ShapeIsDigit | 0 | 0/160 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeIsSpace | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 8/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddPoint | 0 | 0/160 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 15/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddSpan | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 139/123/3/0/0/16/4/16/13/11/40/0/0/0/22 | 110/13/2/0/0/21/11/13/7/0/42/0/0/2/2 |
| Button/cc/paint_queue_ShapeSquareRoot | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Button/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 |
| Button/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 |
| Button/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 614/531/11/90/49/179/20/57/46/38/279/0/3/0/62 | 367/340/21/19/10/94/24/47/25/7/185/0/3/0/38 |
| Button/cc/svg_shape_ShapeFillSubpath | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 6/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeCollapse | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 71/67/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/19/7/7/4/0/14/0/0/0/4 |
| Button/cc/svg_shape_ShapeAddCircle | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 53/35/3/0/0/13/5/8/5/0/19/0/0/1/10 |
| Button/cc/svg_shape_ShapeSqrt | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 21/6/1/0/0/4/0/2/1/0/12/0/0/0/1 |
| Button/cc/svg_shape_ShapeQuad | 0 | 0/160 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 11/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeAbs | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 10/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeTriangle | 0 | 0/160 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 9/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddEdge | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 20/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Button/cc/svg_shape_ShapeAddPolygon | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates | 102/87/3/4/5/27/12/13/10/4/27/0/2/0/14 | 95/68/5/0/0/27/19/15/10/0/23/0/2/2/13 |
| Button/clang/widget_benchmarks_BeginCase | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Button/clang/tree_draw_BeginFrame | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 |
| Button/clang/button_Draw | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Button/clang/tree_draw_EndFrame | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Button/clang/tree_TreeCount | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang/tree_TreeNodeAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang/tree_input_TreePointerUpdate | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 210/110/2/8/1/65/22/28/26/7/18/6/0/4/23 |
| Button/clang/widget_benchmark_host_WidgetTextCount | 0 | 0/160 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang/widget_benchmark_host_WidgetPaintCount | 0 | 0/160 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/160 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang/text_request_TextRequestSend | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 |
| Button/clang/text_request_TextRequestClear | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Button/clang/widget_benchmark_host_WidgetPaintReset | 0 | 0/160 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/clang/tree_input_PointerSample | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Button/clang/tree_input_SecondaryPointerSample | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Button/clang/session_SessionFlagSet | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/paint_queue_PaintSnapshotInvalidate | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Button/clang/tree_input_TreeKeyboardRoute | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 373/232/17/41/14/130/43/58/41/21/26/5/0/4/33 |
| Button/clang/text_editing_TextEditingFrameBegin | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Button/clang/tree_TreeStart | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Button/clang/tree_TreeFrameStatus | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/clang/session_SessionFlagGet | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang/tree_draw_CommitFrame | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Button/clang/paint_queue_PaintSnapshotEmit | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 |
| Button/clang/tree_TreeIsBuilding | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang/tree_input_TreeHitAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Button/clang/tree_input_TreeTextFocused | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Button/clang/tree_input_TreeTextKind | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang/tree_TreeInputAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Button/clang/tree_input_TreeInputAllowedAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Button/clang/tree_TreeTextRange | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 |
| Button/clang/geometry_RectangleIntersection | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Button/clang/tree_TreePointerActivatable | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Button/clang/tree_input_TreeActivateAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Button/clang/cursor_CursorHoverFrameStart | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Button/clang/tree_TreePrevious | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Button/clang/style_ResolveFlags | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Button/clang/tree_input_TreeTakeActivationAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang/style_ResolveState | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Button/clang/tree_input_TreeHoveredAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Button/clang/style_ResolveFont | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Button/clang/font_metrics_GlyphWidth | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/font_metrics_GlyphLineHeight | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/style_StyleMinHeightValue | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang/button_ButtonMinimumHeight | 0 | 0/160 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/button_ButtonFitContentFace | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 377/294/13/41/19/86/4/61/48/25/89/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/1 |
| Button/clang/tree_TreeSetInteractive | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Button/clang/tree_TreeSetCircleHit | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Button/clang/tree_TreeSetSelected | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang/tree_TreeSetSemanticDetail | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/clang/tree_TreeSetStyleClass | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang/tree_TreeSetStyleId | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang/paint_queue_PaintOverflow | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/clang/tree_TreeCancel | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Button/clang/tree_TreeFinish | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Button/clang/paint_queue_PaintOrder | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Button/clang/paint_queue_SnapshotTextRead | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Button/clang/paint_queue_SnapshotTextJoin | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Button/clang/paint_queue_PaintPointCount | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Button/clang/paint_queue_EmitPaint | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Button/clang/input_protocol_PointInBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Button/clang/tree_input_TreeTopModalLayer | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/clang/tree_TreeButtonLike | 0 | 0/160 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/tree_input_PointInCircleBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/clang/tree_TreeOverlayLayerAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/std_utf8_Decode | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Button/clang/std_grapheme_Push | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 |
| Button/clang/tree_TreeParentGeneration | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Button/clang/tree_TreeIdentityHash | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang/tree_TreeExplicitExists | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Button/clang/button_ButtonFaceFinish | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Button/clang/style_sheet_ResolveInstalledStyleRules | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 267/236/11/58/44/144/24/25/14/11/14/6/0/0/37 | 253/196/17/22/19/107/14/33/16/4/0/0/0/12/36 |
| Button/clang/button_ButtonFaceFacts | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Button/clang/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/160 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/button_ButtonDetailFont | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Button/clang/style_StyleLineHeightValue | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang/style_FitHeight | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Button/clang/button_behavior_ShapeWidth | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Button/clang/tree_TreeEnsureInteraction | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/9/0/0/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Button/clang/tree_TreeEnsureSemantic | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Button/clang/style_StyleOpacityValue | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Button/clang/button_behavior_ButtonCornerRadius | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Button/clang/drawing_props_ColorFromPacked | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/surface_Opacity | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Button/clang/paint_queue_PaintRoundedOutline | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Button/clang/focus_FocusDefaultOutlineBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 27/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Button/clang/image_lookup_IconLookupImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Button/clang/button_behavior_ButtonLabeledPictureSize | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 99/53/4/0/0/0/0/11/7/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Button/clang/button_behavior_ContentLayout | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 240/176/5/37/33/79/0/26/21/9/108/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/87/0/0/0/0 |
| Button/clang/svg_icon_behavior_PaintSvgIcon | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 371/316/4/71/71/157/3/27/23/22/113/4/1/0/36 | 246/35/1/23/20/69/6/17/16/6/128/1/0/1/3 |
| Button/clang/image_lookup_ImageLookupTexture | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Button/clang/image_behavior_ImageDrawPlanFor | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Button/clang/paint_queue_PaintLine | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Button/clang/paint_queue_PaintClippedLabel | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Button/clang/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/clang/cursor_CursorHoverFrameCommit | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Button/clang/paint_queue_PaintMeshPointCount | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Button/clang/std_utf8_Continuation | 0 | 0/160 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/std_grapheme_data_Property | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Button/clang/std_grapheme_IsControl | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/session_WidgetPartKey | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang/widget_key_WidgetRepeatedPart | 0 | 0/160 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/style_sheet_StyleBucket | 0 | 0/160 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/style_sheet_StyleFirstBit | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Button/clang/style_sheet_StyleRuleMatchesState | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/style_sheet_StyleSelectorMatches | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Button/clang/style_MergeValues | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Button/clang/theme_StateLayer | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Button/clang/card_behavior_CardDefaultFace | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 73/0/0/6/0/29/1/9/8/3/12/0/0/0/0 |
| Button/clang/theme_ShapeMedium | 0 | 0/160 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang/theme_TypeLabel | 0 | 0/160 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang/theme_FontForRole | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Button/clang/theme_MotionDuration | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Button/clang/tree_TreeLabelSpanAt | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Button/clang/paint_queue_ClipShape | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Button/clang/paint_queue_EmitRoundedOutline | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Button/clang/drawing_props_PaintPixelScale | 0 | 0/160 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/clang/svg_icon_behavior_SvgIconSlot | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 164/41/4/6/7/61/19/20/16/3/10/5/0/0/4 |
| Button/clang/svg_icon_behavior_SvgIconSnap | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 78/29/2/0/0/0/0/11/9/6/23/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Button/clang/svg_icon_behavior_SvgIconTexture | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/93/12/13/9/3/0/16/1/0/22 | 192/156/12/7/3/67/28/24/12/1/9/2/0/10/39 |
| Button/clang/paint_queue_PaintImageWithPolygon | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Button/clang/paint_queue_EmitLine | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Button/clang/paint_queue_ClipLabel | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Button/clang/paint_queue_PendingTextValue | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Button/clang/style_sheet_StyleMatchesOptional | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/theme_ShapeSurface | 0 | 0/160 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang/style_sheet_StyleControlRoleFacts | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Button/clang/paint_queue_ShapeVisible | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Button/clang/paint_queue_ShapeWhole | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Button/clang/raster_shape_FillRoundedRectangle | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/paint_queue_FillShapeClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 |
| Button/clang/paint_queue_EmitPaintNow | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 |
| Button/clang/tree_TreeCurrentPaintGeometry | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Button/clang/raster_shape_StrokeRoundedRectangle | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/svg_icon_behavior_SvgIconRetire | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Button/clang/svg_shape_ShapeRaster | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/220/9/32/17/69/6/47/38/15/17/2/0/0/40 | 129/113/4/3/3/27/9/17/13/4/8/1/0/0/21 |
| Button/clang/image_behavior_ImageFitBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 204/184/8/18/9/31/0/26/18/13/53/0/0/0/18 | 43/0/0/0/0/3/0/9/9/0/30/0/0/0/0 |
| Button/clang/image_behavior_ImagePolygonValid | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 103/59/1/9/1/12/1/16/15/7/16/0/0/0/4 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Button/clang/paint_queue_EmitImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Button/clang/paint_queue_ClipImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Button/clang/raster_StrokeLine | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/text_typeface_TextTypefaceUse | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang/raster_text_PaintTextRotated | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang/raster_text_PaintTextItalicClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Button/clang/raster_text_PaintTextEmphasisClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Button/clang/raster_text_PaintTextClipped | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang/raster_text_PaintText | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/theme_DarkPalette | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/clang/style_sheet_StyleControlFacts | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Button/clang/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Button/clang/paint_queue_ShapeRadius | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Button/clang/paint_queue_FillClippedSpan | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Button/clang/paint_queue_KeepPaintBytes | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Button/clang/paint_queue_KeepMeshPoints | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Button/clang/svg_shape_ShapeNumber | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 349/310/21/56/16/91/15/62/41/19/60/1/0/0/48 | 179/142/8/0/0/31/14/38/30/0/33/0/0/0/25 |
| Button/clang/svg_shape_ShapeCoverEdges | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 334/315/14/23/20/72/15/49/35/14/64/3/0/0/51 | 192/158/16/10/9/58/22/28/12/3/45/0/0/3/12 |
| Button/clang/paint_queue_EmitImageBegin | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Button/clang/widget_benchmark_host_RasterImage | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Button/clang/paint_queue_EmitImageEnd | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Button/clang/text_emphasis_RunTextItalicRaster | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang/text_emphasis_RunTextEmphasisRaster | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Button/clang/style_sheet_StyleDefaultFacts | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Button/clang/widget_benchmark_host_RecordBounds | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Button/clang/svg_shape_ShapeIsDigit | 0 | 0/160 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/svg_shape_ShapeIsSpace | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang/svg_shape_ShapeAddPoint | 0 | 0/160 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Button/clang/svg_shape_ShapeAddSpan | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 139/123/3/0/0/16/4/16/13/11/40/0/0/0/22 | 91/16/2/0/0/22/17/14/12/1/39/0/0/0/1 |
| Button/clang/paint_queue_ShapeSquareRoot | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Button/clang/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang/svg_shape_ShapeStrokeSubpath | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 614/531/11/90/49/179/20/57/46/38/279/0/3/0/62 | 575/417/24/43/25/189/49/61/37/8/319/0/3/10/72 |
| Button/clang/svg_shape_ShapeCollapse | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/67/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Button/clang/svg_shape_ShapeAddCircle | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Button/clang/svg_shape_ShapeQuad | 0 | 0/160 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Button/clang/svg_shape_ShapeAbs | 0 | 0/160 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Button/clang/svg_shape_ShapeTriangle | 0 | 0/160 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Button/clang/svg_shape_ShapeAddEdge | 0 | 0/160 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Text/cc/widget_benchmarks_BeginCase | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Text/cc/tree_draw_EndFrame | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 |
| Text/cc/tree_TreeCount | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeNodeAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 |
| Text/cc/text_request_TextRequestSend | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 |
| Text/cc/text_request_TextRequestClear | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Text/cc/std_grapheme_Boundary | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 |
| Text/cc/session_SessionOpen | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 |
| Text/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/157 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/cc/tree_input_PointerSample | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 |
| Text/cc/tree_input_SecondaryPointerSample | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 |
| Text/cc/session_SessionFlagSet | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Text/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 |
| Text/cc/tree_input_TreeKeyboardRoute | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 |
| Text/cc/text_editing_TextEditingFrameBegin | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_PaintClear | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 |
| Text/cc/tree_TreeFrameStatus | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 |
| Text/cc/text_DrawSession | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 301/266/10/103/83/191/2/16/6/18/155/0/0/0/15 |
| Text/cc/session_SessionFlagGet | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/tree_draw_CommitFrame | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 |
| Text/cc/paint_queue_PaintSnapshotEmit | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 |
| Text/cc/std_utf8_Valid | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 |
| Text/cc/tree_TreeTextRange | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 92/18/2/4/2/22/7/12/10/2/0/4/0/2/2 |
| Text/cc/tree_input_TreeFocusAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 |
| Text/cc/tree_input_TreeFocusedAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 |
| Text/cc/tree_TreeInputAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 |
| Text/cc/geometry_RectangleIntersection | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 |
| Text/cc/tree_TreePointerActivatable | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 |
| Text/cc/tree_input_TreeActivateAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/0/0/1/4 |
| Text/cc/cursor_CursorHoverFrameStart | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 |
| Text/cc/std_math_Finite32 | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 12/0/0/0/0/3/0/2/2/0/4/0/0/0/0 |
| Text/cc/tree_TreeRootBounds | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Text/cc/text_TextOrigin | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 5/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Text/cc/tree_TreeSetLabel | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Text/cc/tree_TreeSetStyleClass | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Text/cc/tree_TreeSetStyleId | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Text/cc/tree_TreeSetSemantic | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 68/0/0/2/2/13/1/8/8/3/0/1/0/1/0 |
| Text/cc/tree_TreeSetHiddenLines | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 |
| Text/cc/tree_draw_FrameInputCommit | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 |
| Text/cc/tree_TreeCancel | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Text/cc/paint_queue_SnapshotTextRead | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_SnapshotTextJoin | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 |
| Text/cc/tree_input_TreeTopModalLayer | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_TreeParentGeneration | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 |
| Text/cc/tree_TreeBuildNewIndex | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/99/11/21/16/8/0/12/0/0/36 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 |
| Text/cc/tree_TreeExplicitExists | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 75/18/2/2/2/16/6/5/3/2/0/4/0/2/2 |
| Text/cc/text_behavior_ResolveTextStyle | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 78/56/6/8/3/19/2/14/6/1/11/0/0/0/3 |
| Text/cc/style_ResolveFont | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Text/cc/style_StyleFontValue | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 |
| Text/cc/drawing_props_ColorFromPacked | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/surface_Opacity | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Text/cc/style_StyleOpacityValue | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 |
| Text/cc/font_metrics_GlyphLineHeight | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_layout_NextTextLine | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 670/608/18/130/245/440/16/51/33/27/19/12/1/0/57 | 265/228/27/12/8/98/40/43/15/3/11/2/0/16/37 |
| Text/cc/text_behavior_TextExtent | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/cc/text_layout_TextLayoutTotalHeight | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 14/0/0/0/0/0/0/2/2/0/0/2/0/0/0 |
| Text/cc/tree_TreeCurrentParent | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Text/cc/geometry_WorldToScreen | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 39/0/0/9/4/13/0/0/0/2/33/0/0/0/0 |
| Text/cc/text_behavior_TextAlignmentOffset | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 16/0/0/0/0/2/1/2/2/0/7/0/0/0/0 |
| Text/cc/text_layout_TextCharactersEnd | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 27/8/1/0/0/1/1/4/3/1/0/0/0/0/1 |
| Text/cc/text_TextSpan | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/cc/text_TextEllipsisStart | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/84/5/23/14/42/1/10/5/4/0/0/0/0/11 | 60/41/4/2/2/11/5/9/5/1/0/0/0/0/6 |
| Text/cc/text_behavior_TextStrikethroughBounds | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 25/0/0/0/0/2/0/0/0/0/10/0/0/2/0 |
| Text/cc/text_PaintTextRule | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 134/0/0/28/29/58/0/4/2/6/68/0/0/3/0 |
| Text/cc/cursor_CursorHoverFrameCommit | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Text/cc/paint_queue_PaintMeshPointCount | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Text/cc/paint_queue_ResolvePaintText | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 |
| Text/cc/tree_TreeModalLayer | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeLayerAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 |
| Text/cc/std_utf8_Continuation | 0 | 0/157 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_grapheme_IsControl | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/session_WidgetPartKey | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/text_style_TextStyleFor | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 32/12/1/6/4/16/1/3/2/2/8/0/0/0/1 |
| Text/cc/surface_Unit | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/157 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextLineHash | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 16/6/1/0/0/2/0/2/1/0/0/1/0/0/1 |
| Text/cc/text_layout_TextLineCacheMatches | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 24/10/4/1/0/10/1/7/3/0/1/0/0/0/0 |
| Text/cc/std_math_CosineDegrees | 0 | 0/157 0% |  | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/1/0/0/0/0 |
| Text/cc/tree_TreeLabelSpanAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Text/cc/text_TextLinkRunAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 84/58/2/4/0/10/6/10/8/0/0/0/0/3/7 |
| Text/cc/text_TextLinkColor | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 22/0/0/0/0/1/0/0/0/3/0/2/0/4/0 |
| Text/cc/paint_queue_ClipLabel | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 |
| Text/cc/text_TextRangeValid | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 34/10/2/0/0/3/2/3/1/0/0/0/0/2/3 |
| Text/cc/paint_queue_PaintClippedEmphasis | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 52/0/0/5/17/25/0/1/1/2/6/0/0/0/0 |
| Text/cc/paint_queue_PaintRoundedFill | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 |
| Text/cc/shape_paint_PaintTriangleShape | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 43/0/0/0/7/10/0/0/0/1/17/0/0/0/0 |
| Text/cc/drawing_props_StyleTokenColorOr | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_typeface_TextTypefaceUse | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Text/cc/raster_text_PaintTextRotated | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 |
| Text/cc/raster_text_PaintTextItalicClipped | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 |
| Text/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 |
| Text/cc/raster_text_PaintTextClipped | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 |
| Text/cc/raster_text_PaintText | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_EmitPaintNow | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 |
| Text/cc/tree_TreeCurrentPaintGeometry | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 |
| Text/cc/paint_queue_ClipShape | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 |
| Text/cc/shape_paint_ShapeOffset | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_SubmitShape | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 31/0/0/0/4/11/0/0/0/1/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 |
| Text/cc/text_emphasis_RunTextItalicRaster | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterText | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 |
| Text/cc/paint_queue_KeepPaintBytes | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 |
| Text/cc/paint_queue_KeepMeshPoints | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 191/162/10/21/26/74/8/29/19/5/21/4/0/1/22 |
| Text/cc/paint_queue_ShapeVisible | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 |
| Text/cc/paint_queue_ShapeWhole | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 |
| Text/cc/raster_shape_FillRoundedRectangle | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_FillShapeClipped | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 |
| Text/cc/shape_paint_EmitShape | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 28/0/0/4/0/8/0/11/7/0/3/0/0/0/0 |
| Text/cc/paint_queue_SubmitPaint | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 12/0/0/4/0/5/0/0/0/1/0/0/0/0/0 |
| Text/cc/theme_DarkPalette | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/text_layout_TextNoBreakBefore | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 115/105/18/0/0/13/2/28/10/0/0/0/0/0/37 |
| Text/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Text/cc/paint_queue_KeepLongPaintBytes | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 |
| Text/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 |
| Text/cc/paint_queue_ShapeRadius | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 |
| Text/cc/paint_queue_FillShapeRows | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 |
| Text/cc/paint_queue_FillClippedSpan | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 |
| Text/cc/shape_paint_EmitLineShape | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 110/47/2/20/1/45/7/7/5/6/44/0/0/2/5 |
| Text/cc/shape_paint_EmitCircleShape | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 223/80/4/50/41/93/6/18/14/17/61/0/0/0/10 | 185/132/10/28/16/70/2/21/11/8/60/0/0/3/15 |
| Text/cc/shape_paint_EmitRingShape | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 111/84/9/20/6/42/1/14/5/8/45/0/0/0/9 |
| Text/cc/shape_paint_EmitTriangleShape | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 38/12/1/7/0/19/1/3/2/2/9/0/0/0/3 |
| Text/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 |
| Text/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 |
| Text/cc/widget_benchmark_host_RecordBounds | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 |
| Text/cc/paint_queue_ShapeCornerInset | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 |
| Text/cc/shape_paint_ShapeAt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 6/0/0/1/0/1/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_LineCornersFor | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 89/0/0/11/9/25/0/5/5/3/65/0/0/0/0 |
| Text/cc/shape_paint_ShapeBatchBegin | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 8/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/cc/shape_paint_ShapeHidden | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/11/2/0/0/6/0/4/2/0/15/0/0/0/1 |
| Text/cc/shape_paint_PointsBounds | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 128/113/5/20/18/46/1/11/6/5/49/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchTriangle | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 376/47/6/82/69/202/16/34/28/28/74/0/0/0/8 | 234/186/9/42/39/91/4/18/8/13/120/0/0/2/11 |
| Text/cc/shape_paint_ShapeBatchFlush | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 20/0/0/0/0/7/1/2/2/1/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcDirections | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 68/28/1/10/12/30/3/4/3/3/25/0/0/0/2 | 51/17/1/3/4/14/3/3/2/2/20/0/0/0/1 |
| Text/cc/shape_paint_ShapeFadeBands | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 74/14/1/1/1/4/1/2/1/2/3/2/0/9/3 |
| Text/cc/shape_paint_ShapeBatchSector | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/29/1/9/8/19/0/2/1/3/5/0/0/0/2 | 36/15/1/3/3/11/2/2/1/1/17/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchBand | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/53/1/17/14/35/0/2/1/6/9/0/0/0/3 | 59/33/1/7/5/17/2/2/1/2/35/0/0/0/1 |
| Text/cc/paint_queue_ShapeSquareRoot | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Text/cc/std_math_Sqrt | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 161/101/4/7/3/11/0/20/16/10/47/0/2/0/16 | 102/80/5/0/0/26/4/17/11/0/57/0/0/1/9 |
| Text/cc/shape_paint_ShapeBatchAdd | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/68/1/11/6/41/5/9/8/6/12/0/0/0/12 | 84/39/3/8/8/32/6/8/5/2/44/0/0/0/6 |
| Text/cc/raster_triangles_FillTriangles | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeChannelChange | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_math_CeilInteger | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/2/0/0/7/0/0/0/0 |
| Text/cc/paint_queue_GradientChannel | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 34/0/0/0/0/7/3/4/3/0/17/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcPoint | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 12/0/0/0/0/2/1/0/0/0/8/0/0/0/0 |
| Text/cc/std_math_NotANumber | 0 | 0/157 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/std_math_Finite64 | 0 | 0/157 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 12/0/0/0/0/3/0/2/2/0/4/0/0/0/0 |
| Text/cc/std_math_ScaleByPowerOfTwo | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/96/7/9/11/20/0/19/12/6/34/0/0/0/8 | 75/48/7/0/0/15/5/17/10/0/21/0/0/0/6 |
| Text/cc/shape_paint_ShapeInside | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 23/0/0/0/0/3/2/3/3/0/5/0/0/0/0 |
| Text/cc/shape_paint_ShapeCrossing | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 44/22/1/0/0/3/1/6/5/0/31/0/0/0/1 |
| Text/cc/widget_benchmark_host_RasterTriangles | 0 | 0/157 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/1/0/6/0/0/1/2 |
| Text/clang/widget_benchmarks_BeginCase | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Text/clang/tree_draw_BeginFrame | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 |
| Text/clang/text___zi_spec_6a19f7c98e79a734 | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang/tree_draw_EndFrame | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Text/clang/tree_TreeCount | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang/tree_TreeNodeAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/clang/widget_benchmark_host_WidgetTextCount | 0 | 0/155 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/155 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang/text_request_TextRequestSend | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 |
| Text/clang/text_request_TextRequestClear | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Text/clang/widget_benchmark_host_WidgetPaintReset | 0 | 0/155 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/clang/tree_input_PointerSample | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Text/clang/tree_input_SecondaryPointerSample | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Text/clang/session_SessionFlagSet | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/paint_queue_PaintSnapshotInvalidate | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Text/clang/tree_input_TreeKeyboardRoute | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 374/232/17/41/14/130/42/58/41/21/26/4/0/5/33 |
| Text/clang/text_editing_TextEditingFrameBegin | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Text/clang/paint_queue_PaintClear | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Text/clang/tree_TreeStart | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Text/clang/tree_TreeFrameStatus | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Text/clang/session_SessionFlagGet | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang/tree_draw_CommitFrame | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Text/clang/paint_queue_PaintSnapshotEmit | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 |
| Text/clang/tree_input_TreeInputAllowedAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Text/clang/tree_TreeTextRange | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 |
| Text/clang/tree_input_TreeFocusAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Text/clang/tree_input_TreeFocusedAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Text/clang/tree_TreeInputAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Text/clang/geometry_RectangleIntersection | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Text/clang/tree_TreePointerActivatable | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Text/clang/tree_input_TreeActivateAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 |
| Text/clang/cursor_CursorHoverFrameStart | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Text/clang/std_math_Finite32 | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang/tree_TreeRootBounds | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 |
| Text/clang/tree_TreeIsBuilding | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang/tree_TreeSetStyleClass | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang/tree_TreeSetSemantic | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Text/clang/text_TextHiddenLines | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/40/2/11/2/22/0/11/9/10/22/0/0/0/4 | 116/20/2/2/1/18/1/11/9/3/55/1/0/0/2 |
| Text/clang/tree_TreeSetHiddenLines | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang/text_selection_host_TextSelectionPresent | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 |
| Text/clang/tree_draw_FrameInputCommit | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Text/clang/paint_queue_PaintOverflow | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/clang/tree_TreeCancel | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Text/clang/paint_queue_PaintOrder | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Text/clang/paint_queue_SnapshotTextRead | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Text/clang/paint_queue_SnapshotTextJoin | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Text/clang/paint_queue_PaintPointCount | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Text/clang/tree_input_TreeTopModalLayer | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang/std_grapheme_Push | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 |
| Text/clang/tree_TreeParentGeneration | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Text/clang/tree_TreeExplicitExists | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Text/clang/tree_TreeFindOld | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 |
| Text/clang/text_DefaultTextFace | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 |
| Text/clang/text_behavior_ResolveTextStyle | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 56/0/0/5/0/8/0/3/3/2/7/0/0/2/0 |
| Text/clang/style_StyleFontValue | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Text/clang/drawing_props_ColorFromPacked | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/surface_Opacity | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Text/clang/style_StyleOpacityValue | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Text/clang/font_metrics_GlyphLineHeight | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/text_behavior_TextWrapPolicy | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang/text_behavior_TextExtent | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/clang/text_layout_TextLayoutTotalHeight | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 |
| Text/clang/tree_TreeCurrentParent | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Text/clang/tree_TreeEnsureSemantic | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/9/0/0/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Text/clang/text_behavior_TextAlignmentOffset | 0 | 0/155 0% | calls/inlining candidates | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 |
| Text/clang/text_layout_TextCharactersEnd | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 |
| Text/clang/text_TextSpan | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/clang/font_metrics_GlyphWidth | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/text_TextEllipsisStart | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/84/5/23/14/42/1/10/5/4/0/0/0/0/11 | 53/21/3/2/2/10/3/7/4/1/0/0/0/0/2 |
| Text/clang/text_layout_TextLineXFor | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 |
| Text/clang/text_PaintTextLineRuns | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 339/321/6/109/109/237/12/13/7/13/36/0/0/0/14 | 312/241/14/78/50/161/9/25/11/16/93/2/0/2/18 |
| Text/clang/text_behavior_TextStrikethroughBounds | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 |
| Text/clang/text_PaintTextRule | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 102/0/0/26/21/52/0/4/2/6/53/0/0/0/0 |
| Text/clang/cursor_CursorHoverFrameCommit | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Text/clang/paint_queue_PaintMeshPointCount | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Text/clang/paint_queue_ResolvePaintText | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Text/clang/tree_TreeModalLayer | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang/std_utf8_Continuation | 0 | 0/155 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/std_grapheme_data_Property | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Text/clang/std_grapheme_IsControl | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/session_WidgetPartKey | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang/widget_key_WidgetRepeatedPart | 0 | 0/155 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/text_style_TextStyleFor | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 |
| Text/clang/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/155 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/font_metrics_GlyphMetricsRevision | 0 | 0/155 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang/text_layout_TextLineCacheMatches | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 |
| Text/clang/tree_TreeLabelSpanAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Text/clang/text_TextLinkRunAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 51/23/7/2/0/9/4/14/7/0/0/0/0/0/2 |
| Text/clang/text_TextLinkColor | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 |
| Text/clang/paint_queue_ClipLabel | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Text/clang/text_TextRangeValid | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 25/0/0/0/0/2/2/3/3/0/0/0/0/1/0 |
| Text/clang/paint_queue_PaintClippedEmphasis | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 |
| Text/clang/paint_queue_PaintRoundedFill | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Text/clang/shape_paint_PaintTriangleShape | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 |
| Text/clang/paint_queue_PendingTextValue | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Text/clang/drawing_props_StyleTokenColorOr | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/theme_CurrentAccent | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang/text_typeface_TextTypefaceUse | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang/raster_text_PaintTextRotated | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang/raster_text_PaintTextItalicClipped | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Text/clang/raster_text_PaintTextEmphasisClipped | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Text/clang/raster_text_PaintText | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/paint_queue_EmitPaintNow | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 |
| Text/clang/tree_TreeCurrentPaintGeometry | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Text/clang/paint_queue_KeepPaintMesh | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 |
| Text/clang/paint_queue_ClipShape | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Text/clang/shape_paint_ShapeOffset | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang/shape_paint_SubmitShape | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 |
| Text/clang/text_emphasis_RunTextItalicRaster | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang/text_emphasis_RunTextEmphasisRaster | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Text/clang/paint_queue_KeepPaintBytes | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Text/clang/paint_queue_KeepMeshPoints | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Text/clang/paint_queue_ShapeVisible | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Text/clang/paint_queue_ShapeWhole | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Text/clang/raster_shape_FillRoundedRectangle | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/paint_queue_FillShapeClipped | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 |
| Text/clang/shape_paint_EmitShape | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 |
| Text/clang/paint_queue_SubmitPaint | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 |
| Text/clang/theme_DarkPalette | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang/theme_DefaultMetrics | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Text/clang/theme_DefaultMotion | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Text/clang/theme_DefaultFonts | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/clang/text_layout_TextNoBreakBefore | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/7/4/0/0/0/0/1/7 |
| Text/clang/text_layout_TextNoBreakAfter | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 |
| Text/clang/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Text/clang/paint_queue_ShapeRadius | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Text/clang/paint_queue_FillClippedSpan | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Text/clang/shape_paint_EmitLineShape | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 |
| Text/clang/shape_paint_EmitRingShape | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 174/55/3/27/13/76/9/19/16/9/97/0/0/0/0 |
| Text/clang/shape_paint_EmitTriangleShape | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 |
| Text/clang/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang/widget_benchmark_host_RecordBounds | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Text/clang/shape_paint_ShapeAt | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 |
| Text/clang/shape_paint_LineCornersFor | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 |
| Text/clang/shape_paint_ShapeBatchBegin | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/clang/shape_paint_ShapeHidden | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 |
| Text/clang/shape_paint_PointsBounds | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 128/113/5/20/18/46/1/11/6/5/49/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/5 |
| Text/clang/shape_paint_ShapeBatchFlush | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 |
| Text/clang/shape_paint_ShapeArcDirections | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 68/28/1/10/12/30/3/4/3/3/25/0/0/0/2 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 |
| Text/clang/shape_paint_ShapeFadeBands | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 |
| Text/clang/shape_paint_ShapeBatchSector | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/29/1/9/8/19/0/2/1/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 |
| Text/clang/shape_paint_ShapeBatchBand | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/53/1/17/14/35/0/2/1/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 |
| Text/clang/paint_queue_ShapeSquareRoot | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Text/clang/shape_paint_ShapeBatchAdd | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/68/1/11/6/41/5/9/8/6/12/0/0/0/12 | 62/0/0/6/4/21/3/6/6/2/28/0/0/0/0 |
| Text/clang/shape_paint_ShapeClipSide | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 |
| Text/clang/raster_triangles_FillTriangles | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/shape_paint_ShapeChannelChange | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/std_math_CeilInteger | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 |
| Text/clang/paint_queue_GradientChannel | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 |
| Text/clang/shape_paint_ShapeArcPoint | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 |
| Text/clang/std_math_NotANumber | 0 | 0/155 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 |
| Text/clang/std_math_Finite64 | 0 | 0/155 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang/std_math_ScaleByPowerOfTwo | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/96/7/9/11/20/0/19/12/6/34/0/0/0/8 | 86/33/5/0/0/14/3/22/17/0/34/0/0/1/4 |
| Text/clang/shape_paint_ShapeTurn | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 |
| Text/clang/shape_paint_ShapeInside | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 |
| Text/clang/shape_paint_ShapeCrossing | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 |
| Text/clang/widget_benchmark_host_RasterTriangles | 0 | 0/155 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 |
| Input/cc/widget_benchmarks_BeginCase | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Input/cc/tree_draw_BeginFrame | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 |
| Input/cc/textfield_Draw | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 204/140/7/49/18/83/3/19/12/10/16/0/0/3/19 |
| Input/cc/tree_draw_EndFrame | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 |
| Input/cc/tree_input_TreePointerUpdate | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 258/199/14/30/15/124/40/31/17/10/12/18/0/1/52 |
| Input/cc/tree_TreeNodeAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 |
| Input/cc/text_request_TextRequestSend | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 |
| Input/cc/text_request_TextRequestClear | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Input/cc/std_grapheme_Boundary | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 |
| Input/cc/session_SessionOpen | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 |
| Input/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/170 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointerSample | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 |
| Input/cc/tree_input_SecondaryPointerSample | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 |
| Input/cc/session_SessionFlagSet | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Input/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 |
| Input/cc/tree_input_TreeKeyboardRoute | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 |
| Input/cc/text_editing_TextEditingFrameBegin | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Input/cc/tree_TreeStart | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 |
| Input/cc/tree_TreeFrameStatus | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 |
| Input/cc/textfield_TextFieldKey | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextSelectionRangeForLength | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuPasteText | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 4/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuOpenRequested | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 64/21/1/6/5/13/2/4/3/4/11/0/0/1/1 |
| Input/cc/system_clipboard_SystemClipboardSet | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_platform_TextInputNoteFocused | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 83/0/0/17/17/53/4/3/3/6/0/4/0/0/0 | 54/21/3/1/1/21/8/5/2/3/0/0/0/0/3 |
| Input/cc/text_context_menu_TextMenuIsOpenFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 23/5/1/0/0/5/0/4/3/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuShow | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 18/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/cc/session_SessionFlagGet | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_PaintSnapshotEmit | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 |
| Input/cc/paint_queue_PaintFlush | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 |
| Input/cc/tree_TreeIsBuilding | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/tree_input_TreeTextFocused | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 46/8/1/0/0/10/2/7/6/1/0/2/0/0/2 |
| Input/cc/tree_input_TreeTextKind | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 8/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeFocusAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 |
| Input/cc/tree_TreeCount | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/std_utf8_Valid | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 |
| Input/cc/tree_TreeTextRange | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 93/18/2/4/2/23/8/12/10/2/0/4/0/2/2 |
| Input/cc/std_grapheme_Next | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | 6/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/tree_input_TreeFocusedAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 |
| Input/cc/geometry_RectangleIntersection | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 |
| Input/cc/tree_TreePointerActivatable | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 |
| Input/cc/tree_input_TreeActivateAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/1/0/0/4 |
| Input/cc/cursor_CursorHoverFrameStart | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 |
| Input/cc/tree_TreeSubmit | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/57/0/0/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 |
| Input/cc/system_clipboard_SystemClipboardText | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 161/139/12/22/8/37/3/31/19/14/0/0/0/0/25 | 73/45/8/3/2/12/5/14/6/2/0/0/0/0/6 |
| Input/cc/input_protocol_PointInBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 25/0/0/0/0/1/1/5/5/0/14/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuOpenAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 85/34/1/14/9/44/0/8/7/7/2/0/0/0/2 | 70/12/2/5/1/19/2/8/6/3/2/0/0/0/2 |
| Input/cc/tree_TreePrevious | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 59/24/2/1/1/11/1/8/6/3/0/2/0/0/5 |
| Input/cc/tree_TreeCurrentParent | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Input/cc/tree_input_TreeTakeDragAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 172/147/13/11/5/53/2/28/15/7/21/7/0/0/20 |
| Input/cc/tree_input_TreeHasFocus | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 96/68/2/11/8/35/6/10/8/8/0/3/0/0/14 | 71/44/3/5/4/20/6/9/6/3/0/1/0/0/7 |
| Input/cc/tree_input_TreeBlurAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 35/7/1/0/0/8/3/5/4/1/0/2/0/0/2 |
| Input/cc/style_ResolveFont | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Input/cc/style_StyleFontValue | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 |
| Input/cc/text_input_TextInputScaledMetric | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 15/0/0/0/0/2/0/3/1/0/7/0/0/0/0 |
| Input/cc/textfield_TextFieldSecureDisplay | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/34/2/16/5/27/1/7/5/3/0/0/0/0/5 | 49/20/2/0/0/4/1/5/3/2/0/0/0/0/3 |
| Input/cc/font_metrics_GlyphWidth | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldScrollFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 53/0/0/0/0/5/2/4/0/0/7/0/0/2/0 |
| Input/cc/textfield_TextFieldCursorAtX | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 68/39/2/7/7/16/1/3/1/3/0/0/0/0/1 |
| Input/cc/text_input_TextPointerOffset | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 36/0/0/0/0/6/1/4/1/0/15/0/0/0/0 |
| Input/cc/textfield_TextFieldCodepoints | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 35/27/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/7/1/0/0/5/1/2/1/1/0/0/0/0/3 |
| Input/cc/text_TextSpan | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldByteAfter | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 40/21/2/0/0/2/2/5/3/1/0/0/0/0/2 |
| Input/cc/textfield_TextFieldNext | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextSelectionAfterMove | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 16/11/1/2/3/6/1/2/1/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldInputTextAllowed | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 22/8/1/0/0/5/3/5/4/0/0/0/0/0/3 |
| Input/cc/textfield_TextFieldReplacementAllowed | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 135/96/8/14/3/35/9/21/13/4/0/0/0/1/9 |
| Input/cc/text_input_TextEditCommandDecisionFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/93/9/5/17/29/3/20/11/0/0/0/0/5/14 |
| Input/cc/textfield_TextFieldEditFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 109/77/9/16/14/37/1/14/5/5/12/0/0/0/6 |
| Input/cc/text_request_TextRequestChangeFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 285/267/12/53/38/116/5/38/26/13/0/0/0/0/24 | 170/141/13/13/5/56/13/24/11/6/3/0/0/0/10 |
| Input/cc/text_input_TextFieldRevealScroll | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 26/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/cc/style_StyleOpacityValue | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 |
| Input/cc/tree_TreeSetInteractive | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Input/cc/tree_TreeSetCaptureTab | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 52/38/5/1/1/8/0/8/3/4/0/1/0/1/5 |
| Input/cc/tree_input_TreeFocusSubmittedAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 227/4/1/39/26/96/10/28/27/14/10/7/0/0/1 | 161/143/18/12/20/70/4/24/6/6/25/5/0/2/17 |
| Input/cc/tree_TreeSetWidgetState | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 186/96/1/37/14/101/12/15/14/7/0/13/0/0/18 | 153/105/7/16/5/51/17/26/19/3/8/5/0/1/15 |
| Input/cc/tree_TreeSetSemantic | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 68/0/0/2/2/13/1/8/8/3/0/1/0/1/0 |
| Input/cc/tree_TreeSetStyleClass | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Input/cc/tree_TreeSetStyleId | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Input/cc/tree_TreeSetSelected | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 |
| Input/cc/drawing_props_ColorFromPacked | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/surface_Opacity | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Input/cc/font_metrics_GlyphLineHeight | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineY | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 7/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineEndX | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeSubmittedNodeAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 |
| Input/cc/button_Draw | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 14/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/cc/system_clipboard_host_SystemClipboardWriteHost | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 21/5/1/0/0/7/3/3/2/0/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputFocusFrameBegin | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/18/17/57/6/3/3/5/0/6/0/0/0 | 52/17/2/1/1/16/6/4/2/3/0/0/0/0/3 |
| Input/cc/text_input_platform_TextInputFocused | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/25/4/0/0/7/2/5/1/3/0/0/0/0/1 |
| Input/cc/tree_TreeDefer | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 51/6/1/1/2/12/7/3/2/1/2/0/0/2/4 |
| Input/cc/text_context_menu_TextMenuDraw | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 162/147/6/15/38/92/2/14/8/5/21/0/0/2/33 |
| Input/cc/tree_draw_FrameInputCommit | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 |
| Input/cc/paint_queue_PaintOverflow | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/cc/tree_TreeCancel | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Input/cc/paint_queue_PaintOrder | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/14/0/0/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 |
| Input/cc/paint_queue_SnapshotTextRead | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_SnapshotTextJoin | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 |
| Input/cc/tree_input_TreeTopModalLayer | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeButtonLike | 0 | 0/170 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointInCircleBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 32/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/cc/std_utf8_Decode | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 |
| Input/cc/tree_TreeParentGeneration | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 |
| Input/cc/tree_TreeIdentityHash | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/system_clipboard_host_SystemClipboardByteHost | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 12/0/0/0/0/3/1/2/2/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 84/63/7/10/5/24/2/12/5/7/1/1/0/0/6 |
| Input/cc/textfield_TextFieldDefaultFace | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 65/0/0/9/12/33/0/0/0/6/24/0/0/0/0 |
| Input/cc/text_input_TextInputContentWidth | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 42/5/1/1/1/2/0/5/4/4/5/1/0/0/0 | 18/0/0/0/0/2/0/2/0/0/4/0/0/0/0 |
| Input/cc/text_input_TextUtf8Continuation | 0 | 0/170 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_utf8_Next | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/text_input_TextCursorForLength | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 10/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_CompositionBoundaryFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 26/15/2/0/0/2/2/6/4/0/0/0/0/0/3 |
| Input/cc/std_grapheme_Previous | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 59/44/2/10/4/14/0/7/5/5/0/0/0/0/4 | 33/9/1/0/0/1/1/4/3/1/0/0/0/0/0 |
| Input/cc/tree_TreeLayerAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 |
| Input/cc/tree_TreeStoreLabel | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 |
| Input/cc/paint_queue_ClipShape | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 |
| Input/cc/surface_Unit | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Input/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/170 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldCursorHeightFor | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 27/0/0/0/0/2/0/2/0/0/4/0/0/0/0 |
| Input/cc/paint_queue_ClipLabel | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 |
| Input/cc/paint_queue_EmitLine | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 83/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Input/cc/button_SubmitButton | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 622/601/18/167/120/309/20/59/41/56/52/0/0/0/60 | 447/414/9/119/78/217/3/21/10/36/134/0/0/1/35 |
| Input/cc/system_clipboard_SystemClipboardHasText | 0 | 0/170 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/contextmenu_Draw | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1020/985/24/257/265/610/28/81/57/58/87/16/0/0/95 | 542/515/31/88/88/248/32/67/36/19/103/9/0/4/70 |
| Input/cc/cursor_CursorHoverFrameCommit | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Input/cc/paint_queue_PaintMeshPointCount | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Input/cc/paint_queue_ResolvePaintText | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 |
| Input/cc/tree_TreeModalLayer | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/std_utf8_Continuation | 0 | 0/170 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_grapheme_IsControl | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/session_WidgetPartKey | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/style_sheet_StyleBucket | 0 | 0/170 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleFirstBit | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 36/0/0/0/0/5/0/0/0/0/0/0/0/5/0 |
| Input/cc/style_sheet_StyleRuleMatchesState | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleSelectorMatches | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 85/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Input/cc/style_MergeValues | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 148/0/0/59/14/82/0/15/15/0/70/0/0/0/0 |
| Input/cc/tree_TreeLabelSpanAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Input/cc/paint_queue_ShapeVisible | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 |
| Input/cc/paint_queue_ShapeWhole | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 |
| Input/cc/raster_shape_FillRoundedRectangle | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_FillShapeClipped | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 |
| Input/cc/paint_queue_EmitPaintNow | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 |
| Input/cc/tree_TreeCurrentPaintGeometry | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 |
| Input/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_typeface_TextTypefaceUse | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Input/cc/raster_text_PaintTextRotated | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 |
| Input/cc/raster_text_PaintTextItalicClipped | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 |
| Input/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 |
| Input/cc/raster_text_PaintTextClipped | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 |
| Input/cc/raster_text_PaintText | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/raster_StrokeLine | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/style_ResolveFlags | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Input/cc/style_ResolveState | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Input/cc/tree_input_TreePressedAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 28/0/0/0/0/5/2/1/1/2/0/3/0/0/0 |
| Input/cc/tree_input_TreeHoveredAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/39/4/3/3/15/4/8/4/3/0/4/0/0/7 |
| Input/cc/button_ButtonDetailSize | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 41/0/0/10/1/11/0/4/4/2/7/2/0/2/0 |
| Input/cc/tree_TreeRootBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Input/cc/button_behavior_MeasureBounds | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 263/223/22/59/30/100/3/41/19/5/145/0/0/0/13 |
| Input/cc/style_StyleMinHeightValue | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/button_ButtonMinimumHeight | 0 | 0/170 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/button_ButtonFitContentFace | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 377/294/13/41/19/86/4/61/48/25/89/0/0/0/41 | 203/183/15/35/11/67/5/31/14/1/113/0/0/0/10 |
| Input/cc/tree_TreeSetCircleHit | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/8/0/6/6/2/0/2/0/1/0 |
| Input/cc/tree_TreeSetSemanticDetail | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Input/cc/button_PaintButtonForNode | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 916/893/51/266/138/459/11/100/43/43/383/2/0/8/84 |
| Input/cc/tree_TreeNextKey | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/6/1/1/1/12/4/5/4/2/0/7/0/2/1 |
| Input/cc/contextmenu_ContextMenuPanelKey | 0 | 0/170 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePreviousCommitted | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 42/8/1/2/2/9/0/4/3/2/0/2/0/0/2 |
| Input/cc/contextmenu_ContextMenuRowKey | 0 | 0/170 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/4/3/0/0/0/0/0/0/1/0 |
| Input/cc/contextmenu_ContextMenuOpens | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 23/7/2/0/0/6/1/6/4/0/0/1/0/0/2 |
| Input/cc/contextmenu_ContextMenuFind | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/1/5/3/0/0/0/0/0/3 |
| Input/cc/contextmenu_ContextMenuBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuSubmenuBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 115/43/1/32/26/64/1/3/2/6/46/1/0/0/1 | 83/0/0/10/8/19/1/2/1/4/46/1/0/0/0 |
| Input/cc/overlay_Draw | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 259/229/10/44/27/80/4/24/14/20/33/0/0/5/32 |
| Input/cc/contextmenu_ContextMenuLevel | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 1459/1242/10/375/368/800/10/72/62/142/339/1/0/0/114 | 1062/861/26/302/118/483/14/69/31/103/299/1/0/3/119 |
| Input/cc/paint_queue_PendingTextValue | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 |
| Input/cc/style_sheet_StyleDefaultFacts | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/3/0/0/0/0/5/0/0/0/0 |
| Input/cc/style_sheet_StyleMatchesOptional | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 |
| Input/cc/paint_queue_ShapeRadius | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 |
| Input/cc/paint_queue_FillShapeRows | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/248/9/54/29/83/0/23/14/12/143/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 |
| Input/cc/paint_queue_FillClippedSpan | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 |
| Input/cc/paint_queue_KeepMeshPoints | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 194/165/10/21/26/76/11/29/19/5/21/4/0/1/22 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Input/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 |
| Input/cc/text_emphasis_RunTextItalicRaster | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterTextClipped | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 |
| Input/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Input/cc/button_ButtonDefaultFace | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 227/179/12/38/30/96/7/27/15/16/39/0/0/0/25 |
| Input/cc/button_ButtonFaceFacts | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 46/31/1/8/1/10/0/3/2/4/0/0/0/0/3 |
| Input/cc/button_ButtonDetailFont | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 11/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Input/cc/style_StyleLineHeightValue | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/style_FitHeight | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 19/0/0/0/0/0/0/1/1/0/16/0/0/0/0 |
| Input/cc/button_behavior_MeasureContent | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 176/152/5/4/13/19/0/28/23/9/58/0/0/0/14 | 74/38/3/0/0/5/2/9/6/0/45/0/0/0/0 |
| Input/cc/button_behavior_ShapeWidth | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 27/0/0/0/0/3/2/4/4/0/12/0/0/0/0 |
| Input/cc/theme_ControlHeightFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 37/23/3/5/2/14/1/7/4/2/8/0/0/0/3 |
| Input/cc/button_behavior_ButtonCornerRadius | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 9/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Input/cc/focus_FocusDefaultOutlineBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 45/22/2/2/1/10/2/6/2/0/28/0/0/0/2 |
| Input/cc/image_lookup_IconLookupImage | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 28/12/1/1/1/6/1/3/2/2/0/0/0/0/2 |
| Input/cc/button_behavior_ButtonPictureSize | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 16/0/0/0/0/1/0/2/2/0/9/0/0/0/0 |
| Input/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 99/53/4/0/0/0/0/11/7/7/34/0/0/0/7 | 23/15/1/0/0/6/1/3/2/0/16/0/0/0/0 |
| Input/cc/button_behavior_ContentLayout | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 240/176/5/37/33/79/0/26/21/9/108/0/0/0/12 | 124/89/8/2/0/14/3/17/9/1/77/0/0/0/1 |
| Input/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 371/316/4/71/71/157/3/27/23/22/113/4/1/0/36 | 225/195/7/30/25/84/9/17/8/8/91/1/0/4/21 |
| Input/cc/image_lookup_ImageLookupTexture | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 58/0/0/19/15/39/0/5/5/2/28/0/0/0/0 |
| Input/cc/image_behavior_ImageDrawPlanFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 76/36/1/15/3/27/1/8/7/1/33/0/0/1/5 |
| Input/cc/paint_queue_PaintImage | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 33/0/0/6/5/11/0/1/1/2/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 460/424/20/93/66/194/11/52/32/33/61/12/0/0/41 | 257/196/11/37/28/99/5/30/19/19/56/2/0/1/32 |
| Input/cc/dpi_ScalePixels | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 43/0/0/0/0/7/0/4/1/0/22/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuRowTop | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 101/83/5/20/9/37/4/13/8/9/25/4/0/0/12 | 86/54/7/3/7/23/5/14/7/2/24/1/0/0/11 |
| Input/cc/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 20/14/1/0/0/1/1/2/1/0/0/0/0/2/4 |
| Input/cc/tree_input_TreeTakeDismissalAt | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/25/2/4/3/14/3/7/5/5/0/0/0/0/4 |
| Input/cc/tree_TreeSetPopup | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 36/0/0/0/0/7/0/5/5/2/0/2/0/0/0 |
| Input/cc/drawing_props_StyleTokenColorOr | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePushScope | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 81/63/5/0/0/12/10/9/4/8/0/8/0/0/12 |
| Input/cc/contextmenu_ContextMenuTextLeft | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/2/2/4/0/1/1/2/2/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelChecks | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 28/13/4/0/0/8/4/7/3/0/0/0/0/0/4 |
| Input/cc/contextmenu_ContextMenuInlineWidth | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 35/0/0/2/0/4/0/2/2/4/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuInlineEnds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 23/6/1/0/0/6/1/3/2/0/0/2/0/0/0 |
| Input/cc/svg_icon_behavior_PaintStroke | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 648/600/8/74/331/442/14/31/23/21/84/0/0/0/35 | 191/96/5/19/43/82/13/12/7/5/77/0/0/1/19 |
| Input/cc/tree_End | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RecordBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 |
| Input/cc/paint_queue_ShapeCornerInset | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 |
| Input/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Input/cc/paint_queue_KeepLongPaintBytes | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 |
| Input/cc/theme_DarkPalette | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/cc/theme_StateLayer | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 55/0/0/0/0/1/1/1/1/0/0/10/0/12/0 |
| Input/cc/card_behavior_CardDefaultFace | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 115/84/6/20/13/54/2/13/7/5/32/0/0/0/10 |
| Input/cc/theme_FontForRole | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 24/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Input/cc/theme_MotionDuration | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 69/52/5/10/4/30/6/9/4/2/27/0/0/0/3 |
| Input/cc/drawing_props_PaintPixelScale | 0 | 0/170 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 4/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 123/104/6/4/12/41/10/10/4/3/4/1/0/1/23 |
| Input/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 78/29/2/0/0/0/0/11/9/6/23/0/0/0/5 | 35/10/1/0/0/5/0/6/3/0/16/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/93/12/13/9/3/0/16/1/0/22 | 125/61/5/6/4/55/20/11/6/2/5/2/0/3/10 |
| Input/cc/image_behavior_ImageFitRect | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 9/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Input/cc/paint_queue_PaintImageWithPolygon | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 105/77/2/20/34/60/0/4/2/3/26/0/0/0/8 |
| Input/cc/svg_icon_behavior_StrokeAppendByte | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 7/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/cc/svg_icon_behavior_StrokeAppendNumber | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/196/7/10/16/40/7/37/30/13/11/0/6/0/35 | 121/97/6/2/7/36/13/14/6/1/6/4/0/6/25 |
| Input/cc/tree_TreePopScope | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 54/26/1/3/2/7/6/4/3/4/0/4/0/0/5 |
| Input/cc/paint_queue_ShapeSquareRoot | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Input/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 |
| Input/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 |
| Input/cc/svg_icon_behavior_SvgIconHash | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 19/6/1/0/0/5/2/2/1/0/0/1/0/0/1 |
| Input/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/27/2/3/4/24/5/4/2/1/5/0/0/1/8 |
| Input/cc/svg_shape_ShapeRaster | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/220/9/32/17/69/6/47/38/15/17/2/0/0/40 | 127/89/5/5/4/35/7/17/12/4/9/1/0/0/18 |
| Input/cc/svg_icon_behavior_SvgIconCells | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses) | 332/300/20/30/35/98/9/48/28/9/0/10/1/0/41 | 330/291/25/30/20/108/27/52/27/1/26/2/0/8/51 |
| Input/cc/image_behavior_ImageFitBounds | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/184/8/18/9/31/0/26/18/13/53/0/0/0/18 | 92/77/6/6/2/15/0/16/10/0/56/0/0/0/7 |
| Input/cc/image_behavior_ImagePolygonValid | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 103/59/1/9/1/12/1/16/15/7/16/0/0/0/4 | 35/14/1/0/0/8/2/8/7/0/12/0/0/0/0 |
| Input/cc/paint_queue_EmitImage | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Input/cc/paint_queue_ClipImage | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 17/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeNumber | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 349/310/21/56/16/91/15/62/41/19/60/1/0/0/48 | 162/151/12/0/0/48/26/32/20/0/21/0/0/0/24 |
| Input/cc/svg_shape_ShapeBuildPath | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 614/592/44/147/107/273/7/91/47/35/160/0/0/0/51 | 447/416/39/36/21/126/22/74/35/14/90/0/0/0/87 |
| Input/cc/svg_shape_ShapeCoverEdges | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 334/315/14/23/20/72/15/49/35/14/64/3/0/0/51 | 222/201/15/15/13/79/21/37/21/2/47/0/0/1/30 |
| Input/cc/paint_queue_EmitImageBegin | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 20/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterImage | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 40/0/0/4/3/11/0/2/0/1/16/0/0/1/0 |
| Input/cc/paint_queue_EmitImageEnd | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsDigit | 0 | 0/170 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsSpace | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 8/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddPoint | 0 | 0/170 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 15/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddSpan | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 139/123/3/0/0/16/4/16/13/11/40/0/0/0/22 | 110/13/2/0/0/21/11/13/7/0/42/0/0/2/2 |
| Input/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 614/531/11/90/49/179/20/57/46/38/279/0/3/0/62 | 367/340/21/19/10/94/24/47/25/7/185/0/3/0/38 |
| Input/cc/svg_shape_ShapeFillSubpath | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 6/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeCollapse | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 71/67/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/19/7/7/4/0/14/0/0/0/4 |
| Input/cc/svg_shape_ShapeAddCircle | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 53/35/3/0/0/13/5/8/5/0/19/0/0/1/10 |
| Input/cc/svg_shape_ShapeSqrt | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 21/6/1/0/0/4/0/2/1/0/12/0/0/0/1 |
| Input/cc/svg_shape_ShapeQuad | 0 | 0/170 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 11/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeAbs | 0 | 0/170 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 10/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeTriangle | 0 | 0/170 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 9/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddEdge | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 20/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Input/cc/svg_shape_ShapeAddPolygon | 0 | 0/170 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates | 102/87/3/4/5/27/12/13/10/4/27/0/2/0/14 | 95/68/5/0/0/27/19/15/10/0/23/0/2/2/13 |
| Input/clang/widget_benchmarks_BeginCase | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Input/clang/tree_draw_BeginFrame | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 |
| Input/clang/textfield_Draw | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 121/0/0/12/6/37/2/9/9/9/2/0/0/0/0 |
| Input/clang/tree_draw_EndFrame | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Input/clang/tree_input_TreePointerUpdate | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 210/110/2/8/1/65/22/28/26/7/18/6/0/4/23 |
| Input/clang/tree_TreeNodeAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang/widget_benchmark_host_WidgetTextCount | 0 | 0/167 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/167 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_request_TextRequestSend | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 |
| Input/clang/text_request_TextRequestClear | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Input/clang/widget_benchmark_host_WidgetPaintReset | 0 | 0/167 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_input_SecondaryPointerSample | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Input/clang/paint_queue_PaintSnapshotInvalidate | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Input/clang/tree_input_TreeKeyboardRoute | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 640/576/16/94/89/247/24/96/80/59/16/7/0/0/98 | 373/232/17/41/14/130/43/58/41/21/26/5/0/4/33 |
| Input/clang/text_editing_TextEditingFrameBegin | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Input/clang/tree_TreeFrameStatus | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Input/clang/textfield_TextFieldKey | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuTakeCommand | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 147/106/4/23/10/47/0/27/23/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 |
| Input/clang/text_context_menu_TextMenuPasteText | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuOpenRequested | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 47/0/0/4/4/8/0/4/4/4/10/0/0/0/0 |
| Input/clang/system_clipboard_SystemClipboardSet | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/text_input_platform_TextInputNoteFocused | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 83/0/0/17/17/53/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuIsOpenFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuShow | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/clang/session_SessionFlagGet | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang/tree_draw_CommitFrame | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Input/clang/paint_queue_PaintSnapshotEmit | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 628/346/8/126/167/421/21/23/15/29/36/5/0/0/29 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 |
| Input/clang/tree_TreeIsBuilding | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang/tree_input_TreeHitAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Input/clang/tree_input_TreeTextFocused | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Input/clang/tree_input_TreeTextKind | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_input_TreeFocusAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Input/clang/tree_TreeInputAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Input/clang/tree_TreeRead | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Input/clang/tree_TreeCount | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang/tree_input_TreeInputAllowedAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Input/clang/tree_TreeTextRange | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 |
| Input/clang/tree_input_TreeFocusedAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Input/clang/geometry_RectangleIntersection | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Input/clang/tree_input_TreeActivateAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Input/clang/cursor_CursorHoverFrameStart | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Input/clang/tree_TreeBuildOldIndex | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/121/17/22/16/8/2/15/0/0/45 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 |
| Input/clang/system_clipboard_SystemClipboardText | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 161/139/12/22/8/37/3/31/19/14/0/0/0/0/25 | 63/15/2/0/0/5/1/9/7/2/0/0/0/0/1 |
| Input/clang/input_protocol_PointInBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuOpenAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 85/34/1/14/9/44/0/8/7/7/2/0/0/0/2 | 64/12/2/5/0/19/1/6/4/4/2/0/0/0/1 |
| Input/clang/tree_TreePrevious | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Input/clang/tree_input_TreeTakeActivationAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang/tree_TreePreviousWidgetState | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 |
| Input/clang/tree_input_TreeHasFocus | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 96/68/2/11/8/35/6/10/8/8/0/3/0/0/14 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 |
| Input/clang/tree_input_TreeBlurAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 |
| Input/clang/style_ResolveFont | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Input/clang/style_StyleFontValue | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Input/clang/textfield_TextFieldSecureDisplay | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/34/2/16/5/27/1/7/5/3/0/0/0/0/5 | 53/8/1/0/0/2/2/5/4/2/0/0/0/0/1 |
| Input/clang/font_metrics_GlyphWidth | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/text_input_TextFieldScrollFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 |
| Input/clang/textfield_TextFieldCursorAtX | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 |
| Input/clang/text_input_TextPointerOffset | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 |
| Input/clang/textfield_TextFieldCodepoints | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 35/27/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 |
| Input/clang/text_TextSpan | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/clang/textfield_TextFieldByteAfter | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 |
| Input/clang/textfield_TextFieldNext | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/text_input_TextSelectionAfterMove | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 |
| Input/clang/textfield_TextFieldInputTextAllowed | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 24/11/1/0/0/3/2/4/3/0/0/0/0/0/3 |
| Input/clang/textfield_TextFieldReplacementAllowed | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 |
| Input/clang/text_input_TextEditCommandDecisionFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 91/0/0/2/0/2/0/13/13/0/0/0/0/6/0 |
| Input/clang/textfield_TextFieldEditFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 |
| Input/clang/text_request_TextRequestTake | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 198/0/0/47/28/118/10/16/16/11/0/9/0/0/0 | 67/0/0/1/0/17/1/7/7/4/3/1/0/0/0 |
| Input/clang/text_request_TextRequestChangeFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 285/267/12/53/38/116/5/38/26/13/0/0/0/0/24 | 147/55/6/6/12/41/8/22/16/5/0/0/0/2/0 |
| Input/clang/font_metrics_GlyphCaret | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 31/0/0/1/3/8/0/2/2/1/2/0/0/0/0 |
| Input/clang/text_input_TextFieldRevealScroll | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang/style_StyleOpacityValue | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Input/clang/tree_TreeSetInteractive | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Input/clang/tree_TreeSetCaptureTab | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 |
| Input/clang/tree_input_TreeFocusSubmittedAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 227/4/1/39/26/96/10/28/27/14/10/7/0/0/1 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 |
| Input/clang/tree_TreeSetWidgetState | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 186/96/1/37/14/101/12/15/14/7/0/13/0/0/18 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 |
| Input/clang/tree_TreeSetSemantic | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Input/clang/tree_TreeSetLabel | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang/tree_TreeSetStyleClass | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang/tree_TreeSetStyleId | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang/tree_TreeSetSelected | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang/tree_TreeSetTextValue | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 285/227/4/47/34/166/24/19/15/10/0/23/0/0/38 | 138/86/3/8/4/48/13/22/19/5/0/1/0/2/6 |
| Input/clang/drawing_props_ColorFromPacked | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/surface_Opacity | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Input/clang/font_metrics_GlyphLineHeight | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentMuted | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentAccent | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/clang/text_input_TextCompositionUnderlineY | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang/text_input_TextCompositionUnderlineEndX | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/paint_queue_PaintLine | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Input/clang/tree_TreeSubmittedNodeAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang/button_Draw | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/clang/text_input_platform_TextInputFocusFrameBegin | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/18/17/57/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 |
| Input/clang/text_input_platform_TextInputFocused | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 |
| Input/clang/text_input_platform_TextInputOwnerMatches | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/tree_TreeDefer | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 |
| Input/clang/text_context_menu_TextMenuDraw | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 143/0/0/6/36/78/0/12/12/5/18/0/0/2/0 |
| Input/clang/tree_draw_FrameInputCommit | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Input/clang/paint_queue_PaintOverflow | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/clang/tree_TreeCancel | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Input/clang/tree_TreeFinish | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Input/clang/paint_queue_SnapshotTextRead | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Input/clang/paint_queue_SnapshotTextJoin | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Input/clang/paint_queue_PaintPointCount | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Input/clang/tree_input_TreeTopModalLayer | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/tree_TreeButtonLike | 0 | 0/167 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_input_PointInCircleBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/clang/tree_TreeOverlayLayerAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/tree_TreeParentGeneration | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Input/clang/tree_TreeIdentityHash | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang/tree_TreeExplicitExists | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Input/clang/system_clipboard_host_SystemClipboardOpenHost | 0 | 0/167 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang/system_clipboard_host_SystemClipboardByteHost | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 |
| Input/clang/tree_input_TreeTakePointerActivationAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 81/12/1/6/0/13/3/12/11/6/5/1/0/2/3 |
| Input/clang/style_sheet_StyleControlRoleFacts | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Input/clang/textfield_TextFieldDefaultFace | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 |
| Input/clang/style_sheet_ResolveInstalledStyleRules | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 267/236/11/58/44/144/24/25/14/11/14/6/0/0/37 | 253/196/17/22/19/107/14/33/16/4/0/0/0/12/36 |
| Input/clang/text_input_TextInputContentWidth | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 42/5/1/1/1/2/0/5/4/4/5/1/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Input/clang/text_input_TextUtf8Continuation | 0 | 0/167 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_input_TextCursorForLength | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_input_CompositionBoundaryFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 25/7/1/0/0/3/2/3/2/0/0/0/0/0/2 |
| Input/clang/tree_TreeEnsureInteraction | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/9/0/0/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Input/clang/tree_TreeLayerAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Input/clang/tree_TreeStoreLabel | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 447/422/15/79/30/177/25/50/35/21/0/17/1/1/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 |
| Input/clang/paint_queue_ClipShape | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Input/clang/surface_Unit | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Input/clang/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/167 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_input_TextFieldCursorHeightFor | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Input/clang/paint_queue_EmitLabel | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Input/clang/paint_queue_ClipLabel | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Input/clang/paint_queue_EmitLine | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Input/clang/button_SubmitButton | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 622/601/18/167/120/309/20/59/41/56/52/0/0/0/60 | 376/10/1/57/34/129/0/17/16/38/33/2/0/3/1 |
| Input/clang/system_clipboard_SystemClipboardHasText | 0 | 0/167 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang/contextmenu_Draw | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1020/985/24/257/265/610/28/81/57/58/87/16/0/0/95 | 553/476/26/87/96/234/31/71/45/19/102/14/0/3/59 |
| Input/clang/cursor_CursorHoverFrameCommit | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Input/clang/paint_queue_PaintMeshPointCount | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang/paint_queue_ResolvePaintText | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Input/clang/tree_TreeModalLayer | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang/std_utf8_Continuation | 0 | 0/167 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/std_grapheme_IsControl | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/session_WidgetPartKey | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang/widget_key_WidgetRepeatedPart | 0 | 0/167 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/theme_CurrentSunken | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentText | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentBorderStrong | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_TypeLabel | 0 | 0/167 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang/style_sheet_StyleBucket | 0 | 0/167 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/style_sheet_StyleFirstBit | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Input/clang/style_sheet_StyleRuleMatchesState | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/style_sheet_StyleSelectorMatches | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Input/clang/style_MergeValues | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Input/clang/tree_TreeLabelSpanAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Input/clang/paint_queue_ShapeWhole | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Input/clang/raster_shape_FillRoundedRectangle | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/paint_queue_FillShapeClipped | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 181/164/5/50/39/89/0/12/7/11/86/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 |
| Input/clang/paint_queue_EmitPaintNow | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/2/0/0/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 |
| Input/clang/tree_TreeCurrentPaintGeometry | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Input/clang/paint_queue_KeepPaintMesh | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 |
| Input/clang/raster_shape_StrokeRoundedRectangle | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/text_typeface_TextTypefaceUse | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang/raster_text_PaintTextRotated | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang/raster_text_PaintTextItalicClipped | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Input/clang/raster_text_PaintTextEmphasisClipped | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Input/clang/raster_text_PaintTextClipped | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang/raster_text_PaintText | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/raster_StrokeLine | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/style_ResolveFlags | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Input/clang/style_ResolveState | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Input/clang/tree_input_TreeHoveredAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Input/clang/button_ButtonDetailSize | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/3/0/8/0/4/4/2/5/2/0/1/0 |
| Input/clang/tree_TreeRootBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 |
| Input/clang/button_behavior_MeasureBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 190/0/0/19/10/49/0/23/23/2/120/0/0/0/0 |
| Input/clang/style_StyleMinHeightValue | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang/button_ButtonMinimumHeight | 0 | 0/167 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/button_ButtonFitContentFace | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 377/294/13/41/19/86/4/61/48/25/89/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/1 |
| Input/clang/tree_TreeSetCircleHit | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Input/clang/tree_TreeSetSemanticDetail | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang/button_PaintButtonForNode | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 793/0/0/161/116/384/0/54/54/41/331/4/0/7/0 |
| Input/clang/tree_TreeNextKey | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 |
| Input/clang/contextmenu_ContextMenuOverlayKey | 0 | 0/167 0% |  | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang/contextmenu_ContextMenuPanelKey | 0 | 0/167 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_TreePreviousCommitted | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 |
| Input/clang/contextmenu_ContextMenuRowKey | 0 | 0/167 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 |
| Input/clang/contextmenu_ContextMenuOpens | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 |
| Input/clang/contextmenu_ContextMenuFind | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 |
| Input/clang/contextmenu_ContextMenuBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/overlay_Draw | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 201/82/1/16/5/38/0/16/15/19/35/0/0/1/9 |
| Input/clang/contextmenu_ContextMenuLevel | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1459/1242/10/375/368/800/10/72/62/142/339/1/0/0/114 | 939/718/13/213/93/370/25/55/42/101/293/2/0/1/93 |
| Input/clang/paint_queue_PendingTextValue | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Input/clang/style_sheet_StyleMatchesOptional | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Input/clang/paint_queue_ShapeRadius | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/45/3/4/0/4/0/7/4/3/23/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Input/clang/paint_queue_FillClippedSpan | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Input/clang/paint_queue_KeepPaintBytes | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Input/clang/paint_queue_KeepMeshPoints | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/4/0/0/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Input/clang/text_emphasis_RunTextItalicRaster | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang/text_emphasis_RunTextEmphasisRaster | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Input/clang/button_ButtonFaceFinish | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Input/clang/button_ButtonDefaultFace | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Input/clang/button_ButtonFaceFacts | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Input/clang/button_ButtonDetailFont | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Input/clang/style_StyleLineHeightValue | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang/style_FitHeight | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Input/clang/button_behavior_MeasureContent | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 176/152/5/4/13/19/0/28/23/9/58/0/0/0/14 | 44/0/0/0/0/1/0/8/8/0/29/0/0/0/0 |
| Input/clang/button_behavior_ShapeWidth | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Input/clang/theme_ControlHeightFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Input/clang/button_behavior_ButtonCornerRadius | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Input/clang/focus_FocusDefaultOutlineBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 27/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Input/clang/image_lookup_IconLookupImage | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Input/clang/button_behavior_ButtonPictureSize | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Input/clang/button_behavior_ButtonLabeledPictureSize | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 99/53/4/0/0/0/0/11/7/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Input/clang/button_behavior_ContentLayout | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 240/176/5/37/33/79/0/26/21/9/108/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/87/0/0/0/0 |
| Input/clang/svg_icon_behavior_PaintSvgIcon | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 371/316/4/71/71/157/3/27/23/22/113/4/1/0/36 | 246/35/1/23/20/69/6/17/16/6/128/1/0/1/3 |
| Input/clang/image_lookup_ImageLookupTexture | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Input/clang/image_behavior_ImageDrawPlanFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Input/clang/contextmenu_ContextMenuLevelBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 460/424/20/93/66/194/11/52/32/33/61/12/0/0/41 | 273/193/8/42/16/91/6/30/22/22/45/1/0/0/32 |
| Input/clang/dpi_ScalePixels | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 |
| Input/clang/contextmenu_ContextMenuRowTop | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 101/83/5/20/9/37/4/13/8/9/25/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 |
| Input/clang/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 21/0/0/0/0/0/0/0/0/0/0/0/0/3/0 |
| Input/clang/tree_input_TreeTakeDismissalAt | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 |
| Input/clang/tree_TreeSetPopup | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 |
| Input/clang/drawing_props_StyleTokenColorOr | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/theme_CurrentSurface | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentBorder | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentDisabled | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentSelection | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/tree_TreePushScope | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 |
| Input/clang/contextmenu_ContextMenuTextLeft | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 |
| Input/clang/contextmenu_ContextMenuLevelChecks | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 25/14/1/0/0/5/0/3/2/0/0/0/0/0/4 |
| Input/clang/contextmenu_ContextMenuInlineWidth | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 |
| Input/clang/contextmenu_ContextMenuInlineEnds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 |
| Input/clang/svg_icon_behavior_PaintStroke | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 648/600/8/74/331/442/14/31/23/21/84/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 |
| Input/clang/widget_benchmark_host_RecordBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Input/clang/theme_DarkPalette | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang/theme_StateLayer | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Input/clang/card_behavior_CardDefaultFace | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 73/0/0/6/0/29/1/9/8/3/12/0/0/0/0 |
| Input/clang/theme_ShapeMedium | 0 | 0/167 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang/theme_FontForRole | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Input/clang/theme_MotionDuration | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Input/clang/drawing_props_PaintPixelScale | 0 | 0/167 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/clang/svg_icon_behavior_SvgIconSlot | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 164/41/4/6/7/61/19/20/16/3/10/5/0/0/4 |
| Input/clang/svg_icon_behavior_SvgIconSnap | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 78/29/2/0/0/0/0/11/9/6/23/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Input/clang/svg_icon_behavior_SvgIconTexture | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/93/12/13/9/3/0/16/1/0/22 | 192/156/12/7/3/67/28/24/12/1/9/2/0/10/39 |
| Input/clang/paint_queue_PaintImageWithPolygon | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Input/clang/svg_icon_behavior_StrokeAppendByte | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/clang/svg_icon_behavior_StrokeAppendNumber | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/196/7/10/16/40/7/37/30/13/11/0/6/0/35 | 95/25/2/1/4/27/12/9/7/1/6/4/0/4/4 |
| Input/clang/tree_TreePopScope | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 42/0/0/0/0/6/4/5/5/4/0/4/0/0/0 |
| Input/clang/paint_queue_ShapeSquareRoot | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Input/clang/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/194/9/29/25/107/15/28/19/10/12/5/0/0/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang/theme_ShapeSurface | 0 | 0/167 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang/svg_icon_behavior_SvgIconRetire | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Input/clang/svg_shape_ShapeRaster | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/220/9/32/17/69/6/47/38/15/17/2/0/0/40 | 129/113/4/3/3/27/9/17/13/4/8/1/0/0/21 |
| Input/clang/image_behavior_ImageFitBounds | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 204/184/8/18/9/31/0/26/18/13/53/0/0/0/18 | 43/0/0/0/0/3/0/9/9/0/30/0/0/0/0 |
| Input/clang/image_behavior_ImagePolygonValid | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 103/59/1/9/1/12/1/16/15/7/16/0/0/0/4 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Input/clang/paint_queue_EmitImage | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Input/clang/paint_queue_ClipImage | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Input/clang/svg_shape_ShapeNumber | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 349/310/21/56/16/91/15/62/41/19/60/1/0/0/48 | 179/142/8/0/0/31/14/38/30/0/33/0/0/0/25 |
| Input/clang/svg_shape_ShapeCoverEdges | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 334/315/14/23/20/72/15/49/35/14/64/3/0/0/51 | 192/158/16/10/9/58/22/28/12/3/45/0/0/3/12 |
| Input/clang/paint_queue_EmitImageBegin | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Input/clang/widget_benchmark_host_RasterImage | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Input/clang/paint_queue_EmitImageEnd | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Input/clang/svg_shape_ShapeIsDigit | 0 | 0/167 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/svg_shape_ShapeIsSpace | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang/svg_shape_ShapeAddPoint | 0 | 0/167 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Input/clang/svg_shape_ShapeAddSpan | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 139/123/3/0/0/16/4/16/13/11/40/0/0/0/22 | 91/16/2/0/0/22/17/14/12/1/39/0/0/0/1 |
| Input/clang/svg_shape_ShapeStrokeSubpath | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 614/531/11/90/49/179/20/57/46/38/279/0/3/0/62 | 575/417/24/43/25/189/49/61/37/8/319/0/3/10/72 |
| Input/clang/svg_shape_ShapeCollapse | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/67/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Input/clang/svg_shape_ShapeAddCircle | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Input/clang/svg_shape_ShapeQuad | 0 | 0/167 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Input/clang/svg_shape_ShapeAbs | 0 | 0/167 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Input/clang/svg_shape_ShapeTriangle | 0 | 0/167 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Input/clang/svg_shape_ShapeAddEdge | 0 | 0/167 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Hello/clang/hello_main | 0 | 0/0 unknown |  | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
