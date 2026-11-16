# C teacher: native assembly comparison

Base revision: 36e6caf9643d82b58434f11aad507da79f895922
Host: alpha
build/bin/zi2c SHA-256: 2e21cbdd222519dc2f54993721c5168719b0340fc00d4271c4ca101dd8300152
build/bin/zi2zx SHA-256: 50f84b648068e97d7e7ded2937f8cc4400edf5ba9a9b113f61e280275972777f
build/bin/zx2native SHA-256: 3e4ce36573c6bc0857b3a571e10088c24464093b5d08a1a7369a1af0cc75891f
Learning off. Generated C compiled with -O2; native compiled from verified ZX. Function extents come from the native map; C symbols match module_name or exported name exactly. Absent symbols are not evidence of inlining. Metrics: instructions/loop-union/back-edges/stack-reads/stack-writes/memory/indexed/branches/forward-branches/calls/vector-register-uses/mul/div/shifts/loop-immediate-or-PC-relative candidates. Stack traffic includes locals and saves. Vector-register uses include scalar floating point. Loop constants are LICM candidates, not proven invariants. Back edges describe loop bodies, not execution frequency. SIGPROF uses a 1ms CPU timer, at most 30 runs; zero samples means unknown time share. Sampling includes process startup and runtime. Ranking weights relative instruction excess by observed function samples; it predicts neither cycles nor speedup.

## Sieve / cc
Native SHA-256: f8190c4ddf6088aee7d4a38e89b3ff80094393c754d79f0d40778ab31dc29a96
C SHA-256: 5f026028c7bbd24e984aec96b546a19feadd55679daf9ab28dcc4ea766fe0784
ZX SHA-256: df125c0d48de6389dcf2561e1a12b084d6ed84392cbd9fb826c24c44013e7a50
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 168/3

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 31/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 54..85: 8 instructions, 0 samples |
| wasm_tier_Sieve | 77/42/7/2/2/24/7/13/6/5/6/1/0/0/12 | 46/23/5/1/2/9/3/8/3/2/0/1/0/0/9 | 168 | 373..443: 16 instructions, 164 samples |

## Sieve / clang
Native SHA-256: f8190c4ddf6088aee7d4a38e89b3ff80094393c754d79f0d40778ab31dc29a96
C SHA-256: 910a60d1644991bdfc3aa15ca5c9251b2bfc6660cacea207f8d76363c4e6f65d
ZX SHA-256: df125c0d48de6389dcf2561e1a12b084d6ed84392cbd9fb826c24c44013e7a50
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 163/3

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 31/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 54..85: 8 instructions, 0 samples |
| wasm_tier_Sieve | 77/42/7/2/2/24/7/13/6/5/6/1/0/0/12 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 | 163 | 373..443: 16 instructions, 156 samples |

## Records / cc
Native SHA-256: e4c46d42e3efe4dc3fb91437e9b96606f12fbd57b4c4cbfcb2afc777d64d3887
C SHA-256: 8679ca26bed0141e970c6c51c5cf0fcef1903c12458bd12eef4095e12b5d15eb
ZX SHA-256: ef16b8ef699b421e58e69b410074367ab582b3a091d1e4002cea5ae34acf1e28
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 162/5

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 31/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Records | 382/112/4/4/260/290/5/24/20/4/0/4/2/0/24 | 80/59/4/1/1/20/3/7/3/1/12/2/0/2/12 | 162 | 2139..2307: 39 instructions, 154 samples |

## Records / clang
Native SHA-256: e4c46d42e3efe4dc3fb91437e9b96606f12fbd57b4c4cbfcb2afc777d64d3887
C SHA-256: 0920384a04220eea8446363dc12b6c03ac25b5bca3ad579a022ad26a408745da
ZX SHA-256: ef16b8ef699b421e58e69b410074367ab582b3a091d1e4002cea5ae34acf1e28
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 162/5

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 31/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Records | 382/112/4/4/260/290/5/24/20/4/0/4/2/0/24 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 | 162 | 2139..2307: 39 instructions, 157 samples |

## Hash / cc
Native SHA-256: c42e0ed1756d1c65012c4745b14e96cf9300d9946f64239a7dbd02c212e41454
C SHA-256: d4c655704cbf4473cd9abc03baabf51e3e94a93680372868e3d9ca77d78d5374
ZX SHA-256: 57b64bac7cd36c69c44d765fbafb41d44c55a5807f89fa282957f99f887bcdb0
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 158/3

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 32/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Hash | 50/46/4/0/0/3/2/9/5/3/0/1/1/0/12 | 26/12/2/0/0/4/1/2/0/0/0/4/0/1/4 | 158 | 166..258: 22 instructions, 152 samples |

## Hash / clang
Native SHA-256: c42e0ed1756d1c65012c4745b14e96cf9300d9946f64239a7dbd02c212e41454
C SHA-256: dddfab1cdc51eef0df1c8151992699e7c2dbe8a2b3a2f370537177c1e74d271b
ZX SHA-256: 57b64bac7cd36c69c44d765fbafb41d44c55a5807f89fa282957f99f887bcdb0
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 162/3

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 32/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Hash | 50/46/4/0/0/3/2/9/5/3/0/1/1/0/12 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 | 162 | 166..258: 22 instructions, 160 samples |

## Vectors / cc
Native SHA-256: caeb860c5bbcadb79d1bb8a46018ef32f946a57b7fa3421206ff08e63d738925
C SHA-256: e78a12e2cd342cd1d4e154306f4d450e0af3ea9b11141e25b21605e0c125b035
ZX SHA-256: 196b5ccacd46bc2512922f4ff460ba3d5b880f5a0ace4bda93bda40502aadfda
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 195/4

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 32/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Vectors | 104/93/5/11/11/24/2/14/9/3/0/1/0/1/13 | 109/97/8/7/8/27/6/18/10/2/7/0/0/0/10 | 178 | 171..467: 79 instructions, 157 samples |

## Vectors / clang
Native SHA-256: caeb860c5bbcadb79d1bb8a46018ef32f946a57b7fa3421206ff08e63d738925
C SHA-256: b1d2f674b3e19e1606cbcab0df96e862cf143b0a859f16fb962ed6908268f315
ZX SHA-256: 196b5ccacd46bc2512922f4ff460ba3d5b880f5a0ace4bda93bda40502aadfda
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 192/4

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 32/8/1/4/2/6/0/3/2/2/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 38..69: 8 instructions, 0 samples |
| wasm_tier_Vectors | 104/93/5/11/11/24/2/14/9/3/0/1/0/1/13 | 123/101/10/3/3/22/15/17/7/2/26/1/0/1/13 | 160 | 171..467: 79 instructions, 139 samples |

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
Native SHA-256: c5bbb4125aa8a59d8244a4502b9b7448c40003f00cf05065731d31831b8212df
C SHA-256: f2d5a4ffb813abaa72568fa45a2da9c83de9f9f8689bea1f43ff301efba8e209
ZX SHA-256: 18a5c85c96a698300434e0ab76dee00954720f58a9e3fe3fb39026b8af07a97d
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 99/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Printing_main | 15/5/1/1/1/2/0/2/1/1/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 29..51: 5 instructions, 0 samples |
| wasm_tier_Printing | 59/44/1/5/5/12/0/5/4/7/0/1/1/0/9 | 32/15/1/0/0/4/1/1/0/1/0/2/0/0/5 | 35 | 96..260: 44 instructions, 35 samples |

## Printing / clang
Native SHA-256: c5bbb4125aa8a59d8244a4502b9b7448c40003f00cf05065731d31831b8212df
C SHA-256: 0a9bc9d89962d93db0b50f31fcaf9a0c02d3d30801b715eeb4225576bd40e44f
ZX SHA-256: 18a5c85c96a698300434e0ab76dee00954720f58a9e3fe3fb39026b8af07a97d
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 98/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Printing_main | 15/5/1/1/1/2/0/2/1/1/0/0/0/0/2 | absent (inline/dead/renamed unknown) | 0 | 29..51: 5 instructions, 0 samples |
| wasm_tier_Printing | 59/44/1/5/5/12/0/5/4/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 | 39 | 96..260: 44 instructions, 39 samples |

## Button / cc
Native SHA-256: 04d4c739f769b6411a3b82d6d82f8cf416a98c7f28d0016d0e69dd1b873ecd80
C SHA-256: ec85279fb49515de632e0e7dac87b57e461b4af308943da2fb5c83557c326f02
ZX SHA-256: f1dade43d5adec430c58ac7b5a323d0d4e22b971e6ce225d3cd35398c55b58b8
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 156/12

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 72..104: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 359/8/2/48/142/207/5/26/24/25/48/0/0/0/2 | 157/142/10/3/9/41/1/19/9/13/8/0/0/0/36 | 6 | 1017..1036: 4 instructions, 1 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 11/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 | 1 | none |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 14/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 1 | 3503..3525: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | 3962..3981: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 258/199/14/30/15/124/40/31/17/10/12/18/0/1/52 | 1 | none |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 | 0 | 6616..8175: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 8378..8397: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | 8565..8719: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 2 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 | 0 | 8794..8904: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 18 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 1 | 9804..9847: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 | 0 | 10829..13234: 517 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 1 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 | 1 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 | 1 | 14905..15164: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 | 0 | none |
| button_SubmitButton | 621/600/18/167/120/309/20/59/41/56/51/0/0/0/60 | 447/414/9/119/78/217/3/21/10/36/134/0/0/1/35 | 7 | 15343..18163: 585 instructions, 7 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 | 0 | none |
| paint_queue_PaintSnapshotEmit | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 | 0 | 19436..20900: 260 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 | 2 | 21909..22249: 67 instructions, 2 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 | 1 | 22894..22916: 4 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 201/158/14/35/13/71/4/27/13/13/41/2/0/1/16 | 0 | 25560..26618: 229 instructions, 0 samples |
| tree_input_TreeTextFocused | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 46/8/1/0/0/10/2/7/6/1/0/2/0/0/2 | 0 | 27202..27296: 22 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 8/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 | 2 | 28413..28429: 4 instructions, 2 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 31/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 | 0 | 28594..28913: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 93/18/2/4/2/23/8/12/10/2/0/4/0/2/2 | 0 | 29307..29399: 25 instructions, 0 samples |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 29770..30028: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 | 0 | none |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 | 0 | 30552..30802: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/1/0/0/4 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 | 1 | 32186..32818: 148 instructions, 1 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 | 2 | 33912..36786: 616 instructions, 1 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 59/24/2/1/1/11/1/8/6/3/0/2/0/0/5 | 0 | 37136..37191: 17 instructions, 0 samples |
| style_ResolveFlags | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 1 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 28/0/0/0/0/5/2/1/1/2/0/3/0/0/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/39/4/3/3/15/4/8/4/3/0/4/0/0/7 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 274/176/4/90/72/183/2/10/6/16/144/0/0/0/7 | 2 | 38197..38219: 4 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| button_ButtonDetailSize | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 41/0/0/10/1/11/0/4/4/2/7/2/0/2/0 | 2 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| button_behavior_MeasureBounds | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 263/223/22/59/30/100/3/41/19/5/145/0/0/0/13 | 0 | 39757..41031: 300 instructions, 0 samples |
| style_StyleMinHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| button_ButtonFitContentFace | 378/291/13/41/19/86/4/61/48/25/90/0/0/0/41 | 203/183/15/35/11/67/5/31/14/1/113/0/0/0/10 | 0 | 41561..42841: 279 instructions, 0 samples |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/8/0/6/6/2/0/2/0/1/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| button_PaintButtonForNode | 1313/1276/16/257/184/508/12/135/119/93/317/0/1/0/133 | 916/893/51/266/138/459/11/100/43/43/383/2/0/8/84 | 3 | 45653..51781: 1258 instructions, 3 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 27/12/1/0/0/1/0/2/1/2/0/0/0/0/1 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 | 3 | 52974..53252: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 | 0 | 55493..56541: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 | 0 | 57518..58740: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 | 1 | 59354..59376: 4 instructions, 1 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 | 6 | 59621..59643: 4 instructions, 1 samples |
| input_protocol_PointInBounds | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 25/0/0/0/0/1/1/5/5/0/14/0/0/0/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 1 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 32/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 | 0 | 61792..62580: 211 instructions, 0 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 421/373/45/30/47/105/9/89/44/1/2/0/0/10/98 | 0 | 62974..64205: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 | 3 | 64958..65206: 62 instructions, 3 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 78/18/2/2/2/19/8/5/3/2/0/4/0/2/2 | 0 | 65756..65939: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 | 0 | 66205..66534: 77 instructions, 0 samples |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 84/63/7/10/5/24/2/12/5/7/1/1/0/0/6 | 0 | none |
| button_ButtonFaceFinish | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 46/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 0 | 67369..67527: 31 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 267/236/11/58/44/144/24/25/14/11/14/2/0/4/37 | 189/151/5/31/50/120/13/11/6/4/76/0/0/5/15 | 1 | 67897..68864: 184 instructions, 1 samples |
| button_ButtonDefaultFace | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 227/179/12/38/30/96/7/27/15/16/39/0/0/0/25 | 2 | 69333..70320: 191 instructions, 1 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 46/31/1/8/1/10/0/3/2/4/0/0/0/0/3 | 1 | 70365..70522: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 35/16/2/0/0/5/3/5/3/0/0/0/0/2/5 | 3 | 70649..70811: 47 instructions, 3 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 11/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 19/0/0/0/0/0/0/1/1/0/16/0/0/0/0 | 0 | 71115..71277: 45 instructions, 0 samples |
| button_behavior_MeasureContent | 158/142/5/4/9/15/0/28/23/9/41/0/0/0/14 | 74/38/3/0/0/5/2/9/6/0/45/0/0/0/0 | 0 | 71362..71928: 138 instructions, 0 samples |
| button_behavior_ShapeWidth | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 27/0/0/0/0/3/2/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 37/23/3/5/2/14/1/7/4/2/8/0/0/0/3 | 1 | 72260..72282: 4 instructions, 0 samples |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 142/95/6/5/8/36/14/20/14/3/0/6/0/6/20 | 1 | 72732..73008: 69 instructions, 1 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 | 0 | 73405..73712: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 | 1 | 73890..75478: 385 instructions, 0 samples |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 | 1 | none |
| button_behavior_ButtonCornerRadius | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 9/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 1 | none |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 | 2 | 76738..76757: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 48/0/0/1/13/18/0/1/1/2/4/0/0/0/0 | 0 | 77611..77630: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 45/32/1/12/2/29/0/3/2/2/20/0/0/0/1 | 1 | 78020..78042: 4 instructions, 1 samples |
| focus_FocusDefaultOutlineBounds | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 45/22/2/2/1/10/2/6/2/0/28/0/0/0/2 | 0 | 78127..78339: 53 instructions, 0 samples |
| image_lookup_IconLookupImage | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 28/12/1/1/1/6/1/3/2/2/0/0/0/0/2 | 0 | 78625..78644: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 16/0/0/0/0/1/0/2/2/0/9/0/0/0/0 | 0 | 78853..78995: 39 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 92/53/4/0/0/0/0/11/7/7/27/0/0/0/7 | 23/15/1/0/0/6/1/3/2/0/16/0/0/0/0 | 0 | 79171..79343: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 223/167/5/37/26/72/0/26/21/9/91/0/0/0/12 | 124/89/8/2/0/14/3/17/9/1/77/0/0/0/1 | 0 | 79611..80298: 162 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 345/292/4/71/57/143/3/27/23/22/88/4/1/0/36 | 225/195/7/30/25/84/9/17/8/8/91/1/0/4/21 | 0 | 80543..81822: 288 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 58/0/0/19/15/39/0/5/5/2/28/0/0/0/0 | 0 | 82091..82113: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 76/36/1/15/3/27/1/8/7/1/33/0/0/1/5 | 0 | 82628..82650: 4 instructions, 0 samples |
| paint_queue_PaintImage | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 33/0/0/6/5/11/0/1/1/2/4/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 45/0/0/1/10/15/0/1/1/2/2/0/0/0/0 | 0 | 83791..83810: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 | 2 | 84655..84674: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 85285..85295: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 | 1 | 85425..85444: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 | 0 | 86076..86404: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 278/259/112/0/0/136/9/116/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 36/0/0/0/0/5/0/0/0/0/0/0/0/5/0 | 0 | 87294..87600: 79 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 85/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 148/0/0/59/14/82/0/15/15/0/70/0/0/0/0 | 0 | 89288..89310: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 | 2 | 89419..89438: 4 instructions, 2 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 55/0/0/0/0/1/1/1/1/0/0/10/0/12/0 | 0 | 89712..90373: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 115/84/6/20/13/54/2/13/7/5/32/0/0/0/10 | 0 | 90903..90922: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 1 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 24/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 69/52/5/10/4/30/6/9/4/2/27/0/0/0/3 | 1 | 91672..91819: 40 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 38/29/1/8/5/14/1/3/2/3/0/0/0/0/3 | 0 | 91887..91966: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/2/15/6/3/3/1/0/2/0/0/0 | 0 | none |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 114/100/2/23/5/33/2/11/8/2/53/0/0/2/12 | 2 | 92454..92476: 4 instructions, 1 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 | 0 | 93074..93096: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 | 1 | 93280..96735: 711 instructions, 1 samples |
| paint_queue_EmitRoundedOutline | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 119/99/2/24/6/35/0/11/8/2/59/0/0/2/11 | 0 | 96802..96824: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 4/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 123/104/6/4/12/41/10/10/4/3/4/1/0/1/23 | 0 | 97376..97793: 99 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 74/27/2/0/0/0/0/11/9/6/19/0/0/0/5 | 35/10/1/0/0/5/0/6/3/0/16/0/0/0/0 | 0 | 98316..98415: 26 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 125/61/5/6/4/55/20/11/6/2/5/2/0/3/10 | 0 | 98585..98787: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 9/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 105/77/2/20/34/60/0/4/2/3/26/0/0/0/8 | 0 | 100023..100042: 4 instructions, 0 samples |
| paint_queue_EmitLine | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 83/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 101102..101697: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 | 0 | 102901..102923: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 | 21 | 114557..114579: 4 instructions, 3 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 21/0/0/3/0/7/0/0/0/0/18/0/0/0/0 | 1 | none |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 | 1 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 | 0 | 115740..116409: 154 instructions, 0 samples |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 | 0 | 116559..117631: 254 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 | 0 | 118211..118230: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 | 2 | 119178..119197: 4 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 19/6/1/0/0/5/2/2/1/0/0/1/0/0/1 | 0 | 119636..119694: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/27/2/3/4/24/5/4/2/1/5/0/0/1/8 | 0 | none |
| svg_shape_ShapeRaster | 258/218/9/32/17/69/6/47/38/15/14/2/0/0/40 | 127/89/5/5/4/35/7/17/12/4/9/1/0/0/18 | 0 | 120222..121029: 197 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 326/294/20/30/30/93/9/48/28/9/0/8/1/2/41 | 330/291/25/30/20/108/27/52/27/1/26/2/0/8/51 | 0 | 121515..122291: 205 instructions, 0 samples |
| image_behavior_ImageFitBounds | 196/177/8/18/9/31/0/26/18/13/45/0/0/0/18 | 92/77/6/6/2/15/0/16/10/0/56/0/0/0/7 | 0 | 122370..123032: 168 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 99/55/1/9/1/12/1/16/15/7/12/0/0/0/4 | 35/14/1/0/0/8/2/8/7/0/12/0/0/0/0 | 0 | 123139..123360: 55 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 123482..123504: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 17/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 123913..123935: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 1 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 | 0 | 125203..125281: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 125864..125883: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 14 | 126333..126352: 4 instructions, 2 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 2 | 126851..126870: 4 instructions, 0 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 | 5 | 127537..127553: 4 instructions, 2 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 | 3 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 3 | 128009..128025: 4 instructions, 2 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 22/0/0/2/0/6/0/0/0/0/18/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 | 0 | 128396..128555: 39 instructions, 0 samples |
| paint_queue_FillShapeRows | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 | 0 | 128632..129584: 225 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 | 0 | 129718..130075: 92 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 194/165/10/21/26/76/11/29/19/5/21/4/0/1/22 | 0 | 131059..131614: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 343/304/21/56/16/91/15/62/41/19/54/1/0/0/48 | 162/151/12/0/0/48/26/32/20/0/21/0/0/0/24 | 0 | 131941..133071: 271 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 610/590/44/147/107/273/7/91/47/35/156/0/0/0/51 | 447/416/39/36/21/126/22/74/35/14/90/0/0/0/87 | 0 | 133290..135557: 504 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 326/308/14/23/20/72/15/49/35/14/56/3/0/0/51 | 222/201/15/15/13/79/21/37/21/2/47/0/0/1/30 | 0 | 136306..137287: 241 instructions, 0 samples |
| paint_queue_EmitImageBegin | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 20/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 40/0/0/4/3/11/0/2/0/1/16/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/0/0/11/0/0/1/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 | 1 | 138642..138687: 12 instructions, 1 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/3/0/0/0/0/5/0/0/0/0 | 1 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 2 | 139274..139360: 21 instructions, 1 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 0 | 139572..139658: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 | 0 | 139880..139966: 21 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 8/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 15/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 137/123/3/0/0/16/4/16/13/11/38/0/0/0/22 | 110/13/2/0/0/21/11/13/7/0/42/0/0/2/2 | 0 | 140473..140950: 122 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 141052..141158: 30 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 | 1 | 141269..143199: 406 instructions, 1 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 1 | 143328..143375: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 | 0 | 143670..144514: 183 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 144643..144690: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 | 0 | 144985..145829: 183 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 145958..146005: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 602/520/11/90/49/179/20/57/46/38/268/0/3/0/61 | 367/340/21/19/10/94/24/47/25/7/185/0/3/0/38 | 0 | 146440..148709: 495 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 6/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | 148891..149014: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 70/66/3/0/0/16/8/8/5/2/17/0/0/0/15 | 52/34/3/0/0/19/7/7/4/0/14/0/0/0/4 | 0 | 149052..149275: 52 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 53/35/3/0/0/13/5/8/5/0/19/0/0/1/10 | 0 | 149360..149566: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 21/6/1/0/0/4/0/2/1/0/12/0/0/0/1 | 0 | 149669..149775: 30 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 11/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 10/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 9/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 20/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 150394..150410: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 150616..150632: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 100/86/3/4/5/27/12/13/10/4/25/0/2/0/14 | 95/68/5/0/0/27/19/15/10/0/23/0/2/2/13 | 0 | 150812..151006: 46 instructions, 0 samples |

## Button / clang
Native SHA-256: 04d4c739f769b6411a3b82d6d82f8cf416a98c7f28d0016d0e69dd1b873ecd80
C SHA-256: 9484f192b9f17b2d12cbe771349c638d9eb8f3747d02d162729d052651eb055f
ZX SHA-256: f1dade43d5adec430c58ac7b5a323d0d4e22b971e6ce225d3cd35398c55b58b8
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 154/12

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 72..104: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 359/8/2/48/142/207/5/26/24/25/48/0/0/0/2 | 141/0/0/3/5/37/0/12/12/13/8/0/0/0/0 | 1 | 1017..1036: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 | 0 | none |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 2 | 3503..3525: 4 instructions, 1 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 1 | 3962..3981: 4 instructions, 1 samples |
| tree_input_TreePointerUpdate | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 210/110/2/8/1/65/22/28/26/7/18/6/0/4/23 | 0 | none |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 | 0 | 6616..8175: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 8378..8397: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 8565..8719: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 2 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 8794..8904: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 28 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 9804..9847: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 373/232/17/41/14/130/43/58/41/21/26/5/0/4/33 | 0 | 10829..13234: 517 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 1 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 14905..15164: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| button_SubmitButton | 621/600/18/167/120/309/20/59/41/56/51/0/0/0/60 | 376/10/1/57/34/129/0/17/16/38/33/2/0/3/1 | 5 | 15343..18163: 585 instructions, 5 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 1 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 | 0 | 19436..20900: 260 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 148/83/4/6/2/51/14/12/8/12/6/1/0/6/13 | 2 | 21909..22249: 67 instructions, 2 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 0 | 22894..22916: 4 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 0 | 25560..26618: 229 instructions, 0 samples |
| tree_input_TreeTextFocused | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 1 | 27202..27296: 22 instructions, 1 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 28352..28368: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 118/103/2/0/0/11/3/22/20/0/0/0/0/3/32 | 0 | 28594..28913: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 | 0 | 29307..29399: 25 instructions, 0 samples |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 29770..30028: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 30552..30802: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 | 1 | 32186..32818: 148 instructions, 1 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 | 5 | 33912..36786: 616 instructions, 4 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 0 | 37136..37191: 17 instructions, 0 samples |
| style_ResolveFlags | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 3 | 38147..38169: 4 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 45/0/0/3/0/8/0/4/4/2/5/2/0/1/0 | 1 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 | 1 | none |
| button_behavior_MeasureBounds | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 190/0/0/19/10/49/0/23/23/2/120/0/0/0/0 | 3 | 39757..41031: 300 instructions, 2 samples |
| style_StyleMinHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| button_ButtonFitContentFace | 378/291/13/41/19/86/4/61/48/25/90/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/1 | 0 | 41561..42841: 279 instructions, 0 samples |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 1 | none |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1313/1276/16/257/184/508/12/135/119/93/317/0/1/0/133 | 793/0/0/161/116/384/0/54/54/41/331/4/0/7/0 | 6 | 45653..51781: 1258 instructions, 6 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 1 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 0 | 52974..53252: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 55493..56541: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 57518..58740: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 1 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 2 | 59354..59376: 4 instructions, 2 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 5 | 60727..60749: 4 instructions, 3 samples |
| input_protocol_PointInBounds | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 61792..62580: 211 instructions, 0 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 | 0 | 62974..64205: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 1 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 | 2 | 64958..65206: 62 instructions, 2 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 65756..65939: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 | 2 | 66205..66534: 77 instructions, 1 samples |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 81/12/1/6/0/13/3/12/11/6/5/1/0/2/3 | 0 | none |
| button_ButtonFaceFinish | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 1 | 67369..67527: 31 instructions, 1 samples |
| style_sheet_ResolveInstalledStyleRules | 267/236/11/58/44/144/24/25/14/11/14/2/0/4/37 | 253/196/17/22/19/107/14/33/16/4/0/0/0/12/36 | 0 | 67897..68864: 184 instructions, 0 samples |
| button_ButtonDefaultFace | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 1 | 69333..70320: 191 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 70365..70522: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 | 1 | 70549..70717: 43 instructions, 1 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 71115..71277: 45 instructions, 0 samples |
| button_behavior_MeasureContent | 158/142/5/4/9/15/0/28/23/9/41/0/0/0/14 | 44/0/0/0/0/1/0/8/8/0/29/0/0/0/0 | 0 | 71362..71928: 138 instructions, 0 samples |
| button_behavior_ShapeWidth | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 1 | 72260..72282: 4 instructions, 1 samples |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 1 | 72732..73008: 69 instructions, 1 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 1 | 73405..73712: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 | 1 | 73890..75478: 385 instructions, 0 samples |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 1 | none |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 76738..76757: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 0 | 77611..77630: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 1 | 78020..78042: 4 instructions, 1 samples |
| focus_FocusDefaultOutlineBounds | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 27/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 78127..78339: 53 instructions, 0 samples |
| image_lookup_IconLookupImage | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 78625..78644: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 78853..78995: 39 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 92/53/4/0/0/0/0/11/7/7/27/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 79171..79343: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 223/167/5/37/26/72/0/26/21/9/91/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/87/0/0/0/0 | 1 | 79611..80298: 162 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 345/292/4/71/57/143/3/27/23/22/88/4/1/0/36 | 246/35/1/23/20/69/6/17/16/6/128/1/0/1/3 | 0 | 80543..81822: 288 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 82091..82113: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 82628..82650: 4 instructions, 0 samples |
| paint_queue_PaintImage | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 83791..83810: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 3 | 84655..84674: 4 instructions, 2 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 1 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 85285..85295: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 1 | 85425..85444: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 2 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 0 | 86076..86404: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 87294..87600: 79 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 89288..89310: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 3 | 89419..89438: 4 instructions, 3 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 89712..90373: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 73/0/0/6/0/29/1/9/8/3/12/0/0/0/0 | 0 | 90903..90922: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 1 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 91672..91819: 40 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 91887..91966: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/0/14/7/3/3/2/0/1/0/0/0 | 0 | none |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 3 | 92454..92476: 4 instructions, 1 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 93074..93096: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 | 5 | 93280..96735: 711 instructions, 5 samples |
| paint_queue_EmitRoundedOutline | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 96802..96824: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 164/41/4/6/7/61/19/20/16/3/10/5/0/0/4 | 0 | 97376..97793: 99 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 74/27/2/0/0/0/0/11/9/6/19/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 98316..98415: 26 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 192/156/12/7/3/67/28/24/12/1/9/2/0/10/39 | 0 | 98585..98787: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 100023..100042: 4 instructions, 0 samples |
| paint_queue_EmitLine | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 101102..101697: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 102901..102923: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 | 15 | 114557..114579: 4 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 1 | none |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 | 0 | 115740..116409: 154 instructions, 0 samples |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 | 0 | 116559..117631: 254 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 118211..118230: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 | 1 | 119178..119197: 4 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 47/22/2/0/0/6/5/6/4/0/0/5/0/0/1 | 0 | 119636..119694: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 258/218/9/32/17/69/6/47/38/15/14/2/0/0/40 | 129/113/4/3/3/27/9/17/13/4/8/1/0/0/21 | 0 | 120222..121029: 197 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 326/294/20/30/30/93/9/48/28/9/0/8/1/2/41 | 372/321/22/23/18/117/47/58/36/1/32/4/0/4/51 | 0 | 121515..122291: 205 instructions, 0 samples |
| image_behavior_ImageFitBounds | 196/177/8/18/9/31/0/26/18/13/45/0/0/0/18 | 43/0/0/0/0/3/0/9/9/0/30/0/0/0/0 | 0 | 122370..123032: 168 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 99/55/1/9/1/12/1/16/15/7/12/0/0/0/4 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 123139..123360: 55 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 123482..123504: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 123913..123935: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 125203..125281: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 125864..125883: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 10 | 126333..126352: 4 instructions, 3 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 3 | 126851..126870: 4 instructions, 1 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 8 | 127537..127553: 4 instructions, 7 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 1 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 2 | 128009..128025: 4 instructions, 1 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 128396..128555: 39 instructions, 0 samples |
| paint_queue_FillShapeRows | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 324/262/6/37/24/70/2/22/16/2/285/0/0/0/2 | 0 | 128632..129584: 225 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 129718..130075: 92 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 131059..131614: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 343/304/21/56/16/91/15/62/41/19/54/1/0/0/48 | 179/142/8/0/0/31/14/38/30/0/33/0/0/0/25 | 0 | 131941..133071: 271 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 610/590/44/147/107/273/7/91/47/35/156/0/0/0/51 | 659/625/33/85/27/229/45/103/69/15/134/0/0/3/82 | 0 | 133290..135557: 504 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 326/308/14/23/20/72/15/49/35/14/56/3/0/0/51 | 192/158/16/10/9/58/22/28/12/3/45/0/0/3/12 | 0 | 136306..137287: 241 instructions, 0 samples |
| paint_queue_EmitImageBegin | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 138642..138687: 12 instructions, 0 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 2 | 139274..139360: 21 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 0 | 139572..139658: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 106/18/2/0/0/18/7/14/12/4/0/0/0/4/1 | 0 | 139880..139966: 21 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 137/123/3/0/0/16/4/16/13/11/38/0/0/0/22 | 91/16/2/0/0/22/17/14/12/1/39/0/0/0/1 | 0 | 140473..140950: 122 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 141052..141158: 30 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 1 | 141269..143199: 406 instructions, 1 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 1 | 143328..143375: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 143670..144514: 183 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 144643..144690: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 144985..145829: 183 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 145958..146005: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 602/520/11/90/49/179/20/57/46/38/268/0/3/0/61 | 575/417/24/43/25/189/49/61/37/8/319/0/3/10/72 | 0 | 146440..148709: 495 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 61/27/3/0/0/26/17/10/7/0/18/0/0/2/5 | 0 | 148891..149014: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 70/66/3/0/0/16/8/8/5/2/17/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 149052..149275: 52 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 149360..149566: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 149669..149775: 30 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 150394..150410: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 150616..150632: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 100/86/3/4/5/27/12/13/10/4/25/0/2/0/14 | 133/55/6/0/0/47/34/24/18/0/46/0/0/2/8 | 0 | 150812..151006: 46 instructions, 0 samples |

## Text / cc
Native SHA-256: 8ec9912f2bcfb6ec77f586433a5d0b2295335502cf3bb822c386e26f67c070fe
C SHA-256: 3a7ef319b774a9e97fad5e99c8231dc96788d5ad7596b3f0801d747ef467ed2e
ZX SHA-256: f78f4a3a4d2ed2fd5021c021d861633b498ff0a07fb8bc7a676a9023c95c890b
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 159/10

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 52..84: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 230/154/5/19/59/102/3/25/20/16/29/0/0/0/27 | 143/127/10/15/19/63/2/17/7/8/26/0/0/1/29 | 3 | 581..1213: 148 instructions, 2 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 11/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 | 2 | none |
| text___zi_spec_6a19f7c98e79a734 | 21/4/1/6/6/12/2/1/0/1/2/0/0/0/1 | 6/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 1 | 2722..2744: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | 3196..3215: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 | 0 | 4018..5577: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 5780..5799: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | 5967..6121: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 | 0 | 6196..6306: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 7 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 7206..7249: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 | 0 | 8231..10636: 517 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 | 1 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 | 2 | 12307..12566: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 | 0 | none |
| text_DrawSession | 342/141/9/96/82/193/14/30/21/29/24/0/0/0/21 | 301/266/10/103/83/191/2/16/6/18/155/0/0/0/15 | 1 | 13868..14493: 121 instructions, 1 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 | 0 | none |
| paint_queue_PaintSnapshotEmit | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 | 0 | 15475..16939: 260 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 | 3 | 17948..18288: 67 instructions, 3 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 | 2 | 18933..18955: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 31/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 | 0 | 21361..21680: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 92/18/2/4/2/22/7/12/10/2/0/4/0/2/2 | 0 | 22074..22166: 25 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 | 0 | none |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 22917..23175: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 | 0 | 23962..23978: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 | 0 | 24270..24520: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/0/0/1/4 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 | 1 | 25904..26536: 148 instructions, 1 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 | 4 | 27630..30504: 616 instructions, 2 samples |
| std_math_Finite32 | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 12/0/0/0/0/3/0/2/2/0/4/0/0/0/0 | 1 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 70/56/1/12/15/33/0/3/2/3/14/0/0/2/13 | 1 | 31194..31216: 4 instructions, 1 samples |
| text_PrepareText | 248/82/2/54/57/153/0/13/11/15/16/0/0/0/2 | 167/85/3/36/22/65/2/7/4/14/32/0/0/3/4 | 3 | 32136..32327: 47 instructions, 1 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 46/0/0/3/3/12/3/4/3/1/4/2/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 165/99/6/38/48/90/1/11/5/6/76/0/0/0/6 | 88/29/1/9/14/27/0/2/1/3/59/0/0/0/4 | 0 | 32960..33264: 68 instructions, 0 samples |
| text_TextOrigin | 23/0/0/2/3/14/0/0/0/0/9/0/0/0/0 | 5/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 1 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 68/0/0/2/2/13/1/8/8/3/0/1/0/1/0 | 0 | 34630..34913: 74 instructions, 0 samples |
| text_TextHiddenLines | 118/40/2/11/2/22/0/11/9/10/21/0/0/0/4 | 76/8/1/13/5/24/3/7/2/2/22/0/0/0/2 | 1 | 35289..35382: 28 instructions, 0 samples |
| tree_TreeSetHiddenLines | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 | 0 | none |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 49/0/0/16/16/36/0/1/1/1/30/0/0/0/0 | 0 | 35798..35820: 4 instructions, 0 samples |
| text_PaintTextLines | 692/610/7/184/149/360/10/49/42/45/83/0/0/0/42 | 556/471/12/192/123/338/6/35/13/15/207/0/0/6/34 | 5 | 36320..39509: 607 instructions, 5 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 | 1 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 | 2 | 40541..40819: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 | 0 | 43060..44108: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 | 0 | 45085..46307: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 | 2 | 46921..46943: 4 instructions, 2 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 | 3 | 47188..47210: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 | 15 | 48792..49580: 211 instructions, 12 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 421/373/45/30/47/105/9/89/44/1/2/0/0/10/98 | 0 | 49974..51205: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 | 0 | 52135..52625: 123 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 75/18/2/2/2/16/6/5/3/2/0/4/0/2/2 | 1 | 52756..52939: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 | 1 | 53506..53701: 54 instructions, 1 samples |
| text_DefaultTextFace | 65/4/1/5/32/45/2/1/0/1/20/0/0/0/1 | 14/0/0/0/0/7/0/0/0/1/0/0/0/0/0 | 1 | 53950..53969: 4 instructions, 1 samples |
| text_TextFaceFor | 59/16/4/20/17/41/8/4/0/1/8/0/0/0/4 | 51/0/0/11/8/23/0/1/1/2/10/0/0/2/0 | 1 | 54057..54076: 4 instructions, 0 samples |
| text_behavior_ResolveTextStyle | 127/99/1/14/14/30/0/23/22/9/8/0/0/2/18 | 78/56/6/8/3/19/2/14/6/1/11/0/0/0/3 | 0 | 54440..54819: 99 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 | 1 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 44/26/1/0/0/0/0/6/5/4/10/0/0/0/6 | 15/0/0/0/0/4/0/3/1/0/7/0/0/0/0 | 1 | 55772..55869: 26 instructions, 0 samples |
| text_behavior_TextWrapPolicy | 21/0/0/0/0/0/0/1/1/1/2/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 537/475/17/65/180/310/14/50/33/27/17/12/1/0/55 | 265/228/27/12/8/98/40/43/15/3/11/2/0/16/37 | 2 | 56316..58556: 440 instructions, 1 samples |
| text_behavior_TextExtent | 17/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 14/0/0/0/0/0/0/2/2/0/0/2/0/0/0 | 0 | 58748..58818: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| geometry_WorldToScreen | 46/0/0/8/5/22/0/0/0/2/30/0/0/0/0 | 39/0/0/9/4/13/0/0/0/2/33/0/0/0/0 | 4 | none |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 | 0 | 59608..59915: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 | 2 | 60093..61681: 385 instructions, 2 samples |
| text_behavior_TextAlignmentOffset | 31/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 16/0/0/0/0/2/1/2/2/0/7/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 27/8/1/0/0/1/1/4/3/1/0/0/0/0/1 | 0 | 61887..61946: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_TextEllipsisStart | 95/84/5/23/14/42/1/10/5/4/0/0/0/0/11 | 60/41/4/2/2/11/5/9/5/1/0/0/0/0/6 | 0 | 62147..62472: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 16/0/0/0/0/1/1/1/1/0/0/0/0/2/0 | 0 | none |
| text_PaintTextLineRuns | 332/314/6/110/106/235/12/13/7/13/30/0/0/0/14 | 277/229/5/89/51/149/4/9/4/11/97/2/0/4/22 | 2 | 62681..64385: 296 instructions, 2 samples |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 | 4 | 65019..65038: 4 instructions, 2 samples |
| text_PaintTextEmphasis | 224/205/10/69/59/145/7/19/9/11/7/0/0/0/13 | 236/186/5/58/38/108/7/15/10/6/37/2/0/7/21 | 0 | 65551..66447: 169 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 38/16/1/3/5/10/0/3/2/1/11/0/0/0/1 | 25/0/0/0/0/2/0/0/0/0/10/0/0/2/0 | 0 | 66564..66624: 16 instructions, 0 samples |
| text_PaintTextRule | 195/4/1/69/73/148/2/2/1/10/46/0/0/0/1 | 134/0/0/28/29/58/0/4/2/6/68/0/0/3/0 | 0 | 66791..66813: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 67894..67904: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 | 4 | 68034..68053: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 | 1 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 | 0 | 68685..69013: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 278/259/112/0/0/136/9/116/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 32/12/1/6/4/16/1/3/2/2/8/0/0/0/1 | 0 | 69859..69881: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 45/32/1/12/2/29/0/3/2/2/20/0/0/0/1 | 1 | 70144..70166: 4 instructions, 1 samples |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 1 | none |
| text_layout_NextTextLineUncached | 375/334/6/115/53/189/6/42/36/22/18/0/0/0/22 | 309/277/8/40/17/70/5/28/20/6/12/0/0/13/28 | 1 | 70744..71865: 279 instructions, 1 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 16/6/1/0/0/2/0/2/1/0/0/1/0/0/1 | 0 | 71904..71956: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 24/10/4/1/0/10/1/7/3/0/1/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 204/111/4/0/0/0/0/21/17/12/63/0/0/0/23 | 95/49/3/0/0/28/1/15/10/0/55/0/0/0/0 | 4 | 72604..73043: 108 instructions, 0 samples |
| std_math_CosineDegrees | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/1/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/2/15/6/3/3/1/0/2/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/3/1/5/3/0/0/0/0/2/5 | 18 | 73411..73579: 43 instructions, 18 samples |
| text_TextLinkRunAt | 119/32/2/37/18/67/1/12/10/4/0/0/0/0/3 | 84/58/2/4/0/10/6/10/8/0/0/0/0/3/7 | 0 | 73762..73890: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 22/0/0/0/0/1/0/0/0/3/0/2/0/4/0 | 0 | none |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 | 1 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 | 0 | 75493..75515: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 | 3 | 75780..79235: 711 instructions, 3 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 34/10/2/0/0/3/2/3/1/0/0/0/0/2/3 | 0 | none |
| paint_queue_PaintClippedEmphasis | 177/4/1/18/84/150/2/2/1/2/25/0/0/0/1 | 52/0/0/5/17/25/0/1/1/2/6/0/0/0/0 | 0 | 80119..80138: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 | 0 | 80998..81017: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 141/4/1/21/93/117/2/1/0/4/32/0/0/0/1 | 43/0/0/0/7/10/0/0/0/1/17/0/0/0/0 | 0 | 81999..82018: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 | 1 | 92684..92703: 4 instructions, 0 samples |
| text_layout_TextTokenNext | 300/277/12/43/22/80/6/60/48/27/0/0/0/0/45 | 253/234/27/2/3/43/10/61/34/6/0/0/0/4/54 | 6 | 93010..94104: 269 instructions, 5 samples |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 94291..94444: 42 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 | 0 | 95668..95746: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 | 1 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 | 0 | 96086..97158: 254 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 1 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 | 0 | 97775..97794: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 | 1 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 | 0 | 98839..99092: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 114/100/2/23/5/33/2/11/8/2/53/0/0/2/12 | 0 | 99137..99159: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 | 0 | 99757..99779: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 19/0/0/6/3/10/0/0/0/0/9/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 123/8/2/10/85/97/4/2/0/1/26/0/0/0/2 | 31/0/0/0/4/11/0/0/0/1/0/0/0/0/0 | 0 | 100340..100359: 4 instructions, 0 samples |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 | 0 | 101440..101462: 4 instructions, 0 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 110/97/14/0/0/21/5/28/14/5/0/0/0/0/20 | 3 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 | 2 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 | 1 | 102717..102762: 12 instructions, 1 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 191/162/10/21/26/74/8/29/19/5/21/4/0/1/22 | 0 | 103733..104288: 127 instructions, 0 samples |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 | 0 | 104988..105657: 154 instructions, 0 samples |
| shape_paint_EmitShape | 104/16/4/17/12/33/8/18/14/7/14/0/0/0/4 | 28/0/0/4/0/8/0/11/7/0/3/0/0/0/0 | 0 | 105863..105885: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 12/0/0/4/0/5/0/0/0/1/0/0/0/0/0 | 0 | 106308..106330: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 106867..106886: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 4 | 107336..107355: 4 instructions, 0 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 1 | 107854..107873: 4 instructions, 1 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 | 0 | 108540..108556: 4 instructions, 0 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 | 1 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 1 | 109012..109028: 4 instructions, 1 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 25/8/2/0/0/6/0/4/2/0/0/0/0/0/2 | 8 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 115/105/18/0/0/13/2/28/10/0/0/0/0/0/37 | 0 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 37/19/2/0/0/4/1/7/5/0/0/0/0/0/6 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 2 | 111978..112064: 21 instructions, 2 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 0 | 112276..112362: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 | 0 | 112584..112670: 21 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 | 0 | 112991..113150: 39 instructions, 0 samples |
| paint_queue_FillShapeRows | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 | 0 | 113227..114179: 225 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 | 0 | 114313..114670: 92 instructions, 0 samples |
| shape_paint_EmitLineShape | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 110/47/2/20/1/45/7/7/5/6/44/0/0/2/5 | 0 | 114755..114777: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 215/79/4/50/37/89/6/18/14/17/53/0/0/0/10 | 185/132/10/28/16/70/2/21/11/8/60/0/0/3/15 | 0 | 116033..116368: 71 instructions, 0 samples |
| shape_paint_EmitRingShape | 198/59/3/35/24/63/4/26/23/18/52/0/0/0/11 | 111/84/9/20/6/42/1/14/5/8/45/0/0/0/9 | 0 | 117206..117384: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 38/12/1/7/0/19/1/3/2/2/9/0/0/0/3 | 0 | 117431..117453: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 | 5 | 117913..119843: 406 instructions, 5 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 119972..120019: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 | 0 | 120314..121158: 183 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 121287..121334: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 | 0 | 121629..122473: 183 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 122602..122649: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 | 0 | none |
| shape_paint_ShapeAt | 19/0/0/2/3/10/0/0/0/0/7/0/0/0/0 | 6/0/0/1/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 164/0/0/32/24/73/0/9/9/7/71/0/0/0/0 | 89/0/0/11/9/25/0/5/5/3/65/0/0/0/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 8/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/11/2/0/0/6/0/4/2/0/15/0/0/0/1 | 0 | none |
| shape_paint_PointsBounds | 114/99/5/20/10/38/1/11/6/5/36/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 | 0 | 124376..124666: 75 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 349/43/5/70/57/178/14/33/28/28/72/0/0/0/7 | 234/186/9/42/39/91/4/18/8/13/120/0/0/2/11 | 0 | 125857..125982: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 20/0/0/0/0/7/1/2/2/1/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcDirections | 65/26/1/10/10/28/3/4/3/3/22/0/0/0/2 | 51/17/1/3/4/14/3/3/2/2/20/0/0/0/1 | 0 | 126575..126689: 26 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 142/127/10/15/3/18/0/24/14/19/6/1/2/0/23 | 74/14/1/1/1/4/1/2/1/2/3/2/0/9/3 | 0 | 126849..127282: 115 instructions, 0 samples |
| paint_queue_GradientColorAt | 62/46/2/13/6/20/0/5/3/6/13/0/0/0/4 | 153/122/12/0/0/24/9/29/13/0/73/0/0/6/14 | 0 | 127396..127570: 44 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 42/29/1/9/8/19/0/2/1/3/5/0/0/0/2 | 36/15/1/3/3/11/2/2/1/1/17/0/0/0/1 | 0 | 127612..127728: 29 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 66/53/1/17/14/35/0/2/1/6/9/0/0/0/3 | 59/33/1/7/5/17/2/2/1/2/35/0/0/0/1 | 0 | 127771..127988: 53 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 128095..128201: 30 instructions, 0 samples |
| std_math_Sqrt | 161/101/4/7/3/11/0/20/16/10/47/0/2/0/16 | 102/80/5/0/0/26/4/17/11/0/57/0/0/1/9 | 0 | 128440..128834: 97 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 103/67/1/11/6/41/5/9/8/6/10/0/0/0/12 | 84/39/3/8/8/32/6/8/5/2/44/0/0/0/6 | 0 | 129017..129267: 67 instructions, 0 samples |
| shape_paint_ShapeClipSide | 150/69/3/35/34/86/10/13/10/6/26/0/0/0/7 | 285/259/23/14/8/69/14/61/38/1/117/0/0/0/22 | 0 | 129486..129750: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 130029..130038: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/2/0/0/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 62/0/0/0/0/0/0/6/6/5/16/0/0/0/0 | 34/0/0/0/0/7/3/4/3/0/17/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcPoint | 25/0/0/6/4/14/1/0/0/0/11/0/0/0/0 | 12/0/0/0/0/2/1/0/0/0/8/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| std_math_Finite64 | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 12/0/0/0/0/3/0/2/2/0/4/0/0/0/0 | 0 | none |
| std_math_MathSplit | 148/139/9/3/3/8/0/18/9/7/35/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 130753..131314: 124 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 106/96/7/9/11/20/0/19/12/6/34/0/0/0/8 | 75/48/7/0/0/15/5/17/10/0/21/0/0/0/6 | 0 | 131416..131763: 85 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/15/0/0/0/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 23/0/0/0/0/3/2/3/3/0/5/0/0/0/0 | 0 | none |
| shape_paint_ShapeCrossing | 98/0/0/22/9/33/0/10/10/2/37/0/0/0/0 | 44/22/1/0/0/3/1/6/5/0/31/0/0/0/1 | 0 | none |
| widget_benchmark_host_RasterTriangles | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/1/0/6/0/0/1/2 | 0 | 132466..132601: 34 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 132648..132750: 24 instructions, 0 samples |

## Text / clang
Native SHA-256: 8ec9912f2bcfb6ec77f586433a5d0b2295335502cf3bb822c386e26f67c070fe
C SHA-256: 83c05852115ae97404c04e04863107bd567438b264aa9639cd94058a515d3932
ZX SHA-256: f78f4a3a4d2ed2fd5021c021d861633b498ff0a07fb8bc7a676a9023c95c890b
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 159/10

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 52..84: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 230/154/5/19/59/102/3/25/20/16/29/0/0/0/27 | 167/53/3/6/15/56/0/18/15/12/37/0/0/1/14 | 1 | 581..1213: 148 instructions, 1 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 | 1 | none |
| text___zi_spec_6a19f7c98e79a734 | 21/4/1/6/6/12/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 1 | 2722..2744: 4 instructions, 1 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3196..3215: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 | 0 | 4018..5577: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 5780..5799: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 5967..6121: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 2 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 6196..6306: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 13 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 2 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 7206..7249: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 374/232/17/41/14/130/42/58/41/21/26/4/0/5/33 | 0 | 8231..10636: 517 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 1 | 12307..12566: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| text_DrawSession | 342/141/9/96/82/193/14/30/21/29/24/0/0/0/21 | 300/20/2/37/35/102/1/25/23/20/115/1/0/2/2 | 0 | 13868..14493: 121 instructions, 0 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 | 0 | 15475..16939: 260 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 148/83/4/6/2/51/14/12/8/12/6/1/0/6/13 | 2 | 17948..18288: 67 instructions, 2 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 0 | 18933..18955: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 118/103/2/0/0/11/3/22/20/0/0/0/0/3/32 | 0 | 21361..21680: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 | 0 | 22074..22166: 25 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 22917..23175: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 23962..23978: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 24270..24520: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 | 0 | 25904..26536: 148 instructions, 0 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 | 2 | 27630..30504: 616 instructions, 1 samples |
| std_math_Finite32 | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 3 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 | 0 | none |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 48/0/0/12/9/28/0/2/2/2/12/0/0/1/0 | 3 | 31068..31090: 4 instructions, 1 samples |
| text_PrepareText | 248/82/2/54/57/153/0/13/11/15/16/0/0/0/2 | 151/22/1/19/7/52/1/6/5/14/18/0/0/3/3 | 1 | 32136..32327: 47 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 2 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 1 | none |
| geometry_RotatedRectangleBounds | 165/99/6/38/48/90/1/11/5/6/76/0/0/0/6 | 116/0/0/28/17/47/0/0/0/10/103/0/0/0/0 | 1 | 32960..33264: 68 instructions, 1 samples |
| text_TextOrigin | 23/0/0/2/3/14/0/0/0/0/9/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 1 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 0 | 34630..34913: 74 instructions, 0 samples |
| text_TextHiddenLines | 118/40/2/11/2/22/0/11/9/10/21/0/0/0/4 | 116/20/2/2/1/18/1/11/9/3/55/1/0/0/2 | 0 | 35289..35382: 28 instructions, 0 samples |
| tree_TreeSetHiddenLines | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 | 0 | 35798..35820: 4 instructions, 0 samples |
| text_PaintTextLines | 692/610/7/184/149/360/10/49/42/45/83/0/0/0/42 | 519/378/9/116/83/259/7/34/25/23/188/0/0/10/38 | 4 | 36320..39509: 607 instructions, 3 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 1 | 40541..40819: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 43060..44108: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 45085..46307: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 1 | 46921..46943: 4 instructions, 1 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 5 | 48294..48316: 4 instructions, 4 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 16 | 48792..49580: 211 instructions, 12 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 | 0 | 49974..51205: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 1 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 | 0 | 52135..52625: 123 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 52756..52939: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 | 1 | 53205..53534: 77 instructions, 0 samples |
| text_DefaultTextFace | 65/4/1/5/32/45/2/1/0/1/20/0/0/0/1 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 | 1 | 53950..53969: 4 instructions, 0 samples |
| text_TextFaceFor | 59/16/4/20/17/41/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 | 0 | 54057..54076: 4 instructions, 0 samples |
| text_behavior_ResolveTextStyle | 127/99/1/14/14/30/0/23/22/9/8/0/0/2/18 | 56/0/0/5/0/8/0/3/3/2/7/0/0/2/0 | 1 | 54440..54819: 99 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 1 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 44/26/1/0/0/0/0/6/5/4/10/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | 55772..55869: 26 instructions, 0 samples |
| text_behavior_TextWrapPolicy | 21/0/0/0/0/0/0/1/1/1/2/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 537/475/17/65/180/310/14/50/33/27/17/12/1/0/55 | 348/109/17/13/6/102/41/57/40/4/17/10/0/13/13 | 0 | 56316..58556: 440 instructions, 0 samples |
| text_behavior_TextExtent | 17/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 | 1 | 58748..58818: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| geometry_WorldToScreen | 46/0/0/8/5/22/0/0/0/2/30/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 | 0 | none |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 1 | 59608..59915: 70 instructions, 1 samples |
| tree_TreeStoreLabel | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 | 3 | 60093..61681: 385 instructions, 2 samples |
| text_behavior_TextAlignmentOffset | 31/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 | 0 | 61887..61946: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_TextEllipsisStart | 95/84/5/23/14/42/1/10/5/4/0/0/0/0/11 | 53/21/3/2/2/10/3/7/4/1/0/0/0/0/2 | 0 | 62147..62472: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 | 0 | none |
| text_PaintTextLineRuns | 332/314/6/110/106/235/12/13/7/13/30/0/0/0/14 | 312/241/14/78/50/161/9/25/11/16/93/2/0/2/18 | 1 | 62681..64385: 296 instructions, 1 samples |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 1 | 65019..65038: 4 instructions, 0 samples |
| text_PaintTextEmphasis | 224/205/10/69/59/145/7/19/9/11/7/0/0/0/13 | 224/156/17/35/33/97/13/30/13/5/14/2/0/3/17 | 0 | 65551..66447: 169 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 38/16/1/3/5/10/0/3/2/1/11/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 | 0 | 66564..66624: 16 instructions, 0 samples |
| text_PaintTextRule | 195/4/1/69/73/148/2/2/1/10/46/0/0/0/1 | 102/0/0/26/21/52/0/4/2/6/53/0/0/0/0 | 0 | 66791..66813: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 67894..67904: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 1 | 68034..68053: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 2 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 1 | 68685..69013: 82 instructions, 1 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 | 1 | 69859..69881: 4 instructions, 1 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 1 | 70186..70208: 4 instructions, 1 samples |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 375/334/6/115/53/189/6/42/36/22/18/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 | 3 | 70744..71865: 279 instructions, 3 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 45/22/2/0/0/7/5/6/4/0/0/5/0/0/1 | 0 | 71904..71956: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 204/111/4/0/0/0/0/21/17/12/63/0/0/0/23 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 | 2 | 72604..73043: 108 instructions, 0 samples |
| std_math_CosineDegrees | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 80/0/0/0/0/24/0/12/12/1/63/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/0/14/7/3/3/2/0/1/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 | 18 | 73411..73579: 43 instructions, 18 samples |
| text_TextLinkRunAt | 119/32/2/37/18/67/1/12/10/4/0/0/0/0/3 | 51/23/7/2/0/9/4/14/7/0/0/0/0/0/2 | 0 | 73762..73890: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 | 0 | none |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 1 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 75493..75515: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 | 3 | 75780..79235: 711 instructions, 3 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 25/0/0/0/0/2/2/3/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_PaintClippedEmphasis | 177/4/1/18/84/150/2/2/1/2/25/0/0/0/1 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 | 0 | 80119..80138: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 80998..81017: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 141/4/1/21/93/117/2/1/0/4/32/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 | 0 | 81999..82018: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 2 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 1 | 92684..92703: 4 instructions, 1 samples |
| text_layout_TextTokenNext | 300/277/12/43/22/80/6/60/48/27/0/0/0/0/45 | 209/116/20/4/2/22/7/55/35/3/0/0/0/3/34 | 8 | 93010..94104: 269 instructions, 8 samples |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 94291..94444: 42 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 1 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 95668..95746: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 | 0 | 96086..97158: 254 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 97775..97794: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 | 0 | 98839..99092: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 99137..99159: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 99757..99779: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 19/0/0/6/3/10/0/0/0/0/9/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 123/8/2/10/85/97/4/2/0/1/26/0/0/0/2 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 | 0 | 100340..100359: 4 instructions, 0 samples |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 | 4 | 101440..101462: 4 instructions, 0 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 2 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 82/46/12/0/0/8/0/24/12/1/0/0/0/0/16 | 1 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 2 | 102717..102762: 12 instructions, 1 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 103733..104288: 127 instructions, 0 samples |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 | 0 | 104988..105657: 154 instructions, 0 samples |
| shape_paint_EmitShape | 104/16/4/17/12/33/8/18/14/7/14/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 | 0 | 105863..105885: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 | 0 | 106308..106330: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 106867..106886: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 2 | 107336..107355: 4 instructions, 0 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 2 | 107854..107873: 4 instructions, 2 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 1 | 108540..108556: 4 instructions, 1 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 2 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 1 | 109012..109028: 4 instructions, 1 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 6 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/7/4/0/0/0/0/1/7 | 4 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 | 1 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 7 | 111978..112064: 21 instructions, 7 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 0 | 112276..112362: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 106/18/2/0/0/18/7/14/12/4/0/0/0/4/1 | 0 | 112584..112670: 21 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 112991..113150: 39 instructions, 0 samples |
| paint_queue_FillShapeRows | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 324/262/6/37/24/70/2/22/16/2/285/0/0/0/2 | 0 | 113227..114179: 225 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 114313..114670: 92 instructions, 0 samples |
| shape_paint_EmitLineShape | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 | 0 | 114755..114777: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 215/79/4/50/37/89/6/18/14/17/53/0/0/0/10 | 279/234/11/37/20/102/11/19/8/10/112/2/0/8/25 | 0 | 116033..116368: 71 instructions, 0 samples |
| shape_paint_EmitRingShape | 198/59/3/35/24/63/4/26/23/18/52/0/0/0/11 | 174/55/3/27/13/76/9/19/16/9/97/0/0/0/0 | 0 | 117206..117384: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 | 0 | 117431..117453: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 2 | 117913..119843: 406 instructions, 2 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 119972..120019: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 120314..121158: 183 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 121287..121334: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 121629..122473: 183 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 122602..122649: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| shape_paint_ShapeAt | 19/0/0/2/3/10/0/0/0/0/7/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 164/0/0/32/24/73/0/9/9/7/71/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 | 0 | none |
| shape_paint_PointsBounds | 114/99/5/20/10/38/1/11/6/5/36/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/5 | 0 | 124376..124666: 75 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 349/43/5/70/57/178/14/33/28/28/72/0/0/0/7 | 566/480/21/63/91/185/30/86/64/10/306/0/0/1/48 | 0 | 125857..125982: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcDirections | 65/26/1/10/10/28/3/4/3/3/22/0/0/0/2 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 | 0 | 126575..126689: 26 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 142/127/10/15/3/18/0/24/14/19/6/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 | 0 | 126849..127282: 115 instructions, 0 samples |
| paint_queue_GradientColorAt | 62/46/2/13/6/20/0/5/3/6/13/0/0/0/4 | 143/0/0/0/0/17/0/16/16/1/72/0/0/13/0 | 0 | 127396..127570: 44 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 42/29/1/9/8/19/0/2/1/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 | 0 | 127612..127728: 29 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 66/53/1/17/14/35/0/2/1/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 | 0 | 127771..127988: 53 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 128095..128201: 30 instructions, 0 samples |
| std_math_Sqrt | 161/101/4/7/3/11/0/20/16/10/47/0/2/0/16 | 226/41/7/0/0/40/6/32/25/0/139/0/0/2/6 | 0 | 128440..128834: 97 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 103/67/1/11/6/41/5/9/8/6/10/0/0/0/12 | 62/0/0/6/4/21/3/6/6/2/28/0/0/0/0 | 0 | 129017..129267: 67 instructions, 0 samples |
| shape_paint_ShapeClipSide | 150/69/3/35/34/86/10/13/10/6/26/0/0/0/7 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 | 0 | 129486..129750: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 130029..130038: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 62/0/0/0/0/0/0/6/6/5/16/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 | 0 | none |
| shape_paint_ShapeArcPoint | 25/0/0/6/4/14/1/0/0/0/11/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 | 0 | none |
| std_math_Finite64 | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| std_math_MathSplit | 148/139/9/3/3/8/0/18/9/7/35/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 130753..131314: 124 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 106/96/7/9/11/20/0/19/12/6/34/0/0/0/8 | 86/33/5/0/0/14/3/22/17/0/34/0/0/1/4 | 0 | 131416..131763: 85 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 | 0 | none |
| shape_paint_ShapeCrossing | 98/0/0/22/9/33/0/10/10/2/37/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTriangles | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 | 0 | 132466..132601: 34 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 132648..132750: 24 instructions, 0 samples |

## Input / cc
Native SHA-256: 41ebf29cc36a375600f4c8331d516e198e9eff7952dbb17cac7b12e08fe9d9eb
C SHA-256: 2f57319d07a5c639b60d0c15b1ff0e821a92a3ea235000a36cba404a77fa242a
ZX SHA-256: 5e1886e969165f7fa4b574326f4d159b27900290d8d19ba18a37220d14af4cd3
Compiler: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
SIGPROF samples/runs: 154/8

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 77..109: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 394/8/2/53/124/218/8/37/35/26/35/0/0/0/2 | 164/148/15/11/14/58/2/18/3/12/8/0/0/0/42 | 1 | 850..869: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 11/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 | 0 | none |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 204/140/7/49/18/83/3/19/12/10/16/0/0/3/19 | 0 | 3616..3638: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 | 0 | none |
| tree_input_TreePointerUpdate | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 258/199/14/30/15/124/40/31/17/10/12/18/0/1/52 | 1 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | 6821..6840: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 | 0 | 7643..9202: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 9405..9424: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 | 0 | 9592..9746: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 1 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 | 0 | 9821..9931: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 22 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 | 1 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 10831..10874: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 | 0 | 11856..14261: 517 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 | 0 | 15932..16191: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 | 0 | none |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/0/0/0/0/0/0/0/0 | 1 | 16417..16555: 38 instructions, 1 samples |
| text_context_menu_TextMenuTakeCommand | 147/106/4/23/10/47/0/27/23/14/0/0/0/0/18 | 76/59/7/9/7/27/2/14/7/4/0/0/0/0/7 | 0 | 16821..17166: 90 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 4/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 64/21/1/6/5/13/2/4/3/4/11/0/0/1/1 | 0 | none |
| textfield_TextFieldCore | 3682/3548/50/745/1032/2092/26/398/348/251/251/1/1/0/263 | 2455/2369/103/737/417/1258/21/253/135/152/372/2/0/16/325 | 4 | 18162..35892: 3467 instructions, 4 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 79/0/0/15/15/49/4/3/3/6/0/4/0/0/0 | 54/21/3/1/1/21/8/5/2/3/0/0/0/0/3 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 23/5/1/0/0/5/0/4/3/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 18/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 | 0 | none |
| paint_queue_PaintSnapshotEmit | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 | 0 | 37900..39364: 260 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 | 2 | 40373..40713: 67 instructions, 2 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 201/158/14/35/13/71/4/27/13/13/41/2/0/1/16 | 1 | 42252..42730: 118 instructions, 1 samples |
| tree_input_TreeTextFocused | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 46/8/1/0/0/10/2/7/6/1/0/2/0/0/2 | 0 | 42849..42943: 22 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 8/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 | 0 | 43999..44015: 4 instructions, 0 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 | 1 | 44630..44652: 4 instructions, 0 samples |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 31/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 | 0 | 47123..47442: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 93/18/2/4/2/23/8/12/10/2/0/4/0/2/2 | 0 | 47836..47928: 25 instructions, 0 samples |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | 6/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | 48299..48557: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 | 0 | none |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 | 0 | 49081..49331: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/1/0/0/4 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 | 0 | 50715..51347: 148 instructions, 0 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 | 3 | 52441..55315: 616 instructions, 2 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_SystemClipboardText | 161/139/12/22/8/37/3/31/19/14/0/0/0/0/25 | 73/45/8/3/2/12/5/14/6/2/0/0/0/0/6 | 0 | 55538..56057: 132 instructions, 0 samples |
| input_protocol_PointInBounds | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 25/0/0/0/0/1/1/5/5/0/14/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenAt | 85/34/1/14/9/44/0/8/7/7/2/0/0/0/2 | 70/12/2/5/1/19/2/8/6/3/2/0/0/0/2 | 0 | 56549..56702: 34 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 59/24/2/1/1/11/1/8/6/3/0/2/0/0/5 | 0 | 57075..57130: 17 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 1 | none |
| tree_input_TreeTakeDragAt | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 172/147/13/11/5/53/2/28/15/7/21/7/0/0/20 | 0 | none |
| tree_TreePreviousWidgetState | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 46/0/0/0/5/15/2/4/4/1/2/2/0/0/0 | 0 | none |
| tree_input_TreeHasFocus | 96/68/2/11/8/35/6/10/8/8/0/3/0/0/14 | 71/44/3/5/4/20/6/9/6/3/0/1/0/0/7 | 0 | 59305..59462: 44 instructions, 0 samples |
| tree_input_TreeBlurAt | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 35/7/1/0/0/8/3/5/4/1/0/2/0/0/2 | 0 | none |
| textfield_TextFieldFaceFor | 144/134/4/24/61/91/4/9/5/8/28/0/0/0/10 | 129/78/1/31/16/65/1/3/2/11/52/0/0/0/5 | 1 | 59776..60491: 132 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 52/38/1/0/0/0/0/7/6/5/9/0/0/0/8 | 15/0/0/0/0/2/0/3/1/0/7/0/0/0/0 | 2 | 60856..60991: 38 instructions, 2 samples |
| textfield_TextFieldSecureDisplay | 60/34/2/16/5/27/1/7/5/3/0/0/0/0/5 | 49/20/2/0/0/4/1/5/3/2/0/0/0/0/3 | 0 | 61086..61204: 34 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 53/0/0/0/0/5/2/4/0/0/7/0/0/2/0 | 0 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 68/39/2/7/7/16/1/3/1/3/0/0/0/0/1 | 0 | 61660..61919: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 69/0/0/0/0/0/0/7/7/7/18/0/0/0/0 | 36/0/0/0/0/6/1/4/1/0/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 35/27/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/7/1/0/0/5/1/2/1/1/0/0/0/0/3 | 0 | 62210..62274: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 40/21/2/0/0/2/2/5/3/1/0/0/0/0/2 | 0 | 62420..62563: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 269/97/2/84/42/171/0/18/16/11/0/0/0/0/3 | 158/125/11/16/29/60/8/28/17/0/23/0/0/0/13 | 0 | 63237..63595: 97 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 16/11/1/2/3/6/1/2/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 22/8/1/0/0/5/3/5/4/0/0/0/0/0/3 | 0 | 63843..63920: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 135/96/8/14/3/35/9/21/13/4/0/0/0/1/9 | 0 | 64135..64280: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/93/9/5/17/29/3/20/11/0/0/0/0/5/14 | 0 | 64704..65365: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 174/4/1/48/39/113/2/18/17/7/2/0/0/0/1 | 109/77/9/16/14/37/1/14/5/5/12/0/0/0/6 | 0 | 65558..65580: 4 instructions, 0 samples |
| text_request_TextRequestTake | 190/0/0/43/24/110/10/16/16/11/0/9/0/0/0 | 85/0/0/5/4/27/3/9/9/5/0/1/0/0/0 | 0 | none |
| text_request_TextRequestChangeFor | 264/246/12/47/26/98/5/38/26/13/0/0/0/0/24 | 170/141/13/13/5/56/13/24/11/6/3/0/0/0/10 | 0 | 67101..67915: 219 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 68/50/5/0/0/7/2/8/3/4/0/0/0/0/5 | 1 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 35/0/0/0/0/5/1/2/2/1/0/0/0/0/0 | 0 | 68116..68342: 59 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 26/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 68377..68544: 51 instructions, 0 samples |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 46/0/0/3/3/12/3/4/3/1/4/2/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetCaptureTab | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 52/38/5/1/1/8/0/8/3/4/0/1/0/1/5 | 0 | none |
| tree_input_TreeFocusSubmittedAt | 225/4/1/39/26/96/10/28/27/14/8/7/0/0/1 | 161/143/18/12/20/70/4/24/6/6/25/5/0/2/17 | 0 | 69942..69961: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 153/105/7/16/5/51/17/26/19/3/8/5/0/1/15 | 0 | 71129..71514: 96 instructions, 0 samples |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 68/0/0/2/2/13/1/8/8/3/0/1/0/1/0 | 0 | 71720..72003: 74 instructions, 0 samples |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetTextValue | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 173/68/6/12/9/59/8/28/22/4/0/3/0/1/8 | 1 | 73839..74548: 178 instructions, 1 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 33/0/0/5/0/9/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 | 3 | 75207..75226: 4 instructions, 1 samples |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 1 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| paint_queue_PaintRoundedOutline | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 48/0/0/1/13/18/0/1/1/2/4/0/0/0/0 | 1 | 76423..76442: 4 instructions, 1 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 173/0/0/21/19/52/1/13/13/12/46/0/1/0/0 | 108/0/0/2/0/13/2/8/0/0/37/0/0/3/0 | 0 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 18/0/0/0/0/4/1/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 18/0/0/0/0/4/1/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 28/0/0/0/0/4/2/4/2/0/6/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 | 2 | 78170..78189: 4 instructions, 2 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 29/0/0/0/0/4/1/4/2/0/7/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 7/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 78637..78654: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 78684..78738: 18 instructions, 0 samples |
| paint_queue_PaintLine | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 45/0/0/1/10/15/0/1/1/2/2/0/0/0/0 | 0 | 79281..79300: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 | 0 | 79648..79667: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 14/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 79746..79768: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 21/5/1/0/0/7/3/3/2/0/0/0/0/0/1 | 0 | 79801..79846: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 86/0/0/16/15/53/6/3/3/5/0/6/0/0/0 | 52/17/2/1/1/16/6/4/2/3/0/0/0/0/3 | 0 | none |
| text_input_platform_TextInputFocused | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/25/4/0/0/7/2/5/1/3/0/0/0/0/1 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 12/6/1/0/0/1/0/3/1/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 51/6/1/1/2/12/7/3/2/1/2/0/0/2/4 | 0 | 80649..80716: 17 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 400/350/2/41/98/270/21/16/14/13/26/16/0/0/82 | 162/147/6/15/38/92/2/14/8/5/21/0/0/2/33 | 0 | 81077..82773: 350 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 | 1 | 83788..84066: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 | 0 | 86307..87355: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 | 0 | 88332..89554: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 | 3 | 90168..90190: 4 instructions, 3 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 | 7 | 90435..90457: 4 instructions, 2 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 32/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 | 1 | 92315..93103: 211 instructions, 1 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 421/373/45/30/47/105/9/89/44/1/2/0/0/10/98 | 2 | 93497..94728: 302 instructions, 1 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 1 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 | 1 | 95481..95729: 62 instructions, 1 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 77/18/2/2/2/18/8/5/3/2/0/4/0/2/2 | 0 | 96279..96462: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 | 1 | 96728..97057: 77 instructions, 0 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 12/0/0/0/0/3/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 84/63/7/10/5/24/2/12/5/7/1/1/0/0/6 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 21/0/0/3/0/7/0/0/0/0/18/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 71/4/1/4/39/46/2/1/0/6/25/0/0/0/1 | 65/0/0/9/12/33/0/0/0/6/24/0/0/0/0 | 0 | 98358..98377: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 267/236/11/58/44/144/24/25/14/11/14/2/0/4/37 | 189/151/5/31/50/120/13/11/6/4/76/0/0/5/15 | 0 | 98773..99740: 184 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/3/1/5/3/0/0/0/0/2/5 | 5 | 99762..99930: 43 instructions, 4 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 18/0/0/0/0/2/0/2/0/0/4/0/0/0/0 | 0 | 100167..100176: 5 instructions, 0 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 10/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 100268..100348: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 26/15/2/0/0/2/2/6/4/0/0/0/0/0/3 | 0 | 100401..100465: 17 instructions, 0 samples |
| std_grapheme_Previous | 59/44/2/10/4/14/0/7/5/5/0/0/0/0/4 | 33/9/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | 100559..100701: 42 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 142/95/6/5/8/36/14/20/14/3/0/6/0/6/20 | 0 | 101039..101315: 69 instructions, 0 samples |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 | 2 | 101444..101772: 82 instructions, 1 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 | 0 | 102169..102476: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 | 0 | 102654..104242: 385 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 114/100/2/23/5/33/2/11/8/2/53/0/0/2/12 | 1 | 104472..104494: 4 instructions, 1 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 | 0 | 104976..104998: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 | 9 | 105182..108637: 711 instructions, 8 samples |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 119/99/2/24/6/35/0/11/8/2/59/0/0/2/11 | 1 | 108952..108974: 4 instructions, 1 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 27/0/0/0/0/2/0/2/0/0/4/0/0/0/0 | 0 | 109407..109613: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 45/32/1/12/2/29/0/3/2/2/20/0/0/0/1 | 1 | 109686..109708: 4 instructions, 0 samples |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 | 1 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 | 0 | 110942..110964: 4 instructions, 0 samples |
| paint_queue_EmitLine | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 83/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 111681..112276: 157 instructions, 0 samples |
| button_SubmitButton | 621/600/18/167/120/309/20/59/41/56/51/0/0/0/60 | 447/414/9/119/78/217/3/21/10/36/134/0/0/1/35 | 0 | 112380..115200: 585 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 966/931/24/230/238/556/28/81/57/58/87/16/0/0/95 | 542/515/31/88/88/248/32/67/36/19/103/9/0/4/70 | 0 | 115479..119868: 906 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 1 | 120256..120266: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 | 2 | 120396..120415: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 278/259/112/0/0/136/9/116/4/0/0/1/0/3/122 | 1 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 22/0/0/2/0/6/0/0/0/0/18/0/0/0/0 | 1 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 36/0/0/0/0/5/0/0/0/0/0/0/0/5/0 | 0 | 122123..122429: 79 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 85/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 148/0/0/59/14/82/0/15/15/0/70/0/0/0/0 | 0 | 124117..124139: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/2/15/6/3/3/1/0/2/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 | 0 | 125182..125851: 154 instructions, 0 samples |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 | 0 | 126001..127073: 254 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 60/44/3/6/6/19/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 | 0 | 127653..127672: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 | 2 | 128548..128567: 4 instructions, 2 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 | 7 | 129088..129107: 4 instructions, 7 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 | 0 | 130530..130608: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 28/0/0/0/0/5/2/1/1/2/0/3/0/0/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/39/4/3/3/15/4/8/4/3/0/4/0/0/7 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 274/176/4/90/72/183/2/10/6/16/144/0/0/0/7 | 0 | 131790..131812: 4 instructions, 0 samples |
| button_ButtonDetailSize | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 41/0/0/10/1/11/0/4/4/2/7/2/0/2/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| button_behavior_MeasureBounds | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 263/223/22/59/30/100/3/41/19/5/145/0/0/0/13 | 0 | 133018..134292: 300 instructions, 0 samples |
| style_StyleMinHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 378/291/13/41/19/86/4/61/48/25/90/0/0/0/41 | 203/183/15/35/11/67/5/31/14/1/113/0/0/0/10 | 0 | 134822..136102: 279 instructions, 0 samples |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/8/0/6/6/2/0/2/0/1/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1313/1276/16/257/184/508/12/135/119/93/317/0/1/0/133 | 916/893/51/266/138/459/11/100/43/43/383/2/0/8/84 | 0 | 137091..143219: 1258 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 27/12/1/0/0/1/0/2/1/2/0/0/0/0/1 | 0 | none |
| tree_TreeNextKey | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/6/1/1/1/12/4/5/4/2/0/7/0/2/1 | 0 | 143627..143689: 15 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 42/8/1/2/2/9/0/4/3/2/0/2/0/0/2 | 0 | none |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/4/3/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 23/7/2/0/0/6/1/6/4/0/0/1/0/0/2 | 0 | 144071..144153: 21 instructions, 0 samples |
| contextmenu_ContextMenuFind | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/1/5/3/0/0/0/0/0/3 | 0 | 144218..144305: 23 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 105/43/1/32/22/60/1/3/2/6/37/1/0/0/1 | 83/0/0/10/8/19/1/2/1/4/46/1/0/0/0 | 0 | 144693..144870: 43 instructions, 0 samples |
| overlay_Draw | 388/311/1/115/78/209/0/28/27/28/19/0/0/0/28 | 259/229/10/44/27/80/4/24/14/20/33/0/0/5/32 | 0 | 145189..146491: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1416/1199/10/370/341/768/10/72/62/142/309/1/0/0/114 | 1062/861/26/302/118/483/14/69/31/103/299/1/0/3/119 | 0 | 147581..153389: 1088 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/3/0/0/0/0/5/0/0/0/0 | 2 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 | 0 | 164818..164977: 39 instructions, 0 samples |
| paint_queue_FillShapeRows | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 | 0 | 165054..166006: 225 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 | 0 | 166140..166497: 92 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 194/165/10/21/26/76/11/29/19/5/21/4/0/1/22 | 0 | 167481..168036: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 | 19 | 169362..169381: 4 instructions, 5 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 | 1 | 170115..170160: 12 instructions, 1 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/0/0/11/0/0/1/0 | 0 | none |
| button_ButtonFaceFinish | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 46/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 0 | 170253..170411: 31 instructions, 0 samples |
| button_ButtonDefaultFace | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 227/179/12/38/30/96/7/27/15/16/39/0/0/0/25 | 0 | 170880..171867: 191 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 46/31/1/8/1/10/0/3/2/4/0/0/0/0/3 | 0 | 171912..172069: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 11/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 19/0/0/0/0/0/0/1/1/0/16/0/0/0/0 | 0 | 172364..172526: 45 instructions, 0 samples |
| button_behavior_MeasureContent | 158/142/5/4/9/15/0/28/23/9/41/0/0/0/14 | 74/38/3/0/0/5/2/9/6/0/45/0/0/0/0 | 0 | 172611..173177: 138 instructions, 0 samples |
| button_behavior_ShapeWidth | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 27/0/0/0/0/3/2/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 37/23/3/5/2/14/1/7/4/2/8/0/0/0/3 | 0 | 173509..173531: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 9/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 45/22/2/2/1/10/2/6/2/0/28/0/0/0/2 | 0 | 173858..174070: 53 instructions, 0 samples |
| image_lookup_IconLookupImage | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 28/12/1/1/1/6/1/3/2/2/0/0/0/0/2 | 0 | 174356..174375: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 16/0/0/0/0/1/0/2/2/0/9/0/0/0/0 | 0 | 174584..174726: 39 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 92/53/4/0/0/0/0/11/7/7/27/0/0/0/7 | 23/15/1/0/0/6/1/3/2/0/16/0/0/0/0 | 0 | 174902..175074: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 223/167/5/37/26/72/0/26/21/9/91/0/0/0/12 | 124/89/8/2/0/14/3/17/9/1/77/0/0/0/1 | 0 | 175342..176029: 162 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 345/292/4/71/57/143/3/27/23/22/88/4/1/0/36 | 225/195/7/30/25/84/9/17/8/8/91/1/0/4/21 | 0 | 176274..177553: 288 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 58/0/0/19/15/39/0/5/5/2/28/0/0/0/0 | 0 | 177822..177844: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 76/36/1/15/3/27/1/8/7/1/33/0/0/1/5 | 0 | 178359..178381: 4 instructions, 0 samples |
| paint_queue_PaintImage | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 33/0/0/6/5/11/0/1/1/2/4/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 456/420/20/93/62/191/12/52/32/33/57/11/0/0/41 | 257/196/11/37/28/99/5/30/19/19/56/2/0/1/32 | 0 | 179674..180950: 288 instructions, 0 samples |
| dpi_ScalePixels | 83/72/1/0/0/0/0/10/9/8/21/0/0/0/14 | 43/0/0/0/0/7/0/4/1/0/22/0/0/0/0 | 0 | 181066..181341: 72 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 101/83/5/20/9/37/4/13/8/9/25/4/0/0/12 | 86/54/7/3/7/23/5/14/7/2/24/1/0/0/11 | 0 | 181417..181673: 59 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 20/14/1/0/0/1/1/2/1/0/0/0/0/2/4 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/25/2/4/3/14/3/7/5/5/0/0/0/0/4 | 0 | none |
| tree_TreeSetPopup | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 36/0/0/0/0/7/0/5/5/2/0/2/0/0/0 | 0 | none |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 182773..182926: 42 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 22/11/1/2/1/7/1/3/2/2/0/0/0/0/1 | 0 | none |
| tree_TreePushScope | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 81/63/5/0/0/12/10/9/4/8/0/8/0/0/12 | 0 | none |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/2/2/4/0/1/1/2/2/0/0/0/0 | 0 | 183827..183840: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 28/13/4/0/0/8/4/7/3/0/0/0/0/0/4 | 0 | 183855..184018: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 35/0/0/2/0/4/0/2/2/4/4/0/0/0/0 | 0 | 184268..184277: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 23/6/1/0/0/6/1/3/2/0/0/2/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 639/592/8/74/327/438/14/31/23/21/75/0/0/0/35 | 191/96/5/19/43/82/13/12/7/5/77/0/0/1/19 | 0 | 184813..187650: 556 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 0 | 188171..188257: 21 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 | 0 | 188469..188555: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 | 0 | 188777..188863: 21 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 189329..189348: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 12 | 189798..189817: 4 instructions, 5 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 2 | 190316..190335: 4 instructions, 2 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 | 3 | 191002..191018: 4 instructions, 1 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 | 3 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 5 | 191474..191490: 4 instructions, 5 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 55/0/0/0/0/1/1/1/1/0/0/10/0/12/0 | 0 | 191558..192219: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 115/84/6/20/13/54/2/13/7/5/32/0/0/0/10 | 0 | 192749..192768: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 24/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 69/52/5/10/4/30/6/9/4/2/27/0/0/0/3 | 0 | 193474..193621: 40 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 38/29/1/8/5/14/1/3/2/3/0/0/0/0/3 | 0 | 193681..193760: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 4/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 123/104/6/4/12/41/10/10/4/3/4/1/0/1/23 | 0 | 193876..194293: 99 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 74/27/2/0/0/0/0/11/9/6/19/0/0/0/5 | 35/10/1/0/0/5/0/6/3/0/16/0/0/0/0 | 0 | 194816..194915: 26 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 125/61/5/6/4/55/20/11/6/2/5/2/0/3/10 | 0 | 195085..195287: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 9/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 105/77/2/20/34/60/0/4/2/3/26/0/0/0/8 | 0 | 196523..196542: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 7/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 230/196/7/10/16/40/7/37/30/13/9/0/6/0/35 | 121/97/6/2/7/36/13/14/6/1/6/4/0/6/25 | 0 | 197207..197863: 195 instructions, 0 samples |
| tree_TreePopScope | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 54/26/1/3/2/7/6/4/3/4/0/4/0/0/5 | 0 | none |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 198347..198453: 30 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 | 5 | 198564..200494: 406 instructions, 5 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 200623..200670: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 | 0 | 200965..201809: 183 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 201938..201985: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 | 0 | 202280..203124: 183 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/55/7/2/2/15/2/18/11/1/0/1/0/1/8 | 0 | 203253..203300: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 21/12/1/2/1/6/1/3/2/2/2/0/0/0/1 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 19/6/1/0/0/5/2/2/1/0/0/1/0/0/1 | 0 | 203488..203546: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/27/2/3/4/24/5/4/2/1/5/0/0/1/8 | 0 | none |
| svg_shape_ShapeRaster | 258/218/9/32/17/69/6/47/38/15/14/2/0/0/40 | 127/89/5/5/4/35/7/17/12/4/9/1/0/0/18 | 0 | 204074..204881: 197 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 326/294/20/30/30/93/9/48/28/9/0/8/1/2/41 | 330/291/25/30/20/108/27/52/27/1/26/2/0/8/51 | 0 | 205367..206143: 205 instructions, 0 samples |
| image_behavior_ImageFitBounds | 196/177/8/18/9/31/0/26/18/13/45/0/0/0/18 | 92/77/6/6/2/15/0/16/10/0/56/0/0/0/7 | 0 | 206222..206884: 168 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 99/55/1/9/1/12/1/16/15/7/12/0/0/0/4 | 35/14/1/0/0/8/2/8/7/0/12/0/0/0/0 | 0 | 206991..207212: 55 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 207334..207356: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 17/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 207765..207787: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 343/304/21/56/16/91/15/62/41/19/54/1/0/0/48 | 162/151/12/0/0/48/26/32/20/0/21/0/0/0/24 | 0 | 208001..209131: 271 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 610/590/44/147/107/273/7/91/47/35/156/0/0/0/51 | 447/416/39/36/21/126/22/74/35/14/90/0/0/0/87 | 0 | 209350..211617: 504 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 326/308/14/23/20/72/15/49/35/14/56/3/0/0/51 | 222/201/15/15/13/79/21/37/21/2/47/0/0/1/30 | 0 | 212366..213347: 241 instructions, 0 samples |
| paint_queue_EmitImageBegin | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 20/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 40/0/0/4/3/11/0/2/0/1/16/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 8/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 18/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 15/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 137/123/3/0/0/16/4/16/13/11/38/0/0/0/22 | 110/13/2/0/0/21/11/13/7/0/42/0/0/2/2 | 0 | 214362..214839: 122 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 602/520/11/90/49/179/20/57/46/38/268/0/3/0/61 | 367/340/21/19/10/94/24/47/25/7/185/0/3/0/38 | 0 | 215202..217471: 495 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 6/0/0/0/0/2/1/2/1/0/0/0/0/0/0 | 0 | 217653..217776: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 70/66/3/0/0/16/8/8/5/2/17/0/0/0/15 | 52/34/3/0/0/19/7/7/4/0/14/0/0/0/4 | 0 | 217814..218037: 52 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 53/35/3/0/0/13/5/8/5/0/19/0/0/1/10 | 0 | 218122..218328: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 21/6/1/0/0/4/0/2/1/0/12/0/0/0/1 | 0 | 218431..218537: 30 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 11/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 10/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 9/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 20/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 219156..219172: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 219378..219394: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 100/86/3/4/5/27/12/13/10/4/25/0/2/0/14 | 95/68/5/0/0/27/19/15/10/0/23/0/2/2/13 | 0 | 219574..219768: 46 instructions, 0 samples |

## Input / clang
Native SHA-256: 41ebf29cc36a375600f4c8331d516e198e9eff7952dbb17cac7b12e08fe9d9eb
C SHA-256: 95a389acb00b04cbeea8f464bff6ff620803c10829e91e4f7ea138139ba44bcb
ZX SHA-256: 5e1886e969165f7fa4b574326f4d159b27900290d8d19ba18a37220d14af4cd3
Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)
SIGPROF samples/runs: 151/8

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 24/7/1/2/1/3/0/3/2/2/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 77..109: 7 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 394/8/2/53/124/218/8/37/35/26/35/0/0/0/2 | 157/94/13/11/24/69/0/19/6/11/23/0/0/0/24 | 0 | 850..869: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 | 0 | none |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 121/0/0/12/6/37/2/9/9/9/2/0/0/0/0 | 0 | 3616..3638: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_input_TreePointerUpdate | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 210/110/2/8/1/65/22/28/26/7/18/6/0/4/23 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 1 | 6821..6840: 4 instructions, 1 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 | 0 | 7643..9202: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 9405..9424: 4 instructions, 0 samples |
| std_grapheme_Boundary | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 9592..9746: 46 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 9821..9931: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 20 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 1 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 10831..10874: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 373/232/17/41/14/130/43/58/41/21/26/5/0/4/33 | 0 | 11856..14261: 517 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 1 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 15932..16191: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 2 | 16417..16555: 38 instructions, 1 samples |
| text_context_menu_TextMenuTakeCommand | 147/106/4/23/10/47/0/27/23/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 | 0 | 16821..17166: 90 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 47/0/0/4/4/8/0/4/4/4/10/0/0/0/0 | 0 | none |
| textfield_TextFieldCore | 3682/3548/50/745/1032/2092/26/398/348/251/251/1/1/0/263 | 2086/536/26/384/149/876/24/235/209/148/294/1/0/13/54 | 12 | 18162..35892: 3467 instructions, 8 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 79/0/0/15/15/49/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 | 0 | 37900..39364: 260 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 148/83/4/6/2/51/14/12/8/12/6/1/0/6/13 | 2 | 40373..40713: 67 instructions, 2 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 0 | 41207..42265: 229 instructions, 0 samples |
| tree_input_TreeTextFocused | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 1 | 42849..42943: 22 instructions, 1 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 1 | 43999..44015: 4 instructions, 0 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 0 | 44630..44652: 4 instructions, 0 samples |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 118/103/2/0/0/11/3/22/20/0/0/0/0/3/32 | 0 | 47123..47442: 82 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 | 0 | 47836..47928: 25 instructions, 0 samples |
| std_grapheme_Next | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | 48299..48557: 72 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 49081..49331: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 | 1 | 50532..50786: 62 instructions, 1 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 | 5 | 52441..55315: 616 instructions, 4 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 1 | none |
| system_clipboard_SystemClipboardText | 161/139/12/22/8/37/3/31/19/14/0/0/0/0/25 | 63/15/2/0/0/5/1/9/7/2/0/0/0/0/1 | 0 | 55538..56057: 132 instructions, 0 samples |
| input_protocol_PointInBounds | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 1 | none |
| text_context_menu_TextMenuOpenAt | 85/34/1/14/9/44/0/8/7/7/2/0/0/0/2 | 64/12/2/5/0/19/1/6/4/4/2/0/0/0/1 | 0 | 56549..56702: 34 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 0 | 57075..57130: 17 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakeDragAt | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 149/69/5/9/1/47/11/25/20/6/8/6/0/0/15 | 0 | none |
| tree_TreePreviousWidgetState | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 | 0 | none |
| tree_input_TreeHasFocus | 96/68/2/11/8/35/6/10/8/8/0/3/0/0/14 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 | 0 | 59305..59462: 44 instructions, 0 samples |
| tree_input_TreeBlurAt | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 | 0 | none |
| textfield_TextFieldFaceFor | 144/134/4/24/61/91/4/9/5/8/28/0/0/0/10 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 | 0 | 59776..60491: 132 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 52/38/1/0/0/0/0/7/6/5/9/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 2 | 60856..60991: 38 instructions, 1 samples |
| textfield_TextFieldSecureDisplay | 60/34/2/16/5/27/1/7/5/3/0/0/0/0/5 | 53/8/1/0/0/2/2/5/4/2/0/0/0/0/1 | 0 | 61086..61204: 34 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 | 0 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 | 0 | 61660..61919: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 69/0/0/0/0/0/0/7/7/7/18/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 35/27/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 | 0 | 62210..62274: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | 62420..62563: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 269/97/2/84/42/171/0/18/16/11/0/0/0/0/3 | 104/92/3/0/0/37/5/14/11/0/19/0/0/0/11 | 0 | 63237..63595: 97 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 32/9/1/0/0/1/0/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 24/11/1/0/0/3/2/4/3/0/0/0/0/0/3 | 0 | 63843..63920: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 | 0 | 64135..64280: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 91/0/0/2/0/2/0/13/13/0/0/0/0/6/0 | 0 | 64704..65365: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 174/4/1/48/39/113/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 | 1 | 65558..65580: 4 instructions, 1 samples |
| text_request_TextRequestTake | 190/0/0/43/24/110/10/16/16/11/0/9/0/0/0 | 67/0/0/1/0/17/1/7/7/4/3/1/0/0/0 | 0 | none |
| text_request_TextRequestChangeFor | 264/246/12/47/26/98/5/38/26/13/0/0/0/0/24 | 147/55/6/6/12/41/8/22/16/5/0/0/0/2/0 | 0 | 67101..67915: 219 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 63/17/2/0/0/3/3/8/6/4/0/0/0/0/1 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 31/0/0/1/3/8/0/2/2/1/2/0/0/0/0 | 0 | 68116..68342: 59 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 68377..68544: 51 instructions, 0 samples |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 1 | none |
| tree_TreeSetCaptureTab | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 | 1 | none |
| tree_input_TreeFocusSubmittedAt | 225/4/1/39/26/96/10/28/27/14/8/7/0/0/1 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 | 0 | 69942..69961: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 | 0 | 71129..71514: 96 instructions, 0 samples |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 0 | 71720..72003: 74 instructions, 0 samples |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetTextValue | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 138/86/3/8/4/48/13/22/19/5/0/1/0/2/6 | 2 | 73839..74548: 178 instructions, 2 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 32/0/0/5/0/8/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 2 | 75207..75226: 4 instructions, 0 samples |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| paint_queue_PaintRoundedOutline | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 3 | 76423..76442: 4 instructions, 2 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 173/0/0/21/19/52/1/13/13/12/46/0/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 | 0 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 19/0/0/0/0/3/0/2/2/1/11/0/0/0/0 | 1 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 1 | 78170..78189: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 78637..78654: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 78684..78738: 18 instructions, 0 samples |
| paint_queue_PaintLine | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 79281..79300: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 79648..79667: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 79746..79768: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 77/28/4/0/0/22/20/15/11/0/4/0/0/0/3 | 0 | 79801..79846: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 86/0/0/16/15/53/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocused | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 | 0 | 80649..80716: 17 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 400/350/2/41/98/270/21/16/14/13/26/16/0/0/82 | 143/0/0/6/36/78/0/12/12/5/18/0/0/2/0 | 0 | 81077..82773: 350 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 1 | 83788..84066: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 86307..87355: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 88332..89554: 290 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 1 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 90022..90044: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 2 | 90538..90560: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 92315..93103: 211 instructions, 0 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 | 2 | 93497..94728: 302 instructions, 2 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 1 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 | 2 | 95658..96148: 123 instructions, 2 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 1 | 96279..96462: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 | 0 | 96728..97057: 77 instructions, 0 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 81/12/1/6/0/13/3/12/11/6/5/1/0/2/3 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 71/4/1/4/39/46/2/1/0/6/25/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 | 0 | 98358..98377: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 267/236/11/58/44/144/24/25/14/11/14/2/0/4/37 | 253/196/17/22/19/107/14/33/16/4/0/0/0/12/36 | 0 | 98773..99740: 184 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 | 3 | 99862..100024: 47 instructions, 2 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 0 | 100167..100176: 5 instructions, 0 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 100268..100348: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 25/7/1/0/0/3/2/3/2/0/0/0/0/0/2 | 0 | 100401..100465: 17 instructions, 0 samples |
| std_grapheme_Previous | 59/44/2/10/4/14/0/7/5/5/0/0/0/0/4 | 65/38/2/6/2/11/2/8/6/2/0/0/0/1/4 | 0 | 100559..100701: 42 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 0 | 101039..101315: 69 instructions, 0 samples |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 0 | 101444..101772: 82 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 1 | 102169..102476: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 | 2 | 102654..104242: 385 instructions, 1 samples |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 3 | 104356..104378: 4 instructions, 1 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 104976..104998: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 | 6 | 105182..108637: 711 instructions, 5 samples |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 108836..108858: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 0 | 109407..109613: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 1 | 109686..109708: 4 instructions, 1 samples |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 110942..110964: 4 instructions, 0 samples |
| paint_queue_EmitLine | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 111681..112276: 157 instructions, 0 samples |
| button_SubmitButton | 621/600/18/167/120/309/20/59/41/56/51/0/0/0/60 | 376/10/1/57/34/129/0/17/16/38/33/2/0/3/1 | 0 | 112380..115200: 585 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 966/931/24/230/238/556/28/81/57/58/87/16/0/0/95 | 553/476/26/87/96/234/31/71/45/19/102/14/0/3/59 | 0 | 115479..119868: 906 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 120256..120266: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 6 | 120396..120415: 4 instructions, 2 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 2 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 1 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 1 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 122123..122429: 79 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 124117..124139: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 34/0/0/0/0/14/7/3/3/2/0/1/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 2 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 1 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 | 0 | 125182..125851: 154 instructions, 0 samples |
| paint_queue_EmitPaintNow | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 | 0 | 126001..127073: 254 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 45/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 127653..127672: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 | 0 | 128717..128970: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 9 | 129088..129107: 4 instructions, 6 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 1 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 130530..130608: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| style_ResolveFlags | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 0 | 131790..131812: 4 instructions, 0 samples |
| button_ButtonDetailSize | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 45/0/0/3/0/8/0/4/4/2/5/2/0/1/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 | 0 | none |
| button_behavior_MeasureBounds | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 190/0/0/19/10/49/0/23/23/2/120/0/0/0/0 | 0 | 133018..134292: 300 instructions, 0 samples |
| style_StyleMinHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 378/291/13/41/19/86/4/61/48/25/90/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/1 | 0 | 134822..136102: 279 instructions, 0 samples |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1313/1276/16/257/184/508/12/135/119/93/317/0/1/0/133 | 793/0/0/161/116/384/0/54/54/41/331/4/0/7/0 | 0 | 137091..143219: 1258 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_TreeNextKey | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 | 0 | 143627..143689: 15 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 | 0 | none |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 | 0 | 144071..144153: 21 instructions, 0 samples |
| contextmenu_ContextMenuFind | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 | 0 | 144218..144305: 23 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 105/43/1/32/22/60/1/3/2/6/37/1/0/0/1 | 122/32/8/25/13/45/3/12/3/5/50/2/0/0/9 | 0 | 144693..144870: 43 instructions, 0 samples |
| overlay_Draw | 388/311/1/115/78/209/0/28/27/28/19/0/0/0/28 | 201/82/1/16/5/38/0/16/15/19/35/0/0/1/9 | 0 | 145189..146491: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1416/1199/10/370/341/768/10/72/62/142/309/1/0/0/114 | 939/718/13/213/93/370/25/55/42/101/293/2/0/1/93 | 0 | 147581..153389: 1088 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 1 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 164818..164977: 39 instructions, 0 samples |
| paint_queue_FillShapeRows | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 324/262/6/37/24/70/2/22/16/2/285/0/0/0/2 | 0 | 165054..166006: 225 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 166140..166497: 92 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 167481..168036: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 | 7 | 169362..169381: 4 instructions, 4 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 170115..170160: 12 instructions, 0 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| button_ButtonFaceFinish | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 170253..170411: 31 instructions, 0 samples |
| button_ButtonDefaultFace | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 0 | 170880..171867: 191 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 171912..172069: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 172364..172526: 45 instructions, 0 samples |
| button_behavior_MeasureContent | 158/142/5/4/9/15/0/28/23/9/41/0/0/0/14 | 44/0/0/0/0/1/0/8/8/0/29/0/0/0/0 | 0 | 172611..173177: 138 instructions, 0 samples |
| button_behavior_ShapeWidth | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 0 | 173509..173531: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 27/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 173858..174070: 53 instructions, 0 samples |
| image_lookup_IconLookupImage | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 174356..174375: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 174584..174726: 39 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 92/53/4/0/0/0/0/11/7/7/27/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 174902..175074: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 223/167/5/37/26/72/0/26/21/9/91/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/87/0/0/0/0 | 0 | 175342..176029: 162 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 345/292/4/71/57/143/3/27/23/22/88/4/1/0/36 | 246/35/1/23/20/69/6/17/16/6/128/1/0/1/3 | 0 | 176274..177553: 288 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 177822..177844: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 178359..178381: 4 instructions, 0 samples |
| paint_queue_PaintImage | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 456/420/20/93/62/191/12/52/32/33/57/11/0/0/41 | 273/193/8/42/16/91/6/30/22/22/45/1/0/0/32 | 0 | 179674..180950: 288 instructions, 0 samples |
| dpi_ScalePixels | 83/72/1/0/0/0/0/10/9/8/21/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 | 0 | 181066..181341: 72 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 101/83/5/20/9/37/4/13/8/9/25/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 | 0 | 181417..181673: 59 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 21/0/0/0/0/0/0/0/0/0/0/0/0/3/0 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 | 0 | none |
| tree_TreeSetPopup | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 | 0 | none |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 182773..182926: 42 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_TreePushScope | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 | 0 | none |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 | 0 | 183827..183840: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 25/14/1/0/0/5/0/3/2/0/0/0/0/0/4 | 0 | 183855..184018: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 | 0 | 184268..184277: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 639/592/8/74/327/438/14/31/23/21/75/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 | 0 | 184813..187650: 556 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 42/0/0/0/0/6/4/5/5/4/0/4/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 1 | 188171..188257: 21 instructions, 1 samples |
| paint_queue_KeepMediumPaintBytes | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 115/18/2/0/0/18/7/15/13/4/0/0/0/4/1 | 0 | 188469..188555: 21 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 106/18/2/0/0/18/7/14/12/4/0/0/0/4/1 | 0 | 188777..188863: 21 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 189329..189348: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 11 | 189798..189817: 4 instructions, 4 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 3 | 190316..190335: 4 instructions, 0 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 4 | 191002..191018: 4 instructions, 3 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 5 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 4 | 191474..191490: 4 instructions, 3 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 191558..192219: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 73/0/0/6/0/29/1/9/8/3/12/0/0/0/0 | 0 | 192749..192768: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 193474..193621: 40 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 193681..193760: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 164/41/4/6/7/61/19/20/16/3/10/5/0/0/4 | 0 | 193876..194293: 99 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 74/27/2/0/0/0/0/11/9/6/19/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 194816..194915: 26 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 192/156/12/7/3/67/28/24/12/1/9/2/0/10/39 | 0 | 195085..195287: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 196523..196542: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 230/196/7/10/16/40/7/37/30/13/9/0/6/0/35 | 95/25/2/1/4/27/12/9/7/1/6/4/0/4/4 | 0 | 197207..197863: 195 instructions, 0 samples |
| tree_TreePopScope | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 42/0/0/0/0/6/4/5/5/4/0/4/0/0/0 | 0 | none |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 198347..198453: 30 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 2 | 198564..200494: 406 instructions, 2 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 200623..200670: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 200965..201809: 183 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 201938..201985: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 202280..203124: 183 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 69/6/1/0/0/7/2/12/11/1/0/0/0/2/1 | 0 | 203253..203300: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 47/22/2/0/0/6/5/6/4/0/0/5/0/0/1 | 0 | 203488..203546: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 258/218/9/32/17/69/6/47/38/15/14/2/0/0/40 | 129/113/4/3/3/27/9/17/13/4/8/1/0/0/21 | 0 | 204074..204881: 197 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 326/294/20/30/30/93/9/48/28/9/0/8/1/2/41 | 372/321/22/23/18/117/47/58/36/1/32/4/0/4/51 | 0 | 205367..206143: 205 instructions, 0 samples |
| image_behavior_ImageFitBounds | 196/177/8/18/9/31/0/26/18/13/45/0/0/0/18 | 43/0/0/0/0/3/0/9/9/0/30/0/0/0/0 | 0 | 206222..206884: 168 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 99/55/1/9/1/12/1/16/15/7/12/0/0/0/4 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 206991..207212: 55 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 207334..207356: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 207765..207787: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 343/304/21/56/16/91/15/62/41/19/54/1/0/0/48 | 179/142/8/0/0/31/14/38/30/0/33/0/0/0/25 | 0 | 208001..209131: 271 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 610/590/44/147/107/273/7/91/47/35/156/0/0/0/51 | 659/625/33/85/27/229/45/103/69/15/134/0/0/3/82 | 0 | 209350..211617: 504 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 326/308/14/23/20/72/15/49/35/14/56/3/0/0/51 | 192/158/16/10/9/58/22/28/12/3/45/0/0/3/12 | 0 | 212366..213347: 241 instructions, 0 samples |
| paint_queue_EmitImageBegin | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 137/123/3/0/0/16/4/16/13/11/38/0/0/0/22 | 91/16/2/0/0/22/17/14/12/1/39/0/0/0/1 | 0 | 214362..214839: 122 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 602/520/11/90/49/179/20/57/46/38/268/0/3/0/61 | 575/417/24/43/25/189/49/61/37/8/319/0/3/10/72 | 0 | 215202..217471: 495 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 61/27/3/0/0/26/17/10/7/0/18/0/0/2/5 | 0 | 217653..217776: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 70/66/3/0/0/16/8/8/5/2/17/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 217814..218037: 52 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 218122..218328: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 218431..218537: 30 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 219156..219172: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 219378..219394: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 100/86/3/4/5/27/12/13/10/4/25/0/2/0/14 | 133/55/6/0/0/47/34/24/18/0/46/0/0/2/8 | 0 | 219574..219768: 46 instructions, 0 samples |

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
| Records/clang/wasm_tier_Records | 80890 | 162/162 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 382/112/4/4/260/290/5/24/20/4/0/4/2/0/24 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 |
| Records/cc/wasm_tier_Records | 79057 | 162/162 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 382/112/4/4/260/290/5/24/20/4/0/4/2/0/24 | 80/59/4/1/1/20/3/7/3/1/12/2/0/2/12 |
| Sieve/clang/wasm_tier_Sieve | 53246 | 163/163 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 77/42/7/2/2/24/7/13/6/5/6/1/0/0/12 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 |
| Hash/cc/wasm_tier_Hash | 48000 | 158/158 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 50/46/4/0/0/3/2/9/5/3/0/1/1/0/12 | 26/12/2/0/0/4/1/2/0/0/0/4/0/1/4 |
| Sieve/cc/wasm_tier_Sieve | 40259 | 168/168 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 77/42/7/2/2/24/7/13/6/5/6/1/0/0/12 | 46/23/5/1/2/9/3/8/3/2/0/1/0/0/9 |
| Hash/clang/wasm_tier_Hash | 22000 | 162/162 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 50/46/4/0/0/3/2/9/5/3/0/1/1/0/12 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 |
| Printing/cc/wasm_tier_Printing | 16178 | 35/99 35% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 59/44/1/5/5/12/0/5/4/7/0/1/1/0/9 | 32/15/1/0/0/4/1/1/0/1/0/2/0/0/5 |
| Button/clang/session_SessionIndex | 13961 | 28/154 18% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Printing/clang/wasm_tier_Printing | 10792 | 39/98 39% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 59/44/1/5/5/12/0/5/4/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 |
| Input/clang/session_SessionIndex | 10170 | 20/151 13% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Input/cc/session_SessionIndex | 9438 | 22/154 14% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 |
| Button/cc/session_SessionIndex | 7623 | 18/156 11% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 |
| Button/cc/theme_LightPalette | 7210 | 14/156 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang/std_utf8_Decode | 6412 | 16/159 10% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Text/clang/session_SessionIndex | 6278 | 13/159 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Input/cc/theme_LightPalette | 6260 | 12/154 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphWidth | 6256 | 18/159 11% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/3/1/5/3/0/0/0/0/2/5 |
| Text/cc/std_utf8_Decode | 5980 | 15/159 9% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 |
| Input/clang/theme_LightPalette | 5914 | 11/151 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/clang/theme_LightPalette | 5272 | 10/154 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang/theme_CurrentTheme | 4257 | 9/151 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Button/clang/theme_DefaultShadows | 3788 | 8/154 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Text/cc/text_layout_TextIdeographic | 3707 | 8/159 5% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 25/8/2/0/0/6/0/4/2/0/0/0/0/0/2 |
| Input/clang/textfield_TextFieldCore | 3444 | 12/151 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3682/3548/50/745/1032/2092/26/398/348/251/251/1/1/0/263 | 2086/536/26/384/149/876/24/235/209/148/294/1/0/13/54 |
| Text/cc/session_SessionIndex | 2908 | 7/159 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 19/0/0/0/0/4/1/2/2/0/0/0/0/1/0 |
| Text/clang/text_layout_TextIdeographic | 2859 | 6/159 3% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Input/cc/theme_CurrentTheme | 2678 | 7/154 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 |
| Input/clang/theme_DefaultMotion | 2649 | 5/151 3% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Input/cc/paint_queue_QueuePaint | 2336 | 9/154 5% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 |
| Text/clang/text_layout_TextNoBreakBefore | 2271 | 4/159 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/7/4/0/0/0/0/1/7 |
| Button/cc/widget_benchmarks_ButtonCase | 2164 | 6/156 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 359/8/2/48/142/207/5/26/24/25/48/0/0/0/2 | 157/142/10/3/9/41/1/19/9/13/8/0/0/0/36 |
| Button/cc/theme_DefaultShadows | 2120 | 5/156 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 |
| Input/cc/paint_queue___zi_spec_9308732530dd822f | 2120 | 5/154 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 |
| Text/cc/paint_queue___zi_spec_9308732530dd822f | 2053 | 5/159 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 |
| Input/cc/theme_DefaultFonts | 2029 | 5/154 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/cc/theme_LightPalette | 2021 | 4/159 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang/theme_DefaultShadows | 1931 | 4/151 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Input/clang/paint_queue_EmitPaintDirect | 1920 | 6/151 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Button/clang/paint_queue_PendingPaintAt | 1897 | 5/154 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Text/clang/paint_queue_PendingPaintAt | 1837 | 5/159 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Input/cc/widget_benchmark_host_MeasureGlyphWidth | 1794 | 5/154 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/3/1/5/3/0/0/0/0/2/5 |
| Text/clang/widget_benchmark_host_MeasureGlyphWidth | 1787 | 18/159 11% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Text/cc/paint_queue_PaintClippedLabel | 1782 | 4/159 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 |
| Input/clang/theme_DefaultFonts | 1710 | 4/151 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_TreeSubmit | 1677 | 5/151 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 |
| Input/cc/paint_queue_PendingPaintAt | 1657 | 7/154 4% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 |
| Button/clang/tree_TreeSubmit | 1644 | 5/154 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 |
| Text/cc/text_layout_TextIsSpace | 1644 | 4/159 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/std_math_Finite32 | 1617 | 3/159 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Input/clang/theme_DefaultMetrics | 1573 | 3/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Input/clang/paint_queue_QueuePaint | 1561 | 6/151 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 |
| Button/clang/button_PaintButtonForNode | 1543 | 6/154 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1313/1276/16/257/184/508/12/135/119/93/317/0/1/0/133 | 793/0/0/161/116/384/0/54/54/41/331/4/0/7/0 |
| Button/clang/theme_DefaultMetrics | 1543 | 3/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Text/clang/text_layout_TextTokenNext | 1526 | 8/159 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 300/277/12/43/22/80/6/60/48/27/0/0/0/0/45 | 209/116/20/4/2/22/7/55/35/3/0/0/0/3/34 |
| Input/cc/paint_queue_PaintRoundedFill | 1409 | 3/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 |
| Button/cc/paint_queue_PendingPaintAt | 1402 | 6/156 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 |
| Button/clang/theme_CurrentTheme | 1391 | 3/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Text/cc/std_math_SineDegrees | 1344 | 4/159 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 204/111/4/0/0/0/0/21/17/12/63/0/0/0/23 | 95/49/3/0/0/28/1/15/10/0/55/0/0/0/0 |
| Input/clang/paint_queue_PaintRoundedOutline | 1339 | 3/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Input/cc/theme_DefaultMotion | 1331 | 3/154 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 |
| Button/cc/theme_DefaultMotion | 1314 | 3/156 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 |
| Button/clang/paint_queue_PaintClippedLabel | 1291 | 3/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Input/cc/theme_DefaultShadows | 1288 | 3/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 |
| Button/clang/button_SubmitButton | 1280 | 5/154 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 621/600/18/167/120/309/20/59/41/56/51/0/0/0/60 | 376/10/1/57/34/129/0/17/16/38/33/2/0/3/1 |
| Button/clang/paint_queue_QueuePaint | 1276 | 5/154 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 |
| Button/cc/button_SubmitButton | 1257 | 7/156 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 621/600/18/167/120/309/20/59/41/56/51/0/0/0/60 | 447/414/9/119/78/217/3/21/10/36/134/0/0/1/35 |
| Text/cc/paint_queue_EmitPaintDirect | 1257 | 4/159 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 |
| Button/cc/theme_DefaultFonts | 1201 | 3/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_TreeFinish | 1138 | 3/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 |
| Input/cc/style_sheet_StyleDefaultFacts | 1051 | 2/154 1% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/3/0/0/0/0/5/0/0/0/0 |
| Button/cc/widget_benchmark_host_MeasureGlyphWidth | 1037 | 3/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 35/16/2/0/0/5/3/5/3/0/0/0/0/2/5 |
| Text/clang/theme_LightPalette | 1021 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/cc/theme_DefaultMetrics | 1015 | 2/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Text/clang/theme_DefaultMotion | 1006 | 2/159 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Button/cc/theme_DefaultMetrics | 1002 | 2/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Text/clang/theme_DefaultMetrics | 996 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Button/cc/theme_DefaultTheme | 980 | 21/156 13% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 |
| Input/clang/std_grapheme_data_Property | 970 | 2/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Text/clang/text_layout_NextTextLineUncached | 935 | 3/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 375/334/6/115/53/189/6/42/36/22/18/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 |
| Button/cc/paint_queue_PaintRoundedFill | 927 | 2/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 |
| Input/cc/text_input_TextInputScaledMetric | 924 | 2/154 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 52/38/1/0/0/0/0/7/6/5/9/0/0/0/8 | 15/0/0/0/0/2/0/3/1/0/7/0/0/0/0 |
| Input/cc/paint_queue_PaintClippedLabel | 920 | 2/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 |
| Text/clang/text_layout_TextIsSpace | 919 | 2/159 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang/paint_queue___zi_spec_9308732530dd822f | 919 | 2/151 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang/paint_queue_PaintRoundedFill | 911 | 2/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Button/cc/paint_queue_PaintClippedLabel | 908 | 2/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 51/0/0/5/16/24/0/1/1/2/6/0/0/0/0 |
| Input/cc/theme_DefaultTheme | 898 | 19/154 12% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 |
| Input/clang/paint_queue_ShapeVisible | 883 | 2/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Text/clang/paint_queue___zi_spec_9308732530dd822f | 873 | 2/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/cc/textfield_TextFieldCore | 865 | 4/154 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3682/3548/50/745/1032/2092/26/398/348/251/251/1/1/0/263 | 2455/2369/103/737/417/1258/21/253/135/152/372/2/0/16/325 |
| Button/clang/button_behavior_MeasureBounds | 846 | 3/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 190/0/0/19/10/49/0/23/23/2/120/0/0/0/0 |
| Button/clang/theme_DefaultFonts | 838 | 2/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/cc/tree_TreeStart | 837 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 |
| Input/clang/text_input_TextInputScaledMetric | 815 | 2/151 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 52/38/1/0/0/0/0/7/6/5/9/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Input/clang/text_input_TextSelectionRangeForLength | 805 | 2/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang/paint_queue_PendingPaintAt | 773 | 2/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Text/clang/std_math_SineDegrees | 770 | 2/159 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 204/111/4/0/0/0/0/21/17/12/63/0/0/0/23 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 |
| Button/cc/button_ButtonDetailSize | 766 | 2/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 41/0/0/10/1/11/0/4/4/2/7/2/0/2/0 |
| Button/cc/theme_CurrentTheme | 755 | 2/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 |
| Text/cc/paint_queue_QueuePaint | 754 | 3/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 |
| Text/clang/tree_input_PointerSample | 750 | 2/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Text/cc/tree_TreeFinish | 744 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 |
| Text/clang/paint_queue_QueuePaint | 741 | 3/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 457/88/7/22/11/148/11/79/72/28/71/3/0/8/15 |
| Button/clang/paint_queue_ResolvePaintText | 739 | 2/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Text/clang/paint_queue_ResolvePaintText | 716 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Text/cc/widget_benchmarks_TextCase | 713 | 3/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 230/154/5/19/59/102/3/25/20/16/29/0/0/0/27 | 143/127/10/15/19/63/2/17/7/8/26/0/0/1/29 |
| Input/clang/std_grapheme_Push | 693 | 2/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 |
| Text/cc/paint_queue_PendingPaintAt | 688 | 3/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 162/101/6/14/19/70/6/16/10/4/0/3/0/2/12 |
| Input/clang/tree_TreeSetTextValue | 683 | 2/151 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 138/86/3/8/4/48/13/22/19/5/0/1/0/2/6 |
| Input/cc/paint_queue_EmitPaintDirect | 649 | 2/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 |
| Text/cc/text_layout_NextTextLine | 637 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 537/475/17/65/180/310/14/50/33/27/17/12/1/0/55 | 265/228/27/12/8/98/40/43/15/3/11/2/0/16/37 |
| Text/clang/tree_TreeSubmit | 637 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 418/372/4/44/34/170/46/53/49/12/42/7/0/15/66 |
| Text/clang/tree_TreeIsBuilding | 628 | 2/159 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang/text_PaintTextLines | 628 | 4/159 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 692/610/7/184/149/360/10/49/42/45/83/0/0/0/42 | 519/378/9/116/83/259/7/34/25/23/188/0/0/10/38 |
| Text/cc/text_PaintTextLines | 618 | 5/159 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 692/610/7/184/149/360/10/49/42/45/83/0/0/0/42 | 556/471/12/192/123/338/6/35/13/15/207/0/0/6/34 |
| Text/cc/text_PrepareText | 616 | 3/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 248/82/2/54/57/153/0/13/11/15/16/0/0/0/2 | 167/85/3/36/22/65/2/7/4/14/32/0/0/3/4 |
| Input/clang/raster_StrokeLine | 607 | 1/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/tree_TreeSubmit | 605 | 4/159 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 |
| Text/clang/text___zi_spec_6a19f7c98e79a734 | 598 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/6/6/12/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextTokenNext | 591 | 6/159 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 300/277/12/43/22/80/6/60/48/27/0/0/0/0/45 | 253/234/27/2/3/43/10/61/34/6/0/0/0/4/54 |
| Input/cc/drawing_props_ColorFromPacked | 591 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/button_PaintButtonForNode | 581 | 3/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1313/1276/16/257/184/508/12/135/119/93/317/0/1/0/133 | 916/893/51/266/138/459/11/100/43/43/383/2/0/8/84 |
| Input/clang/style_sheet_StyleDefaultFacts | 536 | 1/151 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Input/clang/paint_queue_PaintClear | 528 | 1/151 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Button/clang/button_ButtonMinimumHeight | 519 | 1/154 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/theme_DefaultMotion | 519 | 1/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Text/clang/text_TextOrigin | 519 | 1/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 23/0/0/2/3/14/0/0/0/0/9/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Button/cc/style_sheet_StyleDefaultFacts | 518 | 1/156 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/3/0/0/0/0/5/0/0/0/0 |
| Button/clang/paint_queue_PaintClear | 518 | 1/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Input/cc/paint_queue_KeepPaintMesh | 516 | 2/154 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 |
| Text/clang/tree_TreeOverlayLayerAt | 514 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/font_metrics_GlyphLineHeight | 512 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_KeepPaintMesh | 510 | 2/156 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 |
| Text/cc/session_SessionFlagGet | 509 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_PaintClear | 505 | 1/156 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 |
| Text/clang/text_DefaultTextFace | 503 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 65/4/1/5/32/45/2/1/0/1/20/0/0/0/1 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 |
| Text/cc/paint_queue_PaintClear | 495 | 1/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 |
| Button/clang/button_Draw | 494 | 2/154 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Text/cc/text_DefaultTextFace | 493 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 65/4/1/5/32/45/2/1/0/1/20/0/0/0/1 | 14/0/0/0/0/7/0/0/0/1/0/0/0/0/0 |
| Button/cc/text_typeface_TextTypefaceUse | 492 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Button/clang/session_SessionFlagGet | 491 | 1/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/cc/theme_DefaultMetrics | 491 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 22/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Input/clang/text_typeface_TextTypefaceUse | 484 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang/text_input_TextSelectionRangeFor | 479 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/cc/paint_queue_ShapeVisible | 477 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 |
| Input/cc/tree_TreeSubmit | 469 | 3/154 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 |
| Input/cc/paint_queue_PaintRoundedOutline | 468 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 48/0/0/1/13/18/0/1/1/2/4/0/0/0/0 |
| Button/cc/tree_TreeInputAt | 467 | 2/156 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 |
| Input/clang/input_protocol_PointInBounds | 467 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Text/clang/tree_TreeStart | 463 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Text/cc/raster_text_PaintTextClipped | 462 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 |
| Text/clang/text_typeface_TextTypefaceUse | 460 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Button/cc/session_SessionFlagSet | 459 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Text/clang/paint_queue_PendingTextValue | 459 | 2/159 1% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Text/clang/theme_DefaultShadows | 458 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Text/clang/text_layout_TextNoBreakAfter | 452 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 |
| Button/clang/paint_queue___zi_spec_9308732530dd822f | 450 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/cc/text___zi_spec_6a19f7c98e79a734 | 449 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/6/6/12/2/1/0/1/2/0/0/0/1 | 6/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang/theme_CurrentTheme | 449 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Text/clang/style_StyleFontValue | 442 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Input/clang/tree_TreeNodeAt | 441 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/cc/paint_queue_PaintFlush | 440 | 3/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 |
| Input/clang/paint_queue_PaintClippedLabel | 438 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Button/clang/tree_TreeNodeAt | 432 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang/style_sheet_StyleControlFacts | 431 | 1/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Text/cc/theme_DefaultMotion | 429 | 1/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 19/13/1/0/0/10/0/2/1/0/8/0/0/0/1 |
| Input/clang/paint_queue_ShapeWhole | 429 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Button/cc/tree_TreeStart | 426 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 |
| Button/cc/style_StyleOpacityValue | 418 | 1/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 |
| Button/cc/paint_queue___zi_spec_9308732530dd822f | 418 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 438/409/4/24/276/330/6/20/16/7/2/4/0/1/23 | 152/128/13/10/9/52/8/25/12/3/0/4/0/3/21 |
| Text/clang/paint_queue_PaintClippedLabel | 416 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Input/clang/tree_TreeParentGeneration | 416 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Text/cc/text_layout_TextDefaultLineGap | 414 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 44/26/1/0/0/0/0/6/5/4/10/0/0/0/6 | 15/0/0/0/0/4/0/3/1/0/7/0/0/0/0 |
| Text/cc/std_math_Finite32 | 413 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 12/0/0/0/0/3/0/2/2/0/4/0/0/0/0 |
| Input/cc/std_utf8_Decode | 411 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 |
| Text/clang/theme_DefaultFonts | 406 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/clang/text_layout_TextLayoutTotalHeight | 404 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 |
| Button/clang/cursor_CursorHoverFrameCommit | 399 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Input/clang/tree_input_PointerSample | 395 | 1/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Button/clang/widget_benchmarks_ButtonCase | 394 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 359/8/2/48/142/207/5/26/24/25/48/0/0/0/2 | 141/0/0/3/5/37/0/12/12/13/8/0/0/0/0 |
| Text/cc/theme_DefaultFonts | 393 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 18/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/cc/button_ButtonMinimumHeight | 384 | 1/156 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_input_TreeTopModalLayer | 384 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeFinish | 384 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 202/159/8/6/6/116/39/20/12/3/20/7/0/2/20 |
| Text/cc/geometry_WorldToScreen | 382 | 4/159 2% | vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/0/0/8/5/22/0/0/0/2/30/0/0/0/0 | 39/0/0/9/4/13/0/0/0/2/33/0/0/0/0 |
| Input/cc/widget_benchmarks_InputCase | 379 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 394/8/2/53/124/218/8/37/35/26/35/0/0/0/2 | 164/148/15/11/14/58/2/18/3/12/8/0/0/0/42 |
| Input/cc/paint_queue_PaintMeshPointCount | 375 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Text/cc/tree_draw_BeginFrame | 370 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 |
| Text/cc/theme_CurrentTheme | 370 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 23/11/1/2/2/8/1/3/2/2/0/0/0/0/1 |
| Input/clang/tree_TreeInputAt | 364 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Button/clang/button_ButtonDetailSize | 362 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 45/0/0/3/0/8/0/4/4/2/5/2/0/1/0 |
| Text/cc/style_StyleFontValue | 361 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 |
| Button/clang/surface_Opacity | 359 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Button/clang/tree_TreeIdentityHash | 354 | 1/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang/text_behavior_ResolveTextStyle | 351 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 127/99/1/14/14/30/0/23/22/9/8/0/0/2/18 | 56/0/0/5/0/8/0/3/3/2/7/0/0/2/0 |
| Text/clang/text_style_TextStyleFor | 349 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 |
| Text/clang/tree_TreeIdentityHash | 343 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/text_input_TextSelectionRangeForLength | 343 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/0/0/0/0/0/0/0/0 |
| Button/clang/tree_TreeRootBounds | 342 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 |
| Text/cc/tree_TreeSubmittedGeometryAt | 341 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 |
| Input/clang/tree_TreeFinish | 335 | 1/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Input/cc/tree_TreeIdentityHash | 334 | 1/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/tree_TreeLayerAt | 333 | 2/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 |
| Button/clang/tree_TreeFindOld | 332 | 2/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 |
| Input/cc/tree_input_TreeHitAt | 332 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 201/158/14/35/13/71/4/27/13/13/41/2/0/1/16 |
| Input/clang/tree_TreeSetInteractive | 331 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Button/clang/tree_TreeSetInteractive | 324 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Button/clang/paint_queue_PaintOverflow | 324 | 1/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/clang/tree_input_TreeTextFocused | 322 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Text/clang/paint_queue_EmitLabel | 321 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Button/cc/paint_queue_EmitPaintDirect | 320 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 30/0/0/8/3/21/0/1/1/1/2/0/0/0/0 |
| Text/clang/tree_TreeFinish | 318 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Input/cc/style_sheet_StyleControlFacts | 317 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 22/0/0/2/0/6/0/0/0/0/18/0/0/0/0 |
| Button/clang/tree_input_TreeTextFocused | 316 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Text/cc/paint_queue_ResolvePaintText | 314 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 |
| Button/clang/paint_queue_EmitPaintDirect | 313 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Input/clang/widget_benchmark_host_MeasureGlyphWidth | 313 | 3/151 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Button/clang/button_behavior_ContentLayout | 311 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/167/5/37/26/72/0/26/21/9/91/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/87/0/0/0/0 |
| Button/cc/tree_TreeSubmit | 308 | 2/156 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 643/554/35/84/94/310/58/86/51/8/152/31/0/8/115 |
| Button/clang/style_sheet_StyleControlRoleFacts | 303 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Text/clang/paint_queue_EmitPaintDirect | 303 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Input/cc/paint_queue_PaintFlush | 303 | 2/154 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 |
| Button/cc/paint_queue_PaintFlush | 299 | 2/156 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 92/45/2/1/1/14/3/10/7/9/0/1/0/4/9 |
| Button/cc/tree_TreeBuildNewIndex | 296 | 3/156 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 |
| Text/cc/tree_TreeStoreLabel | 295 | 2/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 |
| Input/clang/tree_TreeSetCaptureTab | 294 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 |
| Button/cc/text_editing_TextEditingFrameBegin | 293 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Button/clang/paint_queue_KeepPaintMesh | 293 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 |
| Input/cc/tree_input_PointerSample | 284 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 |
| Text/clang/tree_draw_BeginFrame | 276 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 |
| Input/clang/tree_TreeBuildNewIndex | 274 | 2/151 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 |
| Button/clang/theme_ControlHeightFor | 269 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Button/clang/tree_TreeBuildNewIndex | 268 | 2/154 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 |
| Button/clang/theme_TypeLabel | 259 | 1/154 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/cc/paint_queue_EmitLabel | 257 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 |
| Button/cc/paint_queue_QueuePaint | 256 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 452/427/24/41/25/174/13/71/43/20/74/2/0/7/56 |
| Input/cc/tree_TreeSetTextValue | 255 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 173/68/6/12/9/59/8/28/22/4/0/3/0/1/8 |
| Button/cc/button_ButtonDefaultFace | 250 | 2/156 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 227/179/12/38/30/96/7/27/15/16/39/0/0/0/25 |
| Input/cc/tree_input_TreePointerUpdate | 250 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 258/199/14/30/15/124/40/31/17/10/12/18/0/1/52 |
| Text/cc/paint_queue_EmitLabel | 249 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 |
| Input/clang/tree_TreeEnsureSemantic | 249 | 1/151 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Input/clang/theme_CurrentBorderStrong | 248 | 1/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_input_TreePointerUpdate | 247 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 258/199/14/30/15/124/40/31/17/10/12/18/0/1/52 |
| Button/clang/button_ButtonDefaultFace | 246 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Text/clang/text_PrepareText | 245 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 248/82/2/54/57/153/0/13/11/15/16/0/0/0/2 | 151/22/1/19/7/52/1/6/5/14/18/0/0/3/3 |
| Button/clang/tree_TreeEnsureSemantic | 244 | 1/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Text/clang/tree_TreeEnsureSemantic | 236 | 1/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Input/cc/tree_TreeFindOld | 235 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 |
| Input/clang/textfield_TextFieldEditFor | 232 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/4/1/48/39/113/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 |
| Button/clang/tree_TreeEnsureInteraction | 230 | 1/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Button/clang/button_ButtonFaceFinish | 227 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Text/cc/tree_TreeFindOld | 227 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 |
| Text/cc/tree_TreeSetStyleClass | 226 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Text/cc/text_TextHiddenLines | 223 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 118/40/2/11/2/22/0/11/9/10/21/0/0/0/4 | 76/8/1/13/5/24/3/7/2/2/22/0/0/0/2 |
| Button/cc/surface_Opacity | 217 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Button/cc/button_Draw | 213 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 14/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Button/clang/paint_queue_EmitPaint | 212 | 2/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Text/cc/text_PaintTextLineRuns | 208 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 332/314/6/110/106/235/12/13/7/13/30/0/0/0/14 | 277/229/5/89/51/149/4/9/4/11/97/2/0/4/22 |
| Text/clang/tree_TreeLayerAt | 201 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Button/cc/style_sheet_StyleControlRoleFacts | 192 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 21/0/0/3/0/7/0/0/0/0/18/0/0/0/0 |
| Button/cc/tree_draw_BeginFrame | 188 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 |
| Input/clang/tree_TreeBuildOldIndex | 188 | 1/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 |
| Button/cc/style_sheet_ResolveInstalledStyleRules | 187 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 267/236/11/58/44/144/24/25/14/11/14/2/0/4/37 | 189/151/5/31/50/120/13/11/6/4/76/0/0/5/15 |
| Text/clang/geometry_RotatedRectangleBounds | 186 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 165/99/6/38/48/90/1/11/5/6/76/0/0/0/6 | 116/0/0/28/17/47/0/0/0/10/103/0/0/0/0 |
| Button/clang/tree_TreeBuildOldIndex | 185 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 |
| Input/cc/widget_benchmark_host_RasterText | 180 | 1/154 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 |
| Button/cc/widget_benchmark_host_RasterText | 178 | 1/156 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 |
| Text/cc/widget_benchmark_host_RasterText | 174 | 1/159 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 26/6/1/0/0/8/2/2/1/0/0/0/0/1/1 |
| Text/clang/widget_benchmarks_TextCase | 172 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 230/154/5/19/59/102/3/25/20/16/29/0/0/0/27 | 167/53/3/6/15/56/0/18/15/12/37/0/0/1/14 |
| Input/clang/paint_queue_PaintPointCount | 172 | 1/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Button/clang/paint_queue_PaintPointCount | 169 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Button/cc/tree_TreeBuildOldIndex | 167 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 |
| Text/cc/tree_TreeBuildOldIndex | 164 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 |
| Text/clang/tree_TreeFindOld | 161 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 |
| Button/cc/theme_MotionDuration | 160 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 69/52/5/10/4/30/6/9/4/2/27/0/0/0/3 |
| Button/cc/tree_TreeStoreLabel | 150 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 |
| Text/cc/tree_TreeExplicitExists | 142 | 1/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 75/18/2/2/2/16/6/5/3/2/0/4/0/2/2 |
| Button/clang/theme_DefaultTheme | 141 | 15/154 9% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterTextClipped | 132 | 2/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 |
| Text/cc/text_layout_NextTextLineUncached | 110 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 375/334/6/115/53/189/6/42/36/22/18/0/0/0/22 | 309/277/8/40/17/70/5/28/20/6/12/0/0/13/28 |
| Input/cc/paint_queue_EmitPaint | 106 | 3/154 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 |
| Text/cc/tree_TreeRead | 105 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 |
| Button/clang/widget_benchmark_host_MeasureGlyphWidth | 102 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/69/6/3/0/4/1/19/13/6/0/0/1/0/12 | 64/46/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Text/clang/paint_queue_EmitPaint | 102 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Input/clang/tree_TreeExplicitExists | 102 | 1/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Input/cc/tree_TreeBuildNewIndex | 100 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 |
| Text/cc/tree_draw_FrameInputCommit | 96 | 1/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 |
| Text/cc/text_TextFaceFor | 85 | 1/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 59/16/4/20/17/41/8/4/0/1/8/0/0/0/4 | 51/0/0/11/8/23/0/1/1/2/10/0/0/2/0 |
| Button/cc/button_ButtonFaceFacts | 84 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 46/31/1/8/1/10/0/3/2/4/0/0/0/0/3 |
| Text/clang/tree_TreeStoreLabel | 76 | 3/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 |
| Text/cc/text_DrawSession | 75 | 1/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 342/141/9/96/82/193/14/30/21/29/24/0/0/0/21 | 301/266/10/103/83/191/2/16/6/18/155/0/0/0/15 |
| Text/cc/paint_queue_EmitPaint | 68 | 2/159 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 |
| Input/cc/textfield_TextFieldFaceFor | 67 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 144/134/4/24/61/91/4/9/5/8/28/0/0/0/10 | 129/78/1/31/16/65/1/3/2/11/52/0/0/0/5 |
| Input/clang/theme_DefaultTheme | 67 | 7/151 4% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 |
| Button/cc/theme_ControlHeightFor | 62 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 37/23/3/5/2/14/1/7/4/2/8/0/0/0/3 |
| Input/cc/tree_TreeRead | 54 | 1/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 |
| Button/cc/tree_TreeRead | 53 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 490/426/23/35/94/263/22/44/21/1/90/11/0/13/69 |
| Input/clang/tree_TreeStoreLabel | 53 | 2/151 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 |
| Button/cc/paint_queue_KeepShortPaintBytes | 50 | 2/156 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Text/cc/paint_queue_KeepShortPaintBytes | 49 | 2/159 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Text/clang/text_PaintTextLineRuns | 37 | 1/159 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 332/314/6/110/106/235/12/13/7/13/30/0/0/0/14 | 312/241/14/78/50/161/9/25/11/16/93/2/0/2/18 |
| Text/clang/theme_DefaultTheme | 36 | 4/159 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 203/4/1/0/0/116/0/7/6/1/98/0/0/0/0 |
| Button/cc/paint_queue_EmitPaint | 34 | 1/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 52/0/0/8/3/29/0/3/3/2/2/0/0/0/0 |
| Button/cc/tree_TreeEnsureInteraction | 30 | 1/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 142/95/6/5/8/36/14/20/14/3/0/6/0/6/20 |
| Button/clang/tree_TreeStoreLabel | 26 | 1/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 425/271/22/19/11/129/60/68/46/4/4/2/0/15/38 |
| Calls/clang/wasm_tier_Fib | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 21/7/1/0/0/2/0/2/1/1/0/0/0/0/1 |
| Button/cc/widget_benchmarks_BeginCase | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Button/cc/tree_draw_EndFrame | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 |
| Button/cc/tree_TreeCount | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeNodeAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 |
| Button/cc/text_request_TextRequestSend | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 |
| Button/cc/text_request_TextRequestClear | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Button/cc/std_grapheme_Boundary | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 |
| Button/cc/session_SessionOpen | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 |
| Button/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/156 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_PointerSample | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 |
| Button/cc/tree_input_SecondaryPointerSample | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 |
| Button/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 |
| Button/cc/tree_input_TreeKeyboardRoute | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 |
| Button/cc/tree_TreeFrameStatus | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 |
| Button/cc/tree_TreeCurrentParent | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Button/cc/session_SessionFlagGet | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_draw_CommitFrame | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 |
| Button/cc/paint_queue_PaintSnapshotEmit | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 |
| Button/cc/tree_TreeIsBuilding | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_input_TreeHitAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 201/158/14/35/13/71/4/27/13/13/41/2/0/1/16 |
| Button/cc/tree_input_TreeTextFocused | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 46/8/1/0/0/10/2/7/6/1/0/2/0/0/2 |
| Button/cc/tree_input_TreeTextKind | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 8/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_TreeFocusAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 |
| Button/cc/std_utf8_Valid | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 |
| Button/cc/tree_TreeTextRange | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 93/18/2/4/2/23/8/12/10/2/0/4/0/2/2 |
| Button/cc/tree_input_TreeFocusedAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 |
| Button/cc/geometry_RectangleIntersection | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 |
| Button/cc/tree_TreePointerActivatable | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 |
| Button/cc/tree_input_TreeActivateAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/1/0/0/4 |
| Button/cc/cursor_CursorHoverFrameStart | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 |
| Button/cc/tree_TreePrevious | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 59/24/2/1/1/11/1/8/6/3/0/2/0/0/5 |
| Button/cc/style_ResolveFlags | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Button/cc/style_ResolveState | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Button/cc/tree_input_TreePressedAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 28/0/0/0/0/5/2/1/1/2/0/3/0/0/0 |
| Button/cc/tree_input_TreeHoveredAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/39/4/3/3/15/4/8/4/3/0/4/0/0/7 |
| Button/cc/style_ResolveFont | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Button/cc/style_StyleFontValue | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 |
| Button/cc/font_metrics_GlyphWidth | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_TreeRootBounds | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Button/cc/button_behavior_MeasureBounds | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 263/223/22/59/30/100/3/41/19/5/145/0/0/0/13 |
| Button/cc/style_StyleMinHeightValue | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/cc/button_ButtonFitContentFace | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 378/291/13/41/19/86/4/61/48/25/90/0/0/0/41 | 203/183/15/35/11/67/5/31/14/1/113/0/0/0/10 |
| Button/cc/tree_TreeSetInteractive | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Button/cc/tree_TreeSetCircleHit | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/8/0/6/6/2/0/2/0/1/0 |
| Button/cc/tree_TreeSetSelected | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 |
| Button/cc/tree_TreeSetLabel | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeSetSemanticDetail | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeSetStyleClass | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Button/cc/tree_TreeSetStyleId | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Button/cc/tree_draw_FrameInputCommit | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 |
| Button/cc/paint_queue_PaintOverflow | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/cc/tree_TreeCancel | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Button/cc/paint_queue_PaintOrder | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 |
| Button/cc/paint_queue_SnapshotTextRead | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_SnapshotTextJoin | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 |
| Button/cc/paint_queue_PaintPointCount | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 |
| Button/cc/input_protocol_PointInBounds | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 25/0/0/0/0/1/1/5/5/0/14/0/0/0/0 |
| Button/cc/tree_TreeButtonLike | 0 | 0/156 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_PointInCircleBounds | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 32/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/cc/std_utf8_Decode | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 108/83/3/10/17/43/6/15/12/0/0/0/0/1/23 |
| Button/cc/tree_TreeParentGeneration | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 |
| Button/cc/tree_TreeIdentityHash | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/tree_TreeExplicitExists | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 78/18/2/2/2/19/8/5/3/2/0/4/0/2/2 |
| Button/cc/tree_TreeFindOld | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 102/39/5/7/5/29/5/13/8/4/0/5/0/2/3 |
| Button/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 84/63/7/10/5/24/2/12/5/7/1/1/0/0/6 |
| Button/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/156 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/button_ButtonDetailFont | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 11/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Button/cc/style_StyleLineHeightValue | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/cc/style_FitHeight | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 19/0/0/0/0/0/0/1/1/0/16/0/0/0/0 |
| Button/cc/button_behavior_MeasureContent | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 158/142/5/4/9/15/0/28/23/9/41/0/0/0/14 | 74/38/3/0/0/5/2/9/6/0/45/0/0/0/0 |
| Button/cc/button_behavior_ShapeWidth | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 27/0/0/0/0/3/2/4/4/0/12/0/0/0/0 |
| Button/cc/tree_TreeEnsureSemantic | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 |
| Button/cc/button_behavior_ButtonCornerRadius | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 9/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Button/cc/drawing_props_ColorFromPacked | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/paint_queue_PaintRoundedOutline | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 48/0/0/1/13/18/0/1/1/2/4/0/0/0/0 |
| Button/cc/focus_FocusDefaultOutlineBounds | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 45/22/2/2/1/10/2/6/2/0/28/0/0/0/2 |
| Button/cc/image_lookup_IconLookupImage | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 28/12/1/1/1/6/1/3/2/2/0/0/0/0/2 |
| Button/cc/button_behavior_ButtonPictureSize | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 16/0/0/0/0/1/0/2/2/0/9/0/0/0/0 |
| Button/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 92/53/4/0/0/0/0/11/7/7/27/0/0/0/7 | 23/15/1/0/0/6/1/3/2/0/16/0/0/0/0 |
| Button/cc/button_behavior_ContentLayout | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/167/5/37/26/72/0/26/21/9/91/0/0/0/12 | 124/89/8/2/0/14/3/17/9/1/77/0/0/0/1 |
| Button/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 345/292/4/71/57/143/3/27/23/22/88/4/1/0/36 | 225/195/7/30/25/84/9/17/8/8/91/1/0/4/21 |
| Button/cc/image_lookup_ImageLookupTexture | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 58/0/0/19/15/39/0/5/5/2/28/0/0/0/0 |
| Button/cc/image_behavior_ImageDrawPlanFor | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 76/36/1/15/3/27/1/8/7/1/33/0/0/1/5 |
| Button/cc/paint_queue_PaintImage | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 33/0/0/6/5/11/0/1/1/2/4/0/0/0/0 |
| Button/cc/paint_queue_PaintLine | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 45/0/0/1/10/15/0/1/1/2/2/0/0/0/0 |
| Button/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/cc/cursor_CursorHoverFrameCommit | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Button/cc/paint_queue_PaintMeshPointCount | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Button/cc/paint_queue_ResolvePaintText | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 |
| Button/cc/tree_TreeModalLayer | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeLayerAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 |
| Button/cc/std_utf8_Continuation | 0 | 0/156 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/std_grapheme_IsControl | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/session_WidgetPartKey | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/style_sheet_StyleBucket | 0 | 0/156 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleFirstBit | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 36/0/0/0/0/5/0/0/0/0/0/0/0/5/0 |
| Button/cc/style_sheet_StyleRuleMatchesState | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleSelectorMatches | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 85/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Button/cc/style_MergeValues | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 148/0/0/59/14/82/0/15/15/0/70/0/0/0/0 |
| Button/cc/theme_StateLayer | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 55/0/0/0/0/1/1/1/1/0/0/10/0/12/0 |
| Button/cc/theme_FontForRole | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 24/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Button/cc/tree_TreeLabelSpanAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Button/cc/surface_Unit | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Button/cc/paint_queue_ClipShape | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 |
| Button/cc/drawing_props_PaintPixelScale | 0 | 0/156 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 4/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 123/104/6/4/12/41/10/10/4/3/4/1/0/1/23 |
| Button/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 74/27/2/0/0/0/0/11/9/6/19/0/0/0/5 | 35/10/1/0/0/5/0/6/3/0/16/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 125/61/5/6/4/55/20/11/6/2/5/2/0/3/10 |
| Button/cc/image_behavior_ImageFitRect | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 9/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Button/cc/paint_queue_PaintImageWithPolygon | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 105/77/2/20/34/60/0/4/2/3/26/0/0/0/8 |
| Button/cc/paint_queue_EmitLine | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 83/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Button/cc/paint_queue_EmitLabel | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 105/52/3/38/2/44/4/10/7/6/16/0/0/0/3 |
| Button/cc/paint_queue_ClipLabel | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 |
| Button/cc/paint_queue_PendingTextValue | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 |
| Button/cc/style_sheet_StyleMatchesOptional | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/paint_queue_ShapeWhole | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 |
| Button/cc/raster_shape_FillRoundedRectangle | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_FillShapeClipped | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 |
| Button/cc/paint_queue_EmitPaintNow | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 |
| Button/cc/tree_TreeCurrentPaintGeometry | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 |
| Button/cc/tree_TreeSubmittedGeometryAt | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 |
| Button/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconHash | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 19/6/1/0/0/5/2/2/1/0/0/1/0/0/1 |
| Button/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/27/2/3/4/24/5/4/2/1/5/0/0/1/8 |
| Button/cc/svg_shape_ShapeRaster | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 258/218/9/32/17/69/6/47/38/15/14/2/0/0/40 | 127/89/5/5/4/35/7/17/12/4/9/1/0/0/18 |
| Button/cc/image_behavior_ImageFitBounds | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 196/177/8/18/9/31/0/26/18/13/45/0/0/0/18 | 92/77/6/6/2/15/0/16/10/0/56/0/0/0/7 |
| Button/cc/image_behavior_ImagePolygonValid | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 99/55/1/9/1/12/1/16/15/7/12/0/0/0/4 | 35/14/1/0/0/8/2/8/7/0/12/0/0/0/0 |
| Button/cc/paint_queue_EmitImage | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Button/cc/paint_queue_ClipImage | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 17/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Button/cc/raster_StrokeLine | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/raster_text_PaintTextRotated | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 |
| Button/cc/raster_text_PaintTextItalicClipped | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 |
| Button/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 |
| Button/cc/raster_text_PaintTextClipped | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 |
| Button/cc/raster_text_PaintText | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/theme_DarkPalette | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/style_sheet_StyleControlFacts | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 22/0/0/2/0/6/0/0/0/0/18/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 |
| Button/cc/paint_queue_ShapeRadius | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 |
| Button/cc/paint_queue_FillShapeRows | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 |
| Button/cc/paint_queue_FillClippedSpan | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 |
| Button/cc/paint_queue_KeepPaintBytes | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 |
| Button/cc/paint_queue_KeepMeshPoints | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 194/165/10/21/26/76/11/29/19/5/21/4/0/1/22 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Button/cc/svg_shape_ShapeNumber | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 343/304/21/56/16/91/15/62/41/19/54/1/0/0/48 | 162/151/12/0/0/48/26/32/20/0/21/0/0/0/24 |
| Button/cc/svg_shape_ShapeBuildPath | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 610/590/44/147/107/273/7/91/47/35/156/0/0/0/51 | 447/416/39/36/21/126/22/74/35/14/90/0/0/0/87 |
| Button/cc/svg_shape_ShapeCoverEdges | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 326/308/14/23/20/72/15/49/35/14/56/3/0/0/51 | 222/201/15/15/13/79/21/37/21/2/47/0/0/1/30 |
| Button/cc/paint_queue_EmitImageBegin | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 20/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterImage | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 40/0/0/4/3/11/0/2/0/1/16/0/0/1/0 |
| Button/cc/paint_queue_EmitImageEnd | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 |
| Button/cc/text_emphasis_RunTextItalicRaster | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterTextClipped | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 |
| Button/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RecordBounds | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 |
| Button/cc/paint_queue_ShapeCornerInset | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 |
| Button/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Button/cc/paint_queue_KeepLongPaintBytes | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 |
| Button/cc/svg_shape_ShapeIsDigit | 0 | 0/156 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeIsSpace | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 8/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddPoint | 0 | 0/156 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 15/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddSpan | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 137/123/3/0/0/16/4/16/13/11/38/0/0/0/22 | 110/13/2/0/0/21/11/13/7/0/42/0/0/2/2 |
| Button/cc/paint_queue_ShapeSquareRoot | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Button/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 |
| Button/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 |
| Button/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 602/520/11/90/49/179/20/57/46/38/268/0/3/0/61 | 367/340/21/19/10/94/24/47/25/7/185/0/3/0/38 |
| Button/cc/svg_shape_ShapeFillSubpath | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 6/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeCollapse | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 70/66/3/0/0/16/8/8/5/2/17/0/0/0/15 | 52/34/3/0/0/19/7/7/4/0/14/0/0/0/4 |
| Button/cc/svg_shape_ShapeAddCircle | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 53/35/3/0/0/13/5/8/5/0/19/0/0/1/10 |
| Button/cc/svg_shape_ShapeSqrt | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 21/6/1/0/0/4/0/2/1/0/12/0/0/0/1 |
| Button/cc/svg_shape_ShapeQuad | 0 | 0/156 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 11/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeAbs | 0 | 0/156 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 10/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeTriangle | 0 | 0/156 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 9/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddEdge | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 20/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Button/cc/svg_shape_ShapeAddPolygon | 0 | 0/156 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates | 100/86/3/4/5/27/12/13/10/4/25/0/2/0/14 | 95/68/5/0/0/27/19/15/10/0/23/0/2/2/13 |
| Button/clang/widget_benchmarks_BeginCase | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Button/clang/tree_draw_BeginFrame | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 |
| Button/clang/tree_draw_EndFrame | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Button/clang/tree_TreeCount | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang/tree_input_TreePointerUpdate | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 210/110/2/8/1/65/22/28/26/7/18/6/0/4/23 |
| Button/clang/widget_benchmark_host_WidgetTextCount | 0 | 0/154 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang/widget_benchmark_host_WidgetPaintCount | 0 | 0/154 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/154 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang/text_request_TextRequestSend | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 |
| Button/clang/text_request_TextRequestClear | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Button/clang/widget_benchmark_host_WidgetPaintReset | 0 | 0/154 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/clang/tree_input_PointerSample | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Button/clang/tree_input_SecondaryPointerSample | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Button/clang/session_SessionFlagSet | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/paint_queue_PaintSnapshotInvalidate | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Button/clang/tree_input_TreeKeyboardRoute | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 373/232/17/41/14/130/43/58/41/21/26/5/0/4/33 |
| Button/clang/text_editing_TextEditingFrameBegin | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Button/clang/tree_TreeStart | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Button/clang/tree_TreeFrameStatus | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/clang/tree_TreeCurrentParent | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Button/clang/tree_draw_CommitFrame | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Button/clang/paint_queue_PaintSnapshotEmit | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 |
| Button/clang/tree_TreeRead | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Button/clang/tree_TreeIsBuilding | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang/tree_input_TreeHitAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Button/clang/tree_input_TreeTextKind | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang/tree_input_TreeFocusAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Button/clang/tree_TreeInputAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Button/clang/tree_input_TreeInputAllowedAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Button/clang/tree_TreeTextRange | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 |
| Button/clang/tree_input_TreeFocusedAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Button/clang/geometry_RectangleIntersection | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Button/clang/tree_TreePointerActivatable | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Button/clang/tree_input_TreeActivateAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Button/clang/cursor_CursorHoverFrameStart | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Button/clang/tree_TreePrevious | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Button/clang/style_ResolveFlags | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Button/clang/tree_input_TreeTakeActivationAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang/style_ResolveState | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Button/clang/tree_input_TreeHoveredAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Button/clang/style_ResolveFont | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Button/clang/style_StyleFontValue | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Button/clang/font_metrics_GlyphWidth | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/font_metrics_GlyphLineHeight | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/style_StyleMinHeightValue | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang/button_ButtonFitContentFace | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 378/291/13/41/19/86/4/61/48/25/90/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/1 |
| Button/clang/tree_TreeSetCircleHit | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Button/clang/tree_TreeSetSelected | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang/tree_TreeSetLabel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/clang/tree_TreeSetSemanticDetail | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/clang/tree_TreeSetStyleClass | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang/tree_TreeSetStyleId | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang/tree_draw_FrameInputCommit | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Button/clang/tree_TreeCancel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Button/clang/tree_TreeFinish | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Button/clang/paint_queue_PaintOrder | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Button/clang/paint_queue_SnapshotTextRead | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Button/clang/paint_queue_SnapshotTextJoin | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Button/clang/input_protocol_PointInBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Button/clang/tree_input_TreeTopModalLayer | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/clang/tree_TreeButtonLike | 0 | 0/154 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/tree_input_PointInCircleBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/clang/tree_TreeOverlayLayerAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/std_utf8_Decode | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Button/clang/std_grapheme_Push | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 |
| Button/clang/tree_TreeParentGeneration | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Button/clang/tree_TreeExplicitExists | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Button/clang/tree_input_TreeTakePointerActivationAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 81/12/1/6/0/13/3/12/11/6/5/1/0/2/3 |
| Button/clang/style_sheet_ResolveInstalledStyleRules | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 267/236/11/58/44/144/24/25/14/11/14/2/0/4/37 | 253/196/17/22/19/107/14/33/16/4/0/0/0/12/36 |
| Button/clang/button_ButtonFaceFacts | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Button/clang/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/154 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/button_ButtonDetailFont | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Button/clang/style_StyleLineHeightValue | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang/style_FitHeight | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Button/clang/button_behavior_MeasureContent | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 158/142/5/4/9/15/0/28/23/9/41/0/0/0/14 | 44/0/0/0/0/1/0/8/8/0/29/0/0/0/0 |
| Button/clang/button_behavior_ShapeWidth | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Button/clang/style_StyleOpacityValue | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Button/clang/button_behavior_ButtonCornerRadius | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Button/clang/drawing_props_ColorFromPacked | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/paint_queue_PaintRoundedFill | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Button/clang/paint_queue_PaintRoundedOutline | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Button/clang/focus_FocusDefaultOutlineBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 27/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Button/clang/image_lookup_IconLookupImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Button/clang/button_behavior_ButtonPictureSize | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Button/clang/button_behavior_ButtonLabeledPictureSize | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 92/53/4/0/0/0/0/11/7/7/27/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Button/clang/svg_icon_behavior_PaintSvgIcon | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 345/292/4/71/57/143/3/27/23/22/88/4/1/0/36 | 246/35/1/23/20/69/6/17/16/6/128/1/0/1/3 |
| Button/clang/image_lookup_ImageLookupTexture | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Button/clang/image_behavior_ImageDrawPlanFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Button/clang/paint_queue_PaintLine | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Button/clang/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/clang/paint_queue_PaintMeshPointCount | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Button/clang/tree_TreeModalLayer | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang/tree_TreeLayerAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Button/clang/std_utf8_Continuation | 0 | 0/154 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/std_grapheme_data_Property | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Button/clang/std_grapheme_IsControl | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/session_WidgetPartKey | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang/widget_key_WidgetRepeatedPart | 0 | 0/154 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/style_sheet_StyleBucket | 0 | 0/154 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/style_sheet_StyleFirstBit | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Button/clang/style_sheet_StyleRuleMatchesState | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/style_sheet_StyleSelectorMatches | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Button/clang/style_MergeValues | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Button/clang/theme_StateLayer | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Button/clang/card_behavior_CardDefaultFace | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 73/0/0/6/0/29/1/9/8/3/12/0/0/0/0 |
| Button/clang/theme_ShapeMedium | 0 | 0/154 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang/theme_FontForRole | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Button/clang/theme_MotionDuration | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Button/clang/tree_TreeLabelSpanAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Button/clang/surface_Unit | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Button/clang/paint_queue_ClipShape | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Button/clang/paint_queue_EmitRoundedOutline | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Button/clang/drawing_props_PaintPixelScale | 0 | 0/154 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/clang/svg_icon_behavior_SvgIconSlot | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 164/41/4/6/7/61/19/20/16/3/10/5/0/0/4 |
| Button/clang/svg_icon_behavior_SvgIconSnap | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 74/27/2/0/0/0/0/11/9/6/19/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Button/clang/svg_icon_behavior_SvgIconTexture | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 192/156/12/7/3/67/28/24/12/1/9/2/0/10/39 |
| Button/clang/paint_queue_PaintImageWithPolygon | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Button/clang/paint_queue_EmitLine | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Button/clang/paint_queue_EmitLabel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Button/clang/paint_queue_ClipLabel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Button/clang/paint_queue_PendingTextValue | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Button/clang/style_sheet_StyleMatchesOptional | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/theme_ShapeSurface | 0 | 0/154 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang/paint_queue_ShapeVisible | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Button/clang/paint_queue_ShapeWhole | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Button/clang/raster_shape_FillRoundedRectangle | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/paint_queue_FillShapeClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 |
| Button/clang/paint_queue_EmitPaintNow | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 |
| Button/clang/tree_TreeCurrentPaintGeometry | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Button/clang/tree_TreeSubmittedGeometryAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Button/clang/raster_shape_StrokeRoundedRectangle | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/svg_icon_behavior_SvgIconRetire | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Button/clang/svg_shape_ShapeRaster | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 258/218/9/32/17/69/6/47/38/15/14/2/0/0/40 | 129/113/4/3/3/27/9/17/13/4/8/1/0/0/21 |
| Button/clang/image_behavior_ImageFitBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 196/177/8/18/9/31/0/26/18/13/45/0/0/0/18 | 43/0/0/0/0/3/0/9/9/0/30/0/0/0/0 |
| Button/clang/image_behavior_ImagePolygonValid | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 99/55/1/9/1/12/1/16/15/7/12/0/0/0/4 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Button/clang/paint_queue_EmitImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Button/clang/paint_queue_ClipImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Button/clang/raster_StrokeLine | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/text_typeface_TextTypefaceUse | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang/raster_text_PaintTextRotated | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang/raster_text_PaintTextItalicClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Button/clang/raster_text_PaintTextEmphasisClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Button/clang/raster_text_PaintTextClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang/raster_text_PaintText | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang/theme_DarkPalette | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/clang/style_sheet_StyleControlFacts | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Button/clang/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Button/clang/paint_queue_ShapeRadius | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Button/clang/paint_queue_FillClippedSpan | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Button/clang/paint_queue_KeepPaintBytes | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Button/clang/paint_queue_KeepMeshPoints | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Button/clang/svg_shape_ShapeNumber | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 343/304/21/56/16/91/15/62/41/19/54/1/0/0/48 | 179/142/8/0/0/31/14/38/30/0/33/0/0/0/25 |
| Button/clang/svg_shape_ShapeCoverEdges | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 326/308/14/23/20/72/15/49/35/14/56/3/0/0/51 | 192/158/16/10/9/58/22/28/12/3/45/0/0/3/12 |
| Button/clang/paint_queue_EmitImageBegin | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Button/clang/widget_benchmark_host_RasterImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Button/clang/paint_queue_EmitImageEnd | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Button/clang/text_emphasis_RunTextItalicRaster | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang/text_emphasis_RunTextEmphasisRaster | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Button/clang/style_sheet_StyleDefaultFacts | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Button/clang/widget_benchmark_host_RecordBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Button/clang/svg_shape_ShapeIsDigit | 0 | 0/154 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang/svg_shape_ShapeIsSpace | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang/svg_shape_ShapeAddPoint | 0 | 0/154 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Button/clang/svg_shape_ShapeAddSpan | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 137/123/3/0/0/16/4/16/13/11/38/0/0/0/22 | 91/16/2/0/0/22/17/14/12/1/39/0/0/0/1 |
| Button/clang/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang/svg_shape_ShapeStrokeSubpath | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 602/520/11/90/49/179/20/57/46/38/268/0/3/0/61 | 575/417/24/43/25/189/49/61/37/8/319/0/3/10/72 |
| Button/clang/svg_shape_ShapeCollapse | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/66/3/0/0/16/8/8/5/2/17/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Button/clang/svg_shape_ShapeAddCircle | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Button/clang/svg_shape_ShapeQuad | 0 | 0/154 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Button/clang/svg_shape_ShapeAbs | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Button/clang/svg_shape_ShapeTriangle | 0 | 0/154 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Button/clang/svg_shape_ShapeAddEdge | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Text/cc/widget_benchmarks_BeginCase | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Text/cc/tree_draw_EndFrame | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 |
| Text/cc/tree_TreeCount | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeNodeAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 |
| Text/cc/text_request_TextRequestSend | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 |
| Text/cc/text_request_TextRequestClear | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Text/cc/std_grapheme_Boundary | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 |
| Text/cc/session_SessionOpen | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 |
| Text/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/159 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/cc/tree_input_PointerSample | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 32/19/1/2/5/12/2/2/1/2/1/0/0/0/2 |
| Text/cc/tree_input_SecondaryPointerSample | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 |
| Text/cc/session_SessionFlagSet | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Text/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 |
| Text/cc/tree_input_TreeKeyboardRoute | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 |
| Text/cc/text_editing_TextEditingFrameBegin | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Text/cc/tree_TreeFrameStatus | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 |
| Text/cc/tree_draw_CommitFrame | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 |
| Text/cc/paint_queue_PaintSnapshotEmit | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 |
| Text/cc/std_utf8_Valid | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 |
| Text/cc/tree_TreeTextRange | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 92/18/2/4/2/22/7/12/10/2/0/4/0/2/2 |
| Text/cc/tree_input_TreeFocusAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 |
| Text/cc/tree_input_TreeFocusedAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 |
| Text/cc/tree_TreeInputAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 |
| Text/cc/geometry_RectangleIntersection | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 |
| Text/cc/tree_TreePointerActivatable | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 |
| Text/cc/tree_input_TreeActivateAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/0/0/1/4 |
| Text/cc/cursor_CursorHoverFrameStart | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 |
| Text/cc/tree_TreeRootBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Text/cc/tree_TreeIsBuilding | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/geometry_RotatedRectangleBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 165/99/6/38/48/90/1/11/5/6/76/0/0/0/6 | 88/29/1/9/14/27/0/2/1/3/59/0/0/0/4 |
| Text/cc/text_TextOrigin | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 23/0/0/2/3/14/0/0/0/0/9/0/0/0/0 | 5/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Text/cc/tree_TreeSetLabel | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Text/cc/tree_TreeSetStyleId | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Text/cc/tree_TreeSetSemantic | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 68/0/0/2/2/13/1/8/8/3/0/1/0/1/0 |
| Text/cc/tree_TreeSetHiddenLines | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 |
| Text/cc/paint_queue_PaintOverflow | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/cc/tree_TreeCancel | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Text/cc/paint_queue_PaintOrder | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 |
| Text/cc/paint_queue_SnapshotTextRead | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_SnapshotTextJoin | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 |
| Text/cc/paint_queue_PaintPointCount | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 |
| Text/cc/tree_input_TreeTopModalLayer | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_TreeParentGeneration | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 |
| Text/cc/tree_TreeIdentityHash | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 16/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/tree_TreeBuildNewIndex | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 192/161/18/3/3/49/14/34/16/6/0/6/0/3/25 |
| Text/cc/text_behavior_ResolveTextStyle | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 127/99/1/14/14/30/0/23/22/9/8/0/0/2/18 | 78/56/6/8/3/19/2/14/6/1/11/0/0/0/3 |
| Text/cc/style_ResolveFont | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Text/cc/drawing_props_ColorFromPacked | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/surface_Opacity | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Text/cc/style_StyleOpacityValue | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 |
| Text/cc/font_metrics_GlyphLineHeight | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_behavior_TextWrapPolicy | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 21/0/0/0/0/0/0/1/1/1/2/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/text_behavior_TextExtent | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 17/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/cc/text_layout_TextLayoutTotalHeight | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 14/0/0/0/0/0/0/2/2/0/0/2/0/0/0 |
| Text/cc/tree_TreeCurrentParent | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Text/cc/tree_TreeEnsureSemantic | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 |
| Text/cc/text_behavior_TextAlignmentOffset | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 16/0/0/0/0/2/1/2/2/0/7/0/0/0/0 |
| Text/cc/text_layout_TextCharactersEnd | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 27/8/1/0/0/1/1/4/3/1/0/0/0/0/1 |
| Text/cc/text_TextSpan | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/cc/font_metrics_GlyphWidth | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_TextEllipsisStart | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/84/5/23/14/42/1/10/5/4/0/0/0/0/11 | 60/41/4/2/2/11/5/9/5/1/0/0/0/0/6 |
| Text/cc/text_layout_TextLineXFor | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 16/0/0/0/0/1/1/1/1/0/0/0/0/2/0 |
| Text/cc/text_behavior_TextStrikethroughBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 38/16/1/3/5/10/0/3/2/1/11/0/0/0/1 | 25/0/0/0/0/2/0/0/0/0/10/0/0/2/0 |
| Text/cc/text_PaintTextRule | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/4/1/69/73/148/2/2/1/10/46/0/0/0/1 | 134/0/0/28/29/58/0/4/2/6/68/0/0/3/0 |
| Text/cc/cursor_CursorHoverFrameCommit | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Text/cc/paint_queue_PaintMeshPointCount | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 16/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Text/cc/tree_TreeModalLayer | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeLayerAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 81/30/2/0/0/16/2/15/13/3/0/4/0/0/5 |
| Text/cc/std_utf8_Continuation | 0 | 0/159 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_grapheme_IsControl | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/session_WidgetPartKey | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/text_style_TextStyleFor | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 32/12/1/6/4/16/1/3/2/2/8/0/0/0/1 |
| Text/cc/surface_Unit | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/159 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextLineHash | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 16/6/1/0/0/2/0/2/1/0/0/1/0/0/1 |
| Text/cc/text_layout_TextLineCacheMatches | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 24/10/4/1/0/10/1/7/3/0/1/0/0/0/0 |
| Text/cc/std_math_CosineDegrees | 0 | 0/159 0% |  | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/1/0/0/0/0 |
| Text/cc/tree_TreeLabelSpanAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Text/cc/text_TextLinkRunAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 119/32/2/37/18/67/1/12/10/4/0/0/0/0/3 | 84/58/2/4/0/10/6/10/8/0/0/0/0/3/7 |
| Text/cc/text_TextLinkColor | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 22/0/0/0/0/1/0/0/0/3/0/2/0/4/0 |
| Text/cc/paint_queue_ClipLabel | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 |
| Text/cc/text_TextRangeValid | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 34/10/2/0/0/3/2/3/1/0/0/0/0/2/3 |
| Text/cc/paint_queue_PaintClippedEmphasis | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 177/4/1/18/84/150/2/2/1/2/25/0/0/0/1 | 52/0/0/5/17/25/0/1/1/2/6/0/0/0/0 |
| Text/cc/paint_queue_PaintRoundedFill | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 47/0/0/1/12/17/0/1/1/2/3/0/0/0/0 |
| Text/cc/shape_paint_PaintTriangleShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 141/4/1/21/93/117/2/1/0/4/32/0/0/0/1 | 43/0/0/0/7/10/0/0/0/1/17/0/0/0/0 |
| Text/cc/paint_queue_PendingTextValue | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 |
| Text/cc/drawing_props_StyleTokenColorOr | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_typeface_TextTypefaceUse | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Text/cc/raster_text_PaintTextRotated | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 |
| Text/cc/raster_text_PaintTextItalicClipped | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 |
| Text/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 |
| Text/cc/raster_text_PaintText | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_EmitPaintNow | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 |
| Text/cc/tree_TreeCurrentPaintGeometry | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 |
| Text/cc/paint_queue_KeepPaintMesh | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 56/40/2/2/3/18/3/8/6/1/4/1/0/1/8 |
| Text/cc/paint_queue_ClipShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 |
| Text/cc/shape_paint_ShapeOffset | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 19/0/0/6/3/10/0/0/0/0/9/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_SubmitShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 123/8/2/10/85/97/4/2/0/1/26/0/0/0/2 | 31/0/0/0/4/11/0/0/0/1/0/0/0/0/0 |
| Text/cc/theme_DefaultTheme | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 191/150/2/31/73/146/0/7/5/3/106/0/0/0/25 |
| Text/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 |
| Text/cc/text_emphasis_RunTextItalicRaster | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/paint_queue_KeepPaintBytes | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 |
| Text/cc/paint_queue_KeepMeshPoints | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 191/162/10/21/26/74/8/29/19/5/21/4/0/1/22 |
| Text/cc/paint_queue_ShapeVisible | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 |
| Text/cc/paint_queue_ShapeWhole | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 |
| Text/cc/raster_shape_FillRoundedRectangle | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_FillShapeClipped | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 |
| Text/cc/shape_paint_EmitShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 104/16/4/17/12/33/8/18/14/7/14/0/0/0/4 | 28/0/0/4/0/8/0/11/7/0/3/0/0/0/0 |
| Text/cc/paint_queue_SubmitPaint | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 12/0/0/4/0/5/0/0/0/1/0/0/0/0/0 |
| Text/cc/theme_DarkPalette | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/theme_DefaultShadows | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/31/1/0/0/35/0/2/1/0/27/0/0/0/6 |
| Text/cc/text_layout_TextNoBreakBefore | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 115/105/18/0/0/13/2/28/10/0/0/0/0/0/37 |
| Text/cc/text_layout_TextNoBreakAfter | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 37/19/2/0/0/4/1/7/5/0/0/0/0/0/6 |
| Text/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Text/cc/paint_queue_KeepLongPaintBytes | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 |
| Text/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 |
| Text/cc/paint_queue_ShapeRadius | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 |
| Text/cc/paint_queue_FillShapeRows | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 |
| Text/cc/paint_queue_FillClippedSpan | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 |
| Text/cc/shape_paint_EmitLineShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 110/47/2/20/1/45/7/7/5/6/44/0/0/2/5 |
| Text/cc/shape_paint_EmitCircleShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 215/79/4/50/37/89/6/18/14/17/53/0/0/0/10 | 185/132/10/28/16/70/2/21/11/8/60/0/0/3/15 |
| Text/cc/shape_paint_EmitRingShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 198/59/3/35/24/63/4/26/23/18/52/0/0/0/11 | 111/84/9/20/6/42/1/14/5/8/45/0/0/0/9 |
| Text/cc/shape_paint_EmitTriangleShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 38/12/1/7/0/19/1/3/2/2/9/0/0/0/3 |
| Text/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 |
| Text/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 |
| Text/cc/widget_benchmark_host_RecordBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 |
| Text/cc/paint_queue_ShapeCornerInset | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 |
| Text/cc/shape_paint_ShapeAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 19/0/0/2/3/10/0/0/0/0/7/0/0/0/0 | 6/0/0/1/0/1/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_LineCornersFor | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 164/0/0/32/24/73/0/9/9/7/71/0/0/0/0 | 89/0/0/11/9/25/0/5/5/3/65/0/0/0/0 |
| Text/cc/shape_paint_ShapeBatchBegin | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 8/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/cc/shape_paint_ShapeHidden | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/11/2/0/0/6/0/4/2/0/15/0/0/0/1 |
| Text/cc/shape_paint_PointsBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/5/20/10/38/1/11/6/5/36/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchTriangle | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 349/43/5/70/57/178/14/33/28/28/72/0/0/0/7 | 234/186/9/42/39/91/4/18/8/13/120/0/0/2/11 |
| Text/cc/shape_paint_ShapeBatchFlush | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 20/0/0/0/0/7/1/2/2/1/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcDirections | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 65/26/1/10/10/28/3/4/3/3/22/0/0/0/2 | 51/17/1/3/4/14/3/3/2/2/20/0/0/0/1 |
| Text/cc/shape_paint_ShapeFadeBands | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 142/127/10/15/3/18/0/24/14/19/6/1/2/0/23 | 74/14/1/1/1/4/1/2/1/2/3/2/0/9/3 |
| Text/cc/shape_paint_ShapeBatchSector | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/29/1/9/8/19/0/2/1/3/5/0/0/0/2 | 36/15/1/3/3/11/2/2/1/1/17/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchBand | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/53/1/17/14/35/0/2/1/6/9/0/0/0/3 | 59/33/1/7/5/17/2/2/1/2/35/0/0/0/1 |
| Text/cc/paint_queue_ShapeSquareRoot | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Text/cc/std_math_Sqrt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 161/101/4/7/3/11/0/20/16/10/47/0/2/0/16 | 102/80/5/0/0/26/4/17/11/0/57/0/0/1/9 |
| Text/cc/shape_paint_ShapeBatchAdd | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 103/67/1/11/6/41/5/9/8/6/10/0/0/0/12 | 84/39/3/8/8/32/6/8/5/2/44/0/0/0/6 |
| Text/cc/raster_triangles_FillTriangles | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeChannelChange | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_math_CeilInteger | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/2/0/0/7/0/0/0/0 |
| Text/cc/paint_queue_GradientChannel | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 62/0/0/0/0/0/0/6/6/5/16/0/0/0/0 | 34/0/0/0/0/7/3/4/3/0/17/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcPoint | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/6/4/14/1/0/0/0/11/0/0/0/0 | 12/0/0/0/0/2/1/0/0/0/8/0/0/0/0 |
| Text/cc/std_math_NotANumber | 0 | 0/159 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/std_math_Finite64 | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 12/0/0/0/0/3/0/2/2/0/4/0/0/0/0 |
| Text/cc/std_math_ScaleByPowerOfTwo | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/96/7/9/11/20/0/19/12/6/34/0/0/0/8 | 75/48/7/0/0/15/5/17/10/0/21/0/0/0/6 |
| Text/cc/shape_paint_ShapeInside | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 23/0/0/0/0/3/2/3/3/0/5/0/0/0/0 |
| Text/cc/shape_paint_ShapeCrossing | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 98/0/0/22/9/33/0/10/10/2/37/0/0/0/0 | 44/22/1/0/0/3/1/6/5/0/31/0/0/0/1 |
| Text/cc/widget_benchmark_host_RasterTriangles | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/1/0/6/0/0/1/2 |
| Text/clang/widget_benchmarks_BeginCase | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Text/clang/tree_draw_EndFrame | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Text/clang/tree_TreeCount | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang/tree_TreeNodeAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/clang/widget_benchmark_host_WidgetTextCount | 0 | 0/159 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/159 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang/text_request_TextRequestSend | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 |
| Text/clang/text_request_TextRequestClear | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Text/clang/widget_benchmark_host_WidgetPaintReset | 0 | 0/159 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/clang/tree_input_SecondaryPointerSample | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Text/clang/session_SessionFlagSet | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/paint_queue_PaintSnapshotInvalidate | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Text/clang/tree_input_TreeKeyboardRoute | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 374/232/17/41/14/130/42/58/41/21/26/4/0/5/33 |
| Text/clang/text_editing_TextEditingFrameBegin | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Text/clang/paint_queue_PaintClear | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Text/clang/tree_TreeFrameStatus | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Text/clang/text_DrawSession | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 342/141/9/96/82/193/14/30/21/29/24/0/0/0/21 | 300/20/2/37/35/102/1/25/23/20/115/1/0/2/2 |
| Text/clang/session_SessionFlagGet | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang/tree_draw_CommitFrame | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Text/clang/paint_queue_PaintSnapshotEmit | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 |
| Text/clang/tree_TreeRead | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Text/clang/tree_input_TreeInputAllowedAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Text/clang/tree_TreeTextRange | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 |
| Text/clang/tree_input_TreeFocusAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Text/clang/tree_input_TreeFocusedAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Text/clang/tree_TreeInputAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Text/clang/geometry_RectangleIntersection | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Text/clang/tree_TreePointerActivatable | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Text/clang/tree_input_TreeActivateAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 |
| Text/clang/cursor_CursorHoverFrameStart | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Text/clang/tree_TreeBuildOldIndex | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 183/117/6/3/3/48/17/29/23/5/0/5/0/5/13 |
| Text/clang/tree_TreeRootBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 |
| Text/clang/tree_TreeSetLabel | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Text/clang/tree_TreeSetStyleClass | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang/tree_TreeSetStyleId | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang/tree_TreeSetSemantic | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Text/clang/text_TextHiddenLines | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/40/2/11/2/22/0/11/9/10/21/0/0/0/4 | 116/20/2/2/1/18/1/11/9/3/55/1/0/0/2 |
| Text/clang/tree_TreeSetHiddenLines | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang/text_selection_host_TextSelectionPresent | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 |
| Text/clang/tree_draw_FrameInputCommit | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Text/clang/paint_queue_PaintOverflow | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/clang/tree_TreeCancel | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Text/clang/paint_queue_PaintOrder | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Text/clang/paint_queue_SnapshotTextRead | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Text/clang/paint_queue_SnapshotTextJoin | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Text/clang/paint_queue_PaintPointCount | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Text/clang/tree_input_TreeTopModalLayer | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang/std_grapheme_Push | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 189/157/8/0/0/12/0/23/15/1/0/0/0/7/50 |
| Text/clang/tree_TreeParentGeneration | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Text/clang/tree_TreeBuildNewIndex | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 227/202/5/32/19/102/14/21/16/8/0/8/0/1/34 | 180/155/9/2/2/46/18/33/24/5/0/4/0/5/25 |
| Text/clang/tree_TreeExplicitExists | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Text/clang/text_TextFaceFor | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 59/16/4/20/17/41/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 |
| Text/clang/style_ResolveFont | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Text/clang/theme_CurrentText | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang/drawing_props_ColorFromPacked | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/surface_Opacity | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Text/clang/style_StyleOpacityValue | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Text/clang/font_metrics_GlyphLineHeight | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/text_layout_TextDefaultLineGap | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 44/26/1/0/0/0/0/6/5/4/10/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Text/clang/text_behavior_TextWrapPolicy | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 21/0/0/0/0/0/0/1/1/1/2/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang/text_layout_NextTextLine | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 537/475/17/65/180/310/14/50/33/27/17/12/1/0/55 | 348/109/17/13/6/102/41/57/40/4/17/10/0/13/13 |
| Text/clang/text_behavior_TextExtent | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 17/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/clang/tree_TreeCurrentParent | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Text/clang/geometry_WorldToScreen | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 46/0/0/8/5/22/0/0/0/2/30/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 |
| Text/clang/text_behavior_TextAlignmentOffset | 0 | 0/159 0% | calls/inlining candidates | 31/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 |
| Text/clang/text_layout_TextCharactersEnd | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 |
| Text/clang/text_TextSpan | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/clang/font_metrics_GlyphWidth | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/text_TextEllipsisStart | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/84/5/23/14/42/1/10/5/4/0/0/0/0/11 | 53/21/3/2/2/10/3/7/4/1/0/0/0/0/2 |
| Text/clang/text_layout_TextLineXFor | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 |
| Text/clang/text_behavior_TextStrikethroughBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 38/16/1/3/5/10/0/3/2/1/11/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 |
| Text/clang/text_PaintTextRule | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/4/1/69/73/148/2/2/1/10/46/0/0/0/1 | 102/0/0/26/21/52/0/4/2/6/53/0/0/0/0 |
| Text/clang/cursor_CursorHoverFrameCommit | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Text/clang/paint_queue_PaintMeshPointCount | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Text/clang/tree_TreeModalLayer | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang/std_utf8_Continuation | 0 | 0/159 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/std_grapheme_data_Property | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Text/clang/std_grapheme_IsControl | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/session_WidgetPartKey | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang/widget_key_WidgetRepeatedPart | 0 | 0/159 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/surface_Unit | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Text/clang/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/159 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/font_metrics_GlyphMetricsRevision | 0 | 0/159 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang/text_layout_TextLineCacheMatches | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 |
| Text/clang/tree_TreeLabelSpanAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Text/clang/text_TextLinkRunAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 119/32/2/37/18/67/1/12/10/4/0/0/0/0/3 | 51/23/7/2/0/9/4/14/7/0/0/0/0/0/2 |
| Text/clang/text_TextLinkColor | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 |
| Text/clang/paint_queue_ClipLabel | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Text/clang/text_TextRangeValid | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 25/0/0/0/0/2/2/3/3/0/0/0/0/1/0 |
| Text/clang/paint_queue_PaintClippedEmphasis | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 177/4/1/18/84/150/2/2/1/2/25/0/0/0/1 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 |
| Text/clang/paint_queue_PaintRoundedFill | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Text/clang/shape_paint_PaintTriangleShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 141/4/1/21/93/117/2/1/0/4/32/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 |
| Text/clang/drawing_props_StyleTokenColorOr | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/theme_CurrentAccent | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang/raster_text_PaintTextRotated | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang/raster_text_PaintTextItalicClipped | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Text/clang/raster_text_PaintTextEmphasisClipped | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Text/clang/raster_text_PaintTextClipped | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang/raster_text_PaintText | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/paint_queue_EmitPaintNow | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 |
| Text/clang/tree_TreeCurrentPaintGeometry | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Text/clang/tree_TreeSubmittedGeometryAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Text/clang/paint_queue_KeepPaintMesh | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 |
| Text/clang/paint_queue_ClipShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Text/clang/shape_paint_ShapeOffset | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 19/0/0/6/3/10/0/0/0/0/9/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang/shape_paint_SubmitShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 123/8/2/10/85/97/4/2/0/1/26/0/0/0/2 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 |
| Text/clang/text_emphasis_RunTextItalicRaster | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang/text_emphasis_RunTextEmphasisRaster | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Text/clang/paint_queue_KeepPaintBytes | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Text/clang/paint_queue_KeepMeshPoints | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Text/clang/paint_queue_ShapeVisible | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Text/clang/paint_queue_ShapeWhole | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Text/clang/raster_shape_FillRoundedRectangle | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/paint_queue_FillShapeClipped | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 |
| Text/clang/shape_paint_EmitShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 104/16/4/17/12/33/8/18/14/7/14/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 |
| Text/clang/paint_queue_SubmitPaint | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 |
| Text/clang/theme_DarkPalette | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Text/clang/paint_queue_ShapeRadius | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Text/clang/paint_queue_FillClippedSpan | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Text/clang/shape_paint_EmitLineShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 |
| Text/clang/shape_paint_EmitRingShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 198/59/3/35/24/63/4/26/23/18/52/0/0/0/11 | 174/55/3/27/13/76/9/19/16/9/97/0/0/0/0 |
| Text/clang/shape_paint_EmitTriangleShape | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 |
| Text/clang/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang/widget_benchmark_host_RecordBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Text/clang/shape_paint_ShapeAt | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 19/0/0/2/3/10/0/0/0/0/7/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 |
| Text/clang/shape_paint_LineCornersFor | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 164/0/0/32/24/73/0/9/9/7/71/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 |
| Text/clang/shape_paint_ShapeBatchBegin | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/clang/shape_paint_ShapeHidden | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 |
| Text/clang/shape_paint_PointsBounds | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/99/5/20/10/38/1/11/6/5/36/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/5 |
| Text/clang/shape_paint_ShapeBatchFlush | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 |
| Text/clang/shape_paint_ShapeArcDirections | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 65/26/1/10/10/28/3/4/3/3/22/0/0/0/2 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 |
| Text/clang/shape_paint_ShapeFadeBands | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 142/127/10/15/3/18/0/24/14/19/6/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 |
| Text/clang/shape_paint_ShapeBatchSector | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/29/1/9/8/19/0/2/1/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 |
| Text/clang/shape_paint_ShapeBatchBand | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/53/1/17/14/35/0/2/1/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 |
| Text/clang/shape_paint_ShapeBatchAdd | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 103/67/1/11/6/41/5/9/8/6/10/0/0/0/12 | 62/0/0/6/4/21/3/6/6/2/28/0/0/0/0 |
| Text/clang/shape_paint_ShapeClipSide | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 150/69/3/35/34/86/10/13/10/6/26/0/0/0/7 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 |
| Text/clang/raster_triangles_FillTriangles | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang/shape_paint_ShapeChannelChange | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang/std_math_CeilInteger | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 |
| Text/clang/paint_queue_GradientChannel | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 62/0/0/0/0/0/0/6/6/5/16/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 |
| Text/clang/shape_paint_ShapeArcPoint | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/6/4/14/1/0/0/0/11/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 |
| Text/clang/std_math_NotANumber | 0 | 0/159 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 |
| Text/clang/std_math_Finite64 | 0 | 0/159 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang/std_math_ScaleByPowerOfTwo | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/96/7/9/11/20/0/19/12/6/34/0/0/0/8 | 86/33/5/0/0/14/3/22/17/0/34/0/0/1/4 |
| Text/clang/shape_paint_ShapeTurn | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 |
| Text/clang/shape_paint_ShapeInside | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 |
| Text/clang/shape_paint_ShapeCrossing | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 98/0/0/22/9/33/0/10/10/2/37/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 |
| Text/clang/widget_benchmark_host_RasterTriangles | 0 | 0/159 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 |
| Input/cc/widget_benchmarks_BeginCase | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 20/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Input/cc/tree_draw_BeginFrame | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 199/147/6/18/8/83/22/27/20/11/16/2/0/2/30 |
| Input/cc/textfield_Draw | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 204/140/7/49/18/83/3/19/12/10/16/0/0/3/19 |
| Input/cc/tree_draw_EndFrame | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 54/31/3/1/1/4/1/5/2/6/0/0/0/1/2 |
| Input/cc/tree_TreeNodeAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 |
| Input/cc/text_request_TextRequestSend | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 229/196/22/31/8/71/13/34/12/15/4/6/0/0/31 |
| Input/cc/text_request_TextRequestClear | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 19/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Input/cc/std_grapheme_Boundary | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/1/12/4/16/0/6/5/5/0/0/0/0/4 | 42/8/1/0/0/2/1/4/3/1/0/0/0/0/0 |
| Input/cc/session_SessionOpen | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/4 |
| Input/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/154 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_SecondaryPointerSample | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 33/21/1/2/4/11/1/2/1/2/0/0/0/1/3 |
| Input/cc/session_SessionFlagSet | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 15/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Input/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 45/0/0/0/0/17/7/2/2/2/0/0/0/0/0 |
| Input/cc/tree_input_TreeKeyboardRoute | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 410/368/36/65/50/176/19/53/17/17/30/6/0/2/55 |
| Input/cc/text_editing_TextEditingFrameBegin | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 13/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_PaintClear | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 22/0/0/0/0/13/8/0/0/1/0/0/0/1/0 |
| Input/cc/tree_TreeStart | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 93/66/1/1/1/30/0/3/2/4/4/1/0/0/27 |
| Input/cc/tree_TreeFrameStatus | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 22/0/0/0/0/3/2/1/1/1/0/1/0/0/0 |
| Input/cc/textfield_TextFieldKey | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuTakeCommand | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 147/106/4/23/10/47/0/27/23/14/0/0/0/0/18 | 76/59/7/9/7/27/2/14/7/4/0/0/0/0/7 |
| Input/cc/text_context_menu_TextMenuPasteText | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 4/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuOpenRequested | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 64/21/1/6/5/13/2/4/3/4/11/0/0/1/1 |
| Input/cc/system_clipboard_SystemClipboardSet | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_platform_TextInputNoteFocused | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/0/0/15/15/49/4/3/3/6/0/4/0/0/0 | 54/21/3/1/1/21/8/5/2/3/0/0/0/0/3 |
| Input/cc/text_context_menu_TextMenuIsOpenFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 23/5/1/0/0/5/0/4/3/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuShow | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 18/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/cc/session_SessionFlagGet | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/tree_draw_CommitFrame | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 122/107/3/2/3/30/11/9/6/13/0/0/0/4/16 |
| Input/cc/paint_queue_PaintSnapshotEmit | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 250/233/8/35/47/123/9/24/16/13/5/3/0/2/33 |
| Input/cc/tree_TreeIsBuilding | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/tree_input_TreeTextFocused | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/22/1/2/2/25/7/9/8/6/0/5/0/0/5 | 46/8/1/0/0/10/2/7/6/1/0/2/0/0/2 |
| Input/cc/tree_input_TreeTextKind | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 8/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeFocusAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 70/48/6/8/4/17/2/11/5/6/0/0/0/0/4 |
| Input/cc/tree_TreeInputAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 89/64/2/11/12/40/1/9/7/2/19/2/0/4/7 |
| Input/cc/tree_TreeCount | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/std_utf8_Valid | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 92/87/7/17/5/25/2/18/11/7/0/0/0/0/13 | 67/48/5/0/2/9/3/12/7/1/0/0/0/1/7 |
| Input/cc/tree_TreeTextRange | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 93/18/2/4/2/23/8/12/10/2/0/4/0/2/2 |
| Input/cc/std_grapheme_Next | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 82/72/2/19/18/39/0/6/4/5/0/0/0/0/3 | 6/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/tree_input_TreeFocusedAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 63/26/2/3/3/13/1/7/5/3/0/2/0/1/3 |
| Input/cc/geometry_RectangleIntersection | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 32/0/0/0/0/0/0/0/0/0/30/0/0/0/0 |
| Input/cc/tree_TreePointerActivatable | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 30/0/0/0/0/1/0/5/5/0/0/0/0/1/0 |
| Input/cc/tree_input_TreeActivateAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 45/30/2/6/1/11/3/4/2/4/0/1/0/0/4 |
| Input/cc/cursor_CursorHoverFrameStart | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/4/2/2/1/2/0/0/2/0 |
| Input/cc/tree_TreeBuildOldIndex | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 256/212/6/38/23/123/19/22/16/8/2/12/0/1/43 | 189/150/11/6/6/52/12/32/21/4/0/8/0/3/25 |
| Input/cc/text_input_TextSelectionRangeFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/0/0/0/0/0/0/0/0 |
| Input/cc/system_clipboard_SystemClipboardText | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 161/139/12/22/8/37/3/31/19/14/0/0/0/0/25 | 73/45/8/3/2/12/5/14/6/2/0/0/0/0/6 |
| Input/cc/input_protocol_PointInBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 25/0/0/0/0/1/1/5/5/0/14/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuOpenAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 85/34/1/14/9/44/0/8/7/7/2/0/0/0/2 | 70/12/2/5/1/19/2/8/6/3/2/0/0/0/2 |
| Input/cc/tree_TreePrevious | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 59/24/2/1/1/11/1/8/6/3/0/2/0/0/5 |
| Input/cc/tree_TreeCurrentParent | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 23/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Input/cc/tree_input_TreeTakeDragAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 172/147/13/11/5/53/2/28/15/7/21/7/0/0/20 |
| Input/cc/tree_TreePreviousWidgetState | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 46/0/0/0/5/15/2/4/4/1/2/2/0/0/0 |
| Input/cc/tree_input_TreeHasFocus | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 96/68/2/11/8/35/6/10/8/8/0/3/0/0/14 | 71/44/3/5/4/20/6/9/6/3/0/1/0/0/7 |
| Input/cc/tree_input_TreeBlurAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 35/7/1/0/0/8/3/5/4/1/0/2/0/0/2 |
| Input/cc/style_ResolveFont | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Input/cc/style_StyleFontValue | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/4/2/0/7/0/0/0/0 |
| Input/cc/textfield_TextFieldSecureDisplay | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/34/2/16/5/27/1/7/5/3/0/0/0/0/5 | 49/20/2/0/0/4/1/5/3/2/0/0/0/0/3 |
| Input/cc/font_metrics_GlyphWidth | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldScrollFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 53/0/0/0/0/5/2/4/0/0/7/0/0/2/0 |
| Input/cc/textfield_TextFieldCursorAtX | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 68/39/2/7/7/16/1/3/1/3/0/0/0/0/1 |
| Input/cc/text_input_TextPointerOffset | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 69/0/0/0/0/0/0/7/7/7/18/0/0/0/0 | 36/0/0/0/0/6/1/4/1/0/15/0/0/0/0 |
| Input/cc/textfield_TextFieldCodepoints | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 35/27/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/7/1/0/0/5/1/2/1/1/0/0/0/0/3 |
| Input/cc/text_TextSpan | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldByteAfter | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 40/21/2/0/0/2/2/5/3/1/0/0/0/0/2 |
| Input/cc/text_input_CompositionTransitionFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 269/97/2/84/42/171/0/18/16/11/0/0/0/0/3 | 158/125/11/16/29/60/8/28/17/0/23/0/0/0/13 |
| Input/cc/textfield_TextFieldNext | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextSelectionAfterMove | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 16/11/1/2/3/6/1/2/1/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldInputTextAllowed | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 22/8/1/0/0/5/3/5/4/0/0/0/0/0/3 |
| Input/cc/textfield_TextFieldReplacementAllowed | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 135/96/8/14/3/35/9/21/13/4/0/0/0/1/9 |
| Input/cc/text_input_TextEditCommandDecisionFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/93/9/5/17/29/3/20/11/0/0/0/0/5/14 |
| Input/cc/textfield_TextFieldEditFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/4/1/48/39/113/2/18/17/7/2/0/0/0/1 | 109/77/9/16/14/37/1/14/5/5/12/0/0/0/6 |
| Input/cc/text_request_TextRequestTake | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 190/0/0/43/24/110/10/16/16/11/0/9/0/0/0 | 85/0/0/5/4/27/3/9/9/5/0/1/0/0/0 |
| Input/cc/text_request_TextRequestChangeFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/246/12/47/26/98/5/38/26/13/0/0/0/0/24 | 170/141/13/13/5/56/13/24/11/6/3/0/0/0/10 |
| Input/cc/font_metrics_GlyphCaret | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 35/0/0/0/0/5/1/2/2/1/0/0/0/0/0 |
| Input/cc/text_input_TextFieldRevealScroll | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 26/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/cc/style_StyleOpacityValue | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 17/0/0/0/0/2/0/2/2/0/10/0/0/0/0 |
| Input/cc/tree_TreeSetInteractive | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Input/cc/tree_TreeSetCaptureTab | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 52/38/5/1/1/8/0/8/3/4/0/1/0/1/5 |
| Input/cc/tree_input_TreeFocusSubmittedAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 225/4/1/39/26/96/10/28/27/14/8/7/0/0/1 | 161/143/18/12/20/70/4/24/6/6/25/5/0/2/17 |
| Input/cc/tree_TreeSetWidgetState | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 153/105/7/16/5/51/17/26/19/3/8/5/0/1/15 |
| Input/cc/tree_TreeSetSemantic | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 68/0/0/2/2/13/1/8/8/3/0/1/0/1/0 |
| Input/cc/tree_TreeSetLabel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Input/cc/tree_TreeSetStyleClass | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Input/cc/tree_TreeSetStyleId | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 48/19/1/1/1/10/2/7/6/3/0/1/0/1/2 |
| Input/cc/tree_TreeSetSelected | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/1/1/9/1/6/6/3/0/1/0/1/0 |
| Input/cc/surface_Opacity | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 37/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Input/cc/font_metrics_GlyphLineHeight | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldPaintFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 173/0/0/21/19/52/1/13/13/12/46/0/1/0/0 | 108/0/0/2/0/13/2/8/0/0/37/0/0/3/0 |
| Input/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineY | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 7/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineEndX | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/paint_queue_PaintLine | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 45/0/0/1/10/15/0/1/1/2/2/0/0/0/0 |
| Input/cc/tree_TreeSubmittedNodeAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 17/0/0/1/1/2/0/1/1/2/0/0/0/0/0 |
| Input/cc/button_Draw | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 14/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/cc/system_clipboard_host_SystemClipboardWriteHost | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 21/5/1/0/0/7/3/3/2/0/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputFocusFrameBegin | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 86/0/0/16/15/53/6/3/3/5/0/6/0/0/0 | 52/17/2/1/1/16/6/4/2/3/0/0/0/0/3 |
| Input/cc/text_input_platform_TextInputFocused | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/25/4/0/0/7/2/5/1/3/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputOwnerMatches | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 12/6/1/0/0/1/0/3/1/0/0/0/0/0/0 |
| Input/cc/tree_TreeDefer | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 51/6/1/1/2/12/7/3/2/1/2/0/0/2/4 |
| Input/cc/text_context_menu_TextMenuDraw | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 400/350/2/41/98/270/21/16/14/13/26/16/0/0/82 | 162/147/6/15/38/92/2/14/8/5/21/0/0/2/33 |
| Input/cc/tree_draw_FrameInputCommit | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 33/0/0/1/2/17/5/0/0/1/0/0/0/2/0 |
| Input/cc/paint_queue_PaintOverflow | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/cc/tree_TreeCancel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Input/cc/paint_queue_PaintOrder | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 175/156/12/5/5/63/22/32/20/2/4/2/0/4/26 |
| Input/cc/paint_queue_SnapshotTextRead | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 9/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_SnapshotTextJoin | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 248/229/22/5/5/60/19/51/29/3/0/0/0/8/29 |
| Input/cc/paint_queue_PaintPointCount | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 19/0/0/3/0/5/1/3/3/0/0/1/0/0/0 |
| Input/cc/tree_input_TreeTopModalLayer | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeButtonLike | 0 | 0/154 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointInCircleBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 32/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/cc/tree_TreeParentGeneration | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 42/10/1/1/1/9/5/4/3/1/0/3/0/0/2 |
| Input/cc/tree_TreeExplicitExists | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 77/18/2/2/2/18/8/5/3/2/0/4/0/2/2 |
| Input/cc/system_clipboard_host_SystemClipboardByteHost | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 12/0/0/0/0/3/1/2/2/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 84/63/7/10/5/24/2/12/5/7/1/1/0/0/6 |
| Input/cc/style_sheet_StyleControlRoleFacts | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 21/0/0/3/0/7/0/0/0/0/18/0/0/0/0 |
| Input/cc/textfield_TextFieldDefaultFace | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 71/4/1/4/39/46/2/1/0/6/25/0/0/0/1 | 65/0/0/9/12/33/0/0/0/6/24/0/0/0/0 |
| Input/cc/style_sheet_ResolveInstalledStyleRules | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 267/236/11/58/44/144/24/25/14/11/14/2/0/4/37 | 189/151/5/31/50/120/13/11/6/4/76/0/0/5/15 |
| Input/cc/text_input_TextInputContentWidth | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 18/0/0/0/0/2/0/2/0/0/4/0/0/0/0 |
| Input/cc/text_input_TextUtf8Continuation | 0 | 0/154 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_utf8_Next | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/text_input_TextCursorForLength | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 10/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_CompositionBoundaryFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 26/15/2/0/0/2/2/6/4/0/0/0/0/0/3 |
| Input/cc/std_grapheme_Previous | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 59/44/2/10/4/14/0/7/5/5/0/0/0/0/4 | 33/9/1/0/0/1/1/4/3/1/0/0/0/0/0 |
| Input/cc/tree_TreeEnsureInteraction | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 142/95/6/5/8/36/14/20/14/3/0/6/0/6/20 |
| Input/cc/tree_TreeEnsureSemantic | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 153/106/6/5/5/46/25/20/14/3/4/6/0/3/25 |
| Input/cc/tree_TreeStoreLabel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 443/418/15/79/27/174/25/50/35/21/0/16/1/2/71 | 339/302/25/41/34/139/39/54/29/4/14/9/0/6/39 |
| Input/cc/paint_queue_ClipShape | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/20/1/8/6/17/0/2/1/1/8/0/0/0/2 |
| Input/cc/surface_Unit | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 15/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Input/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/154 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldCursorHeightFor | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 27/0/0/0/0/2/0/2/0/0/4/0/0/0/0 |
| Input/cc/paint_queue_ClipLabel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/25/2/10/8/21/1/5/3/1/12/0/0/0/3 |
| Input/cc/paint_queue_EmitLine | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 83/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Input/cc/button_SubmitButton | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 621/600/18/167/120/309/20/59/41/56/51/0/0/0/60 | 447/414/9/119/78/217/3/21/10/36/134/0/0/1/35 |
| Input/cc/system_clipboard_SystemClipboardHasText | 0 | 0/154 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/contextmenu_Draw | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 966/931/24/230/238/556/28/81/57/58/87/16/0/0/95 | 542/515/31/88/88/248/32/67/36/19/103/9/0/4/70 |
| Input/cc/cursor_CursorHoverFrameCommit | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 20/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Input/cc/paint_queue_ResolvePaintText | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 36/14/1/1/1/21/1/3/2/2/0/0/0/0/2 |
| Input/cc/tree_TreeModalLayer | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 9/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/std_utf8_Continuation | 0 | 0/154 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_grapheme_IsControl | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/session_WidgetPartKey | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/style_sheet_StyleBucket | 0 | 0/154 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleFirstBit | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 36/0/0/0/0/5/0/0/0/0/0/0/0/5/0 |
| Input/cc/style_sheet_StyleRuleMatchesState | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleSelectorMatches | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 85/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Input/cc/style_MergeValues | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 148/0/0/59/14/82/0/15/15/0/70/0/0/0/0 |
| Input/cc/tree_TreeLabelSpanAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 31/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Input/cc/paint_queue_ShapeVisible | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 23/15/1/7/0/8/1/4/2/1/11/0/0/0/2 |
| Input/cc/paint_queue_ShapeWhole | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/4/0/5/1/6/6/0/6/0/0/0/0 |
| Input/cc/raster_shape_FillRoundedRectangle | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_FillShapeClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 144/36/1/33/28/62/0/4/2/4/123/0/0/0/1 |
| Input/cc/paint_queue_EmitPaintNow | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 142/121/8/3/7/22/6/26/16/5/0/0/0/0/13 |
| Input/cc/tree_TreeCurrentPaintGeometry | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 69/26/3/1/1/21/4/8/5/2/11/4/0/0/3 |
| Input/cc/tree_TreeSubmittedGeometryAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 54/0/0/8/8/25/0/5/5/2/16/2/0/0/0 |
| Input/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_typeface_TextTypefaceUse | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 13/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Input/cc/raster_text_PaintTextRotated | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 23/0/0/3/3/6/0/3/3/0/7/0/0/0/0 |
| Input/cc/raster_text_PaintTextItalicClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 47/0/0/10/9/21/0/6/5/1/11/0/0/0/0 |
| Input/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 49/17/1/8/8/19/1/6/5/1/11/1/0/2/5 |
| Input/cc/raster_text_PaintTextClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 13/0/0/0/0/1/1/3/3/0/7/0/0/0/0 |
| Input/cc/raster_text_PaintText | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/raster_StrokeLine | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/style_ResolveFlags | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Input/cc/style_ResolveState | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Input/cc/tree_input_TreePressedAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 28/0/0/0/0/5/2/1/1/2/0/3/0/0/0 |
| Input/cc/tree_input_TreeHoveredAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/39/4/3/3/15/4/8/4/3/0/4/0/0/7 |
| Input/cc/button_ButtonDetailSize | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 41/0/0/10/1/11/0/4/4/2/7/2/0/2/0 |
| Input/cc/tree_TreeRootBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 39/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Input/cc/button_behavior_MeasureBounds | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 263/223/22/59/30/100/3/41/19/5/145/0/0/0/13 |
| Input/cc/style_StyleMinHeightValue | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/button_ButtonMinimumHeight | 0 | 0/154 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/button_ButtonFitContentFace | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 378/291/13/41/19/86/4/61/48/25/90/0/0/0/41 | 203/183/15/35/11/67/5/31/14/1/113/0/0/0/10 |
| Input/cc/tree_TreeSetCircleHit | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/8/0/6/6/2/0/2/0/1/0 |
| Input/cc/tree_TreeSetSemanticDetail | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 64/0/0/2/2/13/1/9/9/4/0/1/0/1/0 |
| Input/cc/button_PaintButtonForNode | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1313/1276/16/257/184/508/12/135/119/93/317/0/1/0/133 | 916/893/51/266/138/459/11/100/43/43/383/2/0/8/84 |
| Input/cc/tree_TreeNextKey | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/6/1/1/1/12/4/5/4/2/0/7/0/2/1 |
| Input/cc/contextmenu_ContextMenuPanelKey | 0 | 0/154 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePreviousCommitted | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 42/8/1/2/2/9/0/4/3/2/0/2/0/0/2 |
| Input/cc/contextmenu_ContextMenuRowKey | 0 | 0/154 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/4/3/0/0/0/0/0/0/1/0 |
| Input/cc/contextmenu_ContextMenuOpens | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 23/7/2/0/0/6/1/6/4/0/0/1/0/0/2 |
| Input/cc/contextmenu_ContextMenuFind | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/1/5/3/0/0/0/0/0/3 |
| Input/cc/contextmenu_ContextMenuBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuSubmenuBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/43/1/32/22/60/1/3/2/6/37/1/0/0/1 | 83/0/0/10/8/19/1/2/1/4/46/1/0/0/0 |
| Input/cc/overlay_Draw | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 388/311/1/115/78/209/0/28/27/28/19/0/0/0/28 | 259/229/10/44/27/80/4/24/14/20/33/0/0/5/32 |
| Input/cc/contextmenu_ContextMenuLevel | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 1416/1199/10/370/341/768/10/72/62/142/309/1/0/0/114 | 1062/861/26/302/118/483/14/69/31/103/299/1/0/3/119 |
| Input/cc/paint_queue_PendingTextValue | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 64/0/0/0/0/16/6/8/8/1/0/3/0/4/0 |
| Input/cc/style_sheet_StyleMatchesOptional | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 29/0/0/0/0/7/0/4/0/0/14/0/0/1/0 |
| Input/cc/paint_queue_ShapeRadius | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 15/0/0/0/0/1/0/0/0/0/13/0/0/0/0 |
| Input/cc/paint_queue_FillShapeRows | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 256/240/9/54/29/83/0/23/14/12/135/0/0/0/11 | 251/192/7/32/21/71/8/22/15/3/165/0/0/6/9 |
| Input/cc/paint_queue_FillClippedSpan | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 32/0/0/0/0/1/1/3/3/0/24/0/0/0/0 |
| Input/cc/paint_queue_KeepPaintBytes | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 20/0/0/0/0/5/2/6/3/0/0/0/0/1/0 |
| Input/cc/paint_queue_KeepMeshPoints | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 194/165/10/21/26/76/11/29/19/5/21/4/0/1/22 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Input/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 38/0/0/6/3/13/0/2/0/1/14/0/0/0/0 |
| Input/cc/text_emphasis_RunTextItalicRaster | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterTextClipped | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 34/6/1/0/0/9/0/5/2/0/7/0/0/1/1 |
| Input/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 20/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Input/cc/button_ButtonDefaultFace | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 227/179/12/38/30/96/7/27/15/16/39/0/0/0/25 |
| Input/cc/button_ButtonFaceFacts | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 46/31/1/8/1/10/0/3/2/4/0/0/0/0/3 |
| Input/cc/button_ButtonDetailFont | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 11/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Input/cc/style_StyleLineHeightValue | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 11/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/style_FitHeight | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 19/0/0/0/0/0/0/1/1/0/16/0/0/0/0 |
| Input/cc/button_behavior_MeasureContent | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 158/142/5/4/9/15/0/28/23/9/41/0/0/0/14 | 74/38/3/0/0/5/2/9/6/0/45/0/0/0/0 |
| Input/cc/button_behavior_ShapeWidth | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 27/0/0/0/0/3/2/4/4/0/12/0/0/0/0 |
| Input/cc/theme_ControlHeightFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 37/23/3/5/2/14/1/7/4/2/8/0/0/0/3 |
| Input/cc/button_behavior_ButtonCornerRadius | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 9/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Input/cc/focus_FocusDefaultOutlineBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 45/22/2/2/1/10/2/6/2/0/28/0/0/0/2 |
| Input/cc/image_lookup_IconLookupImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 28/12/1/1/1/6/1/3/2/2/0/0/0/0/2 |
| Input/cc/button_behavior_ButtonPictureSize | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 16/0/0/0/0/1/0/2/2/0/9/0/0/0/0 |
| Input/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 92/53/4/0/0/0/0/11/7/7/27/0/0/0/7 | 23/15/1/0/0/6/1/3/2/0/16/0/0/0/0 |
| Input/cc/button_behavior_ContentLayout | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/167/5/37/26/72/0/26/21/9/91/0/0/0/12 | 124/89/8/2/0/14/3/17/9/1/77/0/0/0/1 |
| Input/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 345/292/4/71/57/143/3/27/23/22/88/4/1/0/36 | 225/195/7/30/25/84/9/17/8/8/91/1/0/4/21 |
| Input/cc/image_lookup_ImageLookupTexture | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 58/0/0/19/15/39/0/5/5/2/28/0/0/0/0 |
| Input/cc/image_behavior_ImageDrawPlanFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 76/36/1/15/3/27/1/8/7/1/33/0/0/1/5 |
| Input/cc/paint_queue_PaintImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 33/0/0/6/5/11/0/1/1/2/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 456/420/20/93/62/191/12/52/32/33/57/11/0/0/41 | 257/196/11/37/28/99/5/30/19/19/56/2/0/1/32 |
| Input/cc/dpi_ScalePixels | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 83/72/1/0/0/0/0/10/9/8/21/0/0/0/14 | 43/0/0/0/0/7/0/4/1/0/22/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuRowTop | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 101/83/5/20/9/37/4/13/8/9/25/4/0/0/12 | 86/54/7/3/7/23/5/14/7/2/24/1/0/0/11 |
| Input/cc/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 20/14/1/0/0/1/1/2/1/0/0/0/0/2/4 |
| Input/cc/tree_input_TreeTakeDismissalAt | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/25/2/4/3/14/3/7/5/5/0/0/0/0/4 |
| Input/cc/tree_TreeSetPopup | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 36/0/0/0/0/7/0/5/5/2/0/2/0/0/0 |
| Input/cc/drawing_props_StyleTokenColorOr | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePushScope | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 81/63/5/0/0/12/10/9/4/8/0/8/0/0/12 |
| Input/cc/contextmenu_ContextMenuTextLeft | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/2/2/4/0/1/1/2/2/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelChecks | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 28/13/4/0/0/8/4/7/3/0/0/0/0/0/4 |
| Input/cc/contextmenu_ContextMenuInlineWidth | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 35/0/0/2/0/4/0/2/2/4/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuInlineEnds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 23/6/1/0/0/6/1/3/2/0/0/2/0/0/0 |
| Input/cc/svg_icon_behavior_PaintStroke | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 639/592/8/74/327/438/14/31/23/21/75/0/0/0/35 | 191/96/5/19/43/82/13/12/7/5/77/0/0/1/19 |
| Input/cc/tree_End | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 4/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RecordBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 16/0/0/0/0/4/0/2/0/0/10/0/0/0/0 |
| Input/cc/paint_queue_ShapeCornerInset | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 35/6/1/0/0/3/1/5/4/0/24/0/0/0/1 |
| Input/cc/paint_queue_KeepShortPaintBytes | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 76/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 73/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Input/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/21/1/16/3/36/6/4/3/5/0/4/0/0/4 | 74/12/1/4/3/20/5/8/7/4/0/1/0/1/1 |
| Input/cc/paint_queue_KeepLongPaintBytes | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 70/21/1/15/2/34/6/4/3/5/0/4/0/0/4 | 68/12/1/4/2/19/5/8/7/4/0/1/0/1/1 |
| Input/cc/theme_DarkPalette | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 23/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/cc/theme_StateLayer | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 55/0/0/0/0/1/1/1/1/0/0/10/0/12/0 |
| Input/cc/theme_FontForRole | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 24/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Input/cc/theme_MotionDuration | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 69/52/5/10/4/30/6/9/4/2/27/0/0/0/3 |
| Input/cc/drawing_props_PaintPixelScale | 0 | 0/154 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 4/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 123/104/6/4/12/41/10/10/4/3/4/1/0/1/23 |
| Input/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 74/27/2/0/0/0/0/11/9/6/19/0/0/0/5 | 35/10/1/0/0/5/0/6/3/0/16/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 125/61/5/6/4/55/20/11/6/2/5/2/0/3/10 |
| Input/cc/image_behavior_ImageFitRect | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 9/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Input/cc/paint_queue_PaintImageWithPolygon | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 105/77/2/20/34/60/0/4/2/3/26/0/0/0/8 |
| Input/cc/svg_icon_behavior_StrokeAppendByte | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 7/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/cc/svg_icon_behavior_StrokeAppendNumber | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 230/196/7/10/16/40/7/37/30/13/9/0/6/0/35 | 121/97/6/2/7/36/13/14/6/1/6/4/0/6/25 |
| Input/cc/tree_TreePopScope | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 54/26/1/3/2/7/6/4/3/4/0/4/0/0/5 |
| Input/cc/paint_queue_ShapeSquareRoot | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Input/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 154/128/13/10/10/52/7/25/12/3/0/4/0/3/21 |
| Input/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 158/132/14/14/15/55/6/26/12/6/0/4/0/2/19 |
| Input/cc/svg_icon_behavior_SvgIconHash | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 19/6/1/0/0/5/2/2/1/0/0/1/0/0/1 |
| Input/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/27/2/3/4/24/5/4/2/1/5/0/0/1/8 |
| Input/cc/svg_shape_ShapeRaster | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 258/218/9/32/17/69/6/47/38/15/14/2/0/0/40 | 127/89/5/5/4/35/7/17/12/4/9/1/0/0/18 |
| Input/cc/image_behavior_ImageFitBounds | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 196/177/8/18/9/31/0/26/18/13/45/0/0/0/18 | 92/77/6/6/2/15/0/16/10/0/56/0/0/0/7 |
| Input/cc/image_behavior_ImagePolygonValid | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 99/55/1/9/1/12/1/16/15/7/12/0/0/0/4 | 35/14/1/0/0/8/2/8/7/0/12/0/0/0/0 |
| Input/cc/paint_queue_EmitImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Input/cc/paint_queue_ClipImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 17/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeNumber | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 343/304/21/56/16/91/15/62/41/19/54/1/0/0/48 | 162/151/12/0/0/48/26/32/20/0/21/0/0/0/24 |
| Input/cc/svg_shape_ShapeBuildPath | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 610/590/44/147/107/273/7/91/47/35/156/0/0/0/51 | 447/416/39/36/21/126/22/74/35/14/90/0/0/0/87 |
| Input/cc/svg_shape_ShapeCoverEdges | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 326/308/14/23/20/72/15/49/35/14/56/3/0/0/51 | 222/201/15/15/13/79/21/37/21/2/47/0/0/1/30 |
| Input/cc/paint_queue_EmitImageBegin | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 20/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterImage | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 40/0/0/4/3/11/0/2/0/1/16/0/0/1/0 |
| Input/cc/paint_queue_EmitImageEnd | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsDigit | 0 | 0/154 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsSpace | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 8/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddPoint | 0 | 0/154 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 15/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddSpan | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 137/123/3/0/0/16/4/16/13/11/38/0/0/0/22 | 110/13/2/0/0/21/11/13/7/0/42/0/0/2/2 |
| Input/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 602/520/11/90/49/179/20/57/46/38/268/0/3/0/61 | 367/340/21/19/10/94/24/47/25/7/185/0/3/0/38 |
| Input/cc/svg_shape_ShapeFillSubpath | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 6/0/0/0/0/2/1/2/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeCollapse | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 70/66/3/0/0/16/8/8/5/2/17/0/0/0/15 | 52/34/3/0/0/19/7/7/4/0/14/0/0/0/4 |
| Input/cc/svg_shape_ShapeAddCircle | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 53/35/3/0/0/13/5/8/5/0/19/0/0/1/10 |
| Input/cc/svg_shape_ShapeSqrt | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 21/6/1/0/0/4/0/2/1/0/12/0/0/0/1 |
| Input/cc/svg_shape_ShapeQuad | 0 | 0/154 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 11/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeAbs | 0 | 0/154 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 10/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeTriangle | 0 | 0/154 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 9/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddEdge | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 20/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Input/cc/svg_shape_ShapeAddPolygon | 0 | 0/154 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates | 100/86/3/4/5/27/12/13/10/4/25/0/2/0/14 | 95/68/5/0/0/27/19/15/10/0/23/0/2/2/13 |
| Input/clang/widget_benchmarks_InputCase | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 394/8/2/53/124/218/8/37/35/26/35/0/0/0/2 | 157/94/13/11/24/69/0/19/6/11/23/0/0/0/24 |
| Input/clang/widget_benchmarks_BeginCase | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Input/clang/tree_draw_BeginFrame | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 158/0/0/6/4/44/15/17/16/10/16/0/0/2/0 |
| Input/clang/textfield_Draw | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 121/0/0/12/6/37/2/9/9/9/2/0/0/0/0 |
| Input/clang/tree_draw_EndFrame | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Input/clang/tree_input_TreePointerUpdate | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 210/110/2/8/1/65/22/28/26/7/18/6/0/4/23 |
| Input/clang/widget_benchmark_host_WidgetTextCount | 0 | 0/151 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/151 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_request_TextRequestSend | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 192/36/3/23/7/56/4/30/27/13/2/1/0/0/3 |
| Input/clang/text_request_TextRequestClear | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Input/clang/widget_benchmark_host_WidgetPaintReset | 0 | 0/151 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_input_SecondaryPointerSample | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Input/clang/session_SessionFlagSet | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/paint_queue_PaintSnapshotInvalidate | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Input/clang/tree_input_TreeKeyboardRoute | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/569/15/93/88/245/22/95/80/59/12/7/0/0/96 | 373/232/17/41/14/130/43/58/41/21/26/5/0/4/33 |
| Input/clang/text_editing_TextEditingFrameBegin | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Input/clang/tree_TreeStart | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Input/clang/tree_TreeFrameStatus | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Input/clang/textfield_TextFieldKey | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuTakeCommand | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 147/106/4/23/10/47/0/27/23/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 |
| Input/clang/text_context_menu_TextMenuPasteText | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuOpenRequested | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 47/0/0/4/4/8/0/4/4/4/10/0/0/0/0 |
| Input/clang/system_clipboard_SystemClipboardSet | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/text_input_platform_TextInputNoteFocused | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 79/0/0/15/15/49/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuIsOpenFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Input/clang/text_context_menu_TextMenuShow | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/clang/session_SessionFlagGet | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang/tree_draw_CommitFrame | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Input/clang/paint_queue_PaintSnapshotEmit | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 542/260/6/86/137/341/17/21/15/29/32/5/0/0/25 | 353/264/5/43/42/171/30/40/35/15/45/1/0/6/39 |
| Input/clang/tree_TreeIsBuilding | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang/tree_input_TreeHitAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 412/343/6/57/48/179/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Input/clang/tree_input_TreeTextKind | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_input_TreeFocusAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 57/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Input/clang/tree_TreeRead | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Input/clang/tree_TreeCount | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang/tree_input_TreeInputAllowedAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Input/clang/tree_TreeTextRange | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/61/3/0/0/31/20/17/14/2/0/2/0/2/5 |
| Input/clang/tree_input_TreeFocusedAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Input/clang/geometry_RectangleIntersection | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Input/clang/tree_TreePointerActivatable | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Input/clang/tree_input_TreeActivateAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Input/clang/cursor_CursorHoverFrameStart | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Input/clang/system_clipboard_SystemClipboardText | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 161/139/12/22/8/37/3/31/19/14/0/0/0/0/25 | 63/15/2/0/0/5/1/9/7/2/0/0/0/0/1 |
| Input/clang/text_context_menu_TextMenuOpenAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 85/34/1/14/9/44/0/8/7/7/2/0/0/0/2 | 64/12/2/5/0/19/1/6/4/4/2/0/0/0/1 |
| Input/clang/tree_TreePrevious | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Input/clang/tree_TreeCurrentParent | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Input/clang/tree_input_TreeTakeActivationAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang/tree_input_TreeTakeDragAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 149/69/5/9/1/47/11/25/20/6/8/6/0/0/15 |
| Input/clang/tree_TreePreviousWidgetState | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 |
| Input/clang/tree_input_TreeHasFocus | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 96/68/2/11/8/35/6/10/8/8/0/3/0/0/14 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 |
| Input/clang/tree_input_TreeBlurAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 |
| Input/clang/textfield_TextFieldFaceFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 144/134/4/24/61/91/4/9/5/8/28/0/0/0/10 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 |
| Input/clang/style_ResolveFont | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Input/clang/style_StyleFontValue | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Input/clang/textfield_TextFieldSecureDisplay | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/34/2/16/5/27/1/7/5/3/0/0/0/0/5 | 53/8/1/0/0/2/2/5/4/2/0/0/0/0/1 |
| Input/clang/font_metrics_GlyphWidth | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/text_input_TextFieldScrollFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 |
| Input/clang/textfield_TextFieldCursorAtX | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 |
| Input/clang/text_input_TextPointerOffset | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 69/0/0/0/0/0/0/7/7/7/18/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 |
| Input/clang/textfield_TextFieldCodepoints | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 35/27/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 |
| Input/clang/text_TextSpan | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/clang/textfield_TextFieldByteAfter | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 |
| Input/clang/text_input_CompositionTransitionFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 269/97/2/84/42/171/0/18/16/11/0/0/0/0/3 | 104/92/3/0/0/37/5/14/11/0/19/0/0/0/11 |
| Input/clang/textfield_TextFieldNext | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/text_input_TextSelectionAfterMove | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 |
| Input/clang/textfield_TextFieldInputTextAllowed | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 24/11/1/0/0/3/2/4/3/0/0/0/0/0/3 |
| Input/clang/textfield_TextFieldReplacementAllowed | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 |
| Input/clang/text_input_TextEditCommandDecisionFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 91/0/0/2/0/2/0/13/13/0/0/0/0/6/0 |
| Input/clang/text_request_TextRequestTake | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 190/0/0/43/24/110/10/16/16/11/0/9/0/0/0 | 67/0/0/1/0/17/1/7/7/4/3/1/0/0/0 |
| Input/clang/text_request_TextRequestChangeFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 264/246/12/47/26/98/5/38/26/13/0/0/0/0/24 | 147/55/6/6/12/41/8/22/16/5/0/0/0/2/0 |
| Input/clang/font_metrics_GlyphCaret | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 31/0/0/1/3/8/0/2/2/1/2/0/0/0/0 |
| Input/clang/text_input_TextFieldRevealScroll | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang/style_StyleOpacityValue | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Input/clang/tree_input_TreeFocusSubmittedAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 225/4/1/39/26/96/10/28/27/14/8/7/0/0/1 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 |
| Input/clang/tree_TreeSetWidgetState | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 |
| Input/clang/tree_TreeSetSemantic | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Input/clang/tree_TreeSetLabel | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang/tree_TreeSetStyleClass | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang/tree_TreeSetStyleId | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang/tree_TreeSetSelected | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang/drawing_props_ColorFromPacked | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/surface_Opacity | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Input/clang/font_metrics_GlyphLineHeight | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/text_input_TextFieldPaintFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 173/0/0/21/19/52/1/13/13/12/46/0/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 |
| Input/clang/theme_CurrentMuted | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentAccent | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/clang/text_input_TextCompositionUnderlineY | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang/text_input_TextCompositionUnderlineEndX | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/paint_queue_PaintLine | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Input/clang/tree_TreeSubmittedNodeAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang/button_Draw | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/clang/text_input_platform_TextInputFocusFrameBegin | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 86/0/0/16/15/53/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 |
| Input/clang/text_input_platform_TextInputFocused | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 |
| Input/clang/text_input_platform_TextInputOwnerMatches | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/tree_TreeDefer | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 |
| Input/clang/text_context_menu_TextMenuDraw | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 400/350/2/41/98/270/21/16/14/13/26/16/0/0/82 | 143/0/0/6/36/78/0/12/12/5/18/0/0/2/0 |
| Input/clang/tree_draw_FrameInputCommit | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Input/clang/paint_queue_PaintOverflow | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/clang/tree_TreeCancel | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Input/clang/paint_queue_PaintOrder | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Input/clang/paint_queue_SnapshotTextRead | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Input/clang/paint_queue_SnapshotTextJoin | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 428/378/19/62/70/174/14/71/52/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Input/clang/paint_queue_EmitPaint | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Input/clang/tree_input_TreeTopModalLayer | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/tree_TreeButtonLike | 0 | 0/151 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_input_PointInCircleBounds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/clang/tree_TreeOverlayLayerAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/std_utf8_Decode | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 295/211/2/65/19/100/2/37/35/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Input/clang/tree_TreeIdentityHash | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang/tree_TreeFindOld | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 119/53/5/8/6/35/9/16/11/4/0/5/0/4/3 |
| Input/clang/system_clipboard_host_SystemClipboardOpenHost | 0 | 0/151 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang/system_clipboard_host_SystemClipboardByteHost | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 |
| Input/clang/tree_input_TreeTakePointerActivationAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 81/12/1/6/0/13/3/12/11/6/5/1/0/2/3 |
| Input/clang/style_sheet_StyleControlRoleFacts | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Input/clang/textfield_TextFieldDefaultFace | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 71/4/1/4/39/46/2/1/0/6/25/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 |
| Input/clang/style_sheet_ResolveInstalledStyleRules | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 267/236/11/58/44/144/24/25/14/11/14/2/0/4/37 | 253/196/17/22/19/107/14/33/16/4/0/0/0/12/36 |
| Input/clang/text_input_TextInputContentWidth | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Input/clang/text_input_TextUtf8Continuation | 0 | 0/151 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_input_TextCursorForLength | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_input_CompositionBoundaryFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 25/7/1/0/0/3/2/3/2/0/0/0/0/0/2 |
| Input/clang/tree_TreeEnsureInteraction | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Input/clang/tree_TreeLayerAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Input/clang/paint_queue_ClipShape | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Input/clang/surface_Unit | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Input/clang/paint_queue_EmitRoundedOutline | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Input/clang/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/151 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/text_input_TextFieldCursorHeightFor | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Input/clang/paint_queue_EmitLabel | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Input/clang/paint_queue_ClipLabel | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Input/clang/paint_queue_EmitLine | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Input/clang/button_SubmitButton | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 621/600/18/167/120/309/20/59/41/56/51/0/0/0/60 | 376/10/1/57/34/129/0/17/16/38/33/2/0/3/1 |
| Input/clang/system_clipboard_SystemClipboardHasText | 0 | 0/151 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang/contextmenu_Draw | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 966/931/24/230/238/556/28/81/57/58/87/16/0/0/95 | 553/476/26/87/96/234/31/71/45/19/102/14/0/3/59 |
| Input/clang/cursor_CursorHoverFrameCommit | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Input/clang/paint_queue_PaintMeshPointCount | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang/paint_queue_ResolvePaintText | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Input/clang/tree_TreeModalLayer | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang/std_utf8_Continuation | 0 | 0/151 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/std_grapheme_IsControl | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/session_WidgetPartKey | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang/widget_key_WidgetRepeatedPart | 0 | 0/151 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/theme_CurrentSunken | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentText | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_ShapeSmall | 0 | 0/151 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang/theme_TypeLabel | 0 | 0/151 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang/style_sheet_StyleBucket | 0 | 0/151 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/style_sheet_StyleFirstBit | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 100/84/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Input/clang/style_sheet_StyleRuleMatchesState | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/style_sheet_StyleSelectorMatches | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Input/clang/style_MergeValues | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Input/clang/tree_TreeLabelSpanAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Input/clang/raster_shape_FillRoundedRectangle | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/paint_queue_FillShapeClipped | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 178/161/5/50/39/89/0/12/7/11/83/0/0/0/7 | 133/0/0/25/14/40/0/10/9/3/110/0/0/0/0 |
| Input/clang/paint_queue_EmitPaintNow | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 287/262/13/49/38/98/7/44/31/8/2/0/0/2/27 | 167/39/3/1/1/31/7/30/26/6/6/0/0/5/5 |
| Input/clang/tree_TreeCurrentPaintGeometry | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Input/clang/tree_TreeSubmittedGeometryAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Input/clang/paint_queue_KeepPaintMesh | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/15/3/5/5/1/6/1/0/2/0 |
| Input/clang/raster_shape_StrokeRoundedRectangle | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/raster_text_PaintTextRotated | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang/raster_text_PaintTextItalicClipped | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Input/clang/raster_text_PaintTextEmphasisClipped | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Input/clang/raster_text_PaintTextClipped | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang/raster_text_PaintText | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/style_ResolveFlags | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Input/clang/style_ResolveState | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Input/clang/tree_input_TreeHoveredAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Input/clang/button_ButtonDetailSize | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 45/0/0/3/0/8/0/4/4/2/5/2/0/1/0 |
| Input/clang/tree_TreeRootBounds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 35/0/0/0/0/6/3/3/3/3/6/3/0/0/0 |
| Input/clang/button_behavior_MeasureBounds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 190/0/0/19/10/49/0/23/23/2/120/0/0/0/0 |
| Input/clang/style_StyleMinHeightValue | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang/button_ButtonMinimumHeight | 0 | 0/151 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang/button_ButtonFitContentFace | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 378/291/13/41/19/86/4/61/48/25/90/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/1 |
| Input/clang/tree_TreeSetCircleHit | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Input/clang/tree_TreeSetSemanticDetail | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang/button_PaintButtonForNode | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1313/1276/16/257/184/508/12/135/119/93/317/0/1/0/133 | 793/0/0/161/116/384/0/54/54/41/331/4/0/7/0 |
| Input/clang/tree_TreeNextKey | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 |
| Input/clang/contextmenu_ContextMenuOverlayKey | 0 | 0/151 0% |  | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang/contextmenu_ContextMenuPanelKey | 0 | 0/151 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/tree_TreePreviousCommitted | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 |
| Input/clang/contextmenu_ContextMenuRowKey | 0 | 0/151 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 |
| Input/clang/contextmenu_ContextMenuOpens | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 |
| Input/clang/contextmenu_ContextMenuFind | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 |
| Input/clang/contextmenu_ContextMenuBounds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang/overlay_Draw | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 388/311/1/115/78/209/0/28/27/28/19/0/0/0/28 | 201/82/1/16/5/38/0/16/15/19/35/0/0/1/9 |
| Input/clang/contextmenu_ContextMenuLevel | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1416/1199/10/370/341/768/10/72/62/142/309/1/0/0/114 | 939/718/13/213/93/370/25/55/42/101/293/2/0/1/93 |
| Input/clang/paint_queue_PendingTextValue | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Input/clang/style_sheet_StyleMatchesOptional | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Input/clang/paint_queue_ShapeRadius | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 55/41/3/4/0/4/0/7/4/3/17/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Input/clang/paint_queue_FillClippedSpan | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Input/clang/paint_queue_KeepPaintBytes | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Input/clang/paint_queue_KeepMeshPoints | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Input/clang/text_emphasis_RunTextItalicRaster | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang/text_emphasis_RunTextEmphasisRaster | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Input/clang/button_ButtonFaceFinish | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Input/clang/button_ButtonDefaultFace | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Input/clang/button_ButtonFaceFacts | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Input/clang/button_ButtonDetailFont | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Input/clang/style_StyleLineHeightValue | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang/style_FitHeight | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Input/clang/button_behavior_MeasureContent | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 158/142/5/4/9/15/0/28/23/9/41/0/0/0/14 | 44/0/0/0/0/1/0/8/8/0/29/0/0/0/0 |
| Input/clang/button_behavior_ShapeWidth | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Input/clang/theme_ControlHeightFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Input/clang/button_behavior_ButtonCornerRadius | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Input/clang/focus_FocusDefaultOutlineBounds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 27/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Input/clang/image_lookup_IconLookupImage | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Input/clang/button_behavior_ButtonPictureSize | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Input/clang/button_behavior_ButtonLabeledPictureSize | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 92/53/4/0/0/0/0/11/7/7/27/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Input/clang/button_behavior_ContentLayout | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/167/5/37/26/72/0/26/21/9/91/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/87/0/0/0/0 |
| Input/clang/svg_icon_behavior_PaintSvgIcon | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 345/292/4/71/57/143/3/27/23/22/88/4/1/0/36 | 246/35/1/23/20/69/6/17/16/6/128/1/0/1/3 |
| Input/clang/image_lookup_ImageLookupTexture | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Input/clang/image_behavior_ImageDrawPlanFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Input/clang/contextmenu_ContextMenuLevelBounds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 456/420/20/93/62/191/12/52/32/33/57/11/0/0/41 | 273/193/8/42/16/91/6/30/22/22/45/1/0/0/32 |
| Input/clang/dpi_ScalePixels | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 83/72/1/0/0/0/0/10/9/8/21/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 |
| Input/clang/contextmenu_ContextMenuRowTop | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 101/83/5/20/9/37/4/13/8/9/25/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 |
| Input/clang/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 21/0/0/0/0/0/0/0/0/0/0/0/0/3/0 |
| Input/clang/tree_input_TreeTakeDismissalAt | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 |
| Input/clang/tree_TreeSetPopup | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 |
| Input/clang/drawing_props_StyleTokenColorOr | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/theme_CurrentSurface | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentBorder | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentDisabled | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/theme_CurrentSelection | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang/tree_TreePushScope | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 |
| Input/clang/contextmenu_ContextMenuTextLeft | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 |
| Input/clang/contextmenu_ContextMenuLevelChecks | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 25/14/1/0/0/5/0/3/2/0/0/0/0/0/4 |
| Input/clang/contextmenu_ContextMenuInlineWidth | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 |
| Input/clang/contextmenu_ContextMenuInlineEnds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 |
| Input/clang/svg_icon_behavior_PaintStroke | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 639/592/8/74/327/438/14/31/23/21/75/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 |
| Input/clang/widget_benchmark_host_RecordBounds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Input/clang/theme_DarkPalette | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang/theme_StateLayer | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Input/clang/card_behavior_CardDefaultFace | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 73/0/0/6/0/29/1/9/8/3/12/0/0/0/0 |
| Input/clang/theme_ShapeMedium | 0 | 0/151 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang/theme_FontForRole | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Input/clang/theme_MotionDuration | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Input/clang/drawing_props_PaintPixelScale | 0 | 0/151 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/clang/svg_icon_behavior_SvgIconSlot | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 207/185/3/27/24/110/15/17/14/11/0/14/0/0/42 | 164/41/4/6/7/61/19/20/16/3/10/5/0/0/4 |
| Input/clang/svg_icon_behavior_SvgIconSnap | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 74/27/2/0/0/0/0/11/9/6/19/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Input/clang/svg_icon_behavior_SvgIconTexture | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 192/156/12/7/3/67/28/24/12/1/9/2/0/10/39 |
| Input/clang/paint_queue_PaintImageWithPolygon | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Input/clang/svg_icon_behavior_StrokeAppendByte | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/clang/svg_icon_behavior_StrokeAppendNumber | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 230/196/7/10/16/40/7/37/30/13/9/0/6/0/35 | 95/25/2/1/4/27/12/9/7/1/6/4/0/4/4 |
| Input/clang/tree_TreePopScope | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 42/0/0/0/0/6/4/5/5/4/0/4/0/0/0 |
| Input/clang/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 237/189/8/28/24/105/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang/theme_ShapeSurface | 0 | 0/151 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang/svg_icon_behavior_SvgIconRetire | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Input/clang/svg_shape_ShapeRaster | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 258/218/9/32/17/69/6/47/38/15/14/2/0/0/40 | 129/113/4/3/3/27/9/17/13/4/8/1/0/0/21 |
| Input/clang/image_behavior_ImageFitBounds | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 196/177/8/18/9/31/0/26/18/13/45/0/0/0/18 | 43/0/0/0/0/3/0/9/9/0/30/0/0/0/0 |
| Input/clang/image_behavior_ImagePolygonValid | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 99/55/1/9/1/12/1/16/15/7/12/0/0/0/4 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Input/clang/paint_queue_EmitImage | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Input/clang/paint_queue_ClipImage | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Input/clang/svg_shape_ShapeNumber | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 343/304/21/56/16/91/15/62/41/19/54/1/0/0/48 | 179/142/8/0/0/31/14/38/30/0/33/0/0/0/25 |
| Input/clang/svg_shape_ShapeCoverEdges | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 326/308/14/23/20/72/15/49/35/14/56/3/0/0/51 | 192/158/16/10/9/58/22/28/12/3/45/0/0/3/12 |
| Input/clang/paint_queue_EmitImageBegin | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Input/clang/widget_benchmark_host_RasterImage | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Input/clang/paint_queue_EmitImageEnd | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Input/clang/svg_shape_ShapeIsDigit | 0 | 0/151 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang/svg_shape_ShapeIsSpace | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang/svg_shape_ShapeAddPoint | 0 | 0/151 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Input/clang/svg_shape_ShapeAddSpan | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 137/123/3/0/0/16/4/16/13/11/38/0/0/0/22 | 91/16/2/0/0/22/17/14/12/1/39/0/0/0/1 |
| Input/clang/svg_shape_ShapeStrokeSubpath | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 602/520/11/90/49/179/20/57/46/38/268/0/3/0/61 | 575/417/24/43/25/189/49/61/37/8/319/0/3/10/72 |
| Input/clang/svg_shape_ShapeCollapse | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/66/3/0/0/16/8/8/5/2/17/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Input/clang/svg_shape_ShapeAddCircle | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Input/clang/svg_shape_ShapeQuad | 0 | 0/151 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Input/clang/svg_shape_ShapeAbs | 0 | 0/151 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Input/clang/svg_shape_ShapeTriangle | 0 | 0/151 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Input/clang/svg_shape_ShapeAddEdge | 0 | 0/151 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Hello/clang/hello_main | 0 | 0/0 unknown |  | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
