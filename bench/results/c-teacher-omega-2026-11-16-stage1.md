# C teacher: native assembly comparison

Base revision: 888f39a992cc0a18d1f0fa0e41f0070dc0002a6c
Host: omega
build/bin/zi2c SHA-256: 549a9b9f3600255c4b8af83947661f92ec8db8a97d80d26392b1674a8bc59f27
build/bin/zi2zx SHA-256: 89c5e0595d2c8b294b8c7458dd6b3025871f82006b97c514ec5ae2d6900b264a
build/bin/zx2native SHA-256: 3ca3bc51f2bb5f1d8176c8f3e02276e09330587c80f9cbbbb2470054cdc90ea1
Learning off. Generated C compiled with -O2; native compiled from verified ZX. Function extents come from the native map; C symbols match module_name or exported name exactly. Absent symbols are not evidence of inlining. Metrics: instructions/loop-union/back-edges/stack-reads/stack-writes/memory/indexed/branches/forward-branches/calls/vector-register-uses/mul/div/shifts/loop-immediate-or-PC-relative candidates. Stack traffic includes locals and saves. Vector-register uses include scalar floating point. Loop constants are LICM candidates, not proven invariants. Back edges describe loop bodies, not execution frequency. SIGPROF uses a 1ms CPU timer, at most 30 runs; zero samples means unknown time share. Sampling includes process startup and runtime. Ranking weights relative instruction excess by observed function samples; it predicts neither cycles nor speedup.

## Sieve / cc
Native SHA-256: 81468f46fa5939aeec5abd2c18b72fb884295ee84ea0190e788663fed0d10ae9
C SHA-256: a90bdb344798838279f6a10871246589e12193ef0ab912e32c4b0994e021f267
ZX SHA-256: f1fccf1c68dab477a2d0e35a94d9fda6ed8c9873d34deb24c4de73e88f22d92e
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 117/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Sieve | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 34/19/3/0/0/7/6/6/3/1/0/1/0/0/9 | 117 | 421..594: 40 instructions, 109 samples |

## Sieve / clang-19
Native SHA-256: 81468f46fa5939aeec5abd2c18b72fb884295ee84ea0190e788663fed0d10ae9
C SHA-256: ffe0a6f6a17493a2edf3e8be1843da7ff239ae67174011b627c4b07e379a5f8a
ZX SHA-256: f1fccf1c68dab477a2d0e35a94d9fda6ed8c9873d34deb24c4de73e88f22d92e
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 125/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Sieve | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 | 125 | 421..594: 40 instructions, 119 samples |

## Records / cc
Native SHA-256: a23507c88c84fd9677ee731d99bbb9dba2c5480fec14a43b7fb22a98af0c89fa
C SHA-256: c1e5ee0de67ea0bb3254c26d8cba9d59504c91d226b277a243a97265bf234f30
ZX SHA-256: 4dc39f678013ae78a3783b0dc2cf95aa0cd66f3c79ed5b88250f42f8d9d3ea45
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 86/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Records | 929/149/21/260/520/806/4/46/25/5/0/5/2/0/30 | 70/57/4/0/0/20/2/6/2/0/0/2/0/2/11 | 86 | 5509..6090: 132 instructions, 86 samples |

## Records / clang-19
Native SHA-256: a23507c88c84fd9677ee731d99bbb9dba2c5480fec14a43b7fb22a98af0c89fa
C SHA-256: e0fb2ce977684ea9df7560d86ee8b7b931dc72fa33e8fe30b3f4e16c7a7a2782
ZX SHA-256: 4dc39f678013ae78a3783b0dc2cf95aa0cd66f3c79ed5b88250f42f8d9d3ea45
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 84/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Records | 929/149/21/260/520/806/4/46/25/5/0/5/2/0/30 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 | 84 | 5509..6090: 132 instructions, 84 samples |

## Hash / cc
Native SHA-256: 4875728741b927cad36d216942e1dc4bfeb809d2d05daf6ad7539f792b89798c
C SHA-256: 1d1a9c1821d391c1ee6cd78066299c2a37dceb66ccc27db585c8b9f55f3e134c
ZX SHA-256: 41e6991d1c84b4fec12fe82b4ad9275c6394ed284ae862572e2ee2ea7f65f4b2
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 148/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Hash | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 26/12/2/0/0/5/1/2/0/0/0/4/0/1/4 | 148 | 199..408: 52 instructions, 148 samples |

## Hash / clang-19
Native SHA-256: 4875728741b927cad36d216942e1dc4bfeb809d2d05daf6ad7539f792b89798c
C SHA-256: 6f3b626c0e265316a033df753b6a6849e8321f525329a335dba02b936d3b5c59
ZX SHA-256: 41e6991d1c84b4fec12fe82b4ad9275c6394ed284ae862572e2ee2ea7f65f4b2
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 146/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Hash | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 | 146 | 199..408: 52 instructions, 146 samples |

## Vectors / cc
Native SHA-256: 0d3044ebfd41bd48560d20a3003d5337f9e1f99fcecdd75c68a34433171dee57
C SHA-256: 80ce3f982e01a9e5571fe04dda3f94f871709933e51d40514bd28f4127f251f1
ZX SHA-256: 541c0b8413f48e82b2b148136f15a8f226ee7f9e6b4a82413f0df09fc64630d2
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 149/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Vectors | 149/137/15/22/23/47/2/31/16/3/0/2/0/0/15 | 101/90/7/7/8/24/5/16/9/2/7/0/0/0/9 | 121 | 218..758: 136 instructions, 121 samples |

## Vectors / clang-19
Native SHA-256: 0d3044ebfd41bd48560d20a3003d5337f9e1f99fcecdd75c68a34433171dee57
C SHA-256: 69731a1b48535b712c8be6562303fd06cdce49a7447024a162af4e25f7379c31
ZX SHA-256: 541c0b8413f48e82b2b148136f15a8f226ee7f9e6b4a82413f0df09fc64630d2
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 154/29

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 1 | 50..157: 27 instructions, 1 samples |
| wasm_tier_Vectors | 149/137/15/22/23/47/2/31/16/3/0/2/0/0/15 | 123/101/10/3/3/22/15/17/7/2/26/1/0/1/13 | 138 | 218..758: 136 instructions, 138 samples |

## Calls / cc
Native SHA-256: c5a6038ad354524756fc02ba8750c8bee956f625548574f176e01eda8d9a88b9
C SHA-256: 902825e78c07e1762c9f52470a603784758fd5395577a7b7b4a712e063e9a0ca
ZX SHA-256: b55d21c3a286aa61d33a3f7eab3eca09121875b0c839f806192bb1c6c9ef011f
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Calls_main | 26/20/5/0/0/0/0/8/3/1/0/0/0/0/7 | absent (inline/dead/renamed unknown) | 0 | 38..114: 19 instructions, 0 samples |
| wasm_tier_Calls | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 16/6/1/0/0/0/0/1/0/1/0/0/0/0/2 | 0 | none |
| wasm_tier_Fib | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 241/222/10/27/24/72/1/21/11/1/0/0/0/0/28 | 0 | none |

## Calls / clang-19
Native SHA-256: c5a6038ad354524756fc02ba8750c8bee956f625548574f176e01eda8d9a88b9
C SHA-256: ee636a5b4e6c82ce6d7ff87ff2ecb19cb23ad56ca6129b623857ce20a4ca165f
ZX SHA-256: b55d21c3a286aa61d33a3f7eab3eca09121875b0c839f806192bb1c6c9ef011f
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Calls_main | 26/20/5/0/0/0/0/8/3/1/0/0/0/0/7 | absent (inline/dead/renamed unknown) | 0 | 38..114: 19 instructions, 0 samples |
| wasm_tier_Calls | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| wasm_tier_Fib | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 21/7/1/0/0/2/0/2/1/1/0/0/0/0/1 | 0 | none |

## Printing / cc
Native SHA-256: c31a650153065f841a9a529e8a5284c88878ab83ec406821d0661754eb823fa1
C SHA-256: 9ab4e28c0b29b38737501bf4d653c9fa0d77682b00603669543a9ef60e2720aa
ZX SHA-256: b10e5d3d72ba8ccea623fd45c8fbceb20787a7d8bccd47512eb7bbef246d8d9a
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Printing_main | 19/11/3/1/1/2/0/5/2/1/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 44..85: 11 instructions, 0 samples |
| wasm_tier_Printing | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 31/14/1/0/0/4/0/1/0/1/0/2/0/0/3 | 0 | 131..321: 52 instructions, 0 samples |

## Printing / clang-19
Native SHA-256: c31a650153065f841a9a529e8a5284c88878ab83ec406821d0661754eb823fa1
C SHA-256: 99a16912804f94e7f52ce464a6b6c974f88440f4ecf335c127ee487294e5f34e
ZX SHA-256: b10e5d3d72ba8ccea623fd45c8fbceb20787a7d8bccd47512eb7bbef246d8d9a
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Printing_main | 19/11/3/1/1/2/0/5/2/1/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 44..85: 11 instructions, 0 samples |
| wasm_tier_Printing | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 | 0 | 131..321: 52 instructions, 0 samples |

## Button / cc
Native SHA-256: 4c3fe3a29593aa5f1411ad80e349448f078a611de03042cd328a055c85ef4ac8
C SHA-256: cad0c34b670e2363e8361bcac8a7f903af61489b8faaa9fd0b3f8ea74bf59a1b
ZX SHA-256: 1d6b8297e08a8a5583e616b7230214dfd42790064b27839514d8d752e2e4fe8d
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 311/1

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 87..158: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 158/147/9/4/10/45/4/19/10/12/8/0/0/0/36 | 0 | 1177..1199: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 0 | 3735..3754: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 4512..4534: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_TreeCount | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | 4923..4942: 4 instructions, 0 samples |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 5060..5079: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 621/68/17/58/91/370/80/53/36/30/63/46/0/0/17 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 | 2 | 6156..6178: 4 instructions, 2 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 9341..10900: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 11097..11116: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 11370..11522: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 11602..11712: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 0 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 12618..12661: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 13769..16426: 556 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 3 | 18031..18053: 4 instructions, 2 samples |
| tree_TreeFrameStatus | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | 18652..18671: 4 instructions, 0 samples |
| button_SubmitButton | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 | 0 | 18803..21625: 584 instructions, 0 samples |
| tree_TreeCurrentParent | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 7 | 21906..21928: 4 instructions, 3 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 23184..26793: 637 instructions, 0 samples |
| paint_queue_PaintFlush | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 0 | 26899..27391: 98 instructions, 0 samples |
| tree_TreeRead | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 7 | 28941..28963: 4 instructions, 2 samples |
| tree_TreeIsBuilding | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 19 | 32385..32404: 4 instructions, 19 samples |
| tree_input_TreeHitAt | 492/390/12/63/79/240/19/77/65/38/22/7/0/0/66 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 | 12 | 33138..34449: 256 instructions, 6 samples |
| tree_input_TreeTextFocused | 147/107/7/5/22/68/17/19/12/7/10/5/0/0/22 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 | 3 | 35083..35102: 4 instructions, 2 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| tree_TreeInputAt | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 6 | 36539..36561: 4 instructions, 3 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 37278..37591: 80 instructions, 0 samples |
| tree_TreeTextRange | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 38266..38448: 37 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 38831..39095: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 4 | 39245..39264: 4 instructions, 4 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 39891..40157: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 | 10 | 41673..42899: 242 instructions, 7 samples |
| tree_TreeSubmit | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 55 | 45194..50120: 889 instructions, 29 samples |
| tree_TreePrevious | 128/29/4/12/19/59/10/14/10/7/6/4/0/0/3 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 | 6 | 50393..50415: 4 instructions, 3 samples |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 83/12/3/8/17/47/9/5/2/4/6/3/0/0/3 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 | 3 | 51414..51433: 4 instructions, 3 samples |
| tree_input_TreeHoveredAt | 145/20/5/12/26/72/15/19/14/7/10/5/0/0/5 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 | 3 | 51989..52011: 4 instructions, 2 samples |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 266/172/4/89/71/181/2/9/5/15/144/0/0/0/7 | 0 | 52395..52417: 4 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 | 0 | none |
| tree_TreeRootBounds | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 4 | 54094..54116: 4 instructions, 2 samples |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 | 0 | 54425..55723: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 | 0 | 56237..57551: 284 instructions, 0 samples |
| tree_TreeSetInteractive | 171/20/5/21/29/100/18/15/10/7/10/8/0/0/5 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 10 | 57726..57745: 4 instructions, 3 samples |
| tree_TreeSetCircleHit | 140/20/5/11/24/76/16/15/10/6/10/6/0/0/5 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 | 8 | 58727..58749: 4 instructions, 4 samples |
| tree_TreeSetSelected | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 4 | 59729..59751: 4 instructions, 2 samples |
| tree_TreeSetLabel | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 7 | 60490..60512: 4 instructions, 4 samples |
| tree_TreeSetSemanticDetail | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | 60706..60725: 4 instructions, 0 samples |
| tree_TreeSetStyleClass | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 3 | 61474..61493: 4 instructions, 2 samples |
| tree_TreeSetStyleId | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 5 | 62333..62355: 4 instructions, 4 samples |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 | 1 | 62714..69353: 1342 instructions, 1 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 26/12/1/0/0/2/1/2/1/2/0/0/0/0/1 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 25 | 70606..70625: 4 instructions, 5 samples |
| paint_queue_PaintOrder | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 75063..76296: 247 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 77227..79162: 421 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 0 | 79329..79351: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 0 | 79858..79880: 4 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 0 | 82733..83519: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 0 | 83972..85203: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 6 | 85331..85350: 4 instructions, 3 samples |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 6 | 86499..86518: 4 instructions, 4 samples |
| tree_TreeExplicitExists | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 78/21/2/2/2/18/7/5/3/2/0/3/0/3/2 | 7 | 88090..88618: 89 instructions, 4 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 24 | 88992..89606: 113 instructions, 18 samples |
| tree_input_TreeTakePointerActivationAt | 205/20/5/34/39/112/16/25/20/12/17/6/0/0/5 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 | 0 | 90085..90101: 4 instructions, 0 samples |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 45/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 0 | 90818..90984: 33 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 293/260/17/62/47/150/26/36/19/11/16/6/0/0/40 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 | 0 | 91144..92469: 258 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 | 0 | 92974..94042: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 | 0 | 94087..94244: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 33/16/2/0/0/4/2/5/3/0/0/0/0/2/5 | 0 | 94367..94529: 47 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 | 0 | 94855..95033: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 | 0 | 95154..95768: 146 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 | 0 | 96112..96134: 4 instructions, 0 samples |
| tree_TreeEnsureInteraction | 201/93/5/30/37/121/16/13/8/5/8/9/0/0/18 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 | 9 | 96585..96607: 4 instructions, 5 samples |
| tree_TreeEnsureSemantic | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 13 | 97410..97429: 4 instructions, 6 samples |
| tree_TreeStoreLabel | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 9 | 98632..100784: 456 instructions, 8 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 0 | 102398..102420: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 | 0 | 103612..103634: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 0 | 104132..104154: 4 instructions, 0 samples |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 | 0 | 104293..104539: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 | 0 | 105085..105107: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 | 0 | 105319..105477: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 | 0 | 105705..105876: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 | 0 | 106183..106929: 172 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 388/333/6/71/75/167/7/29/23/22/117/4/1/0/40 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 | 0 | 107182..108733: 329 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 | 0 | 109002..109024: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 | 0 | 109666..109688: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 | 0 | 111093..111115: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 0 | 112304..112326: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 113124..113134: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 0 | 113264..113283: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 6 | 113809..113828: 4 instructions, 6 samples |
| tree_TreeLayerAt | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 12 | 114173..114675: 106 instructions, 9 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 278/259/112/0/0/136/10/118/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 | 0 | 115574..115922: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 | 0 | 117613..117635: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | 117744..117763: 4 instructions, 0 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 | 0 | 118037..118698: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 | 0 | 119289..119308: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 | 0 | 120058..120217: 43 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 | 0 | 120285..120364: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | 120530..120549: 4 instructions, 0 samples |
| tree_TreeSetLabelSpan | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | 120801..120820: 4 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 0 | 121111..121133: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 121735..121757: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 0 | 122087..126535: 836 instructions, 0 samples |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 132/113/2/24/6/35/0/11/8/2/63/0/0/6/11 | 0 | 126610..126632: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 246/224/9/35/36/134/27/22/13/11/12/14/0/0/54 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 | 0 | 127192..127956: 139 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 | 0 | 128503..128626: 30 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 240/130/10/33/28/120/24/19/9/3/12/16/1/0/30 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 | 0 | 128900..129234: 70 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 | 0 | 130842..130864: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 132086..132681: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 133893..133915: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 0 | 145996..146015: 4 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 147914..148607: 158 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 148754..149826: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 150509..150531: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 12 | 151320..151339: 4 instructions, 5 samples |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 0 | 152224..152483: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 | 0 | 152591..152649: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 | 0 | none |
| svg_shape_ShapeRaster | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 | 0 | 153186..154036: 207 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 345/311/29/30/34/97/9/61/32/9/0/10/1/0/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 | 0 | 154297..155310: 262 instructions, 0 samples |
| image_behavior_ImageFitBounds | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 | 0 | 155472..156174: 177 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 | 0 | 156290..156644: 91 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 156667..156689: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 157098..157120: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 158412..158490: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 159081..159100: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 159550..159569: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 0 | 160491..160510: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 0 | 162272..162291: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 163064..163080: 4 instructions, 0 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 163459..163654: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 163731..164742: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 164904..165281: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 | 0 | 166383..166974: 132 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 | 0 | 167308..168514: 284 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 | 0 | 168735..170908: 479 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 | 0 | 171339..172734: 332 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 24/0/0/4/4/18/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 22/0/0/0/0/4/0/2/0/0/12/0/0/3/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 174083..174128: 12 instructions, 0 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 174739..174993: 64 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 175089..175353: 66 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 175449..175683: 58 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 19/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 | 0 | 176090..176571: 123 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 176703..176821: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 177027..179403: 435 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 179595..179642: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 180032..181070: 211 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 181262..181309: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 181699..182737: 211 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 182929..182976: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 | 0 | 183435..185854: 523 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | 185978..186101: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 | 0 | 186139..186370: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 | 0 | 186449..186655: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 | 0 | 186778..186896: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 187707..187723: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 188082..188098: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 | 0 | 188157..188516: 90 instructions, 0 samples |

## Button / clang-19
Native SHA-256: 4c3fe3a29593aa5f1411ad80e349448f078a611de03042cd328a055c85ef4ac8
C SHA-256: 9468ebf7a30babea7d8d481ece8d21f51a5fc9acafbeacc5d99c3c4e7c2abd6a
ZX SHA-256: 1d6b8297e08a8a5583e616b7230214dfd42790064b27839514d8d752e2e4fe8d
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 327/1

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 87..158: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 145/0/0/5/5/38/0/13/13/14/8/0/0/0/0 | 1 | 1177..1199: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 0 | 3735..3754: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 4512..4534: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 7 | 4923..4942: 4 instructions, 7 samples |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 5060..5079: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 621/68/17/58/91/370/80/53/36/30/63/46/0/0/17 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 | 5 | 6156..6178: 4 instructions, 3 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 9341..10900: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 11097..11116: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 11370..11522: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 11602..11712: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 1 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 12618..12661: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 | 0 | 13769..16426: 556 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 1 | 18031..18053: 4 instructions, 1 samples |
| tree_TreeFrameStatus | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 3 | 18652..18671: 4 instructions, 3 samples |
| button_SubmitButton | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 | 0 | 18803..21625: 584 instructions, 0 samples |
| tree_TreeCurrentParent | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 6 | 21906..21928: 4 instructions, 5 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 23184..26793: 637 instructions, 0 samples |
| paint_queue_PaintFlush | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 0 | 26899..27391: 98 instructions, 0 samples |
| tree_TreeRead | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 4 | 28941..28963: 4 instructions, 2 samples |
| tree_TreeIsBuilding | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 13 | 32385..32404: 4 instructions, 13 samples |
| tree_input_TreeHitAt | 492/390/12/63/79/240/19/77/65/38/22/7/0/0/66 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 5 | 32609..32628: 4 instructions, 3 samples |
| tree_input_TreeTextFocused | 147/107/7/5/22/68/17/19/12/7/10/5/0/0/22 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 4 | 35169..35733: 103 instructions, 4 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| tree_TreeInputAt | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 6 | 36437..36456: 4 instructions, 3 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 37278..37591: 80 instructions, 0 samples |
| tree_TreeTextRange | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 38266..38448: 37 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 38831..39095: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 3 | 39376..39398: 4 instructions, 3 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 39891..40157: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 13 | 41673..42899: 242 instructions, 11 samples |
| tree_TreeSubmit | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 68 | 45194..50120: 889 instructions, 32 samples |
| tree_TreePrevious | 128/29/4/12/19/59/10/14/10/7/6/4/0/0/3 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 8 | 50260..50279: 4 instructions, 3 samples |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 83/12/3/8/17/47/9/5/2/4/6/3/0/0/3 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 1 | 51414..51433: 4 instructions, 1 samples |
| tree_input_TreeHoveredAt | 145/20/5/12/26/72/15/19/14/7/10/5/0/0/5 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 8 | 51744..51763: 4 instructions, 5 samples |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 0 | 52395..52417: 4 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 | 0 | none |
| tree_TreeRootBounds | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 6 | 53768..53787: 4 instructions, 3 samples |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 | 0 | 54425..55723: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 | 0 | 56237..57551: 284 instructions, 0 samples |
| tree_TreeSetInteractive | 171/20/5/21/29/100/18/15/10/7/10/8/0/0/5 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 17 | 58237..58259: 4 instructions, 9 samples |
| tree_TreeSetCircleHit | 140/20/5/11/24/76/16/15/10/6/10/6/0/0/5 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 9 | 58727..58749: 4 instructions, 4 samples |
| tree_TreeSetSelected | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 4 | 59363..59382: 4 instructions, 2 samples |
| tree_TreeSetLabel | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 13 | 60490..60512: 4 instructions, 5 samples |
| tree_TreeSetSemanticDetail | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | 60706..60725: 4 instructions, 0 samples |
| tree_TreeSetStyleClass | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 7 | 61474..61493: 4 instructions, 3 samples |
| tree_TreeSetStyleId | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 7 | 62048..62067: 4 instructions, 3 samples |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 | 0 | 62714..69353: 1342 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 22 | 70606..70625: 4 instructions, 3 samples |
| paint_queue_PaintOrder | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 75063..76296: 247 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 77227..79162: 421 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 79329..79351: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 0 | 79858..79880: 4 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 82733..83519: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 83972..85203: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 10 | 85842..85864: 4 instructions, 7 samples |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 3 | 86499..86518: 4 instructions, 2 samples |
| tree_TreeExplicitExists | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 11 | 88090..88618: 89 instructions, 7 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 18 | 88992..89606: 113 instructions, 13 samples |
| tree_input_TreeTakePointerActivationAt | 205/20/5/34/39/112/16/25/20/12/17/6/0/0/5 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 | 0 | 90085..90101: 4 instructions, 0 samples |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 90818..90984: 33 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 293/260/17/62/47/150/26/36/19/11/16/6/0/0/40 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 | 0 | 91144..92469: 258 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 0 | 92974..94042: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 94087..94244: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 0 | 94367..94529: 47 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 94855..95033: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 | 0 | 95154..95768: 146 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 0 | 96112..96134: 4 instructions, 0 samples |
| tree_TreeEnsureInteraction | 201/93/5/30/37/121/16/13/8/5/8/9/0/0/18 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 7 | 96370..96389: 4 instructions, 5 samples |
| tree_TreeEnsureSemantic | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 6 | 97410..97429: 4 instructions, 4 samples |
| tree_TreeStoreLabel | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 6 | 98632..100784: 456 instructions, 4 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 102398..102420: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 0 | 103612..103634: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 1 | 104174..104196: 4 instructions, 1 samples |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 104293..104539: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 105085..105107: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 105319..105477: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 105705..105876: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 | 0 | 106183..106929: 172 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 388/333/6/71/75/167/7/29/23/22/117/4/1/0/40 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 | 0 | 107182..108733: 329 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 109002..109024: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 109666..109688: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 111093..111115: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 0 | 112304..112326: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 113124..113134: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 1 | 113264..113283: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 5 | 113809..113828: 4 instructions, 4 samples |
| tree_TreeLayerAt | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 8 | 113994..114013: 4 instructions, 6 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 115574..115922: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 117613..117635: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 0 | 117744..117763: 4 instructions, 0 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 118037..118698: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 | 0 | 119289..119308: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 120058..120217: 43 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 120285..120364: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | 120530..120549: 4 instructions, 0 samples |
| tree_TreeSetLabelSpan | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | 120801..120820: 4 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 121111..121133: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 121735..121757: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 0 | 122087..126535: 836 instructions, 0 samples |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 126610..126632: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 246/224/9/35/36/134/27/22/13/11/12/14/0/0/54 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 | 0 | 127192..127956: 139 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 128503..128626: 30 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 240/130/10/33/28/120/24/19/9/3/12/16/1/0/30 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 | 0 | 128900..129234: 70 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 130842..130864: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 132086..132681: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 133893..133915: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 0 | 145996..146015: 4 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 147914..148607: 158 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 148754..149826: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 150509..150531: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 9 | 151680..151702: 4 instructions, 5 samples |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 0 | 152224..152483: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 46/22/2/0/0/8/7/6/4/0/0/5/0/0/1 | 0 | 152591..152649: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 | 0 | 153186..154036: 207 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 345/311/29/30/34/97/9/61/32/9/0/10/1/0/41 | 375/321/22/23/20/119/48/57/35/1/32/4/0/4/51 | 0 | 154297..155310: 262 instructions, 0 samples |
| image_behavior_ImageFitBounds | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 | 0 | 155472..156174: 177 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 156290..156644: 91 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 156667..156689: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 157098..157120: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 158412..158490: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 159081..159100: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 159550..159569: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 0 | 160491..160510: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 0 | 162272..162291: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 163064..163080: 4 instructions, 0 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 163459..163654: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 163731..164742: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 164904..165281: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 166383..166974: 132 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 | 0 | 167308..168514: 284 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 669/640/30/61/31/225/53/103/72/15/142/0/0/3/94 | 0 | 168735..170908: 479 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 | 0 | 171339..172734: 332 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 24/0/0/4/4/18/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 174083..174128: 12 instructions, 0 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 174739..174993: 64 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 175089..175353: 66 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 175449..175683: 58 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 | 0 | 176090..176571: 123 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 176703..176821: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 177027..179403: 435 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 179595..179642: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 180032..181070: 211 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 181262..181309: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 181699..182737: 211 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 182929..182976: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 | 0 | 183435..185854: 523 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 | 0 | 185978..186101: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 186139..186370: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 186449..186655: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 186778..186896: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 187707..187723: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 188082..188098: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 | 0 | 188157..188516: 90 instructions, 0 samples |

## Text / cc
Native SHA-256: cfe73702ef0c9437e3bbef42642508ee8bc05011afe57bb2ebbe57d77dfe1999
C SHA-256: 4abc4d6c8da1175b763ea53c29cb054974fffe4c2a5f06131c828109890de89c
ZX SHA-256: 743b12e43fe1f1e37f3cdf63d5336ff63049d0d484b8776afa90a7d1c1a08a4d
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 247/1

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 67..138: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 285/159/9/34/82/140/3/30/21/16/47/0/0/0/27 | 140/128/10/16/20/66/4/16/6/7/32/0/0/1/29 | 0 | 902..1583: 158 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 0 | 2503..2522: 4 instructions, 0 samples |
| text___zi_spec_6a19f7c98e79a734 | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | 3281..3303: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_TreeCount | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 2 | 3707..3726: 4 instructions, 2 samples |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3844..3863: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 4829..6388: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 6585..6604: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 6858..7010: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 7090..7200: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 1 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 8106..8149: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 9257..11914: 556 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 1 | 13519..13541: 4 instructions, 1 samples |
| tree_TreeFrameStatus | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 3 | 14140..14159: 4 instructions, 3 samples |
| text_DrawSession | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 293/263/10/101/82/188/2/15/5/17/159/0/0/0/13 | 0 | 15422..16047: 121 instructions, 0 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 17066..20675: 637 instructions, 0 samples |
| paint_queue_PaintFlush | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 0 | 20781..21273: 98 instructions, 0 samples |
| tree_TreeRead | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 7 | 23700..23722: 4 instructions, 3 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 26325..26638: 80 instructions, 0 samples |
| tree_TreeTextRange | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 27313..27495: 37 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 28258..28522: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 0 | 28672..28691: 4 instructions, 0 samples |
| tree_TreeInputAt | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 0 | 29346..29365: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 30255..30521: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 44/20/1/5/0/9/1/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 192/153/11/6/6/53/12/32/21/4/0/7/0/4/25 | 15 | 32037..33263: 242 instructions, 13 samples |
| tree_TreeSubmit | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 68 | 35558..40484: 889 instructions, 40 samples |
| std_math_Finite32 | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 11/0/0/0/0/3/0/2/2/0/4/0/0/0/0 | 0 | none |
| tree_TreeRootBounds | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | 40783..40802: 4 instructions, 0 samples |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 57/47/1/10/12/28/0/2/1/2/14/0/0/1/10 | 0 | 41420..41442: 4 instructions, 0 samples |
| text_PrepareText | 263/81/2/62/65/169/0/13/11/15/16/0/0/0/2 | 172/91/3/37/23/69/2/7/4/14/32/0/0/3/4 | 0 | 42603..42797: 45 instructions, 0 samples |
| tree_TreeIsBuilding | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 13 | 42885..42904: 4 instructions, 13 samples |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 48/0/0/2/2/9/2/4/3/1/4/2/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 209/114/8/44/72/120/1/16/8/6/94/0/0/0/6 | 81/32/1/8/14/25/0/1/0/2/56/0/0/3/4 | 0 | 43581..44139: 109 instructions, 0 samples |
| text_TextOrigin | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| tree_TreeSetLabel | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 10 | 44358..44377: 4 instructions, 3 samples |
| tree_TreeSetStyleClass | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 7 | 45411..45433: 4 instructions, 3 samples |
| tree_TreeSetStyleId | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 4 | 45700..45719: 4 instructions, 2 samples |
| tree_TreeSetSemantic | 171/107/6/18/24/80/14/20/14/10/8/6/0/0/21 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 | 3 | 46282..46301: 4 instructions, 3 samples |
| text_TextHiddenLines | 118/39/2/11/2/22/0/11/9/10/22/0/0/0/4 | 86/45/3/10/2/22/7/11/4/2/22/0/0/0/6 | 0 | 47391..47478: 26 instructions, 0 samples |
| tree_TreeSetHiddenLines | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 0 | 47583..47602: 4 instructions, 0 samples |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 49/0/0/15/15/33/0/1/1/1/30/0/0/0/0 | 0 | 48153..48175: 4 instructions, 0 samples |
| text_PaintTextLines | 694/612/7/184/149/360/10/49/42/45/86/0/0/0/42 | 585/494/12/195/126/348/8/37/15/15/222/0/0/8/37 | 0 | 48675..51872: 609 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 25 | 54869..54891: 4 instructions, 5 samples |
| paint_queue_PaintOrder | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 57421..58654: 247 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 59585..61520: 421 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 0 | 61687..61709: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 0 | 62216..62238: 4 instructions, 0 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 1 | 64504..65290: 209 instructions, 1 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 0 | 65743..66974: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 10 | 67613..67635: 4 instructions, 6 samples |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 1 | none |
| tree_TreeBuildNewIndex | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 6 | 68270..68289: 4 instructions, 3 samples |
| tree_TreeExplicitExists | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 80/21/2/2/2/20/10/5/3/2/0/3/0/3/2 | 5 | 69744..69763: 4 instructions, 4 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 11 | 70763..71377: 113 instructions, 9 samples |
| text_DefaultTextFace | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/7/0/0/0/1/0/0/0/0/0 | 0 | 71765..71784: 4 instructions, 0 samples |
| text_TextFaceFor | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 38/0/0/9/5/17/0/0/0/1/10/0/0/1/0 | 0 | 72025..72044: 4 instructions, 0 samples |
| text_behavior_ResolveTextStyle | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 79/58/6/8/3/19/2/14/6/1/11/0/0/0/3 | 0 | 72408..72795: 100 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 14/0/0/0/0/4/0/3/1/0/7/0/0/0/0 | 0 | 73818..73923: 27 instructions, 0 samples |
| text_behavior_TextWrapPolicy | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 721/657/29/136/252/457/20/70/41/26/24/12/1/0/61 | 264/212/25/12/8/101/33/43/17/3/11/2/0/14/30 | 0 | 74275..77801: 622 instructions, 0 samples |
| text_behavior_TextExtent | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 13/0/0/0/0/0/0/2/2/0/0/2/0/0/0 | 0 | 78101..78171: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 4 | 78539..78561: 4 instructions, 3 samples |
| geometry_WorldToScreen | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 42/0/0/8/3/11/0/0/0/2/35/0/0/0/0 | 0 | none |
| tree_TreeEnsureSemantic | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 14 | 78933..78952: 4 instructions, 10 samples |
| tree_TreeStoreLabel | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 12 | 80155..82307: 456 instructions, 11 samples |
| text_behavior_TextAlignmentOffset | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 18/0/0/0/0/3/1/2/2/0/9/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 31/8/1/0/0/1/0/4/3/1/0/0/0/0/1 | 0 | 82621..82680: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_TextEllipsisStart | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 49/33/3/2/2/8/2/6/3/1/0/0/0/0/5 | 0 | 82884..83209: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 15/0/0/0/0/1/1/1/1/0/0/0/0/2/0 | 0 | none |
| text_PaintTextLineRuns | 343/323/7/109/109/237/12/16/9/13/36/0/0/0/14 | 280/230/5/89/52/150/5/9/4/11/101/2/0/4/22 | 0 | 83429..85250: 323 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 0 | 86009..86031: 4 instructions, 0 samples |
| text_PaintTextEmphasis | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 239/187/5/55/36/103/8/16/11/6/37/2/0/7/21 | 0 | 86691..87599: 172 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 9/0/0/0/0/2/0/0/0/0/8/0/0/0/0 | 0 | 87766..87834: 17 instructions, 0 samples |
| text_PaintTextRule | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 131/0/0/27/26/54/0/4/2/6/72/0/0/0/0 | 0 | 88005..88027: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 89281..89291: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 0 | 89421..89440: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | 89966..89985: 4 instructions, 0 samples |
| tree_TreeLayerAt | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 8 | 90151..90170: 4 instructions, 5 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 278/259/112/0/0/136/10/118/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 27/0/0/4/2/12/1/1/1/1/8/0/0/0/0 | 0 | 91681..91703: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 0 | 91966..91988: 4 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 352/318/8/47/23/84/6/28/20/6/12/0/0/13/29 | 0 | 92594..93719: 280 instructions, 0 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/2/0/2/1/0/0/1/0/0/1 | 0 | 93758..93810: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 25/21/6/1/0/10/0/9/3/0/1/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 249/141/4/0/0/0/0/21/17/12/102/0/0/0/27 | 95/49/3/0/0/28/2/16/11/0/55/0/0/0/0 | 0 | 94552..95175: 137 instructions, 0 samples |
| std_math_CosineDegrees | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/1/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | 95393..95412: 4 instructions, 0 samples |
| tree_TreeSetLabelSpan | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | 95664..95683: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 | 0 | 95891..96053: 47 instructions, 0 samples |
| text_TextLinkRunAt | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 75/52/2/4/0/11/7/11/9/0/0/0/0/3/7 | 0 | 96151..96279: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 21/0/0/0/0/1/0/0/0/3/0/2/0/4/0 | 0 | none |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 97890..97912: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 0 | 98323..102771: 836 instructions, 0 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 32/13/3/0/0/3/2/4/1/0/0/0/0/2/3 | 0 | none |
| paint_queue_PaintClippedEmphasis | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 45/0/0/4/16/23/0/0/0/1/6/0/0/0/0 | 0 | 103857..103879: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 0 | 105074..105096: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 44/0/0/0/7/10/0/0/0/1/18/0/0/2/0 | 0 | 106594..106616: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 1 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | 117372..117391: 4 instructions, 0 samples |
| text_layout_TextTokenNext | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 259/237/31/6/4/46/10/62/31/6/0/0/0/4/53 | 0 | 117701..118825: 277 instructions, 0 samples |
| drawing_props_StyleTokenColorOr | 92/84/5/10/5/33/9/13/8/5/4/3/0/0/18 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 119073..119278: 49 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 120526..120604: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 120949..122021: 253 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 122741..122763: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 14 | 123912..123934: 4 instructions, 9 samples |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 1 | 124456..124715: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 0 | 124760..124782: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 125384..125406: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/4/11/0/0/0/1/0/0/0/0/0 | 0 | 126219..126241: 4 instructions, 0 samples |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 0 | 127819..127838: 4 instructions, 0 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 102/89/15/0/0/18/1/28/13/5/0/0/0/0/20 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 129779..129824: 12 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 178/148/12/9/11/52/15/26/14/4/21/0/0/6/20 | 0 | 130913..131504: 132 instructions, 0 samples |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 132256..132949: 158 instructions, 0 samples |
| shape_paint_EmitShape | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 25/0/0/4/0/8/1/11/7/0/3/0/0/0/0 | 0 | 133163..133185: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 84/12/3/12/21/65/6/4/1/2/6/0/0/0/3 | 11/0/0/4/0/5/0/0/0/1/0/0/0/0/0 | 0 | 133611..133633: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 134375..134394: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 134844..134863: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 0 | 135785..135804: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 0 | 137566..137585: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 138358..138374: 4 instructions, 0 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 24/8/2/0/0/6/0/4/2/0/0/0/0/0/2 | 0 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 114/105/18/0/0/12/2/28/10/0/0/0/0/0/37 | 0 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 36/19/2/0/0/4/1/7/5/0/0/0/0/0/6 | 0 | none |
| paint_queue_KeepShortPaintBytes | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 141336..141590: 64 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 141686..141950: 66 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 142046..142280: 58 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 142501..142696: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 142773..143784: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 143946..144323: 96 instructions, 0 samples |
| shape_paint_EmitLineShape | 180/12/3/42/40/132/13/7/4/12/11/0/0/0/3 | 102/16/1/19/0/39/5/5/4/5/44/0/0/2/1 | 0 | 144408..144430: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 228/121/6/50/41/93/6/22/16/17/61/0/0/0/16 | 176/124/9/25/14/65/1/20/11/8/53/0/0/3/14 | 0 | 145797..146281: 113 instructions, 0 samples |
| shape_paint_EmitRingShape | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 108/82/9/20/6/42/1/14/5/8/45/0/0/0/9 | 0 | 147043..147221: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 37/12/1/7/0/19/1/3/2/2/9/0/0/0/3 | 0 | 147268..147290: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 147845..150221: 435 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 150413..150460: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 150850..151888: 211 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 152080..152127: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 152517..153555: 211 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 153747..153794: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| shape_paint_ShapeAt | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 5/0/0/1/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 96/0/0/8/6/19/0/5/5/3/63/0/0/2/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 96/8/2/15/8/32/4/13/11/4/24/0/0/0/2 | 31/20/3/0/0/6/0/5/2/0/18/0/0/2/2 | 0 | 155374..155393: 4 instructions, 0 samples |
| shape_paint_PointsBounds | 127/109/5/22/20/50/1/11/6/5/45/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 | 0 | 155937..156283: 87 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 395/55/8/85/75/211/20/37/29/29/78/0/0/0/10 | 215/178/9/33/33/78/6/16/6/12/113/0/0/2/11 | 0 | 157900..158025: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 86/16/4/6/18/48/9/10/6/4/8/0/0/0/4 | 19/0/0/0/0/6/0/2/2/1/0/0/0/0/0 | 0 | 158407..158426: 4 instructions, 0 samples |
| shape_paint_ShapeArcDirections | 72/56/2/10/12/30/3/7/5/3/25/0/0/0/5 | 48/18/1/4/5/15/3/3/2/2/21/0/0/0/1 | 0 | 158919..159155: 56 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 77/17/1/0/0/3/2/2/1/2/3/2/0/9/3 | 0 | 159219..159660: 116 instructions, 0 samples |
| paint_queue_GradientColorAt | 72/53/2/13/10/24/0/5/3/6/18/0/0/0/4 | 153/120/12/0/0/25/9/29/13/0/73/0/0/6/14 | 0 | 159786..159994: 52 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 35/15/1/3/3/11/2/2/1/1/17/0/0/0/1 | 0 | 160043..160177: 35 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 59/36/1/7/5/15/1/2/1/2/37/0/0/0/1 | 0 | 160221..160456: 59 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 160573..160691: 32 instructions, 0 samples |
| std_math_Sqrt | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 100/80/5/0/0/24/1/17/11/0/57/0/0/2/10 | 0 | 160930..161329: 98 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 127/80/3/13/12/54/9/12/9/7/16/0/0/0/15 | 79/36/3/2/2/20/7/8/5/2/28/0/0/0/5 | 0 | 161606..161927: 76 instructions, 0 samples |
| shape_paint_ShapeClipSide | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 272/247/28/13/7/60/14/61/33/0/111/0/0/1/21 | 0 | 162266..162581: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 162872..162881: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/3/1/0/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 33/0/0/0/0/7/3/4/3/0/17/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcPoint | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/8/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| std_math_Finite64 | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 10/0/0/0/0/2/0/2/2/0/4/0/0/0/0 | 0 | none |
| std_math_MathSplit | 176/162/16/3/5/10/0/25/9/6/43/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 163625..164292: 151 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 74/48/7/0/0/15/4/17/10/0/21/0/0/0/6 | 0 | 164419..164780: 87 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/20/0/0/1/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 16/0/0/0/0/1/1/3/3/0/4/0/0/1/0 | 0 | none |
| shape_paint_ShapeCrossing | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 44/20/1/0/0/2/1/6/5/0/30/0/0/2/1 | 0 | none |
| widget_benchmark_host_RasterTriangles | 51/36/1/6/1/18/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/9/2/4/1/0/6/0/0/1/2 | 0 | 165526..165669: 36 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 165716..165818: 24 instructions, 0 samples |

## Text / clang-19
Native SHA-256: cfe73702ef0c9437e3bbef42642508ee8bc05011afe57bb2ebbe57d77dfe1999
C SHA-256: 1843fc432bba8da9d944fbc07ff6f1c5107586deae3ebb97aff7ec269525218a
ZX SHA-256: 743b12e43fe1f1e37f3cdf63d5336ff63049d0d484b8776afa90a7d1c1a08a4d
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 286/1

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 67..138: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 285/159/9/34/82/140/3/30/21/16/47/0/0/0/27 | 155/66/6/12/15/54/0/20/14/14/19/0/0/1/12 | 0 | 902..1583: 158 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 0 | 2503..2522: 4 instructions, 0 samples |
| text___zi_spec_6a19f7c98e79a734 | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | 3281..3303: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 1 | 3707..3726: 4 instructions, 1 samples |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3844..3863: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 4829..6388: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 6585..6604: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 6858..7010: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 7090..7200: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 8106..8149: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 379/237/17/42/15/132/43/58/41/21/26/4/0/5/35 | 0 | 9257..11914: 556 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 5 | 13519..13541: 4 instructions, 5 samples |
| tree_TreeFrameStatus | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 4 | 14140..14159: 4 instructions, 4 samples |
| text_DrawSession | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 293/19/2/36/34/100/0/25/23/20/108/1/0/2/2 | 0 | 15422..16047: 121 instructions, 0 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 17066..20675: 637 instructions, 0 samples |
| paint_queue_PaintFlush | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 0 | 20781..21273: 98 instructions, 0 samples |
| tree_TreeRead | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 17 | 22823..22845: 4 instructions, 7 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 26325..26638: 80 instructions, 0 samples |
| tree_TreeTextRange | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 27313..27495: 37 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 28258..28522: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | 28672..28691: 4 instructions, 0 samples |
| tree_TreeInputAt | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 29346..29365: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 30255..30521: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 | 0 | none |
| cursor_CursorHoverFrameStart | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 10 | 32037..33263: 242 instructions, 8 samples |
| tree_TreeSubmit | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 63 | 35558..40484: 889 instructions, 32 samples |
| std_math_Finite32 | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| tree_TreeRootBounds | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 0 | 40783..40802: 4 instructions, 0 samples |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 48/0/0/12/9/28/0/2/2/2/12/0/0/1/0 | 0 | 41420..41442: 4 instructions, 0 samples |
| text_PrepareText | 263/81/2/62/65/169/0/13/11/15/16/0/0/0/2 | 134/18/1/13/2/44/0/6/5/14/18/0/0/3/3 | 0 | 42603..42797: 45 instructions, 0 samples |
| tree_TreeIsBuilding | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 7 | 42885..42904: 4 instructions, 7 samples |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 209/114/8/44/72/120/1/16/8/6/94/0/0/0/6 | 124/0/0/28/18/48/0/0/0/10/111/0/0/0/0 | 0 | 43581..44139: 109 instructions, 0 samples |
| text_TextOrigin | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| tree_TreeSetLabel | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 11 | 44358..44377: 4 instructions, 3 samples |
| tree_TreeSetStyleClass | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 12 | 45259..45281: 4 instructions, 7 samples |
| tree_TreeSetStyleId | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 3 | 45833..45855: 4 instructions, 2 samples |
| tree_TreeSetSemantic | 171/107/6/18/24/80/14/20/14/10/8/6/0/0/21 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 11 | 46545..47008: 98 instructions, 5 samples |
| text_TextHiddenLines | 118/39/2/11/2/22/0/11/9/10/22/0/0/0/4 | 112/19/2/2/1/17/1/11/9/3/51/1/0/0/2 | 0 | 47391..47478: 26 instructions, 0 samples |
| tree_TreeSetHiddenLines | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | 47583..47602: 4 instructions, 0 samples |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 | 0 | 48153..48175: 4 instructions, 0 samples |
| text_PaintTextLines | 694/612/7/184/149/360/10/49/42/45/86/0/0/0/42 | 490/418/9/71/66/235/5/41/32/23/164/0/0/10/38 | 1 | 48675..51872: 609 instructions, 1 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 28 | 54712..54734: 4 instructions, 5 samples |
| paint_queue_PaintOrder | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 57421..58654: 247 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 59585..61520: 421 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 61687..61709: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 0 | 62216..62238: 4 instructions, 0 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 64504..65290: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 65743..66974: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 8 | 67102..67121: 4 instructions, 5 samples |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 7 | 68270..68289: 4 instructions, 4 samples |
| tree_TreeExplicitExists | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 8 | 69861..70389: 89 instructions, 5 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 22 | 70763..71377: 113 instructions, 15 samples |
| text_DefaultTextFace | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 | 0 | 71765..71784: 4 instructions, 0 samples |
| text_TextFaceFor | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 | 0 | 72025..72044: 4 instructions, 0 samples |
| text_behavior_ResolveTextStyle | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 57/0/0/5/0/8/0/3/3/2/7/0/0/2/0 | 0 | 72408..72795: 100 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | 73818..73923: 27 instructions, 0 samples |
| text_behavior_TextWrapPolicy | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 721/657/29/136/252/457/20/70/41/26/24/12/1/0/61 | 345/109/17/14/7/108/42/57/40/5/19/10/0/13/13 | 0 | 74275..77801: 622 instructions, 0 samples |
| text_behavior_TextExtent | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 | 0 | 78101..78171: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 6 | 78539..78561: 4 instructions, 3 samples |
| geometry_WorldToScreen | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 | 0 | none |
| tree_TreeEnsureSemantic | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 15 | 78933..78952: 4 instructions, 11 samples |
| tree_TreeStoreLabel | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 19 | 80155..82307: 456 instructions, 16 samples |
| text_behavior_TextAlignmentOffset | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 | 0 | 82621..82680: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_TextEllipsisStart | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 55/23/3/2/2/9/3/7/4/1/0/0/0/0/2 | 0 | 82884..83209: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 | 0 | none |
| text_PaintTextLineRuns | 343/323/7/109/109/237/12/16/9/13/36/0/0/0/14 | 301/250/14/75/45/157/7/25/11/16/80/2/0/1/20 | 0 | 83429..85250: 323 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 0 | 86009..86031: 4 instructions, 0 samples |
| text_PaintTextEmphasis | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 221/165/17/28/27/87/10/29/12/5/13/2/0/3/17 | 0 | 86691..87599: 172 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 | 0 | 87766..87834: 17 instructions, 0 samples |
| text_PaintTextRule | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 101/0/0/26/22/52/0/4/2/6/51/0/0/0/0 | 0 | 88005..88027: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 89281..89291: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 0 | 89421..89440: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | 89966..89985: 4 instructions, 0 samples |
| tree_TreeLayerAt | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 11 | 90151..90170: 4 instructions, 8 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 | 0 | 91681..91703: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 0 | 91966..91988: 4 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 | 0 | 92594..93719: 280 instructions, 0 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 45/22/2/0/0/7/5/6/4/0/0/5/0/0/1 | 0 | 93758..93810: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 249/141/4/0/0/0/0/21/17/12/102/0/0/0/27 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 | 0 | 94552..95175: 137 instructions, 0 samples |
| std_math_CosineDegrees | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 80/0/0/0/0/24/0/12/12/1/63/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | 95393..95412: 4 instructions, 0 samples |
| tree_TreeSetLabelSpan | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | 95664..95683: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 0 | 95891..96053: 47 instructions, 0 samples |
| text_TextLinkRunAt | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 57/27/7/2/0/10/5/14/7/0/0/0/0/0/2 | 0 | 96151..96279: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 | 0 | none |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 97890..97912: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 1 | 98323..102771: 836 instructions, 1 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 29/0/0/0/0/2/2/3/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_PaintClippedEmphasis | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 | 0 | 103857..103879: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 105074..105096: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 | 0 | 106594..106616: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 0 | 117372..117391: 4 instructions, 0 samples |
| text_layout_TextTokenNext | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 210/116/20/4/2/22/7/53/33/3/0/0/0/3/34 | 1 | 117701..118825: 277 instructions, 1 samples |
| drawing_props_StyleTokenColorOr | 92/84/5/10/5/33/9/13/8/5/4/3/0/0/18 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 119073..119278: 49 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 120526..120604: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 120949..122021: 253 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 122741..122763: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 14 | 123912..123934: 4 instructions, 6 samples |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 0 | 124456..124715: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 124760..124782: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 125384..125406: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 | 0 | 126219..126241: 4 instructions, 0 samples |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 0 | 127819..127838: 4 instructions, 0 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 82/46/12/0/0/8/0/24/12/1/0/0/0/0/16 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 129779..129824: 12 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 130913..131504: 132 instructions, 0 samples |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 132256..132949: 158 instructions, 0 samples |
| shape_paint_EmitShape | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 | 0 | 133163..133185: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 84/12/3/12/21/65/6/4/1/2/6/0/0/0/3 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 | 0 | 133611..133633: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 134375..134394: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 134844..134863: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 0 | 135785..135804: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 0 | 137566..137585: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 138358..138374: 4 instructions, 0 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 0 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/10/7/0/0/0/0/0/7 | 1 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 | 0 | none |
| paint_queue_KeepShortPaintBytes | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 141336..141590: 64 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 141686..141950: 66 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 142046..142280: 58 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 142501..142696: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 142773..143784: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 143946..144323: 96 instructions, 0 samples |
| shape_paint_EmitLineShape | 180/12/3/42/40/132/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 | 0 | 144408..144430: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 228/121/6/50/41/93/6/22/16/17/61/0/0/0/16 | 280/234/11/36/19/99/9/21/10/10/108/2/0/8/25 | 0 | 145797..146281: 113 instructions, 0 samples |
| shape_paint_EmitRingShape | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 182/57/3/32/13/80/9/21/18/9/103/0/0/0/0 | 0 | 147043..147221: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 | 0 | 147268..147290: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 147845..150221: 435 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 150413..150460: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 150850..151888: 211 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 152080..152127: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 152517..153555: 211 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 153747..153794: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| shape_paint_ShapeAt | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 96/8/2/15/8/32/4/13/11/4/24/0/0/0/2 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 | 0 | 155374..155393: 4 instructions, 0 samples |
| shape_paint_PointsBounds | 127/109/5/22/20/50/1/11/6/5/45/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/3 | 0 | 155937..156283: 87 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 395/55/8/85/75/211/20/37/29/29/78/0/0/0/10 | 567/481/23/63/91/187/30/86/62/10/307/0/0/1/48 | 0 | 157900..158025: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 86/16/4/6/18/48/9/10/6/4/8/0/0/0/4 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 | 0 | 158407..158426: 4 instructions, 0 samples |
| shape_paint_ShapeArcDirections | 72/56/2/10/12/30/3/7/5/3/25/0/0/0/5 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 | 0 | 158919..159155: 56 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 | 0 | 159219..159660: 116 instructions, 0 samples |
| paint_queue_GradientColorAt | 72/53/2/13/10/24/0/5/3/6/18/0/0/0/4 | 143/0/0/0/0/17/0/16/16/1/72/0/0/13/0 | 0 | 159786..159994: 52 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 | 0 | 160043..160177: 35 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 | 0 | 160221..160456: 59 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 160573..160691: 32 instructions, 0 samples |
| std_math_Sqrt | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 228/43/7/0/0/40/6/32/25/0/139/0/0/2/6 | 0 | 160930..161329: 98 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 127/80/3/13/12/54/9/12/9/7/16/0/0/0/15 | 63/0/0/6/4/21/3/6/6/2/29/0/0/0/0 | 0 | 161606..161927: 76 instructions, 0 samples |
| shape_paint_ShapeClipSide | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 | 0 | 162266..162581: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 162872..162881: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 | 0 | none |
| shape_paint_ShapeArcPoint | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 | 0 | none |
| std_math_Finite64 | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| std_math_MathSplit | 176/162/16/3/5/10/0/25/9/6/43/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 163625..164292: 151 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 89/35/5/0/0/15/4/22/17/0/34/0/0/1/4 | 0 | 164419..164780: 87 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 | 0 | none |
| shape_paint_ShapeCrossing | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTriangles | 51/36/1/6/1/18/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 | 0 | 165526..165669: 36 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 165716..165818: 24 instructions, 0 samples |

## Input / cc
Native SHA-256: 6d861ddb68dfd5c44cdea2b20fb88797fd9f4919e41374efd59596c69d95483a
C SHA-256: c77c158bdc0e61b8c246fb0fb2efd8e62a5f537c6e0ef7fc7e573b897bd6d311
ZX SHA-256: 5c47cb8218aeed347661abe511c183dca5e1278e852b5791595e3dcbe7f4b75e
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 391/1

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 92..163: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 164/152/15/12/15/57/0/17/2/11/8/0/0/0/42 | 0 | 974..996: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 0 | 3901..3920: 4 instructions, 0 samples |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 195/136/7/50/17/84/3/18/11/9/16/0/0/3/19 | 0 | 4680..4702: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_input_TreePointerUpdate | 621/68/17/58/91/370/80/53/36/30/63/46/0/0/17 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 | 7 | 7056..7078: 4 instructions, 6 samples |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 9349..9368: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 10334..11893: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 12090..12109: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 12363..12515: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 12595..12705: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 1 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 13611..13654: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 14762..17419: 556 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 1 | 19024..19046: 4 instructions, 1 samples |
| tree_TreeFrameStatus | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 3 | 19645..19664: 4 instructions, 3 samples |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 7/0/0/2/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/1/1/0/0/0/0/0/0 | 0 | 19843..19981: 38 instructions, 0 samples |
| text_context_menu_TextMenuTakeCommand | 161/110/7/25/14/55/0/31/24/14/0/0/0/0/18 | 66/46/6/2/0/14/2/13/7/3/0/0/0/0/6 | 0 | 20212..20642: 108 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 63/22/1/6/5/13/1/4/3/4/11/0/0/1/1 | 0 | none |
| textfield_TextFieldCore | 3950/3778/52/857/1164/2342/28/400/348/251/279/1/1/0/265 | 2425/2344/104/678/397/1177/25/259/140/150/359/2/0/15/320 | 0 | 21927..41528: 3694 instructions, 0 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 89/0/0/19/20/59/4/3/3/6/0/4/0/0/0 | 54/20/3/0/0/19/6/5/2/3/0/0/0/0/3 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 52/0/0/4/4/18/0/12/12/3/0/0/0/0/0 | 18/4/1/0/0/4/0/4/3/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 17/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 43682..47291: 637 instructions, 0 samples |
| paint_queue_PaintFlush | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 0 | 47397..47889: 98 instructions, 0 samples |
| tree_TreeIsBuilding | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 14 | 48085..48104: 4 instructions, 14 samples |
| tree_input_TreeHitAt | 492/390/12/63/79/240/19/77/65/38/22/7/0/0/66 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 | 10 | 48838..50149: 256 instructions, 4 samples |
| tree_input_TreeTextFocused | 147/107/7/5/22/68/17/19/12/7/10/5/0/0/22 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 | 6 | 50869..51433: 103 instructions, 5 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| tree_TreeInputAt | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 9 | 52520..52542: 4 instructions, 5 samples |
| tree_TreeRead | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 6 | 55564..55586: 4 instructions, 2 samples |
| tree_TreeCount | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 2 | 57718..57737: 4 instructions, 2 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 57930..58243: 80 instructions, 0 samples |
| tree_TreeTextRange | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 58918..59100: 37 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | 59483..59747: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 1 | 60028..60050: 4 instructions, 1 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 60543..60809: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 | 12 | 62325..63551: 242 instructions, 9 samples |
| tree_TreeSubmit | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 56 | 65846..70772: 889 instructions, 32 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/1/1/0/0/0/0/0/0 | 0 | none |
| system_clipboard_SystemClipboardText | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 73/46/8/2/1/11/6/14/6/2/0/0/0/0/6 | 0 | 71011..71567: 140 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 | 0 | none |
| text_context_menu_TextMenuOpenAt | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 61/12/2/3/0/16/2/6/4/2/2/0/0/0/2 | 0 | 72055..72271: 51 instructions, 0 samples |
| tree_TreePrevious | 128/29/4/12/19/59/10/14/10/7/6/4/0/0/3 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 | 10 | 72376..72395: 4 instructions, 4 samples |
| tree_TreeCurrentParent | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 10 | 72984..73003: 4 instructions, 5 samples |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakeDragAt | 443/56/14/55/84/259/49/47/33/17/45/21/0/0/14 | 173/148/11/13/4/58/10/25/14/6/22/0/0/8/20 | 0 | 73707..73726: 4 instructions, 0 samples |
| tree_TreePreviousWidgetState | 171/20/5/19/31/104/16/15/10/6/10/6/0/0/5 | 48/0/0/0/0/17/3/4/4/1/2/1/0/1/0 | 2 | 75831..75850: 4 instructions, 1 samples |
| tree_input_TreeHasFocus | 140/98/6/13/20/61/12/18/12/9/6/3/0/0/19 | 71/45/3/5/4/20/7/9/6/3/0/1/0/0/7 | 9 | 76827..77285: 94 instructions, 6 samples |
| tree_input_TreeBlurAt | 98/12/3/5/15/46/11/15/12/6/6/3/0/0/3 | 37/7/1/0/0/9/4/5/4/1/0/1/0/1/2 | 0 | 77412..77431: 4 instructions, 0 samples |
| textfield_TextFieldFaceFor | 179/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 121/74/1/30/15/63/1/2/1/10/52/0/0/0/4 | 0 | 77861..78834: 167 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 14/0/0/0/0/2/0/3/1/0/7/0/0/0/0 | 0 | 79220..79355: 38 instructions, 0 samples |
| textfield_TextFieldSecureDisplay | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 49/20/2/1/0/5/1/5/3/2/0/0/0/0/3 | 0 | 79450..79577: 36 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 61/0/0/0/0/5/2/6/2/0/7/0/0/2/0 | 0 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 66/38/2/8/10/20/1/4/2/3/0/0/0/0/2 | 0 | 80043..80302: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 37/0/0/0/0/6/0/6/3/0/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 29/7/1/0/0/5/2/2/1/1/0/0/0/0/3 | 0 | 80613..80676: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 39/21/2/0/0/2/1/5/3/1/0/0/0/0/2 | 0 | 80819..80962: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 159/126/11/16/29/59/8/28/17/0/23/0/0/0/13 | 0 | 81840..82347: 113 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 15/11/1/2/3/6/1/2/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 21/8/1/0/0/6/3/5/4/0/0/0/0/0/3 | 0 | 82595..82672: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 133/98/8/12/3/35/9/24/16/4/0/0/0/0/9 | 0 | 82887..83032: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/88/8/4/3/13/4/18/10/0/0/0/0/5/8 | 0 | 83456..84117: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 106/75/9/16/12/37/1/14/5/5/12/0/0/0/6 | 0 | 84326..84348: 4 instructions, 0 samples |
| text_request_TextRequestTake | 253/28/7/53/42/148/24/24/17/12/14/9/0/0/7 | 79/0/0/4/3/24/4/8/8/4/0/1/0/0/0 | 0 | 85147..85166: 4 instructions, 0 samples |
| text_request_TextRequestChangeFor | 280/263/12/53/38/116/5/36/24/13/0/0/0/0/24 | 160/128/12/20/5/57/8/24/12/6/0/0/0/0/9 | 0 | 86336..87226: 238 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 67/50/5/0/0/7/3/8/3/4/0/0/0/0/5 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 34/0/0/0/0/5/1/2/2/1/0/0/0/0/0 | 0 | 87421..87650: 60 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 25/0/0/0/0/2/2/1/1/0/0/0/0/0/0 | 0 | 87682..87852: 52 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 48/0/0/2/2/9/2/4/3/1/4/2/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 171/20/5/21/29/100/18/15/10/7/10/8/0/0/5 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 13 | 88845..88867: 4 instructions, 4 samples |
| tree_TreeSetCaptureTab | 129/12/3/17/21/64/10/14/11/9/6/4/0/0/3 | 49/36/5/0/0/7/1/8/3/4/0/1/0/1/4 | 5 | 89526..89548: 4 instructions, 3 samples |
| tree_input_TreeFocusSubmittedAt | 289/24/6/42/46/139/20/34/28/15/20/7/0/0/6 | 158/139/17/8/16/61/7/23/6/6/25/3/0/5/15 | 0 | 89874..89893: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 272/140/8/40/42/162/26/23/15/8/14/13/0/0/27 | 155/104/7/15/5/48/10/26/19/3/2/4/0/3/14 | 0 | 92081..92664: 120 instructions, 0 samples |
| tree_TreeSetSemantic | 171/107/6/18/24/80/14/20/14/10/8/6/0/0/21 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 | 10 | 92776..92795: 4 instructions, 4 samples |
| tree_TreeSetLabel | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 12 | 93605..93624: 4 instructions, 4 samples |
| tree_TreeSetStyleClass | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 9 | 94373..94392: 4 instructions, 4 samples |
| tree_TreeSetStyleId | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 7 | 95080..95102: 4 instructions, 3 samples |
| tree_TreeSetSelected | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 8 | 95655..95677: 4 instructions, 5 samples |
| tree_TreeSetTextValue | 432/347/16/49/82/273/48/32/16/11/24/23/0/0/59 | 172/64/6/12/9/59/8/28/22/4/0/3/0/1/8 | 15 | 96768..98368: 286 instructions, 9 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 36/0/0/5/0/8/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 0 | 99215..99237: 4 instructions, 0 samples |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 | 0 | 100786..100808: 4 instructions, 0 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 179/0/0/22/20/53/0/13/13/12/47/1/1/0/0 | 120/0/0/2/0/13/2/13/5/0/41/0/0/4/0 | 0 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 17/0/0/0/0/3/0/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 17/0/0/0/0/3/0/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 27/0/0/0/0/3/0/4/2/0/6/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 0 | 102905..102927: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 28/0/0/0/0/4/2/4/2/0/7/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 103522..103539: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 103569..103623: 18 instructions, 0 samples |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 | 0 | 104351..104373: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 104871..104890: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 104969..104991: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 18/6/1/0/0/6/2/3/2/0/0/0/0/0/1 | 0 | 105024..105069: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 102/0/0/21/23/69/6/3/3/5/0/6/0/0/0 | 53/17/2/0/0/14/5/4/2/3/0/0/0/0/1 | 0 | none |
| text_input_platform_TextInputFocused | 83/0/0/15/20/50/3/5/5/5/0/3/0/0/0 | 38/25/4/0/0/8/3/5/1/3/0/0/0/0/1 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 13/6/1/0/0/1/0/3/1/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 68/21/1/8/5/27/6/4/3/3/0/6/0/0/6 | 49/6/1/0/0/10/5/3/2/1/0/0/0/2/4 | 0 | 106045..106134: 21 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 152/138/6/8/31/79/3/13/7/4/21/0/0/2/33 | 0 | 106507..109008: 460 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 21 | 114014..114036: 4 instructions, 3 samples |
| paint_queue_PaintOrder | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 114540..115773: 247 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 116704..118639: 421 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 0 | 118806..118828: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 0 | 119335..119357: 4 instructions, 0 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 0 | 121911..122697: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 2 | 123150..124381: 302 instructions, 1 samples |
| tree_TreeParentGeneration | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 8 | 124509..124528: 4 instructions, 5 samples |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 8 | 125677..125696: 4 instructions, 6 samples |
| tree_TreeExplicitExists | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 82/21/2/2/2/22/12/5/3/2/0/3/0/3/2 | 13 | 127268..127796: 89 instructions, 9 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 25 | 128170..128784: 113 instructions, 19 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 11/0/0/0/0/4/2/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 205/20/5/34/39/112/16/25/20/12/17/6/0/0/5 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 | 0 | 129374..129390: 4 instructions, 0 samples |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 63/0/0/10/13/35/0/0/0/6/26/0/0/0/0 | 0 | 130522..130541: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 293/260/17/62/47/150/26/36/19/11/16/6/0/0/40 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 | 0 | 130727..132052: 258 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 | 0 | 132178..132340: 47 instructions, 0 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/2/0/5/4/4/5/1/0/0/0 | 18/0/0/0/0/2/0/3/1/0/4/0/0/0/0 | 0 | 132489..132498: 5 instructions, 0 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 132590..132670: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/15/2/0/0/2/2/6/4/0/0/0/0/0/3 | 0 | 132723..132787: 17 instructions, 0 samples |
| std_grapheme_Previous | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 132897..133040: 41 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 201/93/5/30/37/121/16/13/8/5/8/9/0/0/18 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 | 9 | 133698..134091: 81 instructions, 5 samples |
| tree_TreeLayerAt | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 22 | 134210..134229: 4 instructions, 13 samples |
| tree_TreeEnsureSemantic | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 14 | 135001..135020: 4 instructions, 12 samples |
| tree_TreeStoreLabel | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 14 | 136223..138375: 456 instructions, 13 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 0 | 138585..138607: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 139209..139231: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 1 | 139460..139482: 4 instructions, 1 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 132/113/2/24/6/35/0/11/8/2/63/0/0/6/11 | 0 | 144240..144262: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 24/0/0/0/0/2/0/3/1/0/4/0/0/0/0 | 0 | 144819..145025: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 0 | 145098..145120: 4 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 146362..146384: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 147113..147708: 157 instructions, 0 samples |
| button_SubmitButton | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 | 0 | 147812..150634: 584 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 1160/1125/36/268/330/740/28/93/57/58/87/16/0/0/95 | 570/548/32/102/94/285/46/65/33/18/103/1/0/4/61 | 0 | 150913..156437: 1084 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 156965..156975: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 0 | 157105..157124: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 5 | 157650..157669: 4 instructions, 5 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 278/259/112/0/0/136/10/118/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 | 0 | 158930..159278: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 | 0 | 160969..160991: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | 161189..161208: 4 instructions, 0 samples |
| tree_TreeSetLabelSpan | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | 161460..161479: 4 instructions, 0 samples |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 162321..163014: 158 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 163161..164233: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 164916..164938: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 11 | 165727..165746: 4 instructions, 4 samples |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 0 | 166631..166890: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | 167008..167027: 4 instructions, 0 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 168474..168552: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 83/12/3/8/17/47/9/5/2/4/6/3/0/0/3 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 | 0 | 169342..169358: 4 instructions, 0 samples |
| tree_input_TreeHoveredAt | 145/20/5/12/26/72/15/19/14/7/10/5/0/0/5 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 | 0 | 169749..169768: 4 instructions, 0 samples |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 266/172/4/89/71/181/2/9/5/15/144/0/0/0/7 | 0 | 170400..170422: 4 instructions, 0 samples |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 | 0 | none |
| tree_TreeRootBounds | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | 171379..171398: 4 instructions, 0 samples |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 | 0 | 172036..173334: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 | 0 | 173848..175162: 284 instructions, 0 samples |
| tree_TreeSetCircleHit | 140/20/5/11/24/76/16/15/10/6/10/6/0/0/5 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 | 0 | 175316..175335: 4 instructions, 0 samples |
| tree_TreeSetSemanticDetail | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | 176078..176097: 4 instructions, 0 samples |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 | 0 | 176937..183576: 1342 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 26/12/1/0/0/2/1/2/1/2/0/0/0/0/1 | 0 | none |
| tree_TreeNextKey | 143/54/7/9/29/84/18/12/4/5/12/6/0/0/12 | 65/9/1/0/0/11/5/5/4/2/0/5/0/4/1 | 0 | 184336..184568: 38 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 108/12/3/12/19/53/9/12/9/6/6/3/0/0/3 | 46/9/1/0/0/6/2/4/3/2/0/1/0/1/1 | 0 | 184735..184754: 4 instructions, 0 samples |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 8/0/0/0/0/4/3/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 74/49/1/7/14/37/2/5/4/3/0/2/0/0/4 | 25/7/2/0/0/7/3/6/4/0/0/0/0/0/2 | 0 | 185216..185416: 49 instructions, 0 samples |
| contextmenu_ContextMenuFind | 68/51/1/7/14/37/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/2/5/3/0/0/0/0/0/3 | 0 | 185485..185691: 51 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 129/43/1/33/33/78/1/3/2/6/46/1/0/0/1 | 85/0/0/11/8/21/2/2/1/4/48/0/0/0/0 | 0 | 186187..186376: 43 instructions, 0 samples |
| overlay_Draw | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 257/214/9/46/27/82/5/22/13/19/36/0/0/6/31 | 0 | 186711..188013: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1465/1248/10/377/376/810/10/72/62/142/347/1/0/0/114 | 1082/888/26/288/120/488/12/74/36/102/301/0/0/2/121 | 0 | 189103..195293: 1136 instructions, 0 samples |
| paint_queue_PendingTextValue | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 206831..207026: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 207103..208114: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 208276..208653: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 | 0 | 209755..210346: 132 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 0 | 211768..211787: 4 instructions, 0 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 213471..213516: 12 instructions, 0 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 22/0/0/0/0/4/0/2/0/0/12/0/0/3/0 | 0 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 45/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 0 | 213609..213775: 33 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 | 0 | 214272..215340: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 | 0 | 215385..215542: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 | 0 | 215856..216034: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 | 0 | 216155..216769: 146 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 | 0 | 217113..217135: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 | 0 | 217498..217744: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 | 0 | 218290..218312: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 | 0 | 218524..218682: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 | 0 | 218910..219081: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 | 0 | 219388..220134: 172 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 388/333/6/71/75/167/7/29/23/22/117/4/1/0/40 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 | 0 | 220387..221938: 329 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 | 0 | 222207..222229: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 | 0 | 222871..222893: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 587/550/22/99/131/325/11/48/26/33/63/12/0/0/41 | 261/197/11/38/27/100/8/30/19/19/54/0/0/2/32 | 0 | 223744..225823: 417 instructions, 0 samples |
| dpi_ScalePixels | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 42/0/0/0/0/7/0/7/4/0/19/0/0/0/0 | 0 | 226492..226787: 75 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 169/149/7/25/41/98/4/18/11/9/31/4/0/0/12 | 93/61/7/3/3/21/5/14/7/4/26/0/0/1/11 | 0 | 226886..227599: 147 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/14/1/0/0/1/1/2/1/0/0/0/0/2/4 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/17/1/1/0/4/2/5/4/4/0/0/0/0/1 | 0 | none |
| tree_TreeSetPopup | 115/16/4/6/19/63/14/13/9/5/8/6/0/0/4 | 38/0/0/0/0/8/1/5/5/2/0/1/0/1/0 | 0 | 228267..228286: 4 instructions, 0 samples |
| drawing_props_StyleTokenColorOr | 92/84/5/10/5/33/9/13/8/5/4/3/0/0/18 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 229006..229211: 49 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| tree_TreePushScope | 219/24/6/30/43/121/22/25/19/16/12/9/0/0/6 | 81/64/5/0/0/13/9/9/4/8/0/8/0/0/12 | 0 | 229425..229444: 4 instructions, 0 samples |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/1/1/2/0/1/1/2/2/0/0/0/0 | 0 | 230601..230614: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 26/13/4/0/0/8/2/7/3/0/0/0/0/0/4 | 0 | 230629..230795: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/2/0/4/0/2/2/4/4/0/0/0/0 | 0 | 231045..231054: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 115/0/0/11/28/71/4/9/9/2/0/4/0/0/0 | 24/6/1/0/0/9/5/3/2/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 649/596/8/78/335/450/14/31/23/21/80/0/0/0/35 | 182/92/5/18/42/76/9/11/6/4/78/0/0/1/19 | 0 | 231892..234861: 562 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 235393..235647: 64 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 235743..236007: 66 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 236103..236337: 58 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 236695..236714: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 237164..237183: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 0 | 238105..238124: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 0 | 239886..239905: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 240678..240694: 4 instructions, 0 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 | 0 | 240762..241423: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 | 0 | 242014..242033: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 | 0 | 242739..242898: 43 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 | 0 | 242958..243037: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 246/224/9/35/36/134/27/22/13/11/12/14/0/0/54 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 | 0 | 243153..243917: 139 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 | 0 | 244464..244587: 30 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 240/130/10/33/28/120/24/19/9/3/12/16/1/0/30 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 | 0 | 244861..245195: 70 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 | 0 | 246803..246825: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 230/194/7/10/16/40/7/36/29/13/11/0/6/0/35 | 122/102/6/0/6/38/14/13/5/0/6/4/0/7/26 | 0 | 247656..248304: 193 instructions, 0 samples |
| tree_TreePopScope | 122/12/3/15/21/64/11/13/10/9/6/5/0/0/3 | 54/27/1/2/1/6/3/4/3/4/0/4/0/0/5 | 0 | 248397..248416: 4 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 249057..249175: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 249381..251757: 435 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 251949..251996: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 252386..253424: 211 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 253616..253663: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 254053..255091: 211 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 255283..255330: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 | 0 | 255518..255576: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 | 0 | none |
| svg_shape_ShapeRaster | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 | 0 | 256113..256963: 207 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 345/311/29/30/34/97/9/61/32/9/0/10/1/0/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 | 0 | 257224..258237: 262 instructions, 0 samples |
| image_behavior_ImageFitBounds | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 | 0 | 258399..259101: 177 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 | 0 | 259217..259571: 91 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 259594..259616: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 260025..260047: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 | 0 | 260268..261474: 284 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 | 0 | 261695..263868: 479 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 | 0 | 264299..265694: 332 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 24/0/0/4/4/18/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 19/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 | 0 | 266711..267192: 123 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 | 0 | 267589..270008: 523 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | 270132..270255: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 | 0 | 270293..270524: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 | 0 | 270603..270809: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 | 0 | 270932..271050: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 271861..271877: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 272236..272252: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 | 0 | 272311..272670: 90 instructions, 0 samples |

## Input / clang-19
Native SHA-256: 6d861ddb68dfd5c44cdea2b20fb88797fd9f4919e41374efd59596c69d95483a
C SHA-256: 508d91ebbc6bf9126f61f6932319a62a4847f7cba48106af5b72f913a5b9aceb
ZX SHA-256: 5c47cb8218aeed347661abe511c183dca5e1278e852b5791595e3dcbe7f4b75e
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 540/1

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 92..163: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 161/0/0/13/24/70/0/19/19/12/23/0/0/0/0 | 1 | 974..996: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 0 | 3901..3920: 4 instructions, 0 samples |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 117/0/0/14/4/41/2/10/10/9/2/0/0/0/0 | 0 | 4680..4702: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_input_TreePointerUpdate | 621/68/17/58/91/370/80/53/36/30/63/46/0/0/17 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 | 4 | 7056..7078: 4 instructions, 4 samples |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 9349..9368: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 10334..11893: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 12090..12109: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 12363..12515: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 12595..12705: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 2 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 13611..13654: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 | 0 | 14762..17419: 556 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 3 | 19024..19046: 4 instructions, 3 samples |
| tree_TreeFrameStatus | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 3 | 19645..19664: 4 instructions, 3 samples |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | 19843..19981: 38 instructions, 0 samples |
| text_context_menu_TextMenuTakeCommand | 161/110/7/25/14/55/0/31/24/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 | 0 | 20212..20642: 108 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 85/30/2/7/4/25/2/9/7/6/12/0/0/0/3 | 0 | none |
| textfield_TextFieldCore | 3950/3778/52/857/1164/2342/28/400/348/251/279/1/1/0/265 | 2155/1176/35/302/166/909/19/256/221/148/299/1/0/13/120 | 1 | 21927..41528: 3694 instructions, 1 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 89/0/0/19/20/59/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 52/0/0/4/4/18/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 43682..47291: 637 instructions, 0 samples |
| paint_queue_PaintFlush | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 0 | 47397..47889: 98 instructions, 0 samples |
| tree_TreeIsBuilding | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 13 | 48085..48104: 4 instructions, 13 samples |
| tree_input_TreeHitAt | 492/390/12/63/79/240/19/77/65/38/22/7/0/0/66 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 12 | 48838..50149: 256 instructions, 8 samples |
| tree_input_TreeTextFocused | 147/107/7/5/22/68/17/19/12/7/10/5/0/0/22 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 7 | 50869..51433: 103 instructions, 5 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 1 | none |
| tree_TreeInputAt | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 23 | 52239..52261: 4 instructions, 10 samples |
| tree_TreeRead | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 14 | 53550..53572: 4 instructions, 4 samples |
| tree_TreeCount | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 12 | 57718..57737: 4 instructions, 12 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 57930..58243: 80 instructions, 0 samples |
| tree_TreeTextRange | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 58918..59100: 37 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | 59483..59747: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 3 | 60028..60050: 4 instructions, 2 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 60543..60809: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 26 | 62325..63551: 242 instructions, 19 samples |
| tree_TreeSubmit | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 71 | 65846..70772: 889 instructions, 40 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| system_clipboard_SystemClipboardText | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 65/17/2/0/0/5/1/9/7/2/0/0/0/0/1 | 0 | 71011..71567: 140 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenAt | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 55/14/1/3/0/16/1/5/4/3/2/0/0/0/2 | 0 | 72055..72271: 51 instructions, 0 samples |
| tree_TreePrevious | 128/29/4/12/19/59/10/14/10/7/6/4/0/0/3 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 15 | 72623..72645: 4 instructions, 6 samples |
| tree_TreeCurrentParent | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 13 | 73094..73116: 4 instructions, 6 samples |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 1 | none |
| tree_input_TreeTakeDragAt | 443/56/14/55/84/259/49/47/33/17/45/21/0/0/14 | 155/35/3/8/1/45/10/27/24/6/8/6/0/0/6 | 0 | 73707..73726: 4 instructions, 0 samples |
| tree_TreePreviousWidgetState | 171/20/5/19/31/104/16/15/10/6/10/6/0/0/5 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 | 2 | 75930..75952: 4 instructions, 2 samples |
| tree_input_TreeHasFocus | 140/98/6/13/20/61/12/18/12/9/6/3/0/0/19 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 | 6 | 76827..77285: 94 instructions, 5 samples |
| tree_input_TreeBlurAt | 98/12/3/5/15/46/11/15/12/6/6/3/0/0/3 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 | 0 | 77412..77431: 4 instructions, 0 samples |
| textfield_TextFieldFaceFor | 179/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 | 0 | 77861..78834: 167 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | 79220..79355: 38 instructions, 0 samples |
| textfield_TextFieldSecureDisplay | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 57/8/1/0/0/2/2/5/4/2/0/0/0/0/1 | 0 | 79450..79577: 36 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 | 0 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 | 0 | 80043..80302: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 | 0 | 80613..80676: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | 80819..80962: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 110/98/3/0/0/37/4/14/11/0/19/0/0/0/11 | 0 | 81840..82347: 113 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 32/9/1/0/0/1/0/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 25/12/1/0/0/3/2/4/3/0/0/0/0/0/3 | 0 | 82595..82672: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 | 0 | 82887..83032: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/0/0/2/0/2/0/12/12/0/0/0/0/6/0 | 0 | 83456..84117: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 | 0 | 84326..84348: 4 instructions, 0 samples |
| text_request_TextRequestTake | 253/28/7/53/42/148/24/24/17/12/14/9/0/0/7 | 76/0/0/1/0/18/2/9/9/4/3/1/0/0/0 | 1 | 85147..85166: 4 instructions, 1 samples |
| text_request_TextRequestChangeFor | 280/263/12/53/38/116/5/36/24/13/0/0/0/0/24 | 145/55/6/6/10/39/8/22/16/5/0/0/0/2/0 | 0 | 86336..87226: 238 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 68/18/2/0/0/2/2/8/6/4/0/0/0/0/1 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 32/0/0/1/3/8/0/2/2/1/2/0/0/0/0 | 0 | 87421..87650: 60 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 87682..87852: 52 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 171/20/5/21/29/100/18/15/10/7/10/8/0/0/5 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 12 | 88716..88738: 4 instructions, 4 samples |
| tree_TreeSetCaptureTab | 129/12/3/17/21/64/10/14/11/9/6/4/0/0/3 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 | 10 | 89526..89548: 4 instructions, 5 samples |
| tree_input_TreeFocusSubmittedAt | 289/24/6/42/46/139/20/34/28/15/20/7/0/0/6 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 | 0 | 89874..89893: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 272/140/8/40/42/162/26/23/15/8/14/13/0/0/27 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 | 0 | 92081..92664: 120 instructions, 0 samples |
| tree_TreeSetSemantic | 171/107/6/18/24/80/14/20/14/10/8/6/0/0/21 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 5 | 93039..93502: 98 instructions, 5 samples |
| tree_TreeSetLabel | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 14 | 94023..94045: 4 instructions, 7 samples |
| tree_TreeSetStyleClass | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 8 | 94373..94392: 4 instructions, 3 samples |
| tree_TreeSetStyleId | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 7 | 95080..95102: 4 instructions, 4 samples |
| tree_TreeSetSelected | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 3 | 95522..95541: 4 instructions, 2 samples |
| tree_TreeSetTextValue | 432/347/16/49/82/273/48/32/16/11/24/23/0/0/59 | 138/86/3/8/4/47/12/22/19/5/0/1/0/2/6 | 33 | 96768..98368: 286 instructions, 25 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 32/0/0/5/0/8/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 99215..99237: 4 instructions, 0 samples |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 0 | 100786..100808: 4 instructions, 0 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 179/0/0/22/20/53/0/13/13/12/47/1/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 | 0 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 22/0/0/0/0/3/0/2/2/1/11/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 0 | 102905..102927: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 23/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 103522..103539: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 103569..103623: 18 instructions, 0 samples |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 104351..104373: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 104871..104890: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 104969..104991: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 82/28/4/0/0/21/19/15/11/0/4/0/0/0/3 | 0 | 105024..105069: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 102/0/0/21/23/69/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocused | 83/0/0/15/20/50/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 68/21/1/8/5/27/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 | 0 | 106045..106134: 21 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 140/0/0/6/33/75/0/12/12/5/18/0/0/2/0 | 0 | 106507..109008: 460 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 33 | 112324..112346: 4 instructions, 7 samples |
| paint_queue_PaintOrder | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 114540..115773: 247 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 116704..118639: 421 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 118806..118828: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 0 | 119335..119357: 4 instructions, 0 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 121911..122697: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 123150..124381: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 17 | 124509..124528: 4 instructions, 12 samples |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 7 | 125677..125696: 4 instructions, 5 samples |
| tree_TreeExplicitExists | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 9 | 127268..127796: 89 instructions, 6 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 32 | 128170..128784: 113 instructions, 24 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 205/20/5/34/39/112/16/25/20/12/17/6/0/0/5 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 | 1 | 129474..129493: 4 instructions, 1 samples |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 | 0 | 130522..130541: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 293/260/17/62/47/150/26/36/19/11/16/6/0/0/40 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 | 0 | 130727..132052: 258 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 0 | 132178..132340: 47 instructions, 0 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/2/0/5/4/4/5/1/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 2 | 132489..132498: 5 instructions, 0 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 132590..132670: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/9/1/0/0/3/2/3/2/0/0/0/0/0/2 | 0 | 132723..132787: 17 instructions, 0 samples |
| std_grapheme_Previous | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 65/38/2/6/2/11/2/8/6/2/0/0/0/1/4 | 0 | 132897..133040: 41 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 201/93/5/30/37/121/16/13/8/5/8/9/0/0/18 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 13 | 133161..133180: 4 instructions, 5 samples |
| tree_TreeLayerAt | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 27 | 134389..134891: 106 instructions, 14 samples |
| tree_TreeEnsureSemantic | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 16 | 135001..135020: 4 instructions, 12 samples |
| tree_TreeStoreLabel | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 11 | 136223..138375: 456 instructions, 8 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 138585..138607: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 139209..139231: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 0 | 139561..144009: 836 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 144240..144262: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 0 | 144819..145025: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 0 | 145098..145120: 4 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 146362..146384: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 147113..147708: 157 instructions, 0 samples |
| button_SubmitButton | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 | 0 | 147812..150634: 584 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 1160/1125/36/268/330/740/28/93/57/58/87/16/0/0/95 | 588/511/31/82/99/249/33/78/47/19/99/14/0/3/58 | 0 | 150913..156437: 1084 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 156965..156975: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 0 | 157105..157124: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 8 | 157650..157669: 4 instructions, 8 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 0 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 158930..159278: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 160969..160991: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | 161189..161208: 4 instructions, 0 samples |
| tree_TreeSetLabelSpan | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | 161460..161479: 4 instructions, 0 samples |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 162321..163014: 158 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 163161..164233: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 164916..164938: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 36 | 166087..166109: 4 instructions, 19 samples |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 0 | 166631..166890: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 0 | 167008..167027: 4 instructions, 0 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 168474..168552: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 83/12/3/8/17/47/9/5/2/4/6/3/0/0/3 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | 169342..169358: 4 instructions, 0 samples |
| tree_input_TreeHoveredAt | 145/20/5/12/26/72/15/19/14/7/10/5/0/0/5 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | 169749..169768: 4 instructions, 0 samples |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 0 | 170400..170422: 4 instructions, 0 samples |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 | 0 | none |
| tree_TreeRootBounds | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 0 | 171379..171398: 4 instructions, 0 samples |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 | 0 | 172036..173334: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 | 0 | 173848..175162: 284 instructions, 0 samples |
| tree_TreeSetCircleHit | 140/20/5/11/24/76/16/15/10/6/10/6/0/0/5 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | 175316..175335: 4 instructions, 0 samples |
| tree_TreeSetSemanticDetail | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | 176078..176097: 4 instructions, 0 samples |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 | 0 | 176937..183576: 1342 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_TreeNextKey | 143/54/7/9/29/84/18/12/4/5/12/6/0/0/12 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 | 0 | 184336..184568: 38 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 108/12/3/12/19/53/9/12/9/6/6/3/0/0/3 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 | 0 | 184735..184754: 4 instructions, 0 samples |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 74/49/1/7/14/37/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 | 0 | 185216..185416: 49 instructions, 0 samples |
| contextmenu_ContextMenuFind | 68/51/1/7/14/37/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 | 0 | 185485..185691: 51 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 129/43/1/33/33/78/1/3/2/6/46/1/0/0/1 | 122/32/8/25/13/45/3/12/3/5/50/2/0/0/9 | 0 | 186187..186376: 43 instructions, 0 samples |
| overlay_Draw | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 200/82/1/15/5/38/0/16/15/19/34/0/0/1/9 | 0 | 186711..188013: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1465/1248/10/377/376/810/10/72/62/142/347/1/0/0/114 | 940/723/13/211/90/371/26/63/50/101/289/2/0/1/99 | 0 | 189103..195293: 1136 instructions, 0 samples |
| paint_queue_PendingTextValue | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 206831..207026: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 207103..208114: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 208276..208653: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 209755..210346: 132 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 1 | 211842..211864: 4 instructions, 1 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 213471..213516: 12 instructions, 0 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 213609..213775: 33 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 0 | 214272..215340: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 215385..215542: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 215856..216034: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 | 0 | 216155..216769: 146 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 0 | 217113..217135: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 217498..217744: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 218290..218312: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 218524..218682: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 218910..219081: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 | 0 | 219388..220134: 172 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 388/333/6/71/75/167/7/29/23/22/117/4/1/0/40 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 | 0 | 220387..221938: 329 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 222207..222229: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 222871..222893: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 587/550/22/99/131/325/11/48/26/33/63/12/0/0/41 | 267/183/14/44/15/86/6/33/19/22/50/1/0/0/34 | 0 | 223744..225823: 417 instructions, 0 samples |
| dpi_ScalePixels | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 | 0 | 226492..226787: 75 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 169/149/7/25/41/98/4/18/11/9/31/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 | 0 | 226886..227599: 147 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/0/0/0/3/0 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 | 0 | none |
| tree_TreeSetPopup | 115/16/4/6/19/63/14/13/9/5/8/6/0/0/4 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 | 0 | 228267..228286: 4 instructions, 0 samples |
| drawing_props_StyleTokenColorOr | 92/84/5/10/5/33/9/13/8/5/4/3/0/0/18 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 229006..229211: 49 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_TreePushScope | 219/24/6/30/43/121/22/25/19/16/12/9/0/0/6 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 | 0 | 229425..229444: 4 instructions, 0 samples |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 | 0 | 230601..230614: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 30/16/4/0/0/6/1/7/3/0/0/0/0/0/5 | 0 | 230629..230795: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 | 0 | 231045..231054: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 115/0/0/11/28/71/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 649/596/8/78/335/450/14/31/23/21/80/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 | 0 | 231892..234861: 562 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 235393..235647: 64 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 235743..236007: 66 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 236103..236337: 58 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 236695..236714: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 237164..237183: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 0 | 238105..238124: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 0 | 239886..239905: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 1 | 240678..240694: 4 instructions, 1 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 240762..241423: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 | 0 | 242014..242033: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 242739..242898: 43 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 242958..243037: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 246/224/9/35/36/134/27/22/13/11/12/14/0/0/54 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 | 0 | 243153..243917: 139 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 244464..244587: 30 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 240/130/10/33/28/120/24/19/9/3/12/16/1/0/30 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 | 0 | 244861..245195: 70 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 246803..246825: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 230/194/7/10/16/40/7/36/29/13/11/0/6/0/35 | 98/25/2/1/4/25/10/9/7/1/6/4/0/5/4 | 0 | 247656..248304: 193 instructions, 0 samples |
| tree_TreePopScope | 122/12/3/15/21/64/11/13/10/9/6/5/0/0/3 | 52/10/1/0/0/6/4/6/5/4/0/4/0/0/2 | 0 | 248397..248416: 4 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 249057..249175: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 249381..251757: 435 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 251949..251996: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 252386..253424: 211 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 253616..253663: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 254053..255091: 211 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 255283..255330: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 46/22/2/0/0/8/7/6/4/0/0/5/0/0/1 | 0 | 255518..255576: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 | 0 | 256113..256963: 207 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 345/311/29/30/34/97/9/61/32/9/0/10/1/0/41 | 375/321/22/23/20/119/48/57/35/1/32/4/0/4/51 | 0 | 257224..258237: 262 instructions, 0 samples |
| image_behavior_ImageFitBounds | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 | 0 | 258399..259101: 177 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 259217..259571: 91 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 259594..259616: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 260025..260047: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 | 0 | 260268..261474: 284 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 669/640/30/61/31/225/53/103/72/15/142/0/0/3/94 | 0 | 261695..263868: 479 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 | 0 | 264299..265694: 332 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 24/0/0/4/4/18/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 | 0 | 266711..267192: 123 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 | 0 | 267589..270008: 523 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 | 0 | 270132..270255: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 270293..270524: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 270603..270809: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 270932..271050: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 271861..271877: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 272236..272252: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 | 0 | 272311..272670: 90 instructions, 0 samples |

## Hello / cc
Native SHA-256: 66e96c7832b03971230d94006661eb128c4a363afe6b77456ff5b7c0316ca9c0
C SHA-256: 99f598d120c0edcbb9b95afa0bea191beed39a0adb757be948101a530373fc94
ZX SHA-256: da9d8570dcaa74c0357ddf325c39fa1c6827428e86dfc6e41a1dee275cd52d27
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| hello_main | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |

## Hello / clang-19
Native SHA-256: 66e96c7832b03971230d94006661eb128c4a363afe6b77456ff5b7c0316ca9c0
C SHA-256: ac3f7205c3742ad80c897838541a30695125295037f496665665375fcc95f693
ZX SHA-256: da9d8570dcaa74c0357ddf325c39fa1c6827428e86dfc6e41a1dee275cd52d27
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| hello_main | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |

## Ranked C wins

| Workload/compiler/function | Weighted excess | Function samples/total (estimated CPU share) | Patterns to inspect | Native | C |
| --- | ---: | ---: | --- | --- | --- |
| Records/cc/wasm_tier_Records | 92465 | 86/86 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 929/149/21/260/520/806/4/46/25/5/0/5/2/0/30 | 70/57/4/0/0/20/2/6/2/0/0/2/0/2/11 |
| Records/clang-19/wasm_tier_Records | 92142 | 84/84 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 929/149/21/260/520/806/4/46/25/5/0/5/2/0/30 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 |
| Sieve/cc/wasm_tier_Sieve | 62222 | 117/117 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 34/19/3/0/0/7/6/6/3/1/0/1/0/0/9 |
| Sieve/clang-19/wasm_tier_Sieve | 60000 | 125/125 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 |
| Hash/cc/wasm_tier_Hash | 58064 | 148/148 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 26/12/2/0/0/5/1/2/0/0/0/4/0/1/4 |
| Hash/clang-19/wasm_tier_Hash | 37096 | 146/146 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 |
| Vectors/cc/wasm_tier_Vectors | 26160 | 121/149 81% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/137/15/22/23/47/2/31/16/3/0/2/0/0/15 | 101/90/7/7/8/24/5/16/9/2/7/0/0/0/9 |
| Vectors/clang-19/wasm_tier_Vectors | 15636 | 138/154 89% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/137/15/22/23/47/2/31/16/3/0/2/0/0/15 | 123/101/10/3/3/22/15/17/7/2/26/1/0/1/13 |
| Text/clang-19/tree_TreeSubmit | 14881 | 63/286 22% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Button/clang-19/tree_TreeSubmit | 14048 | 68/327 20% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Text/cc/tree_TreeSubmit | 13516 | 68/247 27% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Input/clang-19/tree_TreeSubmit | 8882 | 71/540 13% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Button/cc/tree_TreeSubmit | 8682 | 55/311 17% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Text/cc/tree_TreeFinish | 7212 | 25/247 10% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Input/cc/tree_TreeSubmit | 7031 | 56/391 14% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1273/943/44/136/264/760/130/118/74/34/97/57/0/0/202 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Text/clang-19/tree_TreeFinish | 6566 | 28/286 9% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Button/cc/tree_TreeFinish | 5727 | 25/311 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Input/clang-19/tree_TreeSubmittedGeometryAt | 4825 | 36/540 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Button/cc/tree_TreeIsBuilding | 4581 | 19/311 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang-19/tree_TreeFinish | 4512 | 22/327 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Input/clang-19/tree_TreeSetTextValue | 4158 | 33/540 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 432/347/16/49/82/273/48/32/16/11/24/23/0/0/59 | 138/86/3/8/4/47/12/22/19/5/0/1/0/2/6 |
| Input/clang-19/tree_TreeFinish | 4098 | 33/540 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Text/clang-19/tree_TreeRead | 4034 | 17/286 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Text/cc/tree_TreeIsBuilding | 3947 | 13/247 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeSubmittedGeometryAt | 3914 | 14/247 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Input/cc/tree_TreeFinish | 3826 | 21/391 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 741/272/23/66/141/498/98/55/32/17/42/55/0/0/71 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Button/cc/tree_TreeFindOld | 3767 | 24/311 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Button/clang-19/tree_TreeSetInteractive | 3557 | 17/327 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/20/5/21/29/100/18/15/10/7/10/8/0/0/5 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Text/clang-19/tree_TreeSubmittedGeometryAt | 3542 | 14/286 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Text/clang-19/tree_TreeFindOld | 3250 | 22/286 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Input/cc/tree_TreeFindOld | 3121 | 25/391 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Text/cc/tree_TreeParentGeneration | 3109 | 10/247 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Button/clang-19/tree_TreeIsBuilding | 2981 | 13/327 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_TreeInputAt | 2890 | 23/540 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Text/clang-19/tree_TreeEnsureSemantic | 2769 | 15/286 5% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Text/cc/tree_TreeBuildOldIndex | 2702 | 15/247 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 192/153/11/6/6/53/12/32/21/4/0/7/0/4/25 |
| Input/clang-19/tree_TreeLayerAt | 2687 | 27/540 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Input/cc/tree_TreeIsBuilding | 2685 | 14/391 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeSubmittedGeometryAt | 2664 | 12/311 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Input/cc/tree_TreeLayerAt | 2602 | 22/391 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Input/clang-19/tree_TreeParentGeneration | 2563 | 17/540 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Input/clang-19/tree_TreeFindOld | 2503 | 32/540 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Text/clang-19/tree_TreeSetStyleClass | 2502 | 12/286 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeParentGeneration | 2490 | 10/327 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Text/clang-19/tree_TreeSetSemantic | 2406 | 11/286 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/107/6/18/24/80/14/20/14/10/8/6/0/0/21 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Text/cc/tree_TreeSetLabel | 2370 | 10/247 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetLabel | 2353 | 13/327 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/cc/tree_input_TreeHitAt | 2329 | 12/311 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 492/390/12/63/79/240/19/77/65/38/22/7/0/0/66 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 |
| Button/clang-19/tree_TreeFindOld | 2325 | 18/327 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Input/cc/tree_TreeSetTextValue | 2308 | 15/391 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 432/347/16/49/82/273/48/32/16/11/24/23/0/0/59 | 172/64/6/12/9/59/8/28/22/4/0/3/0/1/8 |
| Input/cc/tree_TreeSetInteractive | 2294 | 13/391 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/20/5/21/29/100/18/15/10/7/10/8/0/0/5 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Text/clang-19/tree_TreeParentGeneration | 2278 | 8/286 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Text/clang-19/tree_TreeSetLabel | 2277 | 11/286 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang-19/tree_TreeBuildOldIndex | 2240 | 26/540 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Button/cc/tree_TreeSetInteractive | 2218 | 10/311 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/20/5/21/29/100/18/15/10/7/10/8/0/0/5 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Text/cc/tree_TreeFindOld | 2174 | 11/247 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 213/162/6/24/34/108/15/22/16/10/8/5/0/0/27 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Text/cc/tree_TreeEnsureSemantic | 2171 | 14/247 5% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Text/clang-19/tree_TreeLayerAt | 2067 | 11/286 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Button/clang-19/tree_TreeSubmittedGeometryAt | 1991 | 9/327 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Button/clang-19/tree_TreeSetCircleHit | 1965 | 9/327 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/20/5/11/24/76/16/15/10/6/10/6/0/0/5 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Input/clang-19/tree_TreeCurrentParent | 1943 | 13/540 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Input/cc/tree_TreeSubmittedGeometryAt | 1942 | 11/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 181/12/3/34/48/131/10/12/9/5/19/4/0/0/3 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Input/cc/tree_TreeCurrentParent | 1879 | 10/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Button/clang-19/tree_TreeBuildOldIndex | 1849 | 13/327 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Text/clang-19/tree_TreeIsBuilding | 1835 | 7/286 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_TreeIsBuilding | 1805 | 13/540 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/tree_TreeSetLabel | 1797 | 12/391 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeLayerAt | 1784 | 12/311 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Button/cc/tree_TreeSetCircleHit | 1782 | 8/311 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/20/5/11/24/76/16/15/10/6/10/6/0/0/5 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 |
| Input/clang-19/tree_TreeRead | 1759 | 14/540 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Text/cc/tree_TreeStoreLabel | 1720 | 12/247 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Text/cc/tree_TreeSetStyleClass | 1715 | 7/247 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Text/clang-19/tree_TreeCurrentParent | 1693 | 6/286 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Input/clang-19/tree_TreeCount | 1666 | 12/540 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeCurrentParent | 1654 | 7/311 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Button/clang-19/tree_TreeExplicitExists | 1650 | 11/327 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Input/cc/tree_TreeExplicitExists | 1631 | 13/391 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 82/21/2/2/2/22/12/5/3/2/0/3/0/3/2 |
| Text/clang-19/tree_TreeBuildOldIndex | 1626 | 10/286 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Button/clang-19/tree_TreeCount | 1605 | 7/327 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeEnsureSemantic | 1601 | 13/311 4% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Input/cc/tree_TreeParentGeneration | 1571 | 8/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Input/clang-19/tree_TreeEnsureSemantic | 1564 | 16/540 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Input/cc/tree_input_TreeHitAt | 1543 | 10/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 492/390/12/63/79/240/19/77/65/38/22/7/0/0/66 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 |
| Input/clang-19/tree_TreeSetLabel | 1535 | 14/540 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/cc/tree_TreeSetSemantic | 1525 | 10/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/107/6/18/24/80/14/20/14/10/8/6/0/0/21 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 |
| Input/clang-19/tree_TreeSetInteractive | 1520 | 12/540 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/20/5/21/29/100/18/15/10/7/10/8/0/0/5 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Text/cc/tree_TreeLayerAt | 1497 | 8/247 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Button/clang-19/tree_input_TreeHoveredAt | 1484 | 8/327 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 145/20/5/12/26/72/15/19/14/7/10/5/0/0/5 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Button/cc/tree_TreeParentGeneration | 1481 | 6/311 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 194/32/8/16/36/113/24/15/7/6/16/8/0/0/8 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Button/clang-19/tree_TreeCurrentParent | 1481 | 6/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Input/clang-19/tree_TreePrevious | 1453 | 15/540 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 128/29/4/12/19/59/10/14/10/7/6/4/0/0/3 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Button/cc/tree_TreeModalLayer | 1446 | 6/311 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_input_TreeHitAt | 1427 | 12/540 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 492/390/12/63/79/240/19/77/65/38/22/7/0/0/66 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Button/cc/tree_TreeBuildOldIndex | 1421 | 10/311 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 |
| Input/cc/tree_TreeSetStyleClass | 1393 | 9/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Text/clang-19/tree_TreeExplicitExists | 1372 | 8/286 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Input/cc/tree_TreeEnsureSemantic | 1371 | 14/391 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Input/cc/tree_TreeBuildOldIndex | 1357 | 12/391 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/280/16/42/51/181/31/34/18/9/16/15/0/0/57 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 |
| Input/cc/tree_TreePrevious | 1338 | 10/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 128/29/4/12/19/59/10/14/10/7/6/4/0/0/3 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 |
| Button/cc/tree_TreeSetLabel | 1317 | 7/311 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeLayerAt | 1314 | 8/327 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/114/7/14/21/76/14/23/16/7/8/6/0/0/20 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Text/clang-19/tree_TreeStart | 1314 | 5/286 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Input/cc/tree_TreeSetSelected | 1306 | 8/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Button/clang-19/tree_TreeRootBounds | 1282 | 6/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Button/clang-19/tree_TreePrevious | 1280 | 8/327 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 128/29/4/12/19/59/10/14/10/7/6/4/0/0/3 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetStyleClass | 1276 | 7/327 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetStyleId | 1276 | 7/327 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/cc/tree_TreeStoreLabel | 1267 | 14/391 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Input/clang-19/tree_TreeEnsureInteraction | 1257 | 13/540 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 201/93/5/30/37/121/16/13/8/5/8/9/0/0/18 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Button/clang-19/tree_TreeInputAt | 1245 | 6/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Input/cc/tree_TreeInputAt | 1209 | 9/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Text/cc/tree_TreeCurrentParent | 1190 | 4/247 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 83/12/3/5/14/41/11/9/6/4/6/4/0/0/3 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Button/cc/tree_TreeEnsureInteraction | 1180 | 9/311 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 201/93/5/30/37/121/16/13/8/5/8/9/0/0/18 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 |
| Text/clang-19/tree_TreeStoreLabel | 1176 | 19/286 6% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Button/cc/tree_TreeExplicitExists | 1160 | 7/311 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 78/21/2/2/2/18/7/5/3/2/0/3/0/3/2 |
| Button/clang-19/tree_TreeModalLayer | 1146 | 5/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/tree_input_TreeHasFocus | 1134 | 9/391 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/98/6/13/20/61/12/18/12/9/6/3/0/0/19 | 71/45/3/5/4/20/7/9/6/3/0/1/0/0/7 |
| Input/clang-19/tree_TreeSetCaptureTab | 1134 | 10/540 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 129/12/3/17/21/64/10/14/11/9/6/4/0/0/3 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 |
| Text/cc/tree_TreeRead | 1129 | 7/247 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Button/clang-19/tree_TreeEnsureInteraction | 1118 | 7/327 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 201/93/5/30/37/121/16/13/8/5/8/9/0/0/18 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Input/clang-19/tree_TreeModalLayer | 1111 | 8/540 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/tree_TreeSetStyleId | 1083 | 7/391 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Input/cc/tree_input_TreeTextFocused | 1043 | 6/391 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 147/107/7/5/22/68/17/19/12/7/10/5/0/0/22 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 |
| Input/cc/tree_input_TreePointerUpdate | 1026 | 7/391 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 621/68/17/58/91/370/80/53/36/30/63/46/0/0/17 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 |
| Button/cc/tree_TreeStoreLabel | 1024 | 9/311 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Button/clang-19/tree_input_TreePointerUpdate | 1021 | 5/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 621/68/17/58/91/370/80/53/36/30/63/46/0/0/17 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 |
| Text/cc/tree_TreeExplicitExists | 1018 | 5/247 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 80/21/2/2/2/20/10/5/3/2/0/3/0/3/2 |
| Button/cc/tree_TreeInputAt | 1013 | 6/311 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Button/cc/tree_TreePrevious | 1009 | 6/311 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 128/29/4/12/19/59/10/14/10/7/6/4/0/0/3 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 |
| Button/clang-19/tree_input_TreeHitAt | 982 | 5/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 492/390/12/63/79/240/19/77/65/38/22/7/0/0/66 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Text/cc/tree_TreeSetStyleId | 980 | 4/247 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Button/cc/tree_TreeSetStyleId | 973 | 5/311 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Button/clang-19/tree_TreeEnsureSemantic | 968 | 6/327 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 214/94/6/29/48/130/18/14/8/5/10/9/0/0/20 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Input/cc/tree_TreeModalLayer | 959 | 5/391 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/tree_TreeBuildNewIndex | 942 | 7/286 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Input/cc/tree_TreeEnsureInteraction | 939 | 9/391 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 201/93/5/30/37/121/16/13/8/5/8/9/0/0/18 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 |
| Input/clang-19/tree_input_TreeTextFocused | 934 | 7/540 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 147/107/7/5/22/68/17/19/12/7/10/5/0/0/22 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Text/clang-19/tree_TreeFrameStatus | 932 | 4/286 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/cc/tree_TreeRead | 896 | 7/311 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Button/cc/tree_TreeRootBounds | 888 | 4/311 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Input/clang-19/tree_TreeSetStyleClass | 883 | 8/540 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_input_TreeTextFocused | 882 | 4/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 147/107/7/5/22/68/17/19/12/7/10/5/0/0/22 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Text/cc/tree_TreeBuildNewIndex | 845 | 6/247 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Button/clang-19/tree_TreeRead | 830 | 4/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Button/cc/tree_TreeSetSelected | 821 | 4/311 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Input/clang-19/tree_TreeExplicitExists | 817 | 9/540 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 161/93/6/15/29/90/17/10/4/6/10/5/0/0/18 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Input/cc/tree_TreeSetCaptureTab | 793 | 5/391 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 129/12/3/17/21/64/10/14/11/9/6/4/0/0/3 | 49/36/5/0/0/7/1/8/3/4/0/1/0/1/4 |
| Input/clang-19/tree_TreeSetStyleId | 773 | 7/540 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetSelected | 750 | 4/327 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/cc/tree_TreeSetSemantic | 724 | 3/247 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/107/6/18/24/80/14/20/14/10/8/6/0/0/21 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 |
| Input/cc/tree_TreeBuildNewIndex | 711 | 8/391 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Text/cc/tree_TreeFrameStatus | 685 | 3/247 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/cc/tree_TreeBuildNewIndex | 671 | 6/311 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Button/cc/tree_TreeStart | 666 | 3/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Button/cc/tree_input_TreeTextFocused | 656 | 3/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 147/107/7/5/22/68/17/19/12/7/10/5/0/0/22 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 |
| Text/clang-19/tree_TreeSetStyleId | 625 | 3/286 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeFrameStatus | 611 | 3/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Input/cc/tree_TreeRead | 611 | 6/391 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 803/100/25/118/242/562/91/70/45/8/67/33/0/0/25 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Text/cc/tree_TreeCount | 607 | 2/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_input_TreePressedAt | 592 | 3/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 83/12/3/8/17/47/9/5/2/4/6/3/0/0/3 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 |
| Button/cc/tree_TreeSetStyleClass | 583 | 3/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 114/12/3/12/19/57/10/13/10/7/6/4/0/0/3 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Input/clang-19/tree_TreeSetSemantic | 579 | 5/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/107/6/18/24/80/14/20/14/10/8/6/0/0/21 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Input/clang-19/tree_input_TreeHasFocus | 571 | 6/540 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/98/6/13/20/61/12/18/12/9/6/3/0/0/19 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 |
| Button/cc/tree_input_TreeHoveredAt | 558 | 3/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 145/20/5/12/26/72/15/19/14/7/10/5/0/0/5 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 |
| Button/cc/tree_input_TreeFocusedAt | 539 | 4/311 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Input/clang-19/tree_TreeBuildNewIndex | 499 | 7/540 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Input/clang-19/tree_input_TreePointerUpdate | 495 | 4/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 621/68/17/58/91/370/80/53/36/30/63/46/0/0/17 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 |
| Button/clang-19/tree_input_TreeFocusedAt | 475 | 3/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Input/cc/tree_TreeFrameStatus | 432 | 3/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Input/clang-19/tree_TreeStart | 417 | 3/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Input/cc/tree_TreeCount | 383 | 2/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_TreeFrameStatus | 370 | 3/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/cc/tree_input_TreePointerUpdate | 368 | 2/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 621/68/17/58/91/370/80/53/36/30/63/46/0/0/17 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 |
| Input/cc/tree_TreePreviousWidgetState | 367 | 2/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/20/5/19/31/104/16/15/10/6/10/6/0/0/5 | 48/0/0/0/0/17/3/4/4/1/2/1/0/1/0 |
| Input/clang-19/tree_TreeStoreLabel | 360 | 11/540 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Button/clang-19/tree_TreeBuildNewIndex | 353 | 3/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 296/255/13/33/39/144/21/31/18/9/10/12/0/0/45 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Input/clang-19/tree_TreeSetSelected | 340 | 3/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeStoreLabel | 324 | 6/327 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 531/493/28/86/55/225/35/70/42/21/10/17/1/1/79 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Text/clang-19/text_layout_TextNoBreakBefore | 315 | 1/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/10/7/0/0/0/0/0/7 |
| Input/clang-19/tree_input_TreeFocusedAt | 287 | 3/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Text/cc/std_utf8_Decode | 286 | 1/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Input/clang-19/session_SessionIndex | 284 | 2/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Input/clang-19/tree_TreePreviousWidgetState | 281 | 2/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 171/20/5/19/31/104/16/15/10/6/10/6/0/0/5 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 |
| Text/cc/tree_TreeStart | 279 | 1/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Text/clang-19/tree_TreeCount | 262 | 1/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/session_SessionIndex | 260 | 1/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Input/clang-19/text_input_TextInputContentWidth | 246 | 2/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 42/5/1/1/1/2/0/5/4/4/5/1/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Button/clang-19/session_SessionIndex | 234 | 1/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Button/clang-19/tree_TreeStart | 229 | 1/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Text/cc/tree_TreeIdentityHash | 220 | 1/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang-19/widget_benchmarks_ButtonCase | 212 | 1/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 145/0/0/5/5/38/0/13/13/14/8/0/0/0/0 |
| Text/cc/paint_queue_KeepPaintMesh | 183 | 1/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Input/cc/tree_TreeStart | 176 | 1/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 294/65/2/48/50/219/28/8/6/9/2/25/0/0/15 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Text/clang-19/paint_queue_QueuePaint | 170 | 1/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Input/cc/session_SessionIndex | 164 | 1/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Input/clang-19/theme_DefaultFonts | 149 | 1/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_EmitPaintDirect | 147 | 1/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Input/clang-19/text_request_TextRequestTake | 129 | 1/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 253/28/7/53/42/148/24/24/17/12/14/9/0/0/7 | 76/0/0/1/0/18/2/9/9/4/3/1/0/0/0 |
| Input/cc/paint_queue_QueuePaint | 127 | 1/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Input/clang-19/widget_benchmarks_InputCase | 127 | 1/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 161/0/0/13/24/70/0/19/19/12/23/0/0/0/0 |
| Text/clang-19/text_layout_TextTokenNext | 113 | 1/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 210/116/20/4/2/22/7/53/33/3/0/0/0/3/34 |
| Button/cc/button_PaintButtonForNode | 109 | 1/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 |
| Input/clang-19/tree_input_TreeTakeActivationAt | 108 | 1/540 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/tree_input_TreeFocusedAt | 107 | 1/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Text/cc/paint_queue_PendingTextValue | 105 | 1/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Text/clang-19/text_PaintTextLines | 102 | 1/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 694/612/7/184/149/360/10/49/42/45/86/0/0/0/42 | 490/418/9/71/66/235/5/41/32/23/164/0/0/10/38 |
| Input/clang-19/tree_input_TreeTakePointerActivationAt | 93 | 1/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 205/20/5/34/39/112/16/25/20/12/17/6/0/0/5 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 |
| Input/clang-19/theme_DefaultTheme | 85 | 1/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Input/clang-19/textfield_TextFieldCore | 84 | 1/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3950/3778/52/857/1164/2342/28/400/348/251/279/1/1/0/265 | 2155/1176/35/302/166/909/19/256/221/148/299/1/0/13/120 |
| Input/clang-19/tree_input_TreeFocusAt | 74 | 1/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Button/clang-19/tree_input_TreePressedAt | 51 | 1/327 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 83/12/3/8/17/47/9/5/2/4/6/3/0/0/3 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 |
| Calls/clang-19/wasm_tier_Fib | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 21/7/1/0/0/2/0/2/1/1/0/0/0/0/1 |
| Printing/cc/wasm_tier_Printing | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 31/14/1/0/0/4/0/1/0/1/0/2/0/0/3 |
| Printing/clang-19/wasm_tier_Printing | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 |
| Button/cc/widget_benchmarks_ButtonCase | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 158/147/9/4/10/45/4/19/10/12/8/0/0/0/36 |
| Button/cc/widget_benchmarks_BeginCase | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Button/cc/tree_draw_BeginFrame | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Button/cc/button_Draw | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Button/cc/tree_draw_EndFrame | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Button/cc/tree_TreeCount | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeNodeAt | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/311 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_WidgetPaintCount | 0 | 0/311 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/311 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/text_request_TextRequestSend | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Button/cc/text_request_TextRequestClear | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Button/cc/std_grapheme_Boundary | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Button/cc/session_SessionOpen | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Button/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/311 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/session_SessionIndex | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Button/cc/tree_input_PointerSample | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Button/cc/tree_input_SecondaryPointerSample | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Button/cc/session_SessionFlagSet | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Button/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Button/cc/tree_input_TreeKeyboardRoute | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Button/cc/text_editing_TextEditingFrameBegin | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_PaintClear | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Button/cc/tree_TreeFrameStatus | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 39/4/1/4/6/17/3/3/2/3/2/1/0/0/1 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/cc/button_SubmitButton | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 |
| Button/cc/session_SessionFlagGet | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_draw_CommitFrame | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Button/cc/paint_queue_PaintSnapshotEmit | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Button/cc/paint_queue_PaintFlush | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Button/cc/tree_input_TreeTextKind | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_TreeFocusAt | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Button/cc/std_utf8_Valid | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Button/cc/tree_TreeTextRange | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Button/cc/geometry_RectangleIntersection | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Button/cc/tree_TreePointerActivatable | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Button/cc/tree_input_TreeActivateAt | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 |
| Button/cc/cursor_CursorHoverFrameStart | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Button/cc/style_ResolveFlags | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Button/cc/style_ResolveState | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Button/cc/style_ResolveFont | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Button/cc/style_StyleFontValue | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Button/cc/font_metrics_GlyphWidth | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/font_metrics_GlyphLineHeight | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/button_ButtonDetailSize | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 |
| Button/cc/button_behavior_MeasureBounds | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 |
| Button/cc/style_StyleMinHeightValue | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/cc/button_ButtonMinimumHeight | 0 | 0/311 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/button_ButtonFitContentFace | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 |
| Button/cc/tree_TreeSetSemanticDetail | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_draw_FrameInputCommit | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Button/cc/paint_queue_PaintOverflow | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/cc/tree_TreeCancel | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Button/cc/paint_queue_PaintOrder | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Button/cc/paint_queue_SnapshotTextRead | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_SnapshotTextJoin | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Button/cc/paint_queue_PaintPointCount | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Button/cc/paint_queue_PendingPaintAt | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Button/cc/input_protocol_PointInBounds | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 |
| Button/cc/tree_input_TreeTopModalLayer | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/cc/tree_TreeModalLayerAt | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/cc/tree_TreeButtonLike | 0 | 0/311 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_PointInCircleBounds | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/cc/std_utf8_Decode | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Button/cc/tree_TreeIdentityHash | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 205/20/5/34/39/112/16/25/20/12/17/6/0/0/5 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 |
| Button/cc/style_sheet_ResolveInstalledStyleRules | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 293/260/17/62/47/150/26/36/19/11/16/6/0/0/40 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 |
| Button/cc/button_ButtonDefaultFace | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 |
| Button/cc/button_ButtonFaceFacts | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 |
| Button/cc/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 33/16/2/0/0/4/2/5/3/0/0/0/0/2/5 |
| Button/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/311 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/button_ButtonDetailFont | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Button/cc/style_StyleLineHeightValue | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/cc/style_FitHeight | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 |
| Button/cc/button_behavior_MeasureContent | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 |
| Button/cc/button_behavior_ShapeWidth | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 |
| Button/cc/theme_ControlHeightFor | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 |
| Button/cc/style_StyleOpacityValue | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Button/cc/button_behavior_ButtonCornerRadius | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Button/cc/drawing_props_ColorFromPacked | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/surface_Opacity | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Button/cc/paint_queue_PaintRoundedFill | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Button/cc/paint_queue_PaintRoundedOutline | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 |
| Button/cc/focus_FocusDefaultOutlineBounds | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 |
| Button/cc/image_lookup_IconLookupImage | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 |
| Button/cc/button_behavior_ButtonPictureSize | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 |
| Button/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 |
| Button/cc/button_behavior_ContentLayout | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 |
| Button/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 388/333/6/71/75/167/7/29/23/22/117/4/1/0/40 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 |
| Button/cc/image_lookup_ImageLookupTexture | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 |
| Button/cc/image_behavior_ImageDrawPlanFor | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 |
| Button/cc/paint_queue_PaintImage | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 |
| Button/cc/paint_queue_PaintLine | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 |
| Button/cc/paint_queue_PaintClippedLabel | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Button/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/cc/cursor_CursorHoverFrameCommit | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Button/cc/paint_queue_PaintMeshPointCount | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Button/cc/paint_queue_EmitPaintDirect | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Button/cc/paint_queue_ResolvePaintText | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Button/cc/std_utf8_Continuation | 0 | 0/311 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/std_grapheme_IsControl | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/session_WidgetPartKey | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/widget_key_WidgetRepeatedPart | 0 | 0/311 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleBucket | 0 | 0/311 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleFirstBit | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 |
| Button/cc/style_sheet_StyleRuleMatchesState | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleSelectorMatches | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Button/cc/style_MergeValues | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 |
| Button/cc/theme_CurrentTheme | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Button/cc/theme_StateLayer | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 |
| Button/cc/card_behavior_CardDefaultFace | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 |
| Button/cc/theme_FontForRole | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Button/cc/theme_MotionDuration | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 |
| Button/cc/button_behavior_ButtonRoleFactsFor | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 |
| Button/cc/tree_TreeLabelSpanAt | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Button/cc/tree_TreeSetLabelSpan | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 |
| Button/cc/surface_Unit | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Button/cc/paint_queue_ClipShape | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Button/cc/paint_queue_QueuePaint | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Button/cc/drawing_props_PaintPixelScale | 0 | 0/311 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 246/224/9/35/36/134/27/22/13/11/12/14/0/0/54 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 |
| Button/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 240/130/10/33/28/120/24/19/9/3/12/16/1/0/30 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 |
| Button/cc/image_behavior_ImageFitRect | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Button/cc/paint_queue_PaintImageWithPolygon | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 |
| Button/cc/paint_queue_EmitLine | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Button/cc/paint_queue_EmitLabel | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Button/cc/paint_queue_ClipLabel | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Button/cc/paint_queue_PendingTextValue | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Button/cc/style_sheet_StyleMatchesOptional | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/theme_DefaultTheme | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Button/cc/style_sheet_StyleControlRoleFacts | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Button/cc/paint_queue_ShapeVisible | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Button/cc/paint_queue_ShapeWhole | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Button/cc/raster_shape_FillRoundedRectangle | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_FillShapeClipped | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Button/cc/paint_queue_EmitPaintNow | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Button/cc/tree_TreeCurrentPaintGeometry | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Button/cc/paint_queue_KeepPaintMesh | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Button/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconHash | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 |
| Button/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 |
| Button/cc/svg_shape_ShapeRaster | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 |
| Button/cc/svg_icon_behavior_SvgIconCells | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 345/311/29/30/34/97/9/61/32/9/0/10/1/0/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 |
| Button/cc/image_behavior_ImageFitBounds | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 |
| Button/cc/image_behavior_ImagePolygonValid | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 |
| Button/cc/paint_queue_EmitImage | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Button/cc/paint_queue_ClipImage | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Button/cc/raster_StrokeLine | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/text_typeface_TextTypefaceUse | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Button/cc/raster_text_PaintTextRotated | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Button/cc/raster_text_PaintTextItalicClipped | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Button/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Button/cc/raster_text_PaintTextClipped | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Button/cc/raster_text_PaintText | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/theme_DarkPalette | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/theme_LightPalette | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/theme_DefaultMetrics | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Button/cc/theme_DefaultShadows | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Button/cc/theme_DefaultMotion | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Button/cc/theme_DefaultFonts | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleControlFacts | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Button/cc/paint_queue_ShapeRadius | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Button/cc/paint_queue_FillShapeRows | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Button/cc/paint_queue_FillClippedSpan | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Button/cc/paint_queue_KeepPaintBytes | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Button/cc/paint_queue_KeepMeshPoints | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Button/cc/svg_shape_ShapeNumber | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 |
| Button/cc/svg_shape_ShapeBuildPath | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 |
| Button/cc/svg_shape_ShapeCoverEdges | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 |
| Button/cc/paint_queue_EmitImageBegin | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterImage | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 |
| Button/cc/paint_queue_EmitImageEnd | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 24/0/0/4/4/18/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Button/cc/text_emphasis_RunTextItalicRaster | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterText | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Button/cc/style_sheet_StyleDefaultFacts | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RecordBounds | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Button/cc/paint_queue_ShapeCornerInset | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Button/cc/paint_queue_KeepShortPaintBytes | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Button/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Button/cc/paint_queue_KeepLongPaintBytes | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Button/cc/svg_shape_ShapeIsDigit | 0 | 0/311 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeIsSpace | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddPoint | 0 | 0/311 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddSpan | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 |
| Button/cc/paint_queue_ShapeSquareRoot | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Button/cc/paint_queue___zi_spec_9308732530dd822f | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Button/cc/paint_queue___zi_spec_27099373fae5fe98 | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Button/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Button/cc/paint_queue___zi_spec_17277838aa4fe0e7 | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Button/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Button/cc/paint_queue___zi_spec_b997e17bf3928592 | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Button/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 |
| Button/cc/svg_shape_ShapeFillSubpath | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeCollapse | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 |
| Button/cc/svg_shape_ShapeAddCircle | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 |
| Button/cc/svg_shape_ShapeSqrt | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 |
| Button/cc/svg_shape_ShapeQuad | 0 | 0/311 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeAbs | 0 | 0/311 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeTriangle | 0 | 0/311 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddEdge | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Button/cc/svg_shape_ShapeAddPolygon | 0 | 0/311 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 |
| Button/clang-19/widget_benchmarks_BeginCase | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/tree_draw_BeginFrame | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Button/clang-19/button_Draw | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Button/clang-19/tree_draw_EndFrame | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Button/clang-19/tree_TreeNodeAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/327 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintCount | 0 | 0/327 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/327 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/text_request_TextRequestSend | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Button/clang-19/text_request_TextRequestClear | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/327 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_PointerSample | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Button/clang-19/tree_input_SecondaryPointerSample | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Button/clang-19/session_SessionFlagSet | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 |
| Button/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_PaintClear | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Button/clang-19/button_SubmitButton | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 |
| Button/clang-19/session_SessionFlagGet | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang-19/tree_draw_CommitFrame | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Button/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Button/clang-19/paint_queue_PaintFlush | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 |
| Button/clang-19/tree_input_TreeTextKind | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeFocusAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/tree_TreeTextRange | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Button/clang-19/geometry_RectangleIntersection | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Button/clang-19/tree_TreePointerActivatable | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Button/clang-19/tree_input_TreeActivateAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Button/clang-19/cursor_CursorHoverFrameStart | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Button/clang-19/style_ResolveFlags | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Button/clang-19/tree_input_TreeTakeActivationAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang-19/style_ResolveState | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Button/clang-19/style_ResolveFont | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Button/clang-19/style_StyleFontValue | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Button/clang-19/font_metrics_GlyphWidth | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/font_metrics_GlyphLineHeight | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/button_ButtonDetailSize | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 |
| Button/clang-19/button_behavior_MeasureBounds | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 |
| Button/clang-19/style_StyleMinHeightValue | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang-19/button_ButtonMinimumHeight | 0 | 0/327 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/button_ButtonFitContentFace | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 |
| Button/clang-19/tree_TreeSetSemanticDetail | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/clang-19/button_PaintButtonForNode | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 |
| Button/clang-19/tree_draw_FrameInputCommit | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Button/clang-19/paint_queue_PaintOverflow | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/clang-19/tree_TreeCancel | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Button/clang-19/paint_queue_PaintOrder | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Button/clang-19/paint_queue_SnapshotTextRead | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Button/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Button/clang-19/paint_queue_PaintPointCount | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Button/clang-19/paint_queue_EmitPaint | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Button/clang-19/paint_queue_PendingPaintAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Button/clang-19/input_protocol_PointInBounds | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Button/clang-19/tree_input_TreeTopModalLayer | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/clang-19/tree_TreeButtonLike | 0 | 0/327 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_PointInCircleBounds | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/clang-19/tree_TreeOverlayLayerAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/std_utf8_Decode | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Button/clang-19/std_grapheme_Push | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Button/clang-19/tree_TreeIdentityHash | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang-19/tree_input_TreeTakePointerActivationAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 205/20/5/34/39/112/16/25/20/12/17/6/0/0/5 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 |
| Button/clang-19/button_ButtonFaceFinish | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Button/clang-19/style_sheet_ResolveInstalledStyleRules | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 293/260/17/62/47/150/26/36/19/11/16/6/0/0/40 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 |
| Button/clang-19/button_ButtonDefaultFace | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Button/clang-19/button_ButtonFaceFacts | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Button/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/327 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/button_ButtonDetailFont | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Button/clang-19/style_StyleLineHeightValue | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang-19/style_FitHeight | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Button/clang-19/button_behavior_MeasureContent | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 |
| Button/clang-19/button_behavior_ShapeWidth | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Button/clang-19/theme_ControlHeightFor | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Button/clang-19/style_StyleOpacityValue | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonCornerRadius | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Button/clang-19/drawing_props_ColorFromPacked | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/surface_Opacity | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Button/clang-19/paint_queue_PaintRoundedFill | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Button/clang-19/paint_queue_PaintRoundedOutline | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Button/clang-19/focus_FocusDefaultOutlineBounds | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Button/clang-19/image_lookup_IconLookupImage | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonPictureSize | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonLabeledPictureSize | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Button/clang-19/button_behavior_ContentLayout | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_PaintSvgIcon | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 388/333/6/71/75/167/7/29/23/22/117/4/1/0/40 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 |
| Button/clang-19/image_lookup_ImageLookupTexture | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Button/clang-19/image_behavior_ImageDrawPlanFor | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Button/clang-19/paint_queue_PaintLine | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Button/clang-19/paint_queue_PaintClippedLabel | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Button/clang-19/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Button/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Button/clang-19/paint_queue_ResolvePaintText | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/std_utf8_Continuation | 0 | 0/327 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/std_grapheme_data_Property | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Button/clang-19/std_grapheme_IsControl | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/session_WidgetPartKey | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/327 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleBucket | 0 | 0/327 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleFirstBit | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Button/clang-19/style_sheet_StyleRuleMatchesState | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleSelectorMatches | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Button/clang-19/style_MergeValues | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Button/clang-19/theme_CurrentTheme | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/theme_StateLayer | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Button/clang-19/card_behavior_CardDefaultFace | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 |
| Button/clang-19/theme_ShapeMedium | 0 | 0/327 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/theme_TypeLabel | 0 | 0/327 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/theme_FontForRole | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Button/clang-19/theme_MotionDuration | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Button/clang-19/tree_TreeLabelSpanAt | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Button/clang-19/tree_TreeSetLabelSpan | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 |
| Button/clang-19/surface_Unit | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Button/clang-19/paint_queue_ClipShape | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Button/clang-19/paint_queue_QueuePaint | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Button/clang-19/paint_queue_EmitRoundedOutline | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Button/clang-19/drawing_props_PaintPixelScale | 0 | 0/327 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconSlot | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 246/224/9/35/36/134/27/22/13/11/12/14/0/0/54 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 |
| Button/clang-19/svg_icon_behavior_SvgIconSnap | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconTexture | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 240/130/10/33/28/120/24/19/9/3/12/16/1/0/30 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 |
| Button/clang-19/paint_queue_PaintImageWithPolygon | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Button/clang-19/paint_queue_EmitLine | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Button/clang-19/paint_queue_EmitLabel | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Button/clang-19/paint_queue_ClipLabel | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Button/clang-19/paint_queue_PendingTextValue | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Button/clang-19/style_sheet_StyleMatchesOptional | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/theme_DefaultTheme | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Button/clang-19/theme_ShapeSurface | 0 | 0/327 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/style_sheet_StyleControlRoleFacts | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_ShapeVisible | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Button/clang-19/paint_queue_ShapeWhole | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Button/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_FillShapeClipped | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Button/clang-19/paint_queue_EmitPaintNow | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Button/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Button/clang-19/paint_queue_KeepPaintMesh | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Button/clang-19/raster_shape_StrokeRoundedRectangle | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconRetire | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeRaster | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 |
| Button/clang-19/image_behavior_ImageFitBounds | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 |
| Button/clang-19/image_behavior_ImagePolygonValid | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Button/clang-19/paint_queue_EmitImage | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Button/clang-19/paint_queue_ClipImage | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Button/clang-19/raster_StrokeLine | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/text_typeface_TextTypefaceUse | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/raster_text_PaintTextRotated | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Button/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Button/clang-19/raster_text_PaintTextClipped | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang-19/raster_text_PaintText | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/theme_DarkPalette | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/clang-19/theme_LightPalette | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/clang-19/theme_DefaultMetrics | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Button/clang-19/theme_DefaultShadows | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Button/clang-19/theme_DefaultMotion | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Button/clang-19/theme_DefaultFonts | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleControlFacts | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Button/clang-19/paint_queue_ShapeRadius | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Button/clang-19/paint_queue_FillClippedSpan | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Button/clang-19/paint_queue_KeepPaintBytes | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Button/clang-19/paint_queue_KeepMeshPoints | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Button/clang-19/svg_shape_ShapeNumber | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 |
| Button/clang-19/svg_shape_ShapeCoverEdges | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 |
| Button/clang-19/paint_queue_EmitImageBegin | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RasterImage | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Button/clang-19/paint_queue_EmitImageEnd | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 24/0/0/4/4/18/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleDefaultFacts | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RecordBounds | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Button/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Button/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Button/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Button/clang-19/svg_shape_ShapeIsDigit | 0 | 0/327 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeIsSpace | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang-19/svg_shape_ShapeAddPoint | 0 | 0/327 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAddSpan | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 |
| Button/clang-19/paint_queue_ShapeSquareRoot | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Button/clang-19/paint_queue___zi_spec_9308732530dd822f | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/paint_queue___zi_spec_27099373fae5fe98 | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Button/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/paint_queue___zi_spec_17277838aa4fe0e7 | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Button/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/paint_queue___zi_spec_b997e17bf3928592 | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Button/clang-19/svg_shape_ShapeStrokeSubpath | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 |
| Button/clang-19/svg_shape_ShapeFillSubpath | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 |
| Button/clang-19/svg_shape_ShapeCollapse | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Button/clang-19/svg_shape_ShapeAddCircle | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Button/clang-19/svg_shape_ShapeQuad | 0 | 0/327 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAbs | 0 | 0/327 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeTriangle | 0 | 0/327 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAddEdge | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Button/clang-19/svg_shape_ShapeAddPolygon | 0 | 0/327 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 |
| Text/cc/widget_benchmarks_TextCase | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 285/159/9/34/82/140/3/30/21/16/47/0/0/0/27 | 140/128/10/16/20/66/4/16/6/7/32/0/0/1/29 |
| Text/cc/widget_benchmarks_BeginCase | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Text/cc/tree_draw_BeginFrame | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Text/cc/text___zi_spec_6a19f7c98e79a734 | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_draw_EndFrame | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Text/cc/tree_TreeNodeAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/247 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/247 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_request_TextRequestSend | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Text/cc/text_request_TextRequestClear | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Text/cc/std_grapheme_Boundary | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Text/cc/session_SessionOpen | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Text/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/247 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/cc/tree_input_PointerSample | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Text/cc/tree_input_SecondaryPointerSample | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Text/cc/session_SessionFlagSet | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Text/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Text/cc/tree_input_TreeKeyboardRoute | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Text/cc/text_editing_TextEditingFrameBegin | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_PaintClear | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Text/cc/text_DrawSession | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 293/263/10/101/82/188/2/15/5/17/159/0/0/0/13 |
| Text/cc/session_SessionFlagGet | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/tree_draw_CommitFrame | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Text/cc/paint_queue_PaintSnapshotEmit | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Text/cc/paint_queue_PaintFlush | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Text/cc/std_utf8_Valid | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Text/cc/tree_TreeTextRange | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Text/cc/tree_input_TreeFocusAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Text/cc/tree_input_TreeFocusedAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Text/cc/tree_TreeInputAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Text/cc/geometry_RectangleIntersection | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Text/cc/tree_TreePointerActivatable | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Text/cc/tree_input_TreeActivateAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 44/20/1/5/0/9/1/2/1/3/0/0/0/1/1 |
| Text/cc/cursor_CursorHoverFrameStart | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Text/cc/std_math_Finite32 | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 11/0/0/0/0/3/0/2/2/0/4/0/0/0/0 |
| Text/cc/tree_TreeRootBounds | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Text/cc/text_PrepareText | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 263/81/2/62/65/169/0/13/11/15/16/0/0/0/2 | 172/91/3/37/23/69/2/7/4/14/32/0/0/3/4 |
| Text/cc/geometry_RotatedRectangleBounds | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 209/114/8/44/72/120/1/16/8/6/94/0/0/0/6 | 81/32/1/8/14/25/0/1/0/2/56/0/0/3/4 |
| Text/cc/text_TextOrigin | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Text/cc/text_TextHiddenLines | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 118/39/2/11/2/22/0/11/9/10/22/0/0/0/4 | 86/45/3/10/2/22/7/11/4/2/22/0/0/0/6 |
| Text/cc/tree_TreeSetHiddenLines | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Text/cc/text_PaintTextLines | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 694/612/7/184/149/360/10/49/42/45/86/0/0/0/42 | 585/494/12/195/126/348/8/37/15/15/222/0/0/8/37 |
| Text/cc/tree_draw_FrameInputCommit | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Text/cc/paint_queue_PaintOverflow | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/cc/tree_TreeCancel | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Text/cc/paint_queue_PaintOrder | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Text/cc/paint_queue_SnapshotTextRead | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_SnapshotTextJoin | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Text/cc/paint_queue_PaintPointCount | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Text/cc/paint_queue_PendingPaintAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Text/cc/tree_input_TreeTopModalLayer | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_TreeModalLayerAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/text_DefaultTextFace | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/7/0/0/0/1/0/0/0/0/0 |
| Text/cc/text_TextFaceFor | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 38/0/0/9/5/17/0/0/0/1/10/0/0/1/0 |
| Text/cc/text_behavior_ResolveTextStyle | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 79/58/6/8/3/19/2/14/6/1/11/0/0/0/3 |
| Text/cc/style_ResolveFont | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Text/cc/style_StyleFontValue | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Text/cc/drawing_props_ColorFromPacked | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/surface_Opacity | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Text/cc/style_StyleOpacityValue | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Text/cc/font_metrics_GlyphLineHeight | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_layout_TextDefaultLineGap | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 14/0/0/0/0/4/0/3/1/0/7/0/0/0/0 |
| Text/cc/text_behavior_TextWrapPolicy | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/text_layout_NextTextLine | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 721/657/29/136/252/457/20/70/41/26/24/12/1/0/61 | 264/212/25/12/8/101/33/43/17/3/11/2/0/14/30 |
| Text/cc/text_behavior_TextExtent | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/cc/text_layout_TextLayoutTotalHeight | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 13/0/0/0/0/0/0/2/2/0/0/2/0/0/0 |
| Text/cc/geometry_WorldToScreen | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 42/0/0/8/3/11/0/0/0/2/35/0/0/0/0 |
| Text/cc/text_behavior_TextAlignmentOffset | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 18/0/0/0/0/3/1/2/2/0/9/0/0/0/0 |
| Text/cc/text_layout_TextCharactersEnd | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 31/8/1/0/0/1/0/4/3/1/0/0/0/0/1 |
| Text/cc/text_TextSpan | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/cc/font_metrics_GlyphWidth | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_TextEllipsisStart | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 49/33/3/2/2/8/2/6/3/1/0/0/0/0/5 |
| Text/cc/text_layout_TextLineXFor | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 15/0/0/0/0/1/1/1/1/0/0/0/0/2/0 |
| Text/cc/text_PaintTextLineRuns | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/323/7/109/109/237/12/16/9/13/36/0/0/0/14 | 280/230/5/89/52/150/5/9/4/11/101/2/0/4/22 |
| Text/cc/paint_queue_PaintClippedLabel | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Text/cc/text_behavior_TextStrikethroughBounds | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 9/0/0/0/0/2/0/0/0/0/8/0/0/0/0 |
| Text/cc/text_PaintTextRule | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 131/0/0/27/26/54/0/4/2/6/72/0/0/0/0 |
| Text/cc/cursor_CursorHoverFrameCommit | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Text/cc/paint_queue_PaintMeshPointCount | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Text/cc/paint_queue_EmitPaintDirect | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Text/cc/paint_queue_ResolvePaintText | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Text/cc/tree_TreeModalLayer | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/std_utf8_Continuation | 0 | 0/247 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_grapheme_IsControl | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/session_WidgetPartKey | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/widget_key_WidgetRepeatedPart | 0 | 0/247 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_style_TextStyleFor | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 27/0/0/4/2/12/1/1/1/1/8/0/0/0/0 |
| Text/cc/surface_Unit | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/247 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/font_metrics_GlyphMetricsRevision | 0 | 0/247 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_NextTextLineUncached | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 352/318/8/47/23/84/6/28/20/6/12/0/0/13/29 |
| Text/cc/text_layout_TextLineHash | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/2/0/2/1/0/0/1/0/0/1 |
| Text/cc/text_layout_TextLineCacheMatches | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 25/21/6/1/0/10/0/9/3/0/1/0/0/0/0 |
| Text/cc/std_math_SineDegrees | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 249/141/4/0/0/0/0/21/17/12/102/0/0/0/27 | 95/49/3/0/0/28/2/16/11/0/55/0/0/0/0 |
| Text/cc/std_math_CosineDegrees | 0 | 0/247 0% |  | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/1/0/0/0/0 |
| Text/cc/tree_TreeLabelSpanAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Text/cc/tree_TreeSetLabelSpan | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 |
| Text/cc/text_TextLinkRunAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 75/52/2/4/0/11/7/11/9/0/0/0/0/3/7 |
| Text/cc/text_TextLinkColor | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 21/0/0/0/0/1/0/0/0/3/0/2/0/4/0 |
| Text/cc/paint_queue_EmitLabel | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Text/cc/paint_queue_ClipLabel | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Text/cc/paint_queue_QueuePaint | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Text/cc/text_TextRangeValid | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 32/13/3/0/0/3/2/4/1/0/0/0/0/2/3 |
| Text/cc/paint_queue_PaintClippedEmphasis | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 45/0/0/4/16/23/0/0/0/1/6/0/0/0/0 |
| Text/cc/paint_queue_PaintRoundedFill | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Text/cc/shape_paint_PaintTriangleShape | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 44/0/0/0/7/10/0/0/0/1/18/0/0/2/0 |
| Text/cc/theme_CurrentTheme | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/text_layout_TextTokenNext | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 259/237/31/6/4/46/10/62/31/6/0/0/0/4/53 |
| Text/cc/drawing_props_StyleTokenColorOr | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 92/84/5/10/5/33/9/13/8/5/4/3/0/0/18 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_typeface_TextTypefaceUse | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Text/cc/raster_text_PaintTextRotated | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Text/cc/raster_text_PaintTextItalicClipped | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Text/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Text/cc/raster_text_PaintTextClipped | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Text/cc/raster_text_PaintText | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_EmitPaintNow | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Text/cc/tree_TreeCurrentPaintGeometry | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Text/cc/paint_queue_ClipShape | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Text/cc/shape_paint_ShapeOffset | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_SubmitShape | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/4/11/0/0/0/1/0/0/0/0/0 |
| Text/cc/theme_DefaultTheme | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Text/cc/text_layout_TextIsSpace | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Text/cc/text_emphasis_RunTextItalicRaster | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterText | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Text/cc/paint_queue_KeepPaintBytes | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Text/cc/paint_queue_KeepMeshPoints | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 178/148/12/9/11/52/15/26/14/4/21/0/0/6/20 |
| Text/cc/paint_queue_ShapeVisible | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Text/cc/paint_queue_ShapeWhole | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Text/cc/raster_shape_FillRoundedRectangle | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_FillShapeClipped | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Text/cc/shape_paint_EmitShape | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 25/0/0/4/0/8/1/11/7/0/3/0/0/0/0 |
| Text/cc/paint_queue_SubmitPaint | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 84/12/3/12/21/65/6/4/1/2/6/0/0/0/3 | 11/0/0/4/0/5/0/0/0/1/0/0/0/0/0 |
| Text/cc/theme_DarkPalette | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/theme_LightPalette | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/theme_DefaultMetrics | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Text/cc/theme_DefaultShadows | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Text/cc/theme_DefaultMotion | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Text/cc/theme_DefaultFonts | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextIdeographic | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 24/8/2/0/0/6/0/4/2/0/0/0/0/0/2 |
| Text/cc/text_layout_TextNoBreakBefore | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 114/105/18/0/0/12/2/28/10/0/0/0/0/0/37 |
| Text/cc/text_layout_TextNoBreakAfter | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 36/19/2/0/0/4/1/7/5/0/0/0/0/0/6 |
| Text/cc/paint_queue_KeepShortPaintBytes | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Text/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Text/cc/paint_queue_KeepLongPaintBytes | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Text/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Text/cc/paint_queue_ShapeRadius | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Text/cc/paint_queue_FillShapeRows | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Text/cc/paint_queue_FillClippedSpan | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Text/cc/shape_paint_EmitLineShape | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 180/12/3/42/40/132/13/7/4/12/11/0/0/0/3 | 102/16/1/19/0/39/5/5/4/5/44/0/0/2/1 |
| Text/cc/shape_paint_EmitCircleShape | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 228/121/6/50/41/93/6/22/16/17/61/0/0/0/16 | 176/124/9/25/14/65/1/20/11/8/53/0/0/3/14 |
| Text/cc/shape_paint_EmitRingShape | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 108/82/9/20/6/42/1/14/5/8/45/0/0/0/9 |
| Text/cc/shape_paint_EmitTriangleShape | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 37/12/1/7/0/19/1/3/2/2/9/0/0/0/3 |
| Text/cc/paint_queue___zi_spec_9308732530dd822f | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Text/cc/paint_queue___zi_spec_27099373fae5fe98 | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Text/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Text/cc/paint_queue___zi_spec_17277838aa4fe0e7 | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Text/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Text/cc/paint_queue___zi_spec_b997e17bf3928592 | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Text/cc/widget_benchmark_host_RecordBounds | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Text/cc/paint_queue_ShapeCornerInset | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Text/cc/shape_paint_ShapeAt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 5/0/0/1/0/1/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_LineCornersFor | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 96/0/0/8/6/19/0/5/5/3/63/0/0/2/0 |
| Text/cc/shape_paint_ShapeBatchBegin | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/cc/shape_paint_ShapeHidden | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 96/8/2/15/8/32/4/13/11/4/24/0/0/0/2 | 31/20/3/0/0/6/0/5/2/0/18/0/0/2/2 |
| Text/cc/shape_paint_PointsBounds | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 127/109/5/22/20/50/1/11/6/5/45/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchTriangle | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 395/55/8/85/75/211/20/37/29/29/78/0/0/0/10 | 215/178/9/33/33/78/6/16/6/12/113/0/0/2/11 |
| Text/cc/shape_paint_ShapeBatchFlush | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 86/16/4/6/18/48/9/10/6/4/8/0/0/0/4 | 19/0/0/0/0/6/0/2/2/1/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcDirections | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/56/2/10/12/30/3/7/5/3/25/0/0/0/5 | 48/18/1/4/5/15/3/3/2/2/21/0/0/0/1 |
| Text/cc/shape_paint_ShapeFadeBands | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 77/17/1/0/0/3/2/2/1/2/3/2/0/9/3 |
| Text/cc/shape_paint_ShapeBatchSector | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 35/15/1/3/3/11/2/2/1/1/17/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchBand | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 59/36/1/7/5/15/1/2/1/2/37/0/0/0/1 |
| Text/cc/paint_queue_ShapeSquareRoot | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Text/cc/std_math_Sqrt | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 100/80/5/0/0/24/1/17/11/0/57/0/0/2/10 |
| Text/cc/shape_paint_ShapeBatchAdd | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 127/80/3/13/12/54/9/12/9/7/16/0/0/0/15 | 79/36/3/2/2/20/7/8/5/2/28/0/0/0/5 |
| Text/cc/raster_triangles_FillTriangles | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeChannelChange | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_math_CeilInteger | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/3/1/0/7/0/0/0/0 |
| Text/cc/paint_queue_GradientChannel | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 33/0/0/0/0/7/3/4/3/0/17/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcPoint | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/8/0/0/0/0 |
| Text/cc/std_math_NotANumber | 0 | 0/247 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/std_math_Finite64 | 0 | 0/247 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 10/0/0/0/0/2/0/2/2/0/4/0/0/0/0 |
| Text/cc/std_math_ScaleByPowerOfTwo | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 74/48/7/0/0/15/4/17/10/0/21/0/0/0/6 |
| Text/cc/shape_paint_ShapeInside | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 16/0/0/0/0/1/1/3/3/0/4/0/0/1/0 |
| Text/cc/shape_paint_ShapeCrossing | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 44/20/1/0/0/2/1/6/5/0/30/0/0/2/1 |
| Text/cc/widget_benchmark_host_RasterTriangles | 0 | 0/247 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/36/1/6/1/18/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/9/2/4/1/0/6/0/0/1/2 |
| Text/clang-19/widget_benchmarks_TextCase | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 285/159/9/34/82/140/3/30/21/16/47/0/0/0/27 | 155/66/6/12/15/54/0/20/14/14/19/0/0/1/12 |
| Text/clang-19/widget_benchmarks_BeginCase | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_draw_BeginFrame | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Text/clang-19/text___zi_spec_6a19f7c98e79a734 | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang-19/tree_draw_EndFrame | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Text/clang-19/tree_TreeNodeAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/286 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/286 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_request_TextRequestSend | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Text/clang-19/text_request_TextRequestClear | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/286 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/session_SessionIndex | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Text/clang-19/tree_input_PointerSample | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Text/clang-19/tree_input_SecondaryPointerSample | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Text/clang-19/session_SessionFlagSet | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Text/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 379/237/17/42/15/132/43/58/41/21/26/4/0/5/35 |
| Text/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_PaintClear | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Text/clang-19/text_DrawSession | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 293/19/2/36/34/100/0/25/23/20/108/1/0/2/2 |
| Text/clang-19/session_SessionFlagGet | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang-19/tree_draw_CommitFrame | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Text/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Text/clang-19/paint_queue_PaintFlush | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 |
| Text/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_TreeTextRange | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Text/clang-19/tree_input_TreeFocusAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Text/clang-19/tree_input_TreeFocusedAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 112/12/3/8/17/51/11/17/14/7/6/3/0/0/3 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Text/clang-19/tree_TreeInputAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 196/20/5/19/58/140/15/13/8/4/19/5/0/0/5 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Text/clang-19/geometry_RectangleIntersection | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Text/clang-19/tree_TreePointerActivatable | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Text/clang-19/tree_input_TreeActivateAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 |
| Text/clang-19/cursor_CursorHoverFrameStart | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Text/clang-19/std_math_Finite32 | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/tree_TreeRootBounds | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Text/clang-19/text_PrepareText | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 263/81/2/62/65/169/0/13/11/15/16/0/0/0/2 | 134/18/1/13/2/44/0/6/5/14/18/0/0/3/3 |
| Text/clang-19/geometry_RotatedRectangleBounds | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/114/8/44/72/120/1/16/8/6/94/0/0/0/6 | 124/0/0/28/18/48/0/0/0/10/111/0/0/0/0 |
| Text/clang-19/text_TextOrigin | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/text_TextHiddenLines | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/39/2/11/2/22/0/11/9/10/22/0/0/0/4 | 112/19/2/2/1/17/1/11/9/3/51/1/0/0/2 |
| Text/clang-19/tree_TreeSetHiddenLines | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 119/12/3/15/19/60/10/13/10/7/6/4/0/0/3 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang-19/text_selection_host_TextSelectionPresent | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 |
| Text/clang-19/tree_draw_FrameInputCommit | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintOverflow | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/clang-19/tree_TreeCancel | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Text/clang-19/paint_queue_PaintOrder | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Text/clang-19/paint_queue_SnapshotTextRead | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Text/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Text/clang-19/paint_queue_PaintPointCount | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Text/clang-19/paint_queue_EmitPaint | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Text/clang-19/paint_queue_PendingPaintAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Text/clang-19/tree_input_TreeTopModalLayer | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang-19/std_utf8_Decode | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Text/clang-19/std_grapheme_Push | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Text/clang-19/tree_TreeIdentityHash | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang-19/text_DefaultTextFace | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 |
| Text/clang-19/text_TextFaceFor | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 |
| Text/clang-19/text_behavior_ResolveTextStyle | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 57/0/0/5/0/8/0/3/3/2/7/0/0/2/0 |
| Text/clang-19/style_ResolveFont | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Text/clang-19/style_StyleFontValue | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Text/clang-19/theme_CurrentText | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/drawing_props_ColorFromPacked | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/surface_Opacity | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Text/clang-19/style_StyleOpacityValue | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphLineHeight | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_layout_TextDefaultLineGap | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Text/clang-19/text_behavior_TextWrapPolicy | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/text_layout_NextTextLine | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 721/657/29/136/252/457/20/70/41/26/24/12/1/0/61 | 345/109/17/14/7/108/42/57/40/5/19/10/0/13/13 |
| Text/clang-19/text_behavior_TextExtent | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/clang-19/text_layout_TextLayoutTotalHeight | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 |
| Text/clang-19/geometry_WorldToScreen | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 |
| Text/clang-19/text_behavior_TextAlignmentOffset | 0 | 0/286 0% | calls/inlining candidates | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 |
| Text/clang-19/text_layout_TextCharactersEnd | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 |
| Text/clang-19/text_TextSpan | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphWidth | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_TextEllipsisStart | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 55/23/3/2/2/9/3/7/4/1/0/0/0/0/2 |
| Text/clang-19/text_layout_TextLineXFor | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 |
| Text/clang-19/text_PaintTextLineRuns | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/323/7/109/109/237/12/16/9/13/36/0/0/0/14 | 301/250/14/75/45/157/7/25/11/16/80/2/0/1/20 |
| Text/clang-19/paint_queue_PaintClippedLabel | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Text/clang-19/text_PaintTextEmphasis | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 221/165/17/28/27/87/10/29/12/5/13/2/0/3/17 |
| Text/clang-19/text_behavior_TextStrikethroughBounds | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 |
| Text/clang-19/text_PaintTextRule | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 101/0/0/26/22/52/0/4/2/6/51/0/0/0/0 |
| Text/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Text/clang-19/paint_queue_EmitPaintDirect | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Text/clang-19/paint_queue_ResolvePaintText | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_TreeModalLayer | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 32/4/1/4/7/18/3/2/1/2/2/1/0/0/1 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/std_utf8_Continuation | 0 | 0/286 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/std_grapheme_data_Property | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Text/clang-19/std_grapheme_IsControl | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/session_WidgetPartKey | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/286 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_style_TextStyleFor | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 |
| Text/clang-19/surface_Unit | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/286 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphMetricsRevision | 0 | 0/286 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_layout_NextTextLineUncached | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 |
| Text/clang-19/text_layout_TextLineCacheMatches | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 |
| Text/clang-19/std_math_SineDegrees | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 249/141/4/0/0/0/0/21/17/12/102/0/0/0/27 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 |
| Text/clang-19/tree_TreeLabelSpanAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Text/clang-19/tree_TreeSetLabelSpan | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 |
| Text/clang-19/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Text/clang-19/text_TextLinkRunAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 57/27/7/2/0/10/5/14/7/0/0/0/0/0/2 |
| Text/clang-19/text_TextLinkColor | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 |
| Text/clang-19/paint_queue_EmitLabel | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Text/clang-19/paint_queue_ClipLabel | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Text/clang-19/text_TextRangeValid | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 29/0/0/0/0/2/2/3/3/0/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintClippedEmphasis | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 |
| Text/clang-19/paint_queue_PaintRoundedFill | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Text/clang-19/shape_paint_PaintTriangleShape | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 |
| Text/clang-19/paint_queue_PendingTextValue | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Text/clang-19/theme_CurrentTheme | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/drawing_props_StyleTokenColorOr | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 92/84/5/10/5/33/9/13/8/5/4/3/0/0/18 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/theme_CurrentAccent | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_typeface_TextTypefaceUse | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/raster_text_PaintTextRotated | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Text/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Text/clang-19/raster_text_PaintTextClipped | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang-19/raster_text_PaintText | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_EmitPaintNow | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Text/clang-19/tree_TreeOverlayLayerAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Text/clang-19/paint_queue_KeepPaintMesh | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Text/clang-19/paint_queue_ClipShape | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeOffset | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/shape_paint_SubmitShape | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 |
| Text/clang-19/theme_DefaultTheme | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Text/clang-19/text_layout_TextIsSpace | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/paint_queue_KeepPaintBytes | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Text/clang-19/paint_queue_KeepMeshPoints | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Text/clang-19/paint_queue_ShapeVisible | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Text/clang-19/paint_queue_ShapeWhole | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Text/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_FillShapeClipped | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Text/clang-19/shape_paint_EmitShape | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 |
| Text/clang-19/paint_queue_SubmitPaint | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 84/12/3/12/21/65/6/4/1/2/6/0/0/0/3 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 |
| Text/clang-19/theme_DarkPalette | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/theme_LightPalette | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/theme_DefaultMetrics | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Text/clang-19/theme_DefaultShadows | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Text/clang-19/theme_DefaultMotion | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Text/clang-19/theme_DefaultFonts | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_layout_TextIdeographic | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Text/clang-19/text_layout_TextNoBreakAfter | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 |
| Text/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Text/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Text/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Text/clang-19/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Text/clang-19/paint_queue_ShapeRadius | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Text/clang-19/paint_queue_FillClippedSpan | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Text/clang-19/shape_paint_EmitLineShape | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 180/12/3/42/40/132/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 |
| Text/clang-19/shape_paint_EmitRingShape | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 182/57/3/32/13/80/9/21/18/9/103/0/0/0/0 |
| Text/clang-19/shape_paint_EmitTriangleShape | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 |
| Text/clang-19/paint_queue___zi_spec_9308732530dd822f | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/paint_queue___zi_spec_27099373fae5fe98 | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Text/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/paint_queue___zi_spec_17277838aa4fe0e7 | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Text/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/paint_queue___zi_spec_b997e17bf3928592 | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Text/clang-19/widget_benchmark_host_RecordBounds | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeAt | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/shape_paint_LineCornersFor | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchBegin | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeHidden | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 96/8/2/15/8/32/4/13/11/4/24/0/0/0/2 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 |
| Text/clang-19/shape_paint_PointsBounds | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 127/109/5/22/20/50/1/11/6/5/45/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/3 |
| Text/clang-19/shape_paint_ShapeBatchFlush | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 86/16/4/6/18/48/9/10/6/4/8/0/0/0/4 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeArcDirections | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/56/2/10/12/30/3/7/5/3/25/0/0/0/5 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeFadeBands | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 |
| Text/clang-19/shape_paint_ShapeBatchSector | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchBand | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 |
| Text/clang-19/paint_queue_ShapeSquareRoot | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchAdd | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 127/80/3/13/12/54/9/12/9/7/16/0/0/0/15 | 63/0/0/6/4/21/3/6/6/2/29/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeClipSide | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 |
| Text/clang-19/raster_triangles_FillTriangles | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeChannelChange | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/std_math_CeilInteger | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 |
| Text/clang-19/paint_queue_GradientChannel | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 |
| Text/clang-19/shape_paint_ShapeArcPoint | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 |
| Text/clang-19/std_math_NotANumber | 0 | 0/286 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 |
| Text/clang-19/std_math_Finite64 | 0 | 0/286 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/std_math_ScaleByPowerOfTwo | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 89/35/5/0/0/15/4/22/17/0/34/0/0/1/4 |
| Text/clang-19/shape_paint_ShapeTurn | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeInside | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeCrossing | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_RasterTriangles | 0 | 0/286 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/36/1/6/1/18/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 |
| Input/cc/widget_benchmarks_InputCase | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 164/152/15/12/15/57/0/17/2/11/8/0/0/0/42 |
| Input/cc/widget_benchmarks_BeginCase | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Input/cc/tree_draw_BeginFrame | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Input/cc/textfield_Draw | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 195/136/7/50/17/84/3/18/11/9/16/0/0/3/19 |
| Input/cc/tree_draw_EndFrame | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Input/cc/tree_TreeNodeAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/391 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/391 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_request_TextRequestSend | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Input/cc/text_request_TextRequestClear | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Input/cc/std_grapheme_Boundary | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Input/cc/session_SessionOpen | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Input/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/391 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointerSample | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Input/cc/tree_input_SecondaryPointerSample | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Input/cc/session_SessionFlagSet | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Input/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Input/cc/tree_input_TreeKeyboardRoute | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Input/cc/text_editing_TextEditingFrameBegin | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_PaintClear | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Input/cc/textfield_TextFieldKey | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 7/0/0/2/0/2/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextSelectionRangeForLength | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuTakeCommand | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 161/110/7/25/14/55/0/31/24/14/0/0/0/0/18 | 66/46/6/2/0/14/2/13/7/3/0/0/0/0/6 |
| Input/cc/text_context_menu_TextMenuPasteText | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuOpenRequested | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 63/22/1/6/5/13/1/4/3/4/11/0/0/1/1 |
| Input/cc/textfield_TextFieldCore | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3950/3778/52/857/1164/2342/28/400/348/251/279/1/1/0/265 | 2425/2344/104/678/397/1177/25/259/140/150/359/2/0/15/320 |
| Input/cc/system_clipboard_SystemClipboardSet | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_platform_TextInputNoteFocused | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/0/0/19/20/59/4/3/3/6/0/4/0/0/0 | 54/20/3/0/0/19/6/5/2/3/0/0/0/0/3 |
| Input/cc/text_context_menu_TextMenuIsOpenFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 52/0/0/4/4/18/0/12/12/3/0/0/0/0/0 | 18/4/1/0/0/4/0/4/3/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuShow | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 17/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/cc/session_SessionFlagGet | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/tree_draw_CommitFrame | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Input/cc/paint_queue_PaintSnapshotEmit | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Input/cc/paint_queue_PaintFlush | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Input/cc/tree_input_TreeTextKind | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeFocusAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Input/cc/std_utf8_Valid | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Input/cc/tree_TreeTextRange | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Input/cc/std_grapheme_Next | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/geometry_RectangleIntersection | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Input/cc/tree_TreePointerActivatable | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Input/cc/tree_input_TreeActivateAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 |
| Input/cc/cursor_CursorHoverFrameStart | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Input/cc/text_input_TextSelectionRangeFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/1/1/0/0/0/0/0/0 |
| Input/cc/system_clipboard_SystemClipboardText | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 73/46/8/2/1/11/6/14/6/2/0/0/0/0/6 |
| Input/cc/input_protocol_PointInBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 |
| Input/cc/text_context_menu_TextMenuOpenAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 61/12/2/3/0/16/2/6/4/2/2/0/0/0/2 |
| Input/cc/tree_input_TreeTakeDragAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 443/56/14/55/84/259/49/47/33/17/45/21/0/0/14 | 173/148/11/13/4/58/10/25/14/6/22/0/0/8/20 |
| Input/cc/tree_input_TreeBlurAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 98/12/3/5/15/46/11/15/12/6/6/3/0/0/3 | 37/7/1/0/0/9/4/5/4/1/0/1/0/1/2 |
| Input/cc/textfield_TextFieldFaceFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 179/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 121/74/1/30/15/63/1/2/1/10/52/0/0/0/4 |
| Input/cc/style_ResolveFont | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Input/cc/style_StyleFontValue | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Input/cc/text_input_TextInputScaledMetric | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 14/0/0/0/0/2/0/3/1/0/7/0/0/0/0 |
| Input/cc/textfield_TextFieldSecureDisplay | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 49/20/2/1/0/5/1/5/3/2/0/0/0/0/3 |
| Input/cc/font_metrics_GlyphWidth | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldScrollFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 61/0/0/0/0/5/2/6/2/0/7/0/0/2/0 |
| Input/cc/textfield_TextFieldCursorAtX | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 66/38/2/8/10/20/1/4/2/3/0/0/0/0/2 |
| Input/cc/text_input_TextPointerOffset | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 37/0/0/0/0/6/0/6/3/0/15/0/0/0/0 |
| Input/cc/textfield_TextFieldCodepoints | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 29/7/1/0/0/5/2/2/1/1/0/0/0/0/3 |
| Input/cc/text_TextSpan | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldByteAfter | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 39/21/2/0/0/2/1/5/3/1/0/0/0/0/2 |
| Input/cc/text_input_CompositionTransitionFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 159/126/11/16/29/59/8/28/17/0/23/0/0/0/13 |
| Input/cc/textfield_TextFieldNext | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextSelectionAfterMove | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 15/11/1/2/3/6/1/2/1/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldInputTextAllowed | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 21/8/1/0/0/6/3/5/4/0/0/0/0/0/3 |
| Input/cc/textfield_TextFieldReplacementAllowed | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 133/98/8/12/3/35/9/24/16/4/0/0/0/0/9 |
| Input/cc/text_input_TextEditCommandDecisionFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/88/8/4/3/13/4/18/10/0/0/0/0/5/8 |
| Input/cc/textfield_TextFieldEditFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 106/75/9/16/12/37/1/14/5/5/12/0/0/0/6 |
| Input/cc/text_request_TextRequestTake | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 253/28/7/53/42/148/24/24/17/12/14/9/0/0/7 | 79/0/0/4/3/24/4/8/8/4/0/1/0/0/0 |
| Input/cc/text_request_TextRequestChangeFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 280/263/12/53/38/116/5/36/24/13/0/0/0/0/24 | 160/128/12/20/5/57/8/24/12/6/0/0/0/0/9 |
| Input/cc/font_metrics_GlyphCaret | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 34/0/0/0/0/5/1/2/2/1/0/0/0/0/0 |
| Input/cc/text_input_TextFieldRevealScroll | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 25/0/0/0/0/2/2/1/1/0/0/0/0/0/0 |
| Input/cc/style_StyleOpacityValue | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Input/cc/tree_input_TreeFocusSubmittedAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 289/24/6/42/46/139/20/34/28/15/20/7/0/0/6 | 158/139/17/8/16/61/7/23/6/6/25/3/0/5/15 |
| Input/cc/tree_TreeSetWidgetState | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 272/140/8/40/42/162/26/23/15/8/14/13/0/0/27 | 155/104/7/15/5/48/10/26/19/3/2/4/0/3/14 |
| Input/cc/paint_queue_PaintRoundedFill | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Input/cc/drawing_props_ColorFromPacked | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/surface_Opacity | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Input/cc/paint_queue_PaintRoundedOutline | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 |
| Input/cc/font_metrics_GlyphLineHeight | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldPaintFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 179/0/0/22/20/53/0/13/13/12/47/1/1/0/0 | 120/0/0/2/0/13/2/13/5/0/41/0/0/4/0 |
| Input/cc/paint_queue_PaintClippedLabel | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Input/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineY | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineEndX | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/paint_queue_PaintLine | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 |
| Input/cc/tree_TreeSubmittedNodeAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/button_Draw | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/cc/system_clipboard_host_SystemClipboardWriteHost | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 18/6/1/0/0/6/2/3/2/0/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputFocusFrameBegin | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 102/0/0/21/23/69/6/3/3/5/0/6/0/0/0 | 53/17/2/0/0/14/5/4/2/3/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputFocused | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 83/0/0/15/20/50/3/5/5/5/0/3/0/0/0 | 38/25/4/0/0/8/3/5/1/3/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputOwnerMatches | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 13/6/1/0/0/1/0/3/1/0/0/0/0/0/0 |
| Input/cc/tree_TreeDefer | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 68/21/1/8/5/27/6/4/3/3/0/6/0/0/6 | 49/6/1/0/0/10/5/3/2/1/0/0/0/2/4 |
| Input/cc/text_context_menu_TextMenuDraw | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 152/138/6/8/31/79/3/13/7/4/21/0/0/2/33 |
| Input/cc/tree_draw_FrameInputCommit | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Input/cc/paint_queue_PaintOverflow | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/cc/tree_TreeCancel | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Input/cc/paint_queue_PaintOrder | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Input/cc/paint_queue_SnapshotTextRead | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_SnapshotTextJoin | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Input/cc/paint_queue_PaintPointCount | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Input/cc/paint_queue_PendingPaintAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Input/cc/tree_input_TreeTopModalLayer | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeModalLayerAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeButtonLike | 0 | 0/391 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointInCircleBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/cc/std_utf8_Decode | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Input/cc/tree_TreeIdentityHash | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/system_clipboard_host_SystemClipboardOpenHost | 0 | 0/391 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/system_clipboard_host_SystemClipboardByteHost | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 11/0/0/0/0/4/2/2/2/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 205/20/5/34/39/112/16/25/20/12/17/6/0/0/5 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 |
| Input/cc/style_sheet_StyleControlRoleFacts | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldDefaultFace | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 63/0/0/10/13/35/0/0/0/6/26/0/0/0/0 |
| Input/cc/style_sheet_ResolveInstalledStyleRules | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 293/260/17/62/47/150/26/36/19/11/16/6/0/0/40 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 |
| Input/cc/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 |
| Input/cc/text_input_TextInputContentWidth | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 42/5/1/1/1/2/0/5/4/4/5/1/0/0/0 | 18/0/0/0/0/2/0/3/1/0/4/0/0/0/0 |
| Input/cc/text_input_TextUtf8Continuation | 0 | 0/391 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_utf8_Next | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/text_input_TextCursorForLength | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_CompositionBoundaryFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/15/2/0/0/2/2/6/4/0/0/0/0/0/3 |
| Input/cc/std_grapheme_Previous | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Input/cc/paint_queue_ClipShape | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Input/cc/surface_Unit | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Input/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/391 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldCursorHeightFor | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 24/0/0/0/0/2/0/3/1/0/4/0/0/0/0 |
| Input/cc/paint_queue_EmitLabel | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Input/cc/paint_queue_ClipLabel | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Input/cc/paint_queue_EmitLine | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Input/cc/button_SubmitButton | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 |
| Input/cc/system_clipboard_SystemClipboardHasText | 0 | 0/391 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/contextmenu_Draw | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1160/1125/36/268/330/740/28/93/57/58/87/16/0/0/95 | 570/548/32/102/94/285/46/65/33/18/103/1/0/4/61 |
| Input/cc/cursor_CursorHoverFrameCommit | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Input/cc/paint_queue_PaintMeshPointCount | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Input/cc/paint_queue_EmitPaintDirect | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Input/cc/paint_queue_ResolvePaintText | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Input/cc/std_utf8_Continuation | 0 | 0/391 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_grapheme_IsControl | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/session_WidgetPartKey | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/widget_key_WidgetRepeatedPart | 0 | 0/391 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleControlFacts | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleBucket | 0 | 0/391 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleFirstBit | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 |
| Input/cc/style_sheet_StyleRuleMatchesState | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleSelectorMatches | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Input/cc/style_MergeValues | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 |
| Input/cc/tree_TreeLabelSpanAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Input/cc/tree_TreeSetLabelSpan | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 |
| Input/cc/paint_queue_ShapeVisible | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Input/cc/paint_queue_ShapeWhole | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Input/cc/raster_shape_FillRoundedRectangle | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_FillShapeClipped | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Input/cc/paint_queue_EmitPaintNow | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Input/cc/tree_TreeCurrentPaintGeometry | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Input/cc/paint_queue_KeepPaintMesh | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Input/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/theme_CurrentTheme | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Input/cc/text_typeface_TextTypefaceUse | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Input/cc/raster_text_PaintTextRotated | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Input/cc/raster_text_PaintTextItalicClipped | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Input/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Input/cc/raster_text_PaintTextClipped | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Input/cc/raster_text_PaintText | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/raster_StrokeLine | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/style_ResolveFlags | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Input/cc/style_ResolveState | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Input/cc/tree_input_TreePressedAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 83/12/3/8/17/47/9/5/2/4/6/3/0/0/3 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 |
| Input/cc/tree_input_TreeHoveredAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 145/20/5/12/26/72/15/19/14/7/10/5/0/0/5 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 |
| Input/cc/button_ButtonDetailSize | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 |
| Input/cc/tree_TreeRootBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Input/cc/button_behavior_MeasureBounds | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 |
| Input/cc/style_StyleMinHeightValue | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/button_ButtonMinimumHeight | 0 | 0/391 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/button_ButtonFitContentFace | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 |
| Input/cc/tree_TreeSetCircleHit | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/20/5/11/24/76/16/15/10/6/10/6/0/0/5 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 |
| Input/cc/tree_TreeSetSemanticDetail | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Input/cc/button_PaintButtonForNode | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 |
| Input/cc/tree_TreeNextKey | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 143/54/7/9/29/84/18/12/4/5/12/6/0/0/12 | 65/9/1/0/0/11/5/5/4/2/0/5/0/4/1 |
| Input/cc/contextmenu_ContextMenuOverlayKey | 0 | 0/391 0% |  | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/cc/contextmenu_ContextMenuPanelKey | 0 | 0/391 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePreviousCommitted | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 108/12/3/12/19/53/9/12/9/6/6/3/0/0/3 | 46/9/1/0/0/6/2/4/3/2/0/1/0/1/1 |
| Input/cc/contextmenu_ContextMenuRowKey | 0 | 0/391 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 8/0/0/0/0/4/3/0/0/0/0/0/0/1/0 |
| Input/cc/contextmenu_ContextMenuOpens | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/49/1/7/14/37/2/5/4/3/0/2/0/0/4 | 25/7/2/0/0/7/3/6/4/0/0/0/0/0/2 |
| Input/cc/contextmenu_ContextMenuFind | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 68/51/1/7/14/37/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/2/5/3/0/0/0/0/0/3 |
| Input/cc/contextmenu_ContextMenuBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuSubmenuBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 129/43/1/33/33/78/1/3/2/6/46/1/0/0/1 | 85/0/0/11/8/21/2/2/1/4/48/0/0/0/0 |
| Input/cc/overlay_Draw | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 257/214/9/46/27/82/5/22/13/19/36/0/0/6/31 |
| Input/cc/contextmenu_ContextMenuLevel | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 1465/1248/10/377/376/810/10/72/62/142/347/1/0/0/114 | 1082/888/26/288/120/488/12/74/36/102/301/0/0/2/121 |
| Input/cc/paint_queue_PendingTextValue | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Input/cc/style_sheet_StyleDefaultFacts | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleMatchesOptional | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Input/cc/paint_queue_ShapeRadius | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Input/cc/paint_queue_FillShapeRows | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Input/cc/paint_queue_FillClippedSpan | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Input/cc/paint_queue_KeepPaintBytes | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Input/cc/paint_queue_KeepMeshPoints | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Input/cc/theme_DefaultTheme | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Input/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Input/cc/text_emphasis_RunTextItalicRaster | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterText | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Input/cc/button_ButtonDefaultFace | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 |
| Input/cc/button_ButtonFaceFacts | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 |
| Input/cc/button_ButtonDetailFont | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Input/cc/style_StyleLineHeightValue | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/style_FitHeight | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 |
| Input/cc/button_behavior_MeasureContent | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 |
| Input/cc/button_behavior_ShapeWidth | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 |
| Input/cc/theme_ControlHeightFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 |
| Input/cc/button_behavior_ButtonCornerRadius | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Input/cc/focus_FocusDefaultOutlineBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 |
| Input/cc/image_lookup_IconLookupImage | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 |
| Input/cc/button_behavior_ButtonPictureSize | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 |
| Input/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 |
| Input/cc/button_behavior_ContentLayout | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 |
| Input/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 388/333/6/71/75/167/7/29/23/22/117/4/1/0/40 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 |
| Input/cc/image_lookup_ImageLookupTexture | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 |
| Input/cc/image_behavior_ImageDrawPlanFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 |
| Input/cc/paint_queue_PaintImage | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 587/550/22/99/131/325/11/48/26/33/63/12/0/0/41 | 261/197/11/38/27/100/8/30/19/19/54/0/0/2/32 |
| Input/cc/dpi_ScalePixels | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 42/0/0/0/0/7/0/7/4/0/19/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuRowTop | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 169/149/7/25/41/98/4/18/11/9/31/4/0/0/12 | 93/61/7/3/3/21/5/14/7/4/26/0/0/1/11 |
| Input/cc/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/14/1/0/0/1/1/2/1/0/0/0/0/2/4 |
| Input/cc/tree_input_TreeTakeDismissalAt | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/17/1/1/0/4/2/5/4/4/0/0/0/0/1 |
| Input/cc/tree_TreeSetPopup | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 115/16/4/6/19/63/14/13/9/5/8/6/0/0/4 | 38/0/0/0/0/8/1/5/5/2/0/1/0/1/0 |
| Input/cc/drawing_props_StyleTokenColorOr | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 92/84/5/10/5/33/9/13/8/5/4/3/0/0/18 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePushScope | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 219/24/6/30/43/121/22/25/19/16/12/9/0/0/6 | 81/64/5/0/0/13/9/9/4/8/0/8/0/0/12 |
| Input/cc/contextmenu_ContextMenuTextLeft | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/1/1/2/0/1/1/2/2/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelChecks | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 26/13/4/0/0/8/2/7/3/0/0/0/0/0/4 |
| Input/cc/contextmenu_ContextMenuInlineWidth | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/2/0/4/0/2/2/4/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuInlineEnds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 115/0/0/11/28/71/4/9/9/2/0/4/0/0/0 | 24/6/1/0/0/9/5/3/2/0/0/0/0/0/0 |
| Input/cc/svg_icon_behavior_PaintStroke | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 649/596/8/78/335/450/14/31/23/21/80/0/0/0/35 | 182/92/5/18/42/76/9/11/6/4/78/0/0/1/19 |
| Input/cc/tree_End | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RecordBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Input/cc/paint_queue_ShapeCornerInset | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Input/cc/paint_queue_KeepShortPaintBytes | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Input/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Input/cc/paint_queue_KeepLongPaintBytes | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Input/cc/theme_DarkPalette | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/cc/theme_LightPalette | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/cc/theme_DefaultMetrics | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Input/cc/theme_DefaultShadows | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Input/cc/theme_DefaultMotion | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Input/cc/theme_DefaultFonts | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Input/cc/theme_StateLayer | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 |
| Input/cc/card_behavior_CardDefaultFace | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 |
| Input/cc/theme_FontForRole | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Input/cc/theme_MotionDuration | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 |
| Input/cc/button_behavior_ButtonRoleFactsFor | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 |
| Input/cc/drawing_props_PaintPixelScale | 0 | 0/391 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 246/224/9/35/36/134/27/22/13/11/12/14/0/0/54 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 |
| Input/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 240/130/10/33/28/120/24/19/9/3/12/16/1/0/30 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 |
| Input/cc/image_behavior_ImageFitRect | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Input/cc/paint_queue_PaintImageWithPolygon | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 |
| Input/cc/svg_icon_behavior_StrokeAppendByte | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/cc/svg_icon_behavior_StrokeAppendNumber | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 230/194/7/10/16/40/7/36/29/13/11/0/6/0/35 | 122/102/6/0/6/38/14/13/5/0/6/4/0/7/26 |
| Input/cc/tree_TreePopScope | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 122/12/3/15/21/64/11/13/10/9/6/5/0/0/3 | 54/27/1/2/1/6/3/4/3/4/0/4/0/0/5 |
| Input/cc/paint_queue_ShapeSquareRoot | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Input/cc/paint_queue___zi_spec_9308732530dd822f | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Input/cc/paint_queue___zi_spec_27099373fae5fe98 | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Input/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Input/cc/paint_queue___zi_spec_17277838aa4fe0e7 | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Input/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Input/cc/paint_queue___zi_spec_b997e17bf3928592 | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 |
| Input/cc/svg_icon_behavior_SvgIconHash | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 |
| Input/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 |
| Input/cc/svg_shape_ShapeRaster | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 |
| Input/cc/svg_icon_behavior_SvgIconCells | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 345/311/29/30/34/97/9/61/32/9/0/10/1/0/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 |
| Input/cc/image_behavior_ImageFitBounds | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 |
| Input/cc/image_behavior_ImagePolygonValid | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 |
| Input/cc/paint_queue_EmitImage | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Input/cc/paint_queue_ClipImage | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeNumber | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 |
| Input/cc/svg_shape_ShapeBuildPath | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 |
| Input/cc/svg_shape_ShapeCoverEdges | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 |
| Input/cc/paint_queue_EmitImageBegin | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterImage | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 |
| Input/cc/paint_queue_EmitImageEnd | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 24/0/0/4/4/18/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsDigit | 0 | 0/391 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsSpace | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddPoint | 0 | 0/391 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddSpan | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 |
| Input/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 |
| Input/cc/svg_shape_ShapeFillSubpath | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeCollapse | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 |
| Input/cc/svg_shape_ShapeAddCircle | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 |
| Input/cc/svg_shape_ShapeSqrt | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 |
| Input/cc/svg_shape_ShapeQuad | 0 | 0/391 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeAbs | 0 | 0/391 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeTriangle | 0 | 0/391 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddEdge | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Input/cc/svg_shape_ShapeAddPolygon | 0 | 0/391 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 |
| Input/clang-19/widget_benchmarks_BeginCase | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/tree_draw_BeginFrame | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 314/4/1/51/42/145/14/41/40/27/12/7/0/0/1 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Input/clang-19/textfield_Draw | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 117/0/0/14/4/41/2/10/10/9/2/0/0/0/0 |
| Input/clang-19/tree_draw_EndFrame | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Input/clang-19/tree_TreeNodeAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/540 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/540 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_request_TextRequestSend | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 539/400/10/101/48/239/26/78/68/40/12/12/0/0/76 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Input/clang-19/text_request_TextRequestClear | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/540 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_PointerSample | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Input/clang-19/tree_input_SecondaryPointerSample | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Input/clang-19/session_SessionFlagSet | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 700/631/23/94/101/277/30/110/87/60/22/7/0/0/104 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 |
| Input/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintClear | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Input/clang-19/textfield_TextFieldKey | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextSelectionRangeForLength | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/text_context_menu_TextMenuTakeCommand | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 161/110/7/25/14/55/0/31/24/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 |
| Input/clang-19/text_context_menu_TextMenuPasteText | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/system_clipboard_SystemClipboardSet | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputNoteFocused | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/0/0/19/20/59/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuIsOpenFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 52/0/0/4/4/18/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuShow | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/session_SessionFlagGet | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/tree_draw_CommitFrame | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Input/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 719/637/10/161/224/523/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Input/clang-19/paint_queue_PaintFlush | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 151/98/4/28/40/98/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 |
| Input/clang-19/tree_input_TreeTextKind | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/tree_TreeTextRange | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/53/6/38/34/121/17/29/23/12/10/5/0/0/9 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Input/clang-19/geometry_RectangleIntersection | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Input/clang-19/tree_TreePointerActivatable | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Input/clang-19/tree_input_TreeActivateAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Input/clang-19/cursor_CursorHoverFrameStart | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 52/0/0/7/8/27/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Input/clang-19/text_input_TextSelectionRangeFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/system_clipboard_SystemClipboardText | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 65/17/2/0/0/5/1/9/7/2/0/0/0/0/1 |
| Input/clang-19/input_protocol_PointInBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuOpenAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 55/14/1/3/0/16/1/5/4/3/2/0/0/0/2 |
| Input/clang-19/tree_input_TreeTakeDragAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 443/56/14/55/84/259/49/47/33/17/45/21/0/0/14 | 155/35/3/8/1/45/10/27/24/6/8/6/0/0/6 |
| Input/clang-19/tree_input_TreeBlurAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 98/12/3/5/15/46/11/15/12/6/6/3/0/0/3 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 |
| Input/clang-19/textfield_TextFieldFaceFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 179/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 |
| Input/clang-19/style_ResolveFont | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Input/clang-19/style_StyleFontValue | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Input/clang-19/text_input_TextInputScaledMetric | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Input/clang-19/textfield_TextFieldSecureDisplay | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 57/8/1/0/0/2/2/5/4/2/0/0/0/0/1 |
| Input/clang-19/font_metrics_GlyphWidth | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextFieldScrollFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 |
| Input/clang-19/textfield_TextFieldCursorAtX | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 |
| Input/clang-19/text_input_TextPointerOffset | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 |
| Input/clang-19/textfield_TextFieldCodepoints | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 |
| Input/clang-19/text_TextSpan | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/clang-19/textfield_TextFieldByteAfter | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 |
| Input/clang-19/text_input_CompositionTransitionFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 110/98/3/0/0/37/4/14/11/0/19/0/0/0/11 |
| Input/clang-19/textfield_TextFieldNext | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextSelectionAfterMove | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 |
| Input/clang-19/textfield_TextFieldInputTextAllowed | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 25/12/1/0/0/3/2/4/3/0/0/0/0/0/3 |
| Input/clang-19/textfield_TextFieldReplacementAllowed | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 |
| Input/clang-19/text_input_TextEditCommandDecisionFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/0/0/2/0/2/0/12/12/0/0/0/0/6/0 |
| Input/clang-19/textfield_TextFieldEditFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 |
| Input/clang-19/text_request_TextRequestChangeFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 280/263/12/53/38/116/5/36/24/13/0/0/0/0/24 | 145/55/6/6/10/39/8/22/16/5/0/0/0/2/0 |
| Input/clang-19/font_metrics_GlyphCaret | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 32/0/0/1/3/8/0/2/2/1/2/0/0/0/0 |
| Input/clang-19/text_input_TextFieldRevealScroll | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_StyleOpacityValue | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Input/clang-19/tree_input_TreeFocusSubmittedAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 289/24/6/42/46/139/20/34/28/15/20/7/0/0/6 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 |
| Input/clang-19/tree_TreeSetWidgetState | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 272/140/8/40/42/162/26/23/15/8/14/13/0/0/27 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 |
| Input/clang-19/paint_queue_PaintRoundedFill | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Input/clang-19/drawing_props_ColorFromPacked | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/surface_Opacity | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Input/clang-19/paint_queue_PaintRoundedOutline | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Input/clang-19/font_metrics_GlyphLineHeight | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextFieldPaintFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 179/0/0/22/20/53/0/13/13/12/47/1/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 |
| Input/clang-19/theme_CurrentMuted | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentAccent | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintClippedLabel | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Input/clang-19/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/clang-19/text_input_TextCompositionUnderlineY | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextCompositionUnderlineEndX | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintLine | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Input/clang-19/tree_TreeSubmittedNodeAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/button_Draw | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputFocusFrameBegin | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 102/0/0/21/23/69/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputFocused | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 83/0/0/15/20/50/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputOwnerMatches | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreeDefer | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 68/21/1/8/5/27/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 |
| Input/clang-19/text_context_menu_TextMenuDraw | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 140/0/0/6/33/75/0/12/12/5/18/0/0/2/0 |
| Input/clang-19/tree_draw_FrameInputCommit | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Input/clang-19/paint_queue_PaintOverflow | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 26/0/0/4/8/19/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/clang-19/tree_TreeCancel | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Input/clang-19/paint_queue_PaintOrder | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 320/253/8/43/54/191/17/29/21/10/4/14/0/0/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Input/clang-19/paint_queue_SnapshotTextRead | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 176/0/0/19/17/68/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Input/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 473/421/28/68/78/190/14/91/63/13/0/6/2/0/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Input/clang-19/paint_queue_PaintPointCount | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Input/clang-19/paint_queue_EmitPaint | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Input/clang-19/paint_queue_PendingPaintAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 372/48/12/55/142/274/31/19/7/7/46/5/0/0/12 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Input/clang-19/tree_input_TreeTopModalLayer | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreeButtonLike | 0 | 0/540 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_PointInCircleBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/clang-19/tree_TreeOverlayLayerAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/std_utf8_Decode | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Input/clang-19/std_grapheme_Push | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Input/clang-19/tree_TreeIdentityHash | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang-19/system_clipboard_host_SystemClipboardOpenHost | 0 | 0/540 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/system_clipboard_host_SystemClipboardByteHost | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleControlRoleFacts | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/textfield_TextFieldDefaultFace | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 |
| Input/clang-19/style_sheet_ResolveInstalledStyleRules | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 293/260/17/62/47/150/26/36/19/11/16/6/0/0/40 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 |
| Input/clang-19/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Input/clang-19/text_input_TextUtf8Continuation | 0 | 0/540 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextCursorForLength | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_CompositionBoundaryFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/9/1/0/0/3/2/3/2/0/0/0/0/0/2 |
| Input/clang-19/paint_queue_ClipShape | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Input/clang-19/paint_queue_QueuePaint | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 902/852/32/102/142/481/59/89/57/48/143/6/0/0/110 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Input/clang-19/surface_Unit | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Input/clang-19/paint_queue_EmitRoundedOutline | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/540 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextFieldCursorHeightFor | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Input/clang-19/paint_queue_EmitLabel | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Input/clang-19/paint_queue_ClipLabel | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Input/clang-19/paint_queue_EmitLine | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Input/clang-19/button_SubmitButton | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 |
| Input/clang-19/system_clipboard_SystemClipboardHasText | 0 | 0/540 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/contextmenu_Draw | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1160/1125/36/268/330/740/28/93/57/58/87/16/0/0/95 | 588/511/31/82/99/249/33/78/47/19/99/14/0/3/58 |
| Input/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/5/6/22/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Input/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang-19/paint_queue_EmitPaintDirect | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Input/clang-19/paint_queue_ResolvePaintText | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/std_utf8_Continuation | 0 | 0/540 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/std_grapheme_data_Property | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/25/6/18/18/10/0/1/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Input/clang-19/std_grapheme_IsControl | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/session_WidgetPartKey | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/540 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleControlFacts | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Input/clang-19/theme_CurrentSunken | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentText | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentBorderStrong | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_ShapeSmall | 0 | 0/540 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/theme_TypeLabel | 0 | 0/540 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/style_sheet_StyleBucket | 0 | 0/540 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleFirstBit | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Input/clang-19/style_sheet_StyleRuleMatchesState | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleSelectorMatches | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Input/clang-19/style_MergeValues | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Input/clang-19/tree_TreeLabelSpanAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/4/1/6/9/39/6/3/2/2/2/4/0/0/1 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Input/clang-19/tree_TreeSetLabelSpan | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 48/4/1/4/4/28/6/4/3/1/2/4/0/0/1 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 |
| Input/clang-19/paint_queue_ShapeVisible | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Input/clang-19/paint_queue_ShapeWhole | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Input/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_FillShapeClipped | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Input/clang-19/paint_queue_EmitPaintNow | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/2/0/0/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Input/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 170/8/2/48/52/130/6/9/7/4/17/2/0/0/2 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Input/clang-19/paint_queue_KeepPaintMesh | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Input/clang-19/raster_shape_StrokeRoundedRectangle | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentTheme | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/text_typeface_TextTypefaceUse | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/raster_text_PaintTextRotated | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Input/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Input/clang-19/raster_text_PaintTextClipped | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang-19/raster_text_PaintText | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/raster_StrokeLine | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/style_ResolveFlags | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Input/clang-19/style_ResolveState | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Input/clang-19/tree_input_TreePressedAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 83/12/3/8/17/47/9/5/2/4/6/3/0/0/3 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 |
| Input/clang-19/tree_input_TreeHoveredAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 145/20/5/12/26/72/15/19/14/7/10/5/0/0/5 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Input/clang-19/button_ButtonDetailSize | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 |
| Input/clang-19/tree_TreeRootBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 123/16/4/18/30/79/11/10/6/6/13/3/0/0/4 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Input/clang-19/button_behavior_MeasureBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 |
| Input/clang-19/style_StyleMinHeightValue | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang-19/button_ButtonMinimumHeight | 0 | 0/540 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/button_ButtonFitContentFace | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 |
| Input/clang-19/tree_TreeSetCircleHit | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/20/5/11/24/76/16/15/10/6/10/6/0/0/5 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Input/clang-19/tree_TreeSetSemanticDetail | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 152/16/4/17/24/79/14/15/11/9/8/6/0/0/4 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang-19/button_PaintButtonForNode | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 |
| Input/clang-19/tree_TreeNextKey | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 143/54/7/9/29/84/18/12/4/5/12/6/0/0/12 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 |
| Input/clang-19/contextmenu_ContextMenuOverlayKey | 0 | 0/540 0% |  | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuPanelKey | 0 | 0/540 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreePreviousCommitted | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 108/12/3/12/19/53/9/12/9/6/6/3/0/0/3 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuRowKey | 0 | 0/540 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuOpens | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/49/1/7/14/37/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 |
| Input/clang-19/contextmenu_ContextMenuFind | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 68/51/1/7/14/37/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 |
| Input/clang-19/contextmenu_ContextMenuBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuSubmenuBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 129/43/1/33/33/78/1/3/2/6/46/1/0/0/1 | 122/32/8/25/13/45/3/12/3/5/50/2/0/0/9 |
| Input/clang-19/overlay_Draw | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 200/82/1/15/5/38/0/16/15/19/34/0/0/1/9 |
| Input/clang-19/contextmenu_ContextMenuLevel | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1465/1248/10/377/376/810/10/72/62/142/347/1/0/0/114 | 940/723/13/211/90/371/26/63/50/101/289/2/0/1/99 |
| Input/clang-19/paint_queue_PendingTextValue | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 111/0/0/15/16/64/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Input/clang-19/style_sheet_StyleDefaultFacts | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Input/clang-19/style_sheet_StyleMatchesOptional | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Input/clang-19/paint_queue_ShapeRadius | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Input/clang-19/paint_queue_FillClippedSpan | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Input/clang-19/paint_queue_KeepPaintBytes | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Input/clang-19/paint_queue_KeepMeshPoints | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 281/155/10/44/35/140/18/30/20/10/20/4/0/0/25 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Input/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/button_ButtonFaceFinish | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Input/clang-19/button_ButtonDefaultFace | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Input/clang-19/button_ButtonFaceFacts | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Input/clang-19/button_ButtonDetailFont | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Input/clang-19/style_StyleLineHeightValue | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang-19/style_FitHeight | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Input/clang-19/button_behavior_MeasureContent | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 |
| Input/clang-19/button_behavior_ShapeWidth | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Input/clang-19/theme_ControlHeightFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Input/clang-19/button_behavior_ButtonCornerRadius | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Input/clang-19/focus_FocusDefaultOutlineBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/18/0/6/5/4/33/2/0/0/8 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Input/clang-19/image_lookup_IconLookupImage | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Input/clang-19/button_behavior_ButtonPictureSize | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Input/clang-19/button_behavior_ButtonLabeledPictureSize | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Input/clang-19/button_behavior_ContentLayout | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_PaintSvgIcon | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 388/333/6/71/75/167/7/29/23/22/117/4/1/0/40 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 |
| Input/clang-19/image_lookup_ImageLookupTexture | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Input/clang-19/image_behavior_ImageDrawPlanFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuLevelBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 587/550/22/99/131/325/11/48/26/33/63/12/0/0/41 | 267/183/14/44/15/86/6/33/19/22/50/1/0/0/34 |
| Input/clang-19/dpi_ScalePixels | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuRowTop | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 169/149/7/25/41/98/4/18/11/9/31/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 |
| Input/clang-19/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/0/0/0/3/0 |
| Input/clang-19/tree_input_TreeTakeDismissalAt | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 |
| Input/clang-19/tree_TreeSetPopup | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 115/16/4/6/19/63/14/13/9/5/8/6/0/0/4 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 |
| Input/clang-19/drawing_props_StyleTokenColorOr | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 92/84/5/10/5/33/9/13/8/5/4/3/0/0/18 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentSurface | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentBorder | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentDisabled | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentSelection | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/tree_TreePushScope | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 219/24/6/30/43/121/22/25/19/16/12/9/0/0/6 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 |
| Input/clang-19/contextmenu_ContextMenuTextLeft | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuLevelChecks | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 30/16/4/0/0/6/1/7/3/0/0/0/0/0/5 |
| Input/clang-19/contextmenu_ContextMenuInlineWidth | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuInlineEnds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 115/0/0/11/28/71/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 |
| Input/clang-19/svg_icon_behavior_PaintStroke | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 649/596/8/78/335/450/14/31/23/21/80/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 |
| Input/clang-19/tree_End | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_RecordBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Input/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 86/64/3/18/6/42/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Input/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 88/66/3/18/6/42/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Input/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/17/5/40/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Input/clang-19/theme_DarkPalette | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang-19/theme_LightPalette | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang-19/theme_DefaultMetrics | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Input/clang-19/theme_DefaultShadows | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Input/clang-19/theme_DefaultMotion | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Input/clang-19/theme_StateLayer | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Input/clang-19/card_behavior_CardDefaultFace | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 |
| Input/clang-19/theme_ShapeMedium | 0 | 0/540 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/theme_FontForRole | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Input/clang-19/theme_MotionDuration | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Input/clang-19/drawing_props_PaintPixelScale | 0 | 0/540 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconSlot | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 246/224/9/35/36/134/27/22/13/11/12/14/0/0/54 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 |
| Input/clang-19/svg_icon_behavior_SvgIconSnap | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconTexture | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 240/130/10/33/28/120/24/19/9/3/12/16/1/0/30 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 |
| Input/clang-19/paint_queue_PaintImageWithPolygon | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_StrokeAppendByte | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_StrokeAppendNumber | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 230/194/7/10/16/40/7/36/29/13/11/0/6/0/35 | 98/25/2/1/4/25/10/9/7/1/6/4/0/5/4 |
| Input/clang-19/tree_TreePopScope | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 122/12/3/15/21/64/11/13/10/9/6/5/0/0/3 | 52/10/1/0/0/6/4/6/5/4/0/4/0/0/2 |
| Input/clang-19/paint_queue_ShapeSquareRoot | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Input/clang-19/paint_queue___zi_spec_9308732530dd822f | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 486/442/6/36/298/375/10/22/16/7/6/5/0/0/26 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang-19/paint_queue___zi_spec_27099373fae5fe98 | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Input/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang-19/paint_queue___zi_spec_17277838aa4fe0e7 | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Input/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 284/221/10/40/46/150/17/29/19/10/14/5/0/0/37 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang-19/paint_queue___zi_spec_b997e17bf3928592 | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 79/14/1/12/16/43/0/8/7/2/0/1/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 |
| Input/clang-19/theme_ShapeSurface | 0 | 0/540 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconRetire | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeRaster | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 |
| Input/clang-19/image_behavior_ImageFitBounds | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 |
| Input/clang-19/image_behavior_ImagePolygonValid | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Input/clang-19/paint_queue_EmitImage | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Input/clang-19/paint_queue_ClipImage | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeNumber | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 |
| Input/clang-19/svg_shape_ShapeCoverEdges | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 |
| Input/clang-19/paint_queue_EmitImageBegin | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_RasterImage | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Input/clang-19/paint_queue_EmitImageEnd | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 24/0/0/4/4/18/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeIsDigit | 0 | 0/540 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeIsSpace | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/svg_shape_ShapeAddPoint | 0 | 0/540 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAddSpan | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 |
| Input/clang-19/svg_shape_ShapeStrokeSubpath | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 |
| Input/clang-19/svg_shape_ShapeFillSubpath | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 |
| Input/clang-19/svg_shape_ShapeCollapse | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Input/clang-19/svg_shape_ShapeAddCircle | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Input/clang-19/svg_shape_ShapeQuad | 0 | 0/540 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAbs | 0 | 0/540 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeTriangle | 0 | 0/540 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAddEdge | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Input/clang-19/svg_shape_ShapeAddPolygon | 0 | 0/540 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 |
| Hello/cc/hello_main | 0 | 0/0 unknown |  | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Hello/clang-19/hello_main | 0 | 0/0 unknown |  | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
