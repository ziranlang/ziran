# C teacher: native assembly comparison

Base revision: 888f39a992cc0a18d1f0fa0e41f0070dc0002a6c
Host: omega
build/bin/zi2c SHA-256: 549a9b9f3600255c4b8af83947661f92ec8db8a97d80d26392b1674a8bc59f27
build/bin/zi2zx SHA-256: 582853558ec22e178d5e8f4f802802b2c81ffc2607b6e258259bc59dbae00520
build/bin/zx2native SHA-256: fc7dd7c5f4409c68a26b634888085d39b7fa0592398119bbed7bdb09a9f4867d
Learning off. Generated C compiled with -O2; native compiled from verified ZX. Function extents come from the native map; C symbols match module_name or exported name exactly. Absent symbols are not evidence of inlining. Metrics: instructions/loop-union/back-edges/stack-reads/stack-writes/memory/indexed/branches/forward-branches/calls/vector-register-uses/mul/div/shifts/loop-immediate-or-PC-relative candidates. Stack traffic includes locals and saves. Vector-register uses include scalar floating point. Loop constants are LICM candidates, not proven invariants. Back edges describe loop bodies, not execution frequency. SIGPROF uses a 1ms CPU timer, at most 30 runs; zero samples means unknown time share. Sampling includes process startup and runtime. Ranking weights relative instruction excess by observed function samples; it predicts neither cycles nor speedup.

## Sieve / cc
Native SHA-256: 81468f46fa5939aeec5abd2c18b72fb884295ee84ea0190e788663fed0d10ae9
C SHA-256: a90bdb344798838279f6a10871246589e12193ef0ab912e32c4b0994e021f267
ZX SHA-256: f1fccf1c68dab477a2d0e35a94d9fda6ed8c9873d34deb24c4de73e88f22d92e
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 127/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Sieve | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 34/19/3/0/0/7/6/6/3/1/0/1/0/0/9 | 127 | 421..594: 40 instructions, 121 samples |

## Sieve / clang-19
Native SHA-256: 81468f46fa5939aeec5abd2c18b72fb884295ee84ea0190e788663fed0d10ae9
C SHA-256: ffe0a6f6a17493a2edf3e8be1843da7ff239ae67174011b627c4b07e379a5f8a
ZX SHA-256: f1fccf1c68dab477a2d0e35a94d9fda6ed8c9873d34deb24c4de73e88f22d92e
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 113/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Sieve | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 | 113 | 421..594: 40 instructions, 108 samples |

## Records / cc
Native SHA-256: e143b0a88fc1d98d166885082590f6c5f683f6e8fe77e49373183050f62d9baa
C SHA-256: c1e5ee0de67ea0bb3254c26d8cba9d59504c91d226b277a243a97265bf234f30
ZX SHA-256: 4dc39f678013ae78a3783b0dc2cf95aa0cd66f3c79ed5b88250f42f8d9d3ea45
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 106/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Records | 929/149/21/260/520/807/5/46/25/5/0/4/2/0/29 | 70/57/4/0/0/20/2/6/2/0/0/2/0/2/11 | 106 | 5509..6075: 132 instructions, 105 samples |

## Records / clang-19
Native SHA-256: e143b0a88fc1d98d166885082590f6c5f683f6e8fe77e49373183050f62d9baa
C SHA-256: e0fb2ce977684ea9df7560d86ee8b7b931dc72fa33e8fe30b3f4e16c7a7a2782
ZX SHA-256: 4dc39f678013ae78a3783b0dc2cf95aa0cd66f3c79ed5b88250f42f8d9d3ea45
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 101/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Records | 929/149/21/260/520/807/5/46/25/5/0/4/2/0/29 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 | 101 | 5509..6075: 132 instructions, 101 samples |

## Hash / cc
Native SHA-256: 4875728741b927cad36d216942e1dc4bfeb809d2d05daf6ad7539f792b89798c
C SHA-256: 1d1a9c1821d391c1ee6cd78066299c2a37dceb66ccc27db585c8b9f55f3e134c
ZX SHA-256: 41e6991d1c84b4fec12fe82b4ad9275c6394ed284ae862572e2ee2ea7f65f4b2
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 149/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Hash | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 26/12/2/0/0/5/1/2/0/0/0/4/0/1/4 | 149 | 199..408: 52 instructions, 149 samples |

## Hash / clang-19
Native SHA-256: 4875728741b927cad36d216942e1dc4bfeb809d2d05daf6ad7539f792b89798c
C SHA-256: 6f3b626c0e265316a033df753b6a6849e8321f525329a335dba02b936d3b5c59
ZX SHA-256: 41e6991d1c84b4fec12fe82b4ad9275c6394ed284ae862572e2ee2ea7f65f4b2
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 142/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Hash | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 | 142 | 199..408: 52 instructions, 142 samples |

## Vectors / cc
Native SHA-256: 533cfc39d0908267889f1c8501bfe4cb4d3f850aa08cbaaf69c17104b6b3a1a6
C SHA-256: 80ce3f982e01a9e5571fe04dda3f94f871709933e51d40514bd28f4127f251f1
ZX SHA-256: 541c0b8413f48e82b2b148136f15a8f226ee7f9e6b4a82413f0df09fc64630d2
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 151/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Vectors | 149/137/15/22/23/47/2/31/16/3/0/1/0/1/15 | 101/90/7/7/8/24/5/16/9/2/7/0/0/0/9 | 125 | 218..755: 136 instructions, 125 samples |

## Vectors / clang-19
Native SHA-256: 533cfc39d0908267889f1c8501bfe4cb4d3f850aa08cbaaf69c17104b6b3a1a6
C SHA-256: 69731a1b48535b712c8be6562303fd06cdce49a7447024a162af4e25f7379c31
ZX SHA-256: 541c0b8413f48e82b2b148136f15a8f226ee7f9e6b4a82413f0df09fc64630d2
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 150/29

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Vectors | 149/137/15/22/23/47/2/31/16/3/0/1/0/1/15 | 123/101/10/3/3/22/15/17/7/2/26/1/0/1/13 | 129 | 218..755: 136 instructions, 129 samples |

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
Native SHA-256: 99f953bd85ed52e718ff319f8e0bcf118b3257dba3a5949bc12020526d3aff4b
C SHA-256: 9ab4e28c0b29b38737501bf4d653c9fa0d77682b00603669543a9ef60e2720aa
ZX SHA-256: b10e5d3d72ba8ccea623fd45c8fbceb20787a7d8bccd47512eb7bbef246d8d9a
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Printing_main | 19/11/3/1/1/2/0/5/2/1/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 44..85: 11 instructions, 0 samples |
| wasm_tier_Printing | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 31/14/1/0/0/4/0/1/0/1/0/2/0/0/3 | 0 | 131..321: 52 instructions, 0 samples |

## Printing / clang-19
Native SHA-256: 99f953bd85ed52e718ff319f8e0bcf118b3257dba3a5949bc12020526d3aff4b
C SHA-256: 99a16912804f94e7f52ce464a6b6c974f88440f4ecf335c127ee487294e5f34e
ZX SHA-256: b10e5d3d72ba8ccea623fd45c8fbceb20787a7d8bccd47512eb7bbef246d8d9a
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 0/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Printing_main | 19/11/3/1/1/2/0/5/2/1/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 44..85: 11 instructions, 0 samples |
| wasm_tier_Printing | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 | 0 | 131..321: 52 instructions, 0 samples |

## Button / cc
Native SHA-256: 23663e8ee244fec155cb141ef3896b0a6666990e8f83e1233bcf41b37d0997de
C SHA-256: cad0c34b670e2363e8361bcac8a7f903af61489b8faaa9fd0b3f8ea74bf59a1b
ZX SHA-256: 1d6b8297e08a8a5583e616b7230214dfd42790064b27839514d8d752e2e4fe8d
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 38/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 87..158: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 158/147/9/4/10/45/4/19/10/12/8/0/0/0/36 | 1 | 1177..1199: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 0 | none |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 4343..4365: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 1 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 4802..4821: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 | 0 | none |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 7471..9030: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 9227..9246: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 9500..9652: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 9732..9842: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 3 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 1 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 10742..10785: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 11767..14211: 526 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 0 | 15936..16195: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| button_SubmitButton | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 | 0 | 16374..19196: 584 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 20480..23438: 557 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 2 | 23544..23938: 76 instructions, 2 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 1 | 24718..24740: 4 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 | 0 | 27683..28763: 232 instructions, 0 samples |
| tree_input_TreeTextFocused | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 | 0 | 29394..29618: 57 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 0 | 30642..30658: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 30885..31198: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 31592..31684: 25 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 32055..32319: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 0 | none |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 32856..33122: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 | 0 | 34334..35162: 195 instructions, 0 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 2 | 36415..39352: 622 instructions, 2 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 | 0 | 39702..39757: 17 instructions, 0 samples |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 266/172/4/89/71/181/2/9/5/15/144/0/0/0/7 | 0 | 40724..40746: 4 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 | 0 | 42394..43692: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 | 0 | 44206..45520: 284 instructions, 0 samples |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 | 2 | 48340..54979: 1342 instructions, 2 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 26/12/1/0/0/2/1/2/1/2/0/0/0/0/1 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 0 | 56172..56450: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 58691..59739: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 60595..62400: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 0 | 62567..62589: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 1 | 62980..63002: 4 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 0 | 65449..66235: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 0 | 66688..67919: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 1 | 68683..69363: 170 instructions, 1 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 78/21/2/2/2/18/7/5/3/2/0/3/0/3/2 | 0 | 69494..69677: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 2 | 69943..70272: 77 instructions, 2 samples |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 | 1 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 45/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 0 | 71107..71273: 33 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 | 0 | 71424..72690: 251 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 | 1 | 73195..74263: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 | 0 | 74308..74465: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 33/16/2/0/0/4/2/5/3/0/0/0/0/2/5 | 0 | 74588..74750: 47 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 | 0 | 75073..75251: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 | 0 | 75372..75986: 146 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 | 0 | 76330..76352: 4 instructions, 0 samples |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 | 0 | 76802..77078: 69 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 0 | 77475..77782: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 0 | 77902..79599: 412 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 1 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 0 | 81192..81214: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 | 0 | 82406..82428: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 0 | 82926..82948: 4 instructions, 0 samples |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/20/2/6/5/4/33/0/0/0/6 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 | 0 | 83087..83327: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 | 0 | 83873..83895: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 | 0 | 84107..84265: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 | 0 | 84493..84664: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 | 0 | 84971..85717: 172 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 372/317/4/71/71/157/3/27/23/22/113/4/1/0/36 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 | 0 | 85970..87386: 313 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 | 0 | 87655..87677: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 | 0 | 88319..88341: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 | 0 | 89746..89768: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 1 | 91092..91114: 4 instructions, 1 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 91737..91747: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 1 | 91877..91896: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 0 | 92528..92856: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 278/259/112/0/0/136/10/118/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 | 0 | 93752..94100: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 | 0 | 95791..95813: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | 95922..95941: 4 instructions, 0 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 | 0 | 96215..96876: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 | 0 | 97467..97486: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 | 0 | 98236..98395: 43 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 | 0 | 98463..98542: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | none |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 0 | 99054..99076: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 99678..99700: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 1 | 99884..103359: 715 instructions, 0 samples |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 132/113/2/24/6/35/0/11/8/2/63/0/0/6/11 | 0 | 103434..103456: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 | 0 | 104016..104425: 97 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 | 0 | 104957..105080: 30 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 | 0 | 105250..105452: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 | 0 | 106882..106904: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 108126..108721: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 109933..109955: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 9 | 122890..122912: 4 instructions, 2 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 123870..124563: 158 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 124710..125776: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 126460..126479: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 0 | 127786..128039: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 | 0 | 128147..128205: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 | 0 | none |
| svg_shape_ShapeRaster | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 | 0 | 128736..129586: 207 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 345/311/29/30/34/97/9/61/32/9/0/8/1/2/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 | 0 | 129841..130845: 262 instructions, 0 samples |
| image_behavior_ImageFitBounds | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 | 0 | 131007..131709: 177 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 | 0 | 131825..132179: 91 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 132202..132224: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 132633..132655: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 133947..134025: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 134616..134635: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 135085..135104: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 0 | 136026..136045: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 3 | 137807..137826: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 1 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 2 | 138599..138615: 4 instructions, 1 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 138994..139189: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 139266..140277: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 140439..140816: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 | 0 | 141838..142393: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 | 0 | 142727..143930: 284 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 | 0 | 144151..146324: 479 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 | 0 | 146755..148147: 332 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 22/0/0/0/0/4/0/2/0/0/12/0/0/3/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 149483..149528: 12 instructions, 0 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 150136..150360: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 150453..150687: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 150780..150984: 52 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 19/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 | 0 | 151391..151872: 123 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 152004..152122: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 152240..154235: 415 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 154364..154411: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 154713..155607: 191 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 155736..155783: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 156085..156979: 191 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 157108..157155: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 | 0 | 157614..160033: 523 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | 160157..160280: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 | 0 | 160318..160549: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 | 0 | 160628..160834: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 | 0 | 160957..161075: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 161883..161899: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 162258..162274: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 | 0 | 162333..162692: 90 instructions, 0 samples |

## Button / clang-19
Native SHA-256: 23663e8ee244fec155cb141ef3896b0a6666990e8f83e1233bcf41b37d0997de
C SHA-256: 9468ebf7a30babea7d8d481ece8d21f51a5fc9acafbeacc5d99c3c4e7c2abd6a
ZX SHA-256: 1d6b8297e08a8a5583e616b7230214dfd42790064b27839514d8d752e2e4fe8d
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 38/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 87..158: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 145/0/0/5/5/38/0/13/13/14/8/0/0/0/0 | 0 | 1177..1199: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 0 | none |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 4343..4365: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 4802..4821: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 | 0 | none |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 7471..9030: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 9227..9246: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 9500..9652: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 9732..9842: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 9 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 10742..10785: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 | 0 | 11767..14211: 526 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 15936..16195: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| button_SubmitButton | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 | 0 | 16374..19196: 584 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 20480..23438: 557 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 3 | 23544..23938: 76 instructions, 3 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 0 | 24718..24740: 4 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 0 | 27683..28763: 232 instructions, 0 samples |
| tree_input_TreeTextFocused | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 0 | 29394..29618: 57 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 30642..30658: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 30885..31198: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 31592..31684: 25 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 32055..32319: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 32856..33122: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 2 | 34334..35162: 195 instructions, 2 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 0 | 36415..39352: 622 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 0 | 39702..39757: 17 instructions, 0 samples |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 2 | 40983..41005: 4 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 0 | none |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 | 2 | 42394..43692: 305 instructions, 1 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 | 0 | 44206..45520: 284 instructions, 0 samples |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 1 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 | 1 | 48340..54979: 1342 instructions, 1 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 0 | 56172..56450: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 58691..59739: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 60595..62400: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 62567..62589: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 0 | 62980..63002: 4 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 65449..66235: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 66688..67919: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 0 | 68683..69363: 170 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 69494..69677: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 0 | 69943..70272: 77 instructions, 0 samples |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 | 0 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 71107..71273: 33 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 | 1 | 71424..72690: 251 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 1 | 73195..74263: 201 instructions, 1 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 74308..74465: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 0 | 74588..74750: 47 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 75073..75251: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 | 0 | 75372..75986: 146 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 0 | 76330..76352: 4 instructions, 0 samples |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 0 | 76802..77078: 69 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 0 | 77475..77782: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 0 | 77902..79599: 412 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 81192..81214: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 0 | 82406..82428: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 0 | 82926..82948: 4 instructions, 0 samples |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/20/2/6/5/4/33/0/0/0/6 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 83087..83327: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 83873..83895: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 84107..84265: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 84493..84664: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 | 0 | 84971..85717: 172 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 372/317/4/71/71/157/3/27/23/22/113/4/1/0/36 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 | 0 | 85970..87386: 313 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 87655..87677: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 88319..88341: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 89746..89768: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 0 | 90957..90979: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 91737..91747: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 2 | 91877..91896: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 1 | 92528..92856: 82 instructions, 1 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 93752..94100: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 95791..95813: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 0 | 95922..95941: 4 instructions, 0 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 96215..96876: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 | 0 | 97467..97486: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 98236..98395: 43 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 98463..98542: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | none |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 1 | 99054..99076: 4 instructions, 1 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 99678..99700: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 0 | 99884..103359: 715 instructions, 0 samples |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 103434..103456: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 | 0 | 104016..104425: 97 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 104957..105080: 30 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 | 0 | 105250..105452: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 106882..106904: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 108126..108721: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 109933..109955: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 6 | 121952..121971: 4 instructions, 1 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 123870..124563: 158 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 124710..125776: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 126460..126479: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 1 | 127599..127618: 4 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 46/22/2/0/0/8/7/6/4/0/0/5/0/0/1 | 0 | 128147..128205: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 | 0 | 128736..129586: 207 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 345/311/29/30/34/97/9/61/32/9/0/8/1/2/41 | 375/321/22/23/20/119/48/57/35/1/32/4/0/4/51 | 0 | 129841..130845: 262 instructions, 0 samples |
| image_behavior_ImageFitBounds | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 | 0 | 131007..131709: 177 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 131825..132179: 91 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 132202..132224: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 132633..132655: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 133947..134025: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 134616..134635: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 1 | 135085..135104: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 1 | 136026..136045: 4 instructions, 1 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 1 | 137807..137826: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 1 | 138599..138615: 4 instructions, 0 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 138994..139189: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 139266..140277: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 140439..140816: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 141838..142393: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 | 0 | 142727..143930: 284 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 669/640/30/61/31/225/53/103/72/15/142/0/0/3/94 | 0 | 144151..146324: 479 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 | 0 | 146755..148147: 332 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 1 | 149483..149528: 12 instructions, 1 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 150136..150360: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 150453..150687: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 150780..150984: 52 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 | 0 | 151391..151872: 123 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 152004..152122: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 152240..154235: 415 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 154364..154411: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 154713..155607: 191 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 155736..155783: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 156085..156979: 191 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 157108..157155: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 | 0 | 157614..160033: 523 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 | 0 | 160157..160280: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 160318..160549: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 160628..160834: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 160957..161075: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 161883..161899: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 162258..162274: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 | 0 | 162333..162692: 90 instructions, 0 samples |

## Text / cc
Native SHA-256: 4e070f9fd8773e0b01f5132a83dd71c25b5d70749ecebd199fe60161201fb58c
C SHA-256: 4abc4d6c8da1175b763ea53c29cb054974fffe4c2a5f06131c828109890de89c
ZX SHA-256: 743b12e43fe1f1e37f3cdf63d5336ff63049d0d484b8776afa90a7d1c1a08a4d
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 36/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 67..138: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 285/159/9/34/82/140/3/30/21/16/47/0/0/0/27 | 140/128/10/16/20/66/4/16/6/7/32/0/0/1/29 | 0 | 902..1583: 158 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 1 | none |
| text___zi_spec_6a19f7c98e79a734 | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | 3112..3134: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3586..3605: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 4408..5967: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 6164..6183: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 6437..6589: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 1 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 6669..6779: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 3 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 7679..7722: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 8704..11148: 526 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 0 | 12873..13132: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| text_DrawSession | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 293/263/10/101/82/188/2/15/5/17/159/0/0/0/13 | 0 | 14442..15067: 121 instructions, 0 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 16060..19018: 557 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 1 | 19124..19518: 76 instructions, 1 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 0 | 20298..20320: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 23023..23336: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 23730..23822: 25 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 24573..24837: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 0 | 25722..25738: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 26033..26299: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 44/20/1/5/0/9/1/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 192/153/11/6/6/53/12/32/21/4/0/7/0/4/25 | 0 | 27511..28339: 195 instructions, 0 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 1 | 29592..32529: 622 instructions, 0 samples |
| std_math_Finite32 | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 11/0/0/0/0/3/0/2/2/0/4/0/0/0/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 57/47/1/10/12/28/0/2/1/2/14/0/0/1/10 | 0 | 33105..33127: 4 instructions, 0 samples |
| text_PrepareText | 263/81/2/62/65/169/0/13/11/15/16/0/0/0/2 | 172/91/3/37/23/69/2/7/4/14/32/0/0/3/4 | 1 | 34108..34298: 39 instructions, 1 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 48/0/0/2/2/9/2/4/3/1/4/2/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 209/114/8/44/72/120/1/16/8/6/94/0/0/0/6 | 81/32/1/8/14/25/0/1/0/2/56/0/0/3/4 | 1 | 35177..35735: 109 instructions, 0 samples |
| text_TextOrigin | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 1 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 | 0 | 37105..37388: 74 instructions, 0 samples |
| text_TextHiddenLines | 118/39/2/11/2/22/0/11/9/10/22/0/0/0/4 | 86/45/3/10/2/22/7/11/4/2/22/0/0/0/6 | 0 | 37771..37858: 26 instructions, 0 samples |
| tree_TreeSetHiddenLines | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 0 | none |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 49/0/0/15/15/33/0/1/1/1/30/0/0/0/0 | 0 | 38274..38296: 4 instructions, 0 samples |
| text_PaintTextLines | 694/612/7/184/149/360/10/49/42/45/86/0/0/0/42 | 585/494/12/195/126/348/8/37/15/15/222/0/0/8/37 | 0 | 38796..41993: 609 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 1 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 0 | 43025..43303: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 45544..46592: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 47448..49253: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 0 | 49420..49442: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 1 | 51141..51163: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 2 | 51715..52501: 209 instructions, 1 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 0 | 52954..54185: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 1 | 54949..55629: 170 instructions, 1 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 80/21/2/2/2/20/10/5/3/2/0/3/0/3/2 | 0 | 55760..55943: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 0 | 56209..56538: 77 instructions, 0 samples |
| text_DefaultTextFace | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/7/0/0/0/1/0/0/0/0/0 | 0 | 56917..56936: 4 instructions, 0 samples |
| text_TextFaceFor | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 38/0/0/9/5/17/0/0/0/1/10/0/0/1/0 | 0 | 57177..57196: 4 instructions, 0 samples |
| text_behavior_ResolveTextStyle | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 79/58/6/8/3/19/2/14/6/1/11/0/0/0/3 | 0 | 57560..57947: 100 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 14/0/0/0/0/4/0/3/1/0/7/0/0/0/0 | 0 | 58970..59075: 27 instructions, 0 samples |
| text_behavior_TextWrapPolicy | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 699/635/27/132/244/441/16/68/41/26/20/12/1/0/57 | 264/212/25/12/8/101/33/43/17/3/11/2/0/14/30 | 0 | 59427..62706: 600 instructions, 0 samples |
| text_behavior_TextExtent | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 13/0/0/0/0/0/0/2/2/0/0/2/0/0/0 | 0 | 63006..63076: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| geometry_WorldToScreen | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 42/0/0/8/3/11/0/0/0/2/35/0/0/0/0 | 0 | none |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 0 | 63876..64183: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 1 | 64303..66000: 412 instructions, 1 samples |
| text_behavior_TextAlignmentOffset | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 18/0/0/0/0/3/1/2/2/0/9/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 31/8/1/0/0/1/0/4/3/1/0/0/0/0/1 | 0 | 66293..66352: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| text_TextEllipsisStart | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 49/33/3/2/2/8/2/6/3/1/0/0/0/0/5 | 0 | 66556..66881: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 15/0/0/0/0/1/1/1/1/0/0/0/0/2/0 | 0 | none |
| text_PaintTextLineRuns | 343/323/7/109/109/237/12/16/9/13/36/0/0/0/14 | 280/230/5/89/52/150/5/9/4/11/101/2/0/4/22 | 1 | 67101..68922: 323 instructions, 1 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 0 | 69681..69703: 4 instructions, 0 samples |
| text_PaintTextEmphasis | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 239/187/5/55/36/103/8/16/11/6/37/2/0/7/21 | 0 | 70363..71271: 172 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 9/0/0/0/0/2/0/0/0/0/8/0/0/0/0 | 0 | 71438..71506: 17 instructions, 0 samples |
| text_PaintTextRule | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 131/0/0/27/26/54/0/4/2/6/72/0/0/0/0 | 0 | 71677..71699: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 72913..72923: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 1 | 73053..73072: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 0 | 73704..74032: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 278/259/112/0/0/136/10/118/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 27/0/0/4/2/12/1/1/1/1/8/0/0/0/0 | 0 | 74878..74900: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 0 | 75163..75185: 4 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 352/318/8/47/23/84/6/28/20/6/12/0/0/13/29 | 2 | 75791..76916: 280 instructions, 1 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/2/0/2/1/0/0/1/0/0/1 | 0 | 76955..77007: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 25/21/6/1/0/10/0/9/3/0/1/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 249/141/4/0/0/0/0/21/17/12/102/0/0/0/27 | 95/49/3/0/0/28/2/16/11/0/55/0/0/0/0 | 0 | 77749..78372: 137 instructions, 0 samples |
| std_math_CosineDegrees | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/1/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 | 3 | 78754..78924: 44 instructions, 3 samples |
| text_TextLinkRunAt | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 75/52/2/4/0/11/7/11/9/0/0/0/0/3/7 | 0 | 79113..79241: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 21/0/0/0/0/1/0/0/0/3/0/2/0/4/0 | 0 | none |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 80852..80874: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 0 | 81139..84614: 715 instructions, 0 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 32/13/3/0/0/3/2/4/1/0/0/0/0/2/3 | 0 | none |
| paint_queue_PaintClippedEmphasis | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 45/0/0/4/16/23/0/0/0/1/6/0/0/0/0 | 0 | 85700..85722: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 0 | 86917..86939: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 44/0/0/0/7/10/0/0/0/1/18/0/0/2/0 | 0 | 88437..88459: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | 99131..99150: 4 instructions, 0 samples |
| text_layout_TextTokenNext | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 259/237/31/6/4/46/10/62/31/6/0/0/0/4/53 | 2 | 99460..100584: 277 instructions, 2 samples |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 100783..100936: 42 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 102184..102262: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 102607..103673: 253 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 104394..104413: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 0 | 105720..105973: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 0 | 106018..106040: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 106642..106664: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/4/11/0/0/0/1/0/0/0/0/0 | 0 | 107477..107499: 4 instructions, 0 samples |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 3 | 109077..109096: 4 instructions, 1 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 102/89/15/0/0/18/1/28/13/5/0/0/0/0/20 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 1 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 111037..111082: 12 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 178/148/12/9/11/52/15/26/14/4/21/0/0/6/20 | 0 | 112091..112646: 127 instructions, 0 samples |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 113398..114091: 158 instructions, 0 samples |
| shape_paint_EmitShape | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 25/0/0/4/0/8/1/11/7/0/3/0/0/0/0 | 0 | 114305..114327: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 11/0/0/4/0/5/0/0/0/1/0/0/0/0/0 | 0 | 114750..114772: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 115309..115328: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 115778..115797: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 0 | 116719..116738: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 3 | 118500..118519: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 119292..119308: 4 instructions, 0 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 24/8/2/0/0/6/0/4/2/0/0/0/0/0/2 | 1 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 114/105/18/0/0/12/2/28/10/0/0/0/0/0/37 | 1 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 36/19/2/0/0/4/1/7/5/0/0/0/0/0/6 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 122267..122491: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 122584..122818: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 122911..123115: 52 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 123336..123531: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 123608..124619: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 124781..125158: 96 instructions, 0 samples |
| shape_paint_EmitLineShape | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 102/16/1/19/0/39/5/5/4/5/44/0/0/2/1 | 0 | 125243..125265: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 228/121/6/50/41/93/6/22/16/17/61/0/0/0/16 | 176/124/9/25/14/65/1/20/11/8/53/0/0/3/14 | 0 | 126568..127052: 113 instructions, 0 samples |
| shape_paint_EmitRingShape | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 108/82/9/20/6/42/1/14/5/8/45/0/0/0/9 | 0 | 127814..127992: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 37/12/1/7/0/19/1/3/2/2/9/0/0/0/3 | 0 | 128039..128061: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 1 | 128528..130523: 415 instructions, 1 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 130652..130699: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 131001..131895: 191 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 132024..132071: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 132373..133267: 191 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 133396..133443: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| shape_paint_ShapeAt | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 5/0/0/1/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 96/0/0/8/6/19/0/5/5/3/63/0/0/2/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 31/20/3/0/0/6/0/5/2/0/18/0/0/2/2 | 0 | none |
| shape_paint_PointsBounds | 124/109/5/20/18/46/1/11/6/5/45/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 | 0 | 135410..135756: 87 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 376/47/6/82/69/202/16/34/28/28/74/0/0/0/8 | 215/178/9/33/33/78/6/16/6/12/113/0/0/2/11 | 0 | 137127..137252: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 19/0/0/0/0/6/0/2/2/1/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcDirections | 72/56/2/10/12/30/3/7/5/3/25/0/0/0/5 | 48/18/1/4/5/15/3/3/2/2/21/0/0/0/1 | 0 | 137856..138092: 56 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 77/17/1/0/0/3/2/2/1/2/3/2/0/9/3 | 0 | 138156..138597: 116 instructions, 0 samples |
| paint_queue_GradientColorAt | 72/53/2/13/10/24/0/5/3/6/18/0/0/0/4 | 153/120/12/0/0/25/9/29/13/0/73/0/0/6/14 | 0 | 138723..138931: 52 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 35/15/1/3/3/11/2/2/1/1/17/0/0/0/1 | 0 | 138980..139114: 35 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 59/36/1/7/5/15/1/2/1/2/37/0/0/0/1 | 0 | 139158..139393: 59 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 139510..139628: 32 instructions, 0 samples |
| std_math_Sqrt | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 100/80/5/0/0/24/1/17/11/0/57/0/0/2/10 | 0 | 139867..140266: 98 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 105/68/1/11/6/41/5/9/8/6/12/0/0/0/12 | 79/36/3/2/2/20/7/8/5/2/28/0/0/0/5 | 0 | 140455..140709: 68 instructions, 0 samples |
| shape_paint_ShapeClipSide | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 272/247/28/13/7/60/14/61/33/0/111/0/0/1/21 | 0 | 141048..141363: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 141654..141663: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/3/1/0/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 33/0/0/0/0/7/3/4/3/0/17/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcPoint | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/8/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| std_math_Finite64 | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 10/0/0/0/0/2/0/2/2/0/4/0/0/0/0 | 0 | none |
| std_math_MathSplit | 176/162/16/3/5/10/0/25/9/6/43/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 142407..143074: 151 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 74/48/7/0/0/15/4/17/10/0/21/0/0/0/6 | 0 | 143201..143562: 87 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/20/0/0/1/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 16/0/0/0/0/1/1/3/3/0/4/0/0/1/0 | 0 | none |
| shape_paint_ShapeCrossing | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 44/20/1/0/0/2/1/6/5/0/30/0/0/2/1 | 0 | none |
| widget_benchmark_host_RasterTriangles | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/9/2/4/1/0/6/0/0/1/2 | 0 | 144304..144439: 34 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 144486..144588: 24 instructions, 0 samples |

## Text / clang-19
Native SHA-256: 4e070f9fd8773e0b01f5132a83dd71c25b5d70749ecebd199fe60161201fb58c
C SHA-256: 1843fc432bba8da9d944fbc07ff6f1c5107586deae3ebb97aff7ec269525218a
ZX SHA-256: 743b12e43fe1f1e37f3cdf63d5336ff63049d0d484b8776afa90a7d1c1a08a4d
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 38/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 67..138: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 285/159/9/34/82/140/3/30/21/16/47/0/0/0/27 | 155/66/6/12/15/54/0/20/14/14/19/0/0/1/12 | 0 | 902..1583: 158 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 0 | none |
| text___zi_spec_6a19f7c98e79a734 | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | 3112..3134: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3586..3605: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 4408..5967: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 6164..6183: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 6437..6589: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 1 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 6669..6779: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 3 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 1 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 7679..7722: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 379/237/17/42/15/132/43/58/41/21/26/4/0/5/35 | 0 | 8704..11148: 526 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 12873..13132: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| text_DrawSession | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 293/19/2/36/34/100/0/25/23/20/108/1/0/2/2 | 2 | 14442..15067: 121 instructions, 1 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 16060..19018: 557 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 1 | 19124..19518: 76 instructions, 1 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 1 | 20298..20320: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 23023..23336: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 23730..23822: 25 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 24573..24837: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 25722..25738: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 26033..26299: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 1 | 27511..28339: 195 instructions, 1 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 2 | 29592..32529: 622 instructions, 1 samples |
| std_math_Finite32 | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 1 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 0 | none |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 48/0/0/12/9/28/0/2/2/2/12/0/0/1/0 | 0 | 33105..33127: 4 instructions, 0 samples |
| text_PrepareText | 263/81/2/62/65/169/0/13/11/15/16/0/0/0/2 | 134/18/1/13/2/44/0/6/5/14/18/0/0/3/3 | 1 | 34288..34482: 45 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 209/114/8/44/72/120/1/16/8/6/94/0/0/0/6 | 124/0/0/28/18/48/0/0/0/10/111/0/0/0/0 | 0 | 35177..35735: 109 instructions, 0 samples |
| text_TextOrigin | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 0 | 37105..37388: 74 instructions, 0 samples |
| text_TextHiddenLines | 118/39/2/11/2/22/0/11/9/10/22/0/0/0/4 | 112/19/2/2/1/17/1/11/9/3/51/1/0/0/2 | 0 | 37771..37858: 26 instructions, 0 samples |
| tree_TreeSetHiddenLines | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 | 0 | 38274..38296: 4 instructions, 0 samples |
| text_PaintTextLines | 694/612/7/184/149/360/10/49/42/45/86/0/0/0/42 | 490/418/9/71/66/235/5/41/32/23/164/0/0/10/38 | 0 | 38796..41993: 609 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 0 | 43025..43303: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 45544..46592: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 47448..49253: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 1 | 49566..49588: 4 instructions, 1 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 1 | 51211..51233: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 3 | 51715..52501: 209 instructions, 3 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 52954..54185: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 1 | 54949..55629: 170 instructions, 1 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 55760..55943: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 1 | 56209..56538: 77 instructions, 1 samples |
| text_DefaultTextFace | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 | 1 | 57045..57067: 4 instructions, 1 samples |
| text_TextFaceFor | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 | 0 | 57177..57196: 4 instructions, 0 samples |
| text_behavior_ResolveTextStyle | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 57/0/0/5/0/8/0/3/3/2/7/0/0/2/0 | 0 | 57560..57947: 100 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | 58970..59075: 27 instructions, 0 samples |
| text_behavior_TextWrapPolicy | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 699/635/27/132/244/441/16/68/41/26/20/12/1/0/57 | 345/109/17/14/7/108/42/57/40/5/19/10/0/13/13 | 0 | 59427..62706: 600 instructions, 0 samples |
| text_behavior_TextExtent | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 | 0 | 63006..63076: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| geometry_WorldToScreen | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 | 0 | none |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 0 | 63876..64183: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 0 | 64303..66000: 412 instructions, 0 samples |
| text_behavior_TextAlignmentOffset | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 | 0 | 66293..66352: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_TextEllipsisStart | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 55/23/3/2/2/9/3/7/4/1/0/0/0/0/2 | 0 | 66556..66881: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 | 0 | none |
| text_PaintTextLineRuns | 343/323/7/109/109/237/12/16/9/13/36/0/0/0/14 | 301/250/14/75/45/157/7/25/11/16/80/2/0/1/20 | 0 | 67101..68922: 323 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 2 | 69681..69703: 4 instructions, 0 samples |
| text_PaintTextEmphasis | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 221/165/17/28/27/87/10/29/12/5/13/2/0/3/17 | 1 | 70363..71271: 172 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 | 0 | 71438..71506: 17 instructions, 0 samples |
| text_PaintTextRule | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 101/0/0/26/22/52/0/4/2/6/51/0/0/0/0 | 0 | 71677..71699: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 72913..72923: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 1 | 73107..73126: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 0 | 73704..74032: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 | 0 | 74878..74900: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 0 | 75163..75185: 4 instructions, 0 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 | 0 | 75791..76916: 280 instructions, 0 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 45/22/2/0/0/7/5/6/4/0/0/5/0/0/1 | 0 | 76955..77007: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 249/141/4/0/0/0/0/21/17/12/102/0/0/0/27 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 | 1 | 77749..78372: 137 instructions, 0 samples |
| std_math_CosineDegrees | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 80/0/0/0/0/24/0/12/12/1/63/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 1 | 78853..79015: 47 instructions, 1 samples |
| text_TextLinkRunAt | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 57/27/7/2/0/10/5/14/7/0/0/0/0/0/2 | 0 | 79113..79241: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 | 0 | none |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 80852..80874: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 1 | 81139..84614: 715 instructions, 1 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 29/0/0/0/0/2/2/3/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_PaintClippedEmphasis | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 | 0 | 85700..85722: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 86917..86939: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 | 0 | 88437..88459: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 1 | 99131..99150: 4 instructions, 1 samples |
| text_layout_TextTokenNext | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 210/116/20/4/2/22/7/53/33/3/0/0/0/3/34 | 2 | 99460..100584: 277 instructions, 2 samples |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 100783..100936: 42 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 102184..102262: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 102607..103673: 253 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 104394..104413: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 0 | 105720..105973: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 106018..106040: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 106642..106664: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 | 0 | 107477..107499: 4 instructions, 0 samples |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 1 | 109077..109096: 4 instructions, 0 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 82/46/12/0/0/8/0/24/12/1/0/0/0/0/16 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 111037..111082: 12 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 112091..112646: 127 instructions, 0 samples |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 113398..114091: 158 instructions, 0 samples |
| shape_paint_EmitShape | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 | 0 | 114305..114327: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 | 0 | 114750..114772: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 115309..115328: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 115778..115797: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 0 | 116719..116738: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 2 | 118500..118519: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 119292..119308: 4 instructions, 0 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 1 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/10/7/0/0/0/0/0/7 | 2 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 122267..122491: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 122584..122818: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 122911..123115: 52 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 123336..123531: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 123608..124619: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 124781..125158: 96 instructions, 0 samples |
| shape_paint_EmitLineShape | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 | 0 | 125243..125265: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 228/121/6/50/41/93/6/22/16/17/61/0/0/0/16 | 280/234/11/36/19/99/9/21/10/10/108/2/0/8/25 | 0 | 126568..127052: 113 instructions, 0 samples |
| shape_paint_EmitRingShape | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 182/57/3/32/13/80/9/21/18/9/103/0/0/0/0 | 0 | 127814..127992: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 | 0 | 128039..128061: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 128528..130523: 415 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 130652..130699: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 131001..131895: 191 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 132024..132071: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 132373..133267: 191 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 133396..133443: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| shape_paint_ShapeAt | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 | 0 | none |
| shape_paint_PointsBounds | 124/109/5/20/18/46/1/11/6/5/45/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/3 | 0 | 135410..135756: 87 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 376/47/6/82/69/202/16/34/28/28/74/0/0/0/8 | 567/481/23/63/91/187/30/86/62/10/307/0/0/1/48 | 0 | 137127..137252: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcDirections | 72/56/2/10/12/30/3/7/5/3/25/0/0/0/5 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 | 0 | 137856..138092: 56 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 | 0 | 138156..138597: 116 instructions, 0 samples |
| paint_queue_GradientColorAt | 72/53/2/13/10/24/0/5/3/6/18/0/0/0/4 | 143/0/0/0/0/17/0/16/16/1/72/0/0/13/0 | 0 | 138723..138931: 52 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 | 0 | 138980..139114: 35 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 | 0 | 139158..139393: 59 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 139510..139628: 32 instructions, 0 samples |
| std_math_Sqrt | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 228/43/7/0/0/40/6/32/25/0/139/0/0/2/6 | 0 | 139867..140266: 98 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 105/68/1/11/6/41/5/9/8/6/12/0/0/0/12 | 63/0/0/6/4/21/3/6/6/2/29/0/0/0/0 | 0 | 140455..140709: 68 instructions, 0 samples |
| shape_paint_ShapeClipSide | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 | 0 | 141048..141363: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 141654..141663: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 | 0 | none |
| shape_paint_ShapeArcPoint | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 | 0 | none |
| std_math_Finite64 | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| std_math_MathSplit | 176/162/16/3/5/10/0/25/9/6/43/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 142407..143074: 151 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 89/35/5/0/0/15/4/22/17/0/34/0/0/1/4 | 0 | 143201..143562: 87 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 | 0 | none |
| shape_paint_ShapeCrossing | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTriangles | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 | 0 | 144304..144439: 34 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 144486..144588: 24 instructions, 0 samples |

## Input / cc
Native SHA-256: 9978ae84253c8a66826459bd18f3379aa9b73784caf6e1942727fc5bffbfe82e
C SHA-256: c77c158bdc0e61b8c246fb0fb2efd8e62a5f537c6e0ef7fc7e573b897bd6d311
ZX SHA-256: 5c47cb8218aeed347661abe511c183dca5e1278e852b5791595e3dcbe7f4b75e
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 81/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 92..163: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 164/152/15/12/15/57/0/17/2/11/8/0/0/0/42 | 0 | 974..996: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 0 | none |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 195/136/7/50/17/84/3/18/11/9/16/0/0/3/19 | 0 | 4511..4533: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_input_TreePointerUpdate | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 7731..7750: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 8553..10112: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 10309..10328: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 10582..10734: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 10814..10924: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 9 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 11824..11867: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 12849..15293: 526 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 0 | 17018..17277: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 7/0/0/2/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/1/1/0/0/0/0/0/0 | 1 | 17503..17641: 38 instructions, 1 samples |
| text_context_menu_TextMenuTakeCommand | 153/110/7/23/10/47/0/31/24/14/0/0/0/0/18 | 66/46/6/2/0/14/2/13/7/3/0/0/0/0/6 | 0 | 17841..18271: 108 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 63/22/1/6/5/13/1/4/3/4/11/0/0/1/1 | 0 | none |
| textfield_TextFieldCore | 3950/3778/52/857/1164/2342/28/400/348/251/279/1/1/0/265 | 2425/2344/104/678/397/1177/25/259/140/150/359/2/0/15/320 | 5 | 19556..39157: 3694 instructions, 5 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 83/0/0/17/17/53/4/3/3/6/0/4/0/0/0 | 54/20/3/0/0/19/6/5/2/3/0/0/0/0/3 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 18/4/1/0/0/4/0/4/3/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 17/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 41212..44170: 557 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 4 | 44276..44670: 76 instructions, 4 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 | 0 | 45164..46244: 232 instructions, 0 samples |
| tree_input_TreeTextFocused | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 | 0 | 46875..47099: 57 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 1 | 48123..48139: 4 instructions, 0 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 0 | 48892..48914: 4 instructions, 0 samples |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 1 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 51682..51995: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 52389..52481: 25 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | 52852..53116: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 1 | none |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 53653..53919: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 | 0 | 55131..55959: 195 instructions, 0 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 1 | 58615..60184: 345 instructions, 1 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/1/1/0/0/0/0/0/0 | 0 | none |
| system_clipboard_SystemClipboardText | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 73/46/8/2/1/11/6/14/6/2/0/0/0/0/6 | 0 | 60388..60944: 140 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 | 1 | none |
| text_context_menu_TextMenuOpenAt | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 61/12/2/3/0/16/2/6/4/2/2/0/0/0/2 | 0 | 61432..61648: 51 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 | 0 | 61963..62018: 17 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakeDragAt | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 173/148/11/13/4/58/10/25/14/6/22/0/0/8/20 | 0 | none |
| tree_TreePreviousWidgetState | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 48/0/0/0/0/17/3/4/4/1/2/1/0/1/0 | 0 | none |
| tree_input_TreeHasFocus | 101/71/3/11/8/35/6/14/11/8/0/3/0/0/14 | 71/45/3/5/4/20/7/9/6/3/0/1/0/0/7 | 0 | 64095..64374: 71 instructions, 0 samples |
| tree_input_TreeBlurAt | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 37/7/1/0/0/9/4/5/4/1/0/1/0/1/2 | 0 | none |
| textfield_TextFieldFaceFor | 179/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 121/74/1/30/15/63/1/2/1/10/52/0/0/0/4 | 1 | 64691..65664: 167 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 14/0/0/0/0/2/0/3/1/0/7/0/0/0/0 | 0 | 66050..66185: 38 instructions, 0 samples |
| textfield_TextFieldSecureDisplay | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 49/20/2/1/0/5/1/5/3/2/0/0/0/0/3 | 0 | 66280..66407: 36 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 61/0/0/0/0/5/2/6/2/0/7/0/0/2/0 | 1 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 66/38/2/8/10/20/1/4/2/3/0/0/0/0/2 | 0 | 66873..67132: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 37/0/0/0/0/6/0/6/3/0/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 29/7/1/0/0/5/2/2/1/1/0/0/0/0/3 | 0 | 67443..67506: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 39/21/2/0/0/2/1/5/3/1/0/0/0/0/2 | 0 | 67649..67792: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 159/126/11/16/29/59/8/28/17/0/23/0/0/0/13 | 0 | 68670..69177: 113 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 15/11/1/2/3/6/1/2/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 21/8/1/0/0/6/3/5/4/0/0/0/0/0/3 | 0 | 69425..69502: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 133/98/8/12/3/35/9/24/16/4/0/0/0/0/9 | 0 | 69717..69862: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/88/8/4/3/13/4/18/10/0/0/0/0/5/8 | 0 | 70286..70947: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 106/75/9/16/12/37/1/14/5/5/12/0/0/0/6 | 0 | 71156..71178: 4 instructions, 0 samples |
| text_request_TextRequestTake | 198/0/0/47/28/118/10/16/16/11/0/9/0/0/0 | 79/0/0/4/3/24/4/8/8/4/0/1/0/0/0 | 0 | none |
| text_request_TextRequestChangeFor | 280/263/12/53/38/116/5/36/24/13/0/0/0/0/24 | 160/128/12/20/5/57/8/24/12/6/0/0/0/0/9 | 0 | 72784..73674: 238 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 67/50/5/0/0/7/3/8/3/4/0/0/0/0/5 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 34/0/0/0/0/5/1/2/2/1/0/0/0/0/0 | 0 | 73869..74098: 60 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 25/0/0/0/0/2/2/1/1/0/0/0/0/0/0 | 0 | 74130..74300: 52 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 48/0/0/2/2/9/2/4/3/1/4/2/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetCaptureTab | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 49/36/5/0/0/7/1/8/3/4/0/1/0/1/4 | 0 | none |
| tree_input_TreeFocusSubmittedAt | 227/4/1/39/26/96/10/28/27/14/10/7/0/0/1 | 158/139/17/8/16/61/7/23/6/6/25/3/0/5/15 | 0 | 75727..75746: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 155/104/7/15/5/48/10/26/19/3/2/4/0/3/14 | 0 | 76922..77307: 96 instructions, 0 samples |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 | 0 | 77513..77796: 74 instructions, 0 samples |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetTextValue | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 172/64/6/12/9/59/8/28/22/4/0/3/0/1/8 | 0 | 79632..80341: 178 instructions, 0 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 36/0/0/5/0/8/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 0 | 81188..81210: 4 instructions, 0 samples |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 1 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 | 1 | 82897..82919: 4 instructions, 1 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 179/0/0/22/20/54/1/13/13/12/47/0/1/0/0 | 120/0/0/2/0/13/2/13/5/0/41/0/0/4/0 | 0 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 17/0/0/0/0/3/0/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 17/0/0/0/0/3/0/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 27/0/0/0/0/3/0/4/2/0/6/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 1 | 84875..84897: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 28/0/0/0/0/4/2/4/2/0/7/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 85492..85509: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 85539..85593: 18 instructions, 0 samples |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 | 1 | 86321..86343: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 86841..86860: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 86939..86961: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 18/6/1/0/0/6/2/3/2/0/0/0/0/0/1 | 0 | 86994..87039: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 90/0/0/18/17/57/6/3/3/5/0/6/0/0/0 | 53/17/2/0/0/14/5/4/2/3/0/0/0/0/1 | 0 | none |
| text_input_platform_TextInputFocused | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 38/25/4/0/0/8/3/5/1/3/0/0/0/0/1 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 13/6/1/0/0/1/0/3/1/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 49/6/1/0/0/10/5/3/2/1/0/0/0/2/4 | 0 | 87858..87925: 17 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 152/138/6/8/31/79/3/13/7/4/21/0/0/2/33 | 0 | 88286..90739: 460 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 1 | 91754..92032: 58 instructions, 1 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 94273..95321: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 96177..97982: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 1 | 98295..98317: 4 instructions, 1 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 2 | 98665..98687: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 0 | 100732..101518: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 0 | 101971..103202: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 0 | 103966..104646: 170 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/21/2/2/2/22/12/5/3/2/0/3/0/3/2 | 1 | 104777..104960: 42 instructions, 1 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 0 | 105226..105555: 77 instructions, 0 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 11/0/0/0/0/4/2/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 63/0/0/10/13/35/0/0/0/6/26/0/0/0/0 | 0 | 106916..106935: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 | 0 | 107112..108378: 251 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 | 0 | 108504..108666: 47 instructions, 0 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 18/0/0/0/0/2/0/3/1/0/4/0/0/0/0 | 0 | 108812..108821: 5 instructions, 0 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 108913..108993: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/15/2/0/0/2/2/6/4/0/0/0/0/0/3 | 0 | 109046..109110: 17 instructions, 0 samples |
| std_grapheme_Previous | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 109220..109363: 41 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 | 0 | 109698..109974: 69 instructions, 0 samples |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 2 | 110103..110431: 82 instructions, 2 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 0 | 110828..111135: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 0 | 111255..112952: 412 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 1 | 113261..113283: 4 instructions, 1 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 113765..113787: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 2 | 113971..117446: 715 instructions, 2 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 132/113/2/24/6/35/0/11/8/2/63/0/0/6/11 | 1 | 117677..117699: 4 instructions, 1 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 24/0/0/0/0/2/0/3/1/0/4/0/0/0/0 | 0 | 118256..118462: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 0 | 118535..118557: 4 instructions, 0 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 119799..119821: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 1 | 120550..121145: 157 instructions, 0 samples |
| button_SubmitButton | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 | 0 | 121249..124071: 584 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 1034/999/36/259/267/614/28/93/57/58/87/16/0/0/95 | 570/548/32/102/94/285/46/65/33/18/103/1/0/4/61 | 0 | 124350..129102: 958 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 129590..129600: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 0 | 129730..129749: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 278/259/112/0/0/136/10/118/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 1 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 | 0 | 131463..131811: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 | 0 | 133502..133524: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 1 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 134619..135312: 158 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 135459..136525: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 1 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 137209..137228: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 0 | 138535..138788: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 3 | 138906..138925: 4 instructions, 3 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 140372..140450: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 266/172/4/89/71/181/2/9/5/15/144/0/0/0/7 | 0 | 141651..141673: 4 instructions, 0 samples |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 | 0 | 142927..144225: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 | 0 | 144739..146053: 284 instructions, 0 samples |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 | 0 | 147050..153689: 1342 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 26/12/1/0/0/2/1/2/1/2/0/0/0/0/1 | 0 | none |
| tree_TreeNextKey | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 65/9/1/0/0/11/5/5/4/2/0/5/0/4/1 | 0 | 154097..154159: 15 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 46/9/1/0/0/6/2/4/3/2/0/1/0/1/1 | 0 | none |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 8/0/0/0/0/4/3/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 25/7/2/0/0/7/3/6/4/0/0/0/0/0/2 | 0 | 154541..154623: 21 instructions, 0 samples |
| contextmenu_ContextMenuFind | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/2/5/3/0/0/0/0/0/3 | 0 | 154688..154775: 23 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 115/43/1/32/26/64/1/3/2/6/46/1/0/0/1 | 85/0/0/11/8/21/2/2/1/4/48/0/0/0/0 | 0 | 155211..155388: 43 instructions, 0 samples |
| overlay_Draw | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 257/214/9/46/27/82/5/22/13/19/36/0/0/6/31 | 0 | 155723..157025: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1465/1248/10/377/376/810/10/72/62/142/347/1/0/0/114 | 1082/888/26/288/120/488/12/74/36/102/301/0/0/2/121 | 0 | 158115..164302: 1136 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 1 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 0 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 175756..175951: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 176028..177039: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 177201..177578: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 | 0 | 178600..179155: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 15 | 180714..180736: 4 instructions, 1 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 182280..182325: 12 instructions, 0 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 22/0/0/0/0/4/0/2/0/0/12/0/0/3/0 | 0 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 45/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 0 | 182418..182584: 33 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 | 0 | 183081..184149: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 | 0 | 184194..184351: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 | 0 | 184662..184840: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 | 0 | 184961..185575: 146 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 | 0 | 185919..185941: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/20/2/6/5/4/33/0/0/0/6 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 | 0 | 186304..186544: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 | 0 | 187090..187112: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 | 0 | 187324..187482: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 | 0 | 187710..187881: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 | 0 | 188188..188934: 172 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 372/317/4/71/71/157/3/27/23/22/113/4/1/0/36 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 | 0 | 189187..190603: 313 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 | 0 | 190872..190894: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 | 0 | 191536..191558: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 446/409/22/88/61/185/12/48/26/33/63/11/0/0/41 | 261/197/11/38/27/100/8/30/19/19/54/0/0/2/32 | 0 | 192406..193614: 276 instructions, 0 samples |
| dpi_ScalePixels | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 42/0/0/0/0/7/0/7/4/0/19/0/0/0/0 | 0 | 194283..194578: 75 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 113/93/7/21/13/42/4/18/11/9/31/4/0/0/12 | 93/61/7/3/3/21/5/14/7/4/26/0/0/1/11 | 0 | 194665..195058: 91 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/14/1/0/0/1/1/2/1/0/0/0/0/2/4 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/17/1/1/0/4/2/5/4/4/0/0/0/0/1 | 0 | none |
| tree_TreeSetPopup | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 38/0/0/0/0/8/1/5/5/2/0/1/0/1/0 | 0 | none |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 196070..196223: 42 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| tree_TreePushScope | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 81/64/5/0/0/13/9/9/4/8/0/8/0/0/12 | 0 | none |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/1/1/2/0/1/1/2/2/0/0/0/0 | 0 | 197124..197137: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 26/13/4/0/0/8/2/7/3/0/0/0/0/0/4 | 0 | 197152..197315: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/2/0/4/0/2/2/4/4/0/0/0/0 | 0 | 197565..197574: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 24/6/1/0/0/9/5/3/2/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 644/596/8/74/331/442/14/31/23/21/80/0/0/0/35 | 182/92/5/18/42/76/9/11/6/4/78/0/0/1/19 | 0 | 198118..200991: 562 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 1 | 201520..201744: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 201837..202071: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 202164..202368: 52 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 202726..202745: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 2 | 203195..203214: 4 instructions, 0 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 3 | 204136..204155: 4 instructions, 1 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 6 | 205917..205936: 4 instructions, 0 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 0 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 3 | 206709..206725: 4 instructions, 3 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 | 0 | 206793..207454: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 | 0 | 208045..208064: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 | 0 | 208770..208929: 43 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 | 0 | 208989..209068: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 | 0 | 209184..209593: 97 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 | 0 | 210125..210248: 30 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 | 0 | 210418..210620: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 | 0 | 212050..212072: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 230/194/7/10/16/40/7/36/29/13/11/0/6/0/35 | 122/102/6/0/6/38/14/13/5/0/6/4/0/7/26 | 0 | 212903..213551: 193 instructions, 0 samples |
| tree_TreePopScope | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 54/27/1/2/1/6/3/4/3/4/0/4/0/0/5 | 0 | none |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 214055..214173: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 2 | 214291..216286: 415 instructions, 2 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 216415..216462: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 216764..217658: 191 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 217787..217834: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 218136..219030: 191 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 219159..219206: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 | 0 | 219394..219452: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 | 0 | none |
| svg_shape_ShapeRaster | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 | 0 | 219983..220833: 207 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 345/311/29/30/34/97/9/61/32/9/0/8/1/2/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 | 0 | 221088..222092: 262 instructions, 0 samples |
| image_behavior_ImageFitBounds | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 | 0 | 222254..222956: 177 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 | 0 | 223072..223426: 91 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 223449..223471: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 223880..223902: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 | 0 | 224123..225326: 284 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 | 0 | 225547..227720: 479 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 | 0 | 228151..229543: 332 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 19/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 | 0 | 230547..231028: 123 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 | 0 | 231425..233844: 523 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | 233968..234091: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 | 0 | 234129..234360: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 | 0 | 234439..234645: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 | 0 | 234768..234886: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 235694..235710: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 236069..236085: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 | 0 | 236144..236503: 90 instructions, 0 samples |

## Input / clang-19
Native SHA-256: 9978ae84253c8a66826459bd18f3379aa9b73784caf6e1942727fc5bffbfe82e
C SHA-256: 508d91ebbc6bf9126f61f6932319a62a4847f7cba48106af5b72f913a5b9aceb
ZX SHA-256: 5c47cb8218aeed347661abe511c183dca5e1278e852b5791595e3dcbe7f4b75e
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 80/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 92..163: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 161/0/0/13/24/70/0/19/19/12/23/0/0/0/0 | 1 | 974..996: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 0 | none |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 117/0/0/14/4/41/2/10/10/9/2/0/0/0/0 | 0 | 4511..4533: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_input_TreePointerUpdate | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 | 2 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 7731..7750: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 8553..10112: 355 instructions, 0 samples |
| text_request_TextRequestClear | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 10309..10328: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 10582..10734: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 10814..10924: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 10 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 11824..11867: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 | 0 | 12849..15293: 526 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 17018..17277: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 | 1 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | 17503..17641: 38 instructions, 0 samples |
| text_context_menu_TextMenuTakeCommand | 153/110/7/23/10/47/0/31/24/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 | 0 | 17841..18271: 108 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 85/30/2/7/4/25/2/9/7/6/12/0/0/0/3 | 0 | none |
| textfield_TextFieldCore | 3950/3778/52/857/1164/2342/28/400/348/251/279/1/1/0/265 | 2155/1176/35/302/166/909/19/256/221/148/299/1/0/13/120 | 6 | 19556..39157: 3694 instructions, 5 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 83/0/0/17/17/53/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 41212..44170: 557 instructions, 0 samples |
| paint_queue_PaintFlush | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 2 | 44276..44670: 76 instructions, 2 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 1 | 45164..46244: 232 instructions, 1 samples |
| tree_input_TreeTextFocused | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 0 | 46875..47099: 57 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| tree_TreeInputAt | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 1 | 48123..48139: 4 instructions, 0 samples |
| tree_TreeRead | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 0 | 48892..48914: 4 instructions, 0 samples |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 51682..51995: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 52389..52481: 25 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | 52852..53116: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| geometry_RectangleIntersection | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 53653..53919: 69 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 0 | 55131..55959: 195 instructions, 0 samples |
| tree_TreeSubmit | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 1 | 57212..60149: 622 instructions, 0 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| system_clipboard_SystemClipboardText | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 65/17/2/0/0/5/1/9/7/2/0/0/0/0/1 | 0 | 60388..60944: 140 instructions, 0 samples |
| input_protocol_PointInBounds | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenAt | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 55/14/1/3/0/16/1/5/4/3/2/0/0/0/2 | 0 | 61432..61648: 51 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 0 | 61963..62018: 17 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakeDragAt | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 155/35/3/8/1/45/10/27/24/6/8/6/0/0/6 | 0 | none |
| tree_TreePreviousWidgetState | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 | 0 | none |
| tree_input_TreeHasFocus | 101/71/3/11/8/35/6/14/11/8/0/3/0/0/14 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 | 0 | 64095..64374: 71 instructions, 0 samples |
| tree_input_TreeBlurAt | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 | 0 | none |
| textfield_TextFieldFaceFor | 179/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 | 4 | 64691..65664: 167 instructions, 4 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 1 | 66050..66185: 38 instructions, 0 samples |
| textfield_TextFieldSecureDisplay | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 57/8/1/0/0/2/2/5/4/2/0/0/0/0/1 | 0 | 66280..66407: 36 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 | 0 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 | 0 | 66873..67132: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 | 1 | none |
| textfield_TextFieldCodepoints | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 | 0 | 67443..67506: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 1 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | 67649..67792: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 110/98/3/0/0/37/4/14/11/0/19/0/0/0/11 | 0 | 68670..69177: 113 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 32/9/1/0/0/1/0/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 25/12/1/0/0/3/2/4/3/0/0/0/0/0/3 | 0 | 69425..69502: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 | 0 | 69717..69862: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/0/0/2/0/2/0/12/12/0/0/0/0/6/0 | 0 | 70286..70947: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 | 0 | 71156..71178: 4 instructions, 0 samples |
| text_request_TextRequestTake | 198/0/0/47/28/118/10/16/16/11/0/9/0/0/0 | 76/0/0/1/0/18/2/9/9/4/3/1/0/0/0 | 0 | none |
| text_request_TextRequestChangeFor | 280/263/12/53/38/116/5/36/24/13/0/0/0/0/24 | 145/55/6/6/10/39/8/22/16/5/0/0/0/2/0 | 0 | 72784..73674: 238 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 68/18/2/0/0/2/2/8/6/4/0/0/0/0/1 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 32/0/0/1/3/8/0/2/2/1/2/0/0/0/0 | 0 | 73869..74098: 60 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 74130..74300: 52 instructions, 0 samples |
| style_StyleOpacityValue | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetCaptureTab | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 | 0 | none |
| tree_input_TreeFocusSubmittedAt | 227/4/1/39/26/96/10/28/27/14/10/7/0/0/1 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 | 0 | 75727..75746: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 | 0 | 76922..77307: 96 instructions, 0 samples |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 0 | 77513..77796: 74 instructions, 0 samples |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetTextValue | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 138/86/3/8/4/47/12/22/19/5/0/1/0/2/6 | 0 | 79632..80341: 178 instructions, 0 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 32/0/0/5/0/8/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 81188..81210: 4 instructions, 0 samples |
| drawing_props_ColorFromPacked | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| paint_queue_PaintRoundedOutline | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 0 | 82759..82781: 4 instructions, 0 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| text_input_TextFieldPaintFor | 179/0/0/22/20/54/1/13/13/12/47/0/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 | 1 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 22/0/0/0/0/3/0/2/2/1/11/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClippedLabel | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 0 | 84875..84897: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 23/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 85492..85509: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 85539..85593: 18 instructions, 0 samples |
| paint_queue_PaintLine | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 86321..86343: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 86841..86860: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 86939..86961: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 82/28/4/0/0/21/19/15/11/0/4/0/0/0/3 | 0 | 86994..87039: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 90/0/0/18/17/57/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocused | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 | 0 | 87858..87925: 17 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 140/0/0/6/33/75/0/12/12/5/18/0/0/2/0 | 0 | 88286..90739: 460 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 0 | 91754..92032: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 94273..95321: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 96177..97982: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 98149..98171: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 4 | 98562..98584: 4 instructions, 2 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 1 | 100732..101518: 209 instructions, 0 samples |
| std_grapheme_Push | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 101971..103202: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 0 | 103966..104646: 170 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 104777..104960: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 0 | 105226..105555: 77 instructions, 0 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 | 0 | 106916..106935: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 | 0 | 107112..108378: 251 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 0 | 108504..108666: 47 instructions, 0 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 1 | 108812..108821: 5 instructions, 1 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 108913..108993: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/9/1/0/0/3/2/3/2/0/0/0/0/0/2 | 0 | 109046..109110: 17 instructions, 0 samples |
| std_grapheme_Previous | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 65/38/2/6/2/11/2/8/6/2/0/0/0/1/4 | 0 | 109220..109363: 41 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 0 | 109698..109974: 69 instructions, 0 samples |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 0 | 110103..110431: 82 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 0 | 110828..111135: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 0 | 111255..112952: 412 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 100/8/2/26/22/52/4/6/4/7/11/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 113141..113163: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 113765..113787: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 2 | 113971..117446: 715 instructions, 2 samples |
| surface_Unit | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 117677..117699: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 0 | 118256..118462: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 1 | 118535..118557: 4 instructions, 1 samples |
| paint_queue_EmitLabel | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 119799..119821: 4 instructions, 0 samples |
| paint_queue_EmitLine | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 120550..121145: 157 instructions, 0 samples |
| button_SubmitButton | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 | 0 | 121249..124071: 584 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 1034/999/36/259/267/614/28/93/57/58/87/16/0/0/95 | 588/511/31/82/99/249/33/78/47/19/99/14/0/3/58 | 0 | 124350..129102: 958 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 129590..129600: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 1 | 129730..129749: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 0 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 1 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 1 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 131463..131811: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 133502..133524: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | none |
| paint_queue_ShapeVisible | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 1 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 134619..135312: 158 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 135459..136525: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 137209..137228: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 1 | none |
| paint_queue_KeepPaintMesh | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 0 | 138535..138788: 56 instructions, 0 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 2 | 138906..138925: 4 instructions, 2 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 140372..140450: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 0 | 141651..141673: 4 instructions, 0 samples |
| button_ButtonDetailSize | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 0 | none |
| button_behavior_MeasureBounds | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 | 0 | 142927..144225: 305 instructions, 0 samples |
| style_StyleMinHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 | 0 | 144739..146053: 284 instructions, 0 samples |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 | 0 | 147050..153689: 1342 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_TreeNextKey | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 | 0 | 154097..154159: 15 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 | 0 | none |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 | 0 | 154541..154623: 21 instructions, 0 samples |
| contextmenu_ContextMenuFind | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 | 0 | 154688..154775: 23 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 115/43/1/32/26/64/1/3/2/6/46/1/0/0/1 | 122/32/8/25/13/45/3/12/3/5/50/2/0/0/9 | 0 | 155211..155388: 43 instructions, 0 samples |
| overlay_Draw | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 200/82/1/15/5/38/0/16/15/19/34/0/0/1/9 | 0 | 155723..157025: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1465/1248/10/377/376/810/10/72/62/142/347/1/0/0/114 | 940/723/13/211/90/371/26/63/50/101/289/2/0/1/99 | 0 | 158115..164302: 1136 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 1 | none |
| paint_queue_ShapeRadius | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 175756..175951: 44 instructions, 0 samples |
| paint_queue_FillShapeRows | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 176028..177039: 238 instructions, 0 samples |
| paint_queue_FillClippedSpan | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 177201..177578: 96 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 178600..179155: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 13 | 180821..180843: 4 instructions, 1 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 182280..182325: 12 instructions, 0 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| button_ButtonFaceFinish | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 182418..182584: 33 instructions, 0 samples |
| button_ButtonDefaultFace | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 0 | 183081..184149: 201 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 184194..184351: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 184662..184840: 48 instructions, 0 samples |
| button_behavior_MeasureContent | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 | 0 | 184961..185575: 146 instructions, 0 samples |
| button_behavior_ShapeWidth | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 0 | 185919..185941: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 80/58/1/7/9/20/2/6/5/4/33/0/0/0/6 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 186304..186544: 58 instructions, 0 samples |
| image_lookup_IconLookupImage | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 187090..187112: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 187324..187482: 42 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 187710..187881: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 | 0 | 188188..188934: 172 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 372/317/4/71/71/157/3/27/23/22/113/4/1/0/36 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 | 0 | 189187..190603: 313 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 190872..190894: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 191536..191558: 4 instructions, 0 samples |
| paint_queue_PaintImage | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 446/409/22/88/61/185/12/48/26/33/63/11/0/0/41 | 267/183/14/44/15/86/6/33/19/22/50/1/0/0/34 | 0 | 192406..193614: 276 instructions, 0 samples |
| dpi_ScalePixels | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 | 0 | 194283..194578: 75 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 113/93/7/21/13/42/4/18/11/9/31/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 | 0 | 194665..195058: 91 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/0/0/0/3/0 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 | 0 | none |
| tree_TreeSetPopup | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 | 0 | none |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 196070..196223: 42 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_TreePushScope | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 | 0 | none |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 | 0 | 197124..197137: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 30/16/4/0/0/6/1/7/3/0/0/0/0/0/5 | 0 | 197152..197315: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 | 0 | 197565..197574: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 644/596/8/74/331/442/14/31/23/21/80/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 | 0 | 198118..200991: 562 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 1 | none |
| paint_queue_ShapeCornerInset | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 201520..201744: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 201837..202071: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 202164..202368: 52 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 202726..202745: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 2 | 203195..203214: 4 instructions, 1 samples |
| theme_DefaultMetrics | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 1 | 204136..204155: 4 instructions, 0 samples |
| theme_DefaultShadows | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 8 | 205917..205936: 4 instructions, 1 samples |
| theme_DefaultMotion | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 1 | none |
| theme_DefaultFonts | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 2 | 206709..206725: 4 instructions, 2 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 206793..207454: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 | 0 | 208045..208064: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 208770..208929: 43 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 208989..209068: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 | 0 | 209184..209593: 97 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 210125..210248: 30 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 | 0 | 210418..210620: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 212050..212072: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 230/194/7/10/16/40/7/36/29/13/11/0/6/0/35 | 98/25/2/1/4/25/10/9/7/1/6/4/0/5/4 | 0 | 212903..213551: 193 instructions, 0 samples |
| tree_TreePopScope | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 52/10/1/0/0/6/4/6/5/4/0/4/0/0/2 | 0 | none |
| paint_queue_ShapeSquareRoot | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 214055..214173: 32 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 1 | 214291..216286: 415 instructions, 1 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 216415..216462: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 216764..217658: 191 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 217787..217834: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 218136..219030: 191 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 219159..219206: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 46/22/2/0/0/8/7/6/4/0/0/5/0/0/1 | 0 | 219394..219452: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 | 0 | 219983..220833: 207 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 345/311/29/30/34/97/9/61/32/9/0/8/1/2/41 | 375/321/22/23/20/119/48/57/35/1/32/4/0/4/51 | 0 | 221088..222092: 262 instructions, 0 samples |
| image_behavior_ImageFitBounds | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 | 0 | 222254..222956: 177 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 223072..223426: 91 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 223449..223471: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 223880..223902: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 | 0 | 224123..225326: 284 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 669/640/30/61/31/225/53/103/72/15/142/0/0/3/94 | 0 | 225547..227720: 479 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 | 0 | 228151..229543: 332 instructions, 0 samples |
| paint_queue_EmitImageBegin | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 | 0 | 230547..231028: 123 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 | 0 | 231425..233844: 523 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 | 0 | 233968..234091: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 234129..234360: 53 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 234439..234645: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 234768..234886: 32 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 80/4/1/1/32/35/2/1/0/0/59/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 235694..235710: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 77/4/1/1/32/35/2/1/0/0/56/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 236069..236085: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 | 0 | 236144..236503: 90 instructions, 0 samples |

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
| Records/cc/wasm_tier_Records | 92465 | 106/106 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 929/149/21/260/520/807/5/46/25/5/0/4/2/0/29 | 70/57/4/0/0/20/2/6/2/0/0/2/0/2/11 |
| Records/clang-19/wasm_tier_Records | 92142 | 101/101 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 929/149/21/260/520/807/5/46/25/5/0/4/2/0/29 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 |
| Sieve/cc/wasm_tier_Sieve | 62222 | 127/127 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 34/19/3/0/0/7/6/6/3/1/0/1/0/0/9 |
| Sieve/clang-19/wasm_tier_Sieve | 60000 | 113/113 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 |
| Hash/cc/wasm_tier_Hash | 58064 | 149/149 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 26/12/2/0/0/5/1/2/0/0/0/4/0/1/4 |
| Hash/clang-19/wasm_tier_Hash | 37096 | 142/142 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 |
| Vectors/cc/wasm_tier_Vectors | 26667 | 125/151 82% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/137/15/22/23/47/2/31/16/3/0/1/0/1/15 | 101/90/7/7/8/24/5/16/9/2/7/0/0/0/9 |
| Button/clang-19/session_SessionIndex | 18186 | 9/38 23% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Vectors/clang-19/wasm_tier_Vectors | 15006 | 129/150 86% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/137/15/22/23/47/2/31/16/3/0/1/0/1/15 | 123/101/10/3/3/22/15/17/7/2/26/1/0/1/13 |
| Button/cc/theme_DefaultTheme | 11157 | 9/38 23% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Input/clang-19/session_SessionIndex | 9598 | 10/80 12% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Input/clang-19/theme_DefaultShadows | 8745 | 8/80 10% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Input/cc/theme_DefaultTheme | 8723 | 15/81 18% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Input/clang-19/theme_DefaultTheme | 7520 | 13/80 16% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Button/clang-19/theme_DefaultTheme | 7307 | 6/38 15% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Input/cc/session_SessionIndex | 7142 | 9/81 11% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Text/cc/theme_DefaultShadows | 7026 | 3/36 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Button/cc/theme_DefaultShadows | 6656 | 3/38 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Input/cc/theme_DefaultShadows | 6245 | 6/81 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Text/clang-19/session_SessionIndex | 6062 | 3/38 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Text/cc/session_SessionIndex | 5357 | 3/36 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Text/clang-19/std_utf8_Decode | 5097 | 3/38 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Button/cc/session_SessionIndex | 5075 | 3/38 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Text/clang-19/text_layout_TextNoBreakBefore | 4752 | 2/38 5% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/10/7/0/0/0/0/0/7 |
| Text/cc/widget_benchmark_host_MeasureGlyphWidth | 4605 | 3/36 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 |
| Text/clang-19/theme_DefaultShadows | 4602 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Button/cc/theme_DefaultFonts | 4246 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_utf8_Decode | 3936 | 2/36 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Text/cc/theme_DefaultTheme | 3925 | 3/36 8% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Text/clang-19/paint_queue_PaintClippedLabel | 3901 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Input/clang-19/textfield_TextFieldCore | 3408 | 6/80 7% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3950/3778/52/857/1164/2342/28/400/348/251/279/1/1/0/265 | 2155/1176/35/302/166/909/19/256/221/148/299/1/0/13/120 |
| Input/cc/theme_DefaultMetrics | 3248 | 3/81 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Input/clang-19/paint_queue_PendingPaintAt | 3227 | 4/80 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Input/cc/theme_DefaultFonts | 2988 | 3/81 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/tree_TreeSubmit | 2784 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Input/cc/theme_CurrentTheme | 2645 | 3/81 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/font_metrics_GlyphWidth | 2579 | 1/36 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_EmitPaintDirect | 2543 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Button/cc/drawing_props_ColorFromPacked | 2473 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldCore | 2383 | 5/81 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3950/3778/52/857/1164/2342/28/400/348/251/279/1/1/0/265 | 2425/2344/104/678/397/1177/25/259/140/150/359/2/0/15/320 |
| Button/clang-19/button_behavior_MeasureBounds | 2378 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 |
| Text/cc/text_TextOrigin | 2333 | 1/36 2% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Button/clang-19/theme_DefaultMetrics | 2308 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Button/clang-19/theme_DefaultShadows | 2301 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 287/4/1/38/115/176/2/3/2/0/163/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Input/clang-19/textfield_TextFieldFaceFor | 2290 | 4/80 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 179/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 |
| Text/clang-19/std_math_Finite32 | 2275 | 1/38 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/text_DefaultTextFace | 2198 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 |
| Button/clang-19/theme_LightPalette | 2136 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/paint_queue_PaintClippedLabel | 2123 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Button/clang-19/theme_DefaultFonts | 2123 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextIdeographic | 2076 | 1/36 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 24/8/2/0/0/6/0/4/2/0/0/0/0/0/2 |
| Text/cc/text_layout_TextNoBreakBefore | 2062 | 1/36 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 114/105/18/0/0/12/2/28/10/0/0/0/0/0/37 |
| Input/clang-19/theme_LightPalette | 2029 | 2/80 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang-19/theme_DefaultFonts | 2017 | 2/80 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Input/cc/theme_LightPalette | 2004 | 2/81 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/text_layout_TextIdeographic | 1994 | 1/38 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Button/cc/theme_DefaultMotion | 1945 | 1/38 2% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Text/clang-19/theme_CurrentTheme | 1879 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/std_math_SineDegrees | 1796 | 1/38 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 249/141/4/0/0/0/0/21/17/12/102/0/0/0/27 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 |
| Button/cc/button_PaintButtonForNode | 1792 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 |
| Input/clang-19/theme_CurrentTheme | 1785 | 2/80 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Button/cc/widget_benchmarks_ButtonCase | 1759 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 158/147/9/4/10/45/4/19/10/12/8/0/0/0/36 |
| Text/cc/paint_queue___zi_spec_9308732530dd822f | 1719 | 1/36 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Text/clang-19/text_layout_TextTokenNext | 1709 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 210/116/20/4/2/22/7/53/33/3/0/0/0/3/34 |
| Text/cc/geometry_RotatedRectangleBounds | 1701 | 1/36 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 209/114/8/44/72/120/1/16/8/6/94/0/0/0/6 | 81/32/1/8/14/25/0/1/0/2/56/0/0/3/4 |
| Text/clang-19/paint_queue_PendingPaintAt | 1698 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Button/cc/tree_TreeFindOld | 1677 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Button/cc/paint_queue_PaintFlush | 1631 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Button/clang-19/tree_TreeBuildOldIndex | 1532 | 2/38 5% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Input/cc/paint_queue_PaintFlush | 1531 | 4/81 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Input/cc/paint_queue___zi_spec_9308732530dd822f | 1528 | 2/81 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Text/cc/paint_queue_EmitPaintDirect | 1527 | 1/36 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Text/clang-19/tree_TreeRead | 1482 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Button/cc/paint_queue_EmitPaintDirect | 1447 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Text/cc/paint_queue_PaintOverflow | 1388 | 1/36 2% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/cc/tree_TreeSubmit | 1374 | 2/38 5% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Button/cc/tree_TreeCount | 1315 | 1/38 2% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/text_PrepareText | 1290 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 263/81/2/62/65/169/0/13/11/15/16/0/0/0/2 | 134/18/1/13/2/44/0/6/5/14/18/0/0/3/3 |
| Text/clang-19/tree_input_SecondaryPointerSample | 1287 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Input/clang-19/tree_input_TreePointerUpdate | 1285 | 2/80 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 |
| Text/clang-19/paint_queue_EmitPaintDirect | 1271 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Button/clang-19/paint_queue_KeepPaintMesh | 1247 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Text/clang-19/theme_DefaultTheme | 1217 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 363/36/9/69/206/295/18/13/4/6/82/0/0/0/9 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Button/cc/tree_input_PointerSample | 1200 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Text/cc/paint_queue_PendingPaintAt | 1198 | 1/36 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Button/cc/tree_input_TreeTakePointerActivationAt | 1192 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 |
| Input/cc/drawing_props_ColorFromPacked | 1160 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/paint_queue_PendingPaintAt | 1135 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Input/clang-19/font_metrics_GlyphLineHeight | 1125 | 1/80 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/button_PaintButtonForNode | 1123 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 |
| Input/clang-19/theme_DefaultMetrics | 1096 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Button/clang-19/button_ButtonDefaultFace | 1075 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Input/cc/paint_queue_PendingPaintAt | 1065 | 2/81 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Button/cc/paint_queue_QueuePaint | 1055 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Input/clang-19/theme_DefaultMotion | 1032 | 1/80 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Text/clang-19/paint_queue_QueuePaint | 1031 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Input/cc/paint_queue_PaintLine | 1029 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 |
| Input/cc/paint_queue_PaintRoundedOutline | 1020 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 |
| Button/clang-19/tree_TreeSetLabel | 1016 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/cc/paint_queue_PaintClippedLabel | 996 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Input/cc/paint_queue_QueuePaint | 990 | 2/81 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Input/clang-19/paint_queue_QueuePaint | 979 | 2/80 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Text/cc/text_PrepareText | 961 | 1/36 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 263/81/2/62/65/169/0/13/11/15/16/0/0/0/2 | 172/91/3/37/23/69/2/7/4/14/32/0/0/3/4 |
| Text/cc/text_layout_TextTokenNext | 928 | 2/36 5% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 259/237/31/6/4/46/10/62/31/6/0/0/0/4/53 |
| Input/cc/paint_queue_EmitLine | 906 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Input/cc/paint_queue_ShapeVisible | 895 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Input/clang-19/paint_queue___zi_spec_9308732530dd822f | 876 | 1/80 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/cc/paint_queue_PaintFlush | 861 | 1/36 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 129/76/4/28/30/76/9/10/6/11/6/3/0/0/12 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Input/clang-19/widget_benchmarks_InputCase | 858 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 161/0/0/13/24/70/0/19/19/12/23/0/0/0/0 |
| Input/clang-19/textfield_TextFieldKey | 850 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_TreeLayerAt | 845 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Input/clang-19/text_input_TextInputContentWidth | 833 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Text/cc/tree_draw_BeginFrame | 814 | 1/36 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Input/clang-19/paint_queue_ShapeWhole | 811 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Input/clang-19/tree_TreeSubmittedGeometryAt | 809 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Input/clang-19/std_utf8_Decode | 807 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Input/cc/input_protocol_PointInBounds | 801 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 |
| Input/clang-19/text_input_TextInputScaledMetric | 787 | 1/80 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Input/clang-19/text_input_TextPointerOffset | 781 | 1/80 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 |
| Text/clang-19/text_DrawSession | 780 | 2/38 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 293/19/2/36/34/100/0/25/23/20/108/1/0/2/2 |
| Text/clang-19/tree_TreeBuildOldIndex | 766 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Text/cc/tree_TreeStoreLabel | 754 | 1/36 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Input/clang-19/tree_TreeInputAt | 745 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Input/clang-19/text_input_TextFieldPaintFor | 740 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 179/0/0/22/20/54/1/13/13/12/47/0/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 |
| Text/cc/tree_TreeSubmit | 725 | 1/36 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Input/clang-19/tree_input_TreeHitAt | 724 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Input/clang-19/text_TextSpan | 714 | 1/80 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeFinish | 703 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Button/cc/button_ButtonDefaultFace | 702 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 |
| Input/clang-19/widget_benchmark_host_RecordBounds | 690 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Input/clang-19/tree_TreeSubmit | 661 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Input/cc/text_input_TextSelectionRangeForLength | 653 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/tree_TreeFindOld | 608 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Input/clang-19/paint_queue_EmitPaintDirect | 604 | 1/80 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Text/clang-19/tree_TreeBuildNewIndex | 567 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Input/cc/tree_TreeLayerAt | 521 | 2/81 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Text/cc/text_PaintTextLineRuns | 510 | 1/36 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/323/7/109/109/237/12/16/9/13/36/0/0/0/14 | 280/230/5/89/52/150/5/9/4/11/101/2/0/4/22 |
| Input/clang-19/theme_TypeLabel | 500 | 1/80 1% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/cc/tree_TreeInputAt | 498 | 1/81 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Button/cc/tree_TreeRead | 480 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Input/clang-19/theme_CurrentText | 468 | 1/80 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/cc/tree_TreeBuildNewIndex | 466 | 1/36 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Button/cc/tree_TreeBuildNewIndex | 442 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Text/clang-19/paint_queue_EmitPaint | 430 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Input/cc/textfield_TextFieldFaceFor | 400 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 179/168/6/38/75/119/8/11/5/8/32/0/0/0/14 | 121/74/1/30/15/63/1/2/1/10/52/0/0/0/4 |
| Text/cc/text_layout_NextTextLineUncached | 368 | 2/36 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 352/318/8/47/23/84/6/28/20/6/12/0/0/13/29 |
| Input/clang-19/widget_benchmark_host_RasterRoundedRectangle | 357 | 1/80 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Input/cc/tree_TreeSubmit | 322 | 1/81 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Button/clang-19/style_sheet_ResolveInstalledStyleRules | 312 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 |
| Text/clang-19/widget_benchmark_host_MeasureGlyphWidth | 311 | 1/38 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Input/cc/text_input_TextFieldScrollFor | 269 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 61/0/0/0/0/5/2/6/2/0/7/0/0/2/0 |
| Input/cc/tree_TreeExplicitExists | 190 | 1/81 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/21/2/2/2/22/12/5/3/2/0/3/0/3/2 |
| Input/cc/tree_input_TreeFocusedAt | 135 | 1/81 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Text/clang-19/text_PaintTextEmphasis | 113 | 1/38 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 221/165/17/28/27/87/10/29/12/5/13/2/0/3/17 |
| Input/cc/paint_queue_KeepShortPaintBytes | 92 | 1/81 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Calls/clang-19/wasm_tier_Fib | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 21/7/1/0/0/2/0/2/1/1/0/0/0/0/1 |
| Printing/cc/wasm_tier_Printing | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 31/14/1/0/0/4/0/1/0/1/0/2/0/0/3 |
| Printing/clang-19/wasm_tier_Printing | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 |
| Button/cc/widget_benchmarks_BeginCase | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Button/cc/tree_draw_BeginFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Button/cc/button_Draw | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Button/cc/tree_draw_EndFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Button/cc/tree_TreeNodeAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/cc/tree_input_TreePointerUpdate | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 |
| Button/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_WidgetPaintCount | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/text_request_TextRequestSend | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Button/cc/text_request_TextRequestClear | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Button/cc/std_grapheme_Boundary | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Button/cc/session_SessionOpen | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Button/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/38 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_SecondaryPointerSample | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Button/cc/session_SessionFlagSet | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Button/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Button/cc/tree_input_TreeKeyboardRoute | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Button/cc/text_editing_TextEditingFrameBegin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_PaintClear | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Button/cc/tree_TreeStart | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Button/cc/tree_TreeFrameStatus | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/cc/button_SubmitButton | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 |
| Button/cc/tree_TreeCurrentParent | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Button/cc/session_SessionFlagGet | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_draw_CommitFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Button/cc/paint_queue_PaintSnapshotEmit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Button/cc/tree_TreeIsBuilding | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_input_TreeHitAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 |
| Button/cc/tree_input_TreeTextFocused | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 |
| Button/cc/tree_input_TreeTextKind | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_TreeFocusAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Button/cc/tree_TreeInputAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Button/cc/std_utf8_Valid | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Button/cc/tree_TreeTextRange | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Button/cc/tree_input_TreeFocusedAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Button/cc/geometry_RectangleIntersection | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Button/cc/tree_TreePointerActivatable | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Button/cc/tree_input_TreeActivateAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 |
| Button/cc/cursor_CursorHoverFrameStart | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Button/cc/tree_TreeBuildOldIndex | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 |
| Button/cc/tree_TreePrevious | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 |
| Button/cc/style_ResolveFlags | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Button/cc/style_ResolveState | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Button/cc/tree_input_TreePressedAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 |
| Button/cc/tree_input_TreeHoveredAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 |
| Button/cc/style_ResolveFont | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Button/cc/style_StyleFontValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Button/cc/font_metrics_GlyphWidth | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/font_metrics_GlyphLineHeight | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/button_ButtonDetailSize | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 |
| Button/cc/tree_TreeRootBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Button/cc/button_behavior_MeasureBounds | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 |
| Button/cc/style_StyleMinHeightValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/cc/button_ButtonMinimumHeight | 0 | 0/38 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/button_ButtonFitContentFace | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 |
| Button/cc/tree_TreeSetInteractive | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Button/cc/tree_TreeSetCircleHit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 |
| Button/cc/tree_TreeSetSelected | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Button/cc/tree_TreeSetLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeSetSemanticDetail | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeSetStyleClass | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Button/cc/tree_TreeSetStyleId | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Button/cc/tree_draw_FrameInputCommit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Button/cc/paint_queue_PaintOverflow | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/cc/tree_TreeCancel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeFinish | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Button/cc/paint_queue_PaintOrder | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Button/cc/paint_queue_SnapshotTextRead | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_SnapshotTextJoin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Button/cc/paint_queue_PaintPointCount | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Button/cc/input_protocol_PointInBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 |
| Button/cc/tree_input_TreeTopModalLayer | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/cc/tree_TreeModalLayerAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/cc/tree_TreeButtonLike | 0 | 0/38 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_PointInCircleBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/cc/std_utf8_Decode | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Button/cc/tree_TreeParentGeneration | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Button/cc/tree_TreeIdentityHash | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/tree_TreeExplicitExists | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 78/21/2/2/2/18/7/5/3/2/0/3/0/3/2 |
| Button/cc/style_sheet_ResolveInstalledStyleRules | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 |
| Button/cc/button_ButtonFaceFacts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 |
| Button/cc/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 33/16/2/0/0/4/2/5/3/0/0/0/0/2/5 |
| Button/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/38 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/button_ButtonDetailFont | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Button/cc/style_StyleLineHeightValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/cc/style_FitHeight | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 |
| Button/cc/button_behavior_MeasureContent | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 |
| Button/cc/button_behavior_ShapeWidth | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 |
| Button/cc/theme_ControlHeightFor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 |
| Button/cc/tree_TreeEnsureInteraction | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 |
| Button/cc/tree_TreeEnsureSemantic | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Button/cc/tree_TreeStoreLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Button/cc/style_StyleOpacityValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Button/cc/button_behavior_ButtonCornerRadius | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Button/cc/surface_Opacity | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Button/cc/paint_queue_PaintRoundedFill | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Button/cc/paint_queue_PaintRoundedOutline | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 |
| Button/cc/focus_FocusDefaultOutlineBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/20/2/6/5/4/33/0/0/0/6 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 |
| Button/cc/image_lookup_IconLookupImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 |
| Button/cc/button_behavior_ButtonPictureSize | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 |
| Button/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 |
| Button/cc/button_behavior_ContentLayout | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 |
| Button/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 372/317/4/71/71/157/3/27/23/22/113/4/1/0/36 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 |
| Button/cc/image_lookup_ImageLookupTexture | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 |
| Button/cc/image_behavior_ImageDrawPlanFor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 |
| Button/cc/paint_queue_PaintImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 |
| Button/cc/paint_queue_PaintLine | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 |
| Button/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/cc/cursor_CursorHoverFrameCommit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Button/cc/paint_queue_PaintMeshPointCount | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Button/cc/paint_queue_ResolvePaintText | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Button/cc/tree_TreeModalLayer | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeLayerAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Button/cc/std_utf8_Continuation | 0 | 0/38 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/std_grapheme_IsControl | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/session_WidgetPartKey | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/widget_key_WidgetRepeatedPart | 0 | 0/38 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleBucket | 0 | 0/38 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleFirstBit | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 |
| Button/cc/style_sheet_StyleRuleMatchesState | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleSelectorMatches | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Button/cc/style_MergeValues | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 |
| Button/cc/theme_CurrentTheme | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Button/cc/theme_StateLayer | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 |
| Button/cc/card_behavior_CardDefaultFace | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 |
| Button/cc/theme_FontForRole | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Button/cc/theme_MotionDuration | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 |
| Button/cc/button_behavior_ButtonRoleFactsFor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 |
| Button/cc/tree_TreeLabelSpanAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Button/cc/surface_Unit | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Button/cc/paint_queue_ClipShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Button/cc/drawing_props_PaintPixelScale | 0 | 0/38 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 |
| Button/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 |
| Button/cc/image_behavior_ImageFitRect | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Button/cc/paint_queue_PaintImageWithPolygon | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 |
| Button/cc/paint_queue_EmitLine | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Button/cc/paint_queue_EmitLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Button/cc/paint_queue_ClipLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Button/cc/paint_queue_PendingTextValue | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Button/cc/style_sheet_StyleMatchesOptional | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleControlRoleFacts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Button/cc/paint_queue_ShapeVisible | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Button/cc/paint_queue_ShapeWhole | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Button/cc/raster_shape_FillRoundedRectangle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_FillShapeClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Button/cc/paint_queue_EmitPaintNow | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Button/cc/tree_TreeCurrentPaintGeometry | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Button/cc/tree_TreeSubmittedGeometryAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Button/cc/paint_queue_KeepPaintMesh | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Button/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconHash | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 |
| Button/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 |
| Button/cc/svg_shape_ShapeRaster | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 |
| Button/cc/svg_icon_behavior_SvgIconCells | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 345/311/29/30/34/97/9/61/32/9/0/8/1/2/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 |
| Button/cc/image_behavior_ImageFitBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 |
| Button/cc/image_behavior_ImagePolygonValid | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 |
| Button/cc/paint_queue_EmitImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Button/cc/paint_queue_ClipImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Button/cc/raster_StrokeLine | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/text_typeface_TextTypefaceUse | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Button/cc/raster_text_PaintTextRotated | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Button/cc/raster_text_PaintTextItalicClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Button/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Button/cc/raster_text_PaintTextClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Button/cc/raster_text_PaintText | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/theme_DarkPalette | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/theme_LightPalette | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/theme_DefaultMetrics | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Button/cc/style_sheet_StyleControlFacts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Button/cc/paint_queue_ShapeRadius | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Button/cc/paint_queue_FillShapeRows | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Button/cc/paint_queue_FillClippedSpan | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Button/cc/paint_queue_KeepPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Button/cc/paint_queue_KeepMeshPoints | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Button/cc/svg_shape_ShapeNumber | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 |
| Button/cc/svg_shape_ShapeBuildPath | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 |
| Button/cc/svg_shape_ShapeCoverEdges | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 |
| Button/cc/paint_queue_EmitImageBegin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 |
| Button/cc/paint_queue_EmitImageEnd | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Button/cc/text_emphasis_RunTextItalicRaster | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterText | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Button/cc/style_sheet_StyleDefaultFacts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RecordBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Button/cc/paint_queue_ShapeCornerInset | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Button/cc/paint_queue_KeepShortPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Button/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Button/cc/paint_queue_KeepLongPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Button/cc/svg_shape_ShapeIsDigit | 0 | 0/38 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeIsSpace | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddPoint | 0 | 0/38 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddSpan | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 |
| Button/cc/paint_queue_ShapeSquareRoot | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Button/cc/paint_queue___zi_spec_9308732530dd822f | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Button/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Button/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Button/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 |
| Button/cc/svg_shape_ShapeFillSubpath | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeCollapse | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 |
| Button/cc/svg_shape_ShapeAddCircle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 |
| Button/cc/svg_shape_ShapeSqrt | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 |
| Button/cc/svg_shape_ShapeQuad | 0 | 0/38 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeAbs | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeTriangle | 0 | 0/38 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddEdge | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Button/cc/svg_shape_ShapeAddPolygon | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 |
| Button/clang-19/widget_benchmarks_ButtonCase | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 477/12/3/90/201/308/7/27/24/25/74/0/0/0/3 | 145/0/0/5/5/38/0/13/13/14/8/0/0/0/0 |
| Button/clang-19/widget_benchmarks_BeginCase | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/tree_draw_BeginFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Button/clang-19/button_Draw | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Button/clang-19/tree_draw_EndFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Button/clang-19/tree_TreeCount | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang-19/tree_TreeNodeAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang-19/tree_input_TreePointerUpdate | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 |
| Button/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintCount | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/text_request_TextRequestSend | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Button/clang-19/text_request_TextRequestClear | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/38 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_PointerSample | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Button/clang-19/tree_input_SecondaryPointerSample | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Button/clang-19/session_SessionFlagSet | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 |
| Button/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_PaintClear | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Button/clang-19/tree_TreeStart | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Button/clang-19/tree_TreeFrameStatus | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/clang-19/button_SubmitButton | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 |
| Button/clang-19/tree_TreeCurrentParent | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Button/clang-19/session_SessionFlagGet | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang-19/tree_draw_CommitFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Button/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Button/clang-19/tree_TreeRead | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Button/clang-19/tree_TreeIsBuilding | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang-19/tree_input_TreeHitAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Button/clang-19/tree_input_TreeTextFocused | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Button/clang-19/tree_input_TreeTextKind | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeFocusAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Button/clang-19/tree_TreeInputAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Button/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/tree_TreeTextRange | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Button/clang-19/tree_input_TreeFocusedAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Button/clang-19/geometry_RectangleIntersection | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Button/clang-19/tree_TreePointerActivatable | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Button/clang-19/tree_input_TreeActivateAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Button/clang-19/cursor_CursorHoverFrameStart | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Button/clang-19/tree_TreeSubmit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 877/632/10/119/128/470/62/83/73/33/29/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Button/clang-19/tree_TreePrevious | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Button/clang-19/style_ResolveFlags | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Button/clang-19/tree_input_TreeTakeActivationAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang-19/style_ResolveState | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeHoveredAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Button/clang-19/style_ResolveFont | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Button/clang-19/style_StyleFontValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Button/clang-19/font_metrics_GlyphWidth | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/font_metrics_GlyphLineHeight | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/button_ButtonDetailSize | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 |
| Button/clang-19/tree_TreeRootBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Button/clang-19/style_StyleMinHeightValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang-19/button_ButtonMinimumHeight | 0 | 0/38 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/button_ButtonFitContentFace | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 |
| Button/clang-19/tree_TreeSetInteractive | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetCircleHit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Button/clang-19/tree_TreeSetSelected | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetSemanticDetail | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetStyleClass | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetStyleId | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_draw_FrameInputCommit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Button/clang-19/paint_queue_PaintOverflow | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/clang-19/tree_TreeCancel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Button/clang-19/tree_TreeFinish | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Button/clang-19/paint_queue_PaintOrder | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Button/clang-19/paint_queue_SnapshotTextRead | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Button/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Button/clang-19/paint_queue_PaintPointCount | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Button/clang-19/paint_queue_EmitPaint | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Button/clang-19/paint_queue_PendingPaintAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 299/28/7/46/112/212/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Button/clang-19/input_protocol_PointInBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Button/clang-19/tree_input_TreeTopModalLayer | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/clang-19/tree_TreeButtonLike | 0 | 0/38 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_PointInCircleBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/clang-19/tree_TreeOverlayLayerAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/std_utf8_Decode | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Button/clang-19/std_grapheme_Push | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Button/clang-19/tree_TreeParentGeneration | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Button/clang-19/tree_TreeIdentityHash | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang-19/tree_TreeBuildNewIndex | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Button/clang-19/tree_TreeExplicitExists | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Button/clang-19/tree_TreeFindOld | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Button/clang-19/tree_input_TreeTakePointerActivationAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 |
| Button/clang-19/button_ButtonFaceFinish | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Button/clang-19/button_ButtonFaceFacts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Button/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/38 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/button_ButtonDetailFont | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Button/clang-19/style_StyleLineHeightValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang-19/style_FitHeight | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Button/clang-19/button_behavior_MeasureContent | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 |
| Button/clang-19/button_behavior_ShapeWidth | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Button/clang-19/theme_ControlHeightFor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Button/clang-19/tree_TreeEnsureInteraction | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Button/clang-19/tree_TreeEnsureSemantic | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Button/clang-19/tree_TreeStoreLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Button/clang-19/style_StyleOpacityValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonCornerRadius | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Button/clang-19/drawing_props_ColorFromPacked | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/surface_Opacity | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Button/clang-19/paint_queue_PaintRoundedFill | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Button/clang-19/paint_queue_PaintRoundedOutline | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Button/clang-19/focus_FocusDefaultOutlineBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/20/2/6/5/4/33/0/0/0/6 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Button/clang-19/image_lookup_IconLookupImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonPictureSize | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonLabeledPictureSize | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Button/clang-19/button_behavior_ContentLayout | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_PaintSvgIcon | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 372/317/4/71/71/157/3/27/23/22/113/4/1/0/36 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 |
| Button/clang-19/image_lookup_ImageLookupTexture | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Button/clang-19/image_behavior_ImageDrawPlanFor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Button/clang-19/paint_queue_PaintLine | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Button/clang-19/paint_queue_PaintClippedLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Button/clang-19/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Button/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Button/clang-19/paint_queue_ResolvePaintText | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/tree_TreeModalLayer | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang-19/std_utf8_Continuation | 0 | 0/38 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/std_grapheme_data_Property | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Button/clang-19/std_grapheme_IsControl | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/session_WidgetPartKey | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/38 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleBucket | 0 | 0/38 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleFirstBit | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Button/clang-19/style_sheet_StyleRuleMatchesState | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleSelectorMatches | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Button/clang-19/style_MergeValues | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Button/clang-19/theme_CurrentTheme | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/theme_StateLayer | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Button/clang-19/card_behavior_CardDefaultFace | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 |
| Button/clang-19/theme_ShapeMedium | 0 | 0/38 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/theme_TypeLabel | 0 | 0/38 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/theme_FontForRole | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Button/clang-19/theme_MotionDuration | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Button/clang-19/tree_TreeLabelSpanAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Button/clang-19/surface_Unit | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Button/clang-19/paint_queue_ClipShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Button/clang-19/paint_queue_QueuePaint | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Button/clang-19/paint_queue_EmitRoundedOutline | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Button/clang-19/drawing_props_PaintPixelScale | 0 | 0/38 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconSlot | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 |
| Button/clang-19/svg_icon_behavior_SvgIconSnap | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconTexture | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 |
| Button/clang-19/paint_queue_PaintImageWithPolygon | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Button/clang-19/paint_queue_EmitLine | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Button/clang-19/paint_queue_EmitLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Button/clang-19/paint_queue_ClipLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Button/clang-19/paint_queue_PendingTextValue | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Button/clang-19/style_sheet_StyleMatchesOptional | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/theme_ShapeSurface | 0 | 0/38 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/style_sheet_StyleControlRoleFacts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_ShapeVisible | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Button/clang-19/paint_queue_ShapeWhole | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Button/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_FillShapeClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Button/clang-19/paint_queue_EmitPaintNow | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Button/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Button/clang-19/tree_TreeSubmittedGeometryAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Button/clang-19/raster_shape_StrokeRoundedRectangle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconRetire | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeRaster | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 |
| Button/clang-19/image_behavior_ImageFitBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 |
| Button/clang-19/image_behavior_ImagePolygonValid | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Button/clang-19/paint_queue_EmitImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Button/clang-19/paint_queue_ClipImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Button/clang-19/raster_StrokeLine | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/text_typeface_TextTypefaceUse | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/raster_text_PaintTextRotated | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Button/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Button/clang-19/raster_text_PaintTextClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang-19/raster_text_PaintText | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/theme_DarkPalette | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/clang-19/theme_DefaultMotion | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Button/clang-19/style_sheet_StyleControlFacts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Button/clang-19/paint_queue_ShapeRadius | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Button/clang-19/paint_queue_FillClippedSpan | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Button/clang-19/paint_queue_KeepPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Button/clang-19/paint_queue_KeepMeshPoints | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Button/clang-19/svg_shape_ShapeNumber | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 |
| Button/clang-19/svg_shape_ShapeCoverEdges | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 |
| Button/clang-19/paint_queue_EmitImageBegin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RasterImage | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Button/clang-19/paint_queue_EmitImageEnd | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleDefaultFacts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RecordBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Button/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Button/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Button/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Button/clang-19/svg_shape_ShapeIsDigit | 0 | 0/38 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeIsSpace | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang-19/svg_shape_ShapeAddPoint | 0 | 0/38 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAddSpan | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 |
| Button/clang-19/paint_queue_ShapeSquareRoot | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Button/clang-19/paint_queue___zi_spec_9308732530dd822f | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/svg_shape_ShapeStrokeSubpath | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 |
| Button/clang-19/svg_shape_ShapeFillSubpath | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 |
| Button/clang-19/svg_shape_ShapeCollapse | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Button/clang-19/svg_shape_ShapeAddCircle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Button/clang-19/svg_shape_ShapeQuad | 0 | 0/38 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAbs | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeTriangle | 0 | 0/38 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAddEdge | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Button/clang-19/svg_shape_ShapeAddPolygon | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 |
| Text/cc/widget_benchmarks_TextCase | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 285/159/9/34/82/140/3/30/21/16/47/0/0/0/27 | 140/128/10/16/20/66/4/16/6/7/32/0/0/1/29 |
| Text/cc/widget_benchmarks_BeginCase | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Text/cc/text___zi_spec_6a19f7c98e79a734 | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_draw_EndFrame | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Text/cc/tree_TreeCount | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeNodeAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/36 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/36 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_request_TextRequestSend | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Text/cc/text_request_TextRequestClear | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Text/cc/std_grapheme_Boundary | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Text/cc/session_SessionOpen | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Text/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/36 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/cc/tree_input_PointerSample | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Text/cc/tree_input_SecondaryPointerSample | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Text/cc/session_SessionFlagSet | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Text/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Text/cc/tree_input_TreeKeyboardRoute | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Text/cc/text_editing_TextEditingFrameBegin | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_PaintClear | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Text/cc/tree_TreeStart | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Text/cc/tree_TreeFrameStatus | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Text/cc/text_DrawSession | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 344/141/9/96/82/193/14/30/21/29/26/0/0/0/21 | 293/263/10/101/82/188/2/15/5/17/159/0/0/0/13 |
| Text/cc/session_SessionFlagGet | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/tree_draw_CommitFrame | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Text/cc/paint_queue_PaintSnapshotEmit | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Text/cc/tree_TreeRead | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Text/cc/std_utf8_Valid | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Text/cc/tree_TreeTextRange | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Text/cc/tree_input_TreeFocusAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Text/cc/tree_input_TreeFocusedAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Text/cc/tree_TreeInputAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Text/cc/geometry_RectangleIntersection | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Text/cc/tree_TreePointerActivatable | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Text/cc/tree_input_TreeActivateAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 44/20/1/5/0/9/1/2/1/3/0/0/0/1/1 |
| Text/cc/cursor_CursorHoverFrameStart | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Text/cc/tree_TreeBuildOldIndex | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 192/153/11/6/6/53/12/32/21/4/0/7/0/4/25 |
| Text/cc/std_math_Finite32 | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 11/0/0/0/0/3/0/2/2/0/4/0/0/0/0 |
| Text/cc/tree_TreeRootBounds | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Text/cc/tree_TreeIsBuilding | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeSetLabel | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Text/cc/tree_TreeSetStyleClass | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Text/cc/tree_TreeSetStyleId | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Text/cc/tree_TreeSetSemantic | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 |
| Text/cc/text_TextHiddenLines | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 118/39/2/11/2/22/0/11/9/10/22/0/0/0/4 | 86/45/3/10/2/22/7/11/4/2/22/0/0/0/6 |
| Text/cc/tree_TreeSetHiddenLines | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Text/cc/text_PaintTextLines | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 694/612/7/184/149/360/10/49/42/45/86/0/0/0/42 | 585/494/12/195/126/348/8/37/15/15/222/0/0/8/37 |
| Text/cc/tree_draw_FrameInputCommit | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Text/cc/tree_TreeCancel | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeFinish | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Text/cc/paint_queue_PaintOrder | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Text/cc/paint_queue_SnapshotTextRead | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_SnapshotTextJoin | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Text/cc/paint_queue_PaintPointCount | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Text/cc/tree_input_TreeTopModalLayer | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_TreeModalLayerAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_TreeParentGeneration | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Text/cc/tree_TreeIdentityHash | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/tree_TreeExplicitExists | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 80/21/2/2/2/20/10/5/3/2/0/3/0/3/2 |
| Text/cc/tree_TreeFindOld | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Text/cc/text_DefaultTextFace | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 79/8/2/10/37/55/4/2/0/1/23/0/0/0/2 | 13/0/0/0/0/7/0/0/0/1/0/0/0/0/0 |
| Text/cc/text_TextFaceFor | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 38/0/0/9/5/17/0/0/0/1/10/0/0/1/0 |
| Text/cc/text_behavior_ResolveTextStyle | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 79/58/6/8/3/19/2/14/6/1/11/0/0/0/3 |
| Text/cc/style_ResolveFont | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Text/cc/style_StyleFontValue | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Text/cc/drawing_props_ColorFromPacked | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/surface_Opacity | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Text/cc/style_StyleOpacityValue | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Text/cc/font_metrics_GlyphLineHeight | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_layout_TextDefaultLineGap | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 14/0/0/0/0/4/0/3/1/0/7/0/0/0/0 |
| Text/cc/text_behavior_TextWrapPolicy | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/text_layout_NextTextLine | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 699/635/27/132/244/441/16/68/41/26/20/12/1/0/57 | 264/212/25/12/8/101/33/43/17/3/11/2/0/14/30 |
| Text/cc/text_behavior_TextExtent | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/cc/text_layout_TextLayoutTotalHeight | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 13/0/0/0/0/0/0/2/2/0/0/2/0/0/0 |
| Text/cc/tree_TreeCurrentParent | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Text/cc/geometry_WorldToScreen | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 42/0/0/8/3/11/0/0/0/2/35/0/0/0/0 |
| Text/cc/tree_TreeEnsureSemantic | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Text/cc/text_behavior_TextAlignmentOffset | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 18/0/0/0/0/3/1/2/2/0/9/0/0/0/0 |
| Text/cc/text_layout_TextCharactersEnd | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 31/8/1/0/0/1/0/4/3/1/0/0/0/0/1 |
| Text/cc/text_TextSpan | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/cc/text_TextEllipsisStart | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 49/33/3/2/2/8/2/6/3/1/0/0/0/0/5 |
| Text/cc/text_layout_TextLineXFor | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 15/0/0/0/0/1/1/1/1/0/0/0/0/2/0 |
| Text/cc/paint_queue_PaintClippedLabel | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Text/cc/text_behavior_TextStrikethroughBounds | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 9/0/0/0/0/2/0/0/0/0/8/0/0/0/0 |
| Text/cc/text_PaintTextRule | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 131/0/0/27/26/54/0/4/2/6/72/0/0/0/0 |
| Text/cc/cursor_CursorHoverFrameCommit | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Text/cc/paint_queue_PaintMeshPointCount | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Text/cc/paint_queue_ResolvePaintText | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Text/cc/tree_TreeModalLayer | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeLayerAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Text/cc/std_utf8_Continuation | 0 | 0/36 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_grapheme_IsControl | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/session_WidgetPartKey | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/widget_key_WidgetRepeatedPart | 0 | 0/36 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_style_TextStyleFor | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 27/0/0/4/2/12/1/1/1/1/8/0/0/0/0 |
| Text/cc/surface_Unit | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/36 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/font_metrics_GlyphMetricsRevision | 0 | 0/36 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextLineHash | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/2/0/2/1/0/0/1/0/0/1 |
| Text/cc/text_layout_TextLineCacheMatches | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 25/21/6/1/0/10/0/9/3/0/1/0/0/0/0 |
| Text/cc/std_math_SineDegrees | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 249/141/4/0/0/0/0/21/17/12/102/0/0/0/27 | 95/49/3/0/0/28/2/16/11/0/55/0/0/0/0 |
| Text/cc/std_math_CosineDegrees | 0 | 0/36 0% |  | 8/0/0/0/0/0/0/1/0/0/3/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/1/0/0/0/0 |
| Text/cc/tree_TreeLabelSpanAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Text/cc/text_TextLinkRunAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 75/52/2/4/0/11/7/11/9/0/0/0/0/3/7 |
| Text/cc/text_TextLinkColor | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 21/0/0/0/0/1/0/0/0/3/0/2/0/4/0 |
| Text/cc/paint_queue_EmitLabel | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Text/cc/paint_queue_ClipLabel | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Text/cc/paint_queue_QueuePaint | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 758/723/15/93/91/388/25/71/56/47/109/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Text/cc/text_TextRangeValid | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 32/13/3/0/0/3/2/4/1/0/0/0/0/2/3 |
| Text/cc/paint_queue_PaintClippedEmphasis | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 45/0/0/4/16/23/0/0/0/1/6/0/0/0/0 |
| Text/cc/paint_queue_PaintRoundedFill | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Text/cc/shape_paint_PaintTriangleShape | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 44/0/0/0/7/10/0/0/0/1/18/0/0/2/0 |
| Text/cc/paint_queue_PendingTextValue | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Text/cc/theme_CurrentTheme | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/drawing_props_StyleTokenColorOr | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_typeface_TextTypefaceUse | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Text/cc/raster_text_PaintTextRotated | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Text/cc/raster_text_PaintTextItalicClipped | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Text/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Text/cc/raster_text_PaintTextClipped | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Text/cc/raster_text_PaintText | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_EmitPaintNow | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Text/cc/tree_TreeCurrentPaintGeometry | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Text/cc/tree_TreeSubmittedGeometryAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Text/cc/paint_queue_KeepPaintMesh | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Text/cc/paint_queue_ClipShape | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Text/cc/shape_paint_ShapeOffset | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_SubmitShape | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/4/11/0/0/0/1/0/0/0/0/0 |
| Text/cc/text_layout_TextIsSpace | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Text/cc/text_emphasis_RunTextItalicRaster | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterText | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Text/cc/paint_queue_KeepPaintBytes | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Text/cc/paint_queue_KeepMeshPoints | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 178/148/12/9/11/52/15/26/14/4/21/0/0/6/20 |
| Text/cc/paint_queue_ShapeVisible | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Text/cc/paint_queue_ShapeWhole | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Text/cc/raster_shape_FillRoundedRectangle | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_FillShapeClipped | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Text/cc/shape_paint_EmitShape | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 25/0/0/4/0/8/1/11/7/0/3/0/0/0/0 |
| Text/cc/paint_queue_SubmitPaint | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 11/0/0/4/0/5/0/0/0/1/0/0/0/0/0 |
| Text/cc/theme_DarkPalette | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/theme_LightPalette | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/theme_DefaultMetrics | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Text/cc/theme_DefaultMotion | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Text/cc/theme_DefaultFonts | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextNoBreakAfter | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 36/19/2/0/0/4/1/7/5/0/0/0/0/0/6 |
| Text/cc/paint_queue_KeepShortPaintBytes | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Text/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Text/cc/paint_queue_KeepLongPaintBytes | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Text/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Text/cc/paint_queue_ShapeRadius | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Text/cc/paint_queue_FillShapeRows | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Text/cc/paint_queue_FillClippedSpan | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Text/cc/shape_paint_EmitLineShape | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 102/16/1/19/0/39/5/5/4/5/44/0/0/2/1 |
| Text/cc/shape_paint_EmitCircleShape | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 228/121/6/50/41/93/6/22/16/17/61/0/0/0/16 | 176/124/9/25/14/65/1/20/11/8/53/0/0/3/14 |
| Text/cc/shape_paint_EmitRingShape | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 108/82/9/20/6/42/1/14/5/8/45/0/0/0/9 |
| Text/cc/shape_paint_EmitTriangleShape | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 37/12/1/7/0/19/1/3/2/2/9/0/0/0/3 |
| Text/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Text/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Text/cc/widget_benchmark_host_RecordBounds | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Text/cc/paint_queue_ShapeCornerInset | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Text/cc/shape_paint_ShapeAt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 5/0/0/1/0/1/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_LineCornersFor | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 96/0/0/8/6/19/0/5/5/3/63/0/0/2/0 |
| Text/cc/shape_paint_ShapeBatchBegin | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/cc/shape_paint_ShapeHidden | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 31/20/3/0/0/6/0/5/2/0/18/0/0/2/2 |
| Text/cc/shape_paint_PointsBounds | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 124/109/5/20/18/46/1/11/6/5/45/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchTriangle | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 376/47/6/82/69/202/16/34/28/28/74/0/0/0/8 | 215/178/9/33/33/78/6/16/6/12/113/0/0/2/11 |
| Text/cc/shape_paint_ShapeBatchFlush | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 19/0/0/0/0/6/0/2/2/1/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcDirections | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/56/2/10/12/30/3/7/5/3/25/0/0/0/5 | 48/18/1/4/5/15/3/3/2/2/21/0/0/0/1 |
| Text/cc/shape_paint_ShapeFadeBands | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 77/17/1/0/0/3/2/2/1/2/3/2/0/9/3 |
| Text/cc/shape_paint_ShapeBatchSector | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 35/15/1/3/3/11/2/2/1/1/17/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchBand | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 59/36/1/7/5/15/1/2/1/2/37/0/0/0/1 |
| Text/cc/paint_queue_ShapeSquareRoot | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Text/cc/std_math_Sqrt | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 100/80/5/0/0/24/1/17/11/0/57/0/0/2/10 |
| Text/cc/shape_paint_ShapeBatchAdd | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/68/1/11/6/41/5/9/8/6/12/0/0/0/12 | 79/36/3/2/2/20/7/8/5/2/28/0/0/0/5 |
| Text/cc/raster_triangles_FillTriangles | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeChannelChange | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_math_CeilInteger | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/3/1/0/7/0/0/0/0 |
| Text/cc/paint_queue_GradientChannel | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 33/0/0/0/0/7/3/4/3/0/17/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcPoint | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/8/0/0/0/0 |
| Text/cc/std_math_NotANumber | 0 | 0/36 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/std_math_Finite64 | 0 | 0/36 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 10/0/0/0/0/2/0/2/2/0/4/0/0/0/0 |
| Text/cc/std_math_ScaleByPowerOfTwo | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 74/48/7/0/0/15/4/17/10/0/21/0/0/0/6 |
| Text/cc/shape_paint_ShapeInside | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 16/0/0/0/0/1/1/3/3/0/4/0/0/1/0 |
| Text/cc/shape_paint_ShapeCrossing | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 44/20/1/0/0/2/1/6/5/0/30/0/0/2/1 |
| Text/cc/widget_benchmark_host_RasterTriangles | 0 | 0/36 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/9/2/4/1/0/6/0/0/1/2 |
| Text/clang-19/widget_benchmarks_TextCase | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 285/159/9/34/82/140/3/30/21/16/47/0/0/0/27 | 155/66/6/12/15/54/0/20/14/14/19/0/0/1/12 |
| Text/clang-19/widget_benchmarks_BeginCase | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_draw_BeginFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Text/clang-19/text___zi_spec_6a19f7c98e79a734 | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 25/4/1/6/8/14/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang-19/tree_draw_EndFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Text/clang-19/tree_TreeCount | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/tree_TreeNodeAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_request_TextRequestSend | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Text/clang-19/text_request_TextRequestClear | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/38 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/tree_input_PointerSample | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Text/clang-19/session_SessionFlagSet | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Text/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 379/237/17/42/15/132/43/58/41/21/26/4/0/5/35 |
| Text/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_PaintClear | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Text/clang-19/tree_TreeStart | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Text/clang-19/tree_TreeFrameStatus | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Text/clang-19/session_SessionFlagGet | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang-19/tree_draw_CommitFrame | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Text/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Text/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_TreeTextRange | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Text/clang-19/tree_input_TreeFocusAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Text/clang-19/tree_input_TreeFocusedAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Text/clang-19/tree_TreeInputAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 156/8/2/18/46/113/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Text/clang-19/geometry_RectangleIntersection | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Text/clang-19/tree_TreePointerActivatable | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Text/clang-19/tree_input_TreeActivateAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 |
| Text/clang-19/cursor_CursorHoverFrameStart | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Text/clang-19/tree_TreeRootBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Text/clang-19/tree_TreeIsBuilding | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/geometry_RotatedRectangleBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/114/8/44/72/120/1/16/8/6/94/0/0/0/6 | 124/0/0/28/18/48/0/0/0/10/111/0/0/0/0 |
| Text/clang-19/text_TextOrigin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/2/5/16/0/0/0/0/11/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/tree_TreeSetLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Text/clang-19/tree_TreeSetStyleClass | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang-19/tree_TreeSetStyleId | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang-19/tree_TreeSetSemantic | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Text/clang-19/text_TextHiddenLines | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/39/2/11/2/22/0/11/9/10/22/0/0/0/4 | 112/19/2/2/1/17/1/11/9/3/51/1/0/0/2 |
| Text/clang-19/tree_TreeSetHiddenLines | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang-19/text_selection_host_TextSelectionPresent | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 |
| Text/clang-19/text_PaintTextLines | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 694/612/7/184/149/360/10/49/42/45/86/0/0/0/42 | 490/418/9/71/66/235/5/41/32/23/164/0/0/10/38 |
| Text/clang-19/tree_draw_FrameInputCommit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintOverflow | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/clang-19/tree_TreeCancel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Text/clang-19/tree_TreeFinish | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Text/clang-19/paint_queue_PaintOrder | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Text/clang-19/paint_queue_SnapshotTextRead | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Text/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Text/clang-19/paint_queue_PaintPointCount | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Text/clang-19/tree_input_TreeTopModalLayer | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang-19/std_grapheme_Push | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Text/clang-19/tree_TreeParentGeneration | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Text/clang-19/tree_TreeIdentityHash | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang-19/tree_TreeExplicitExists | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Text/clang-19/text_TextFaceFor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 65/16/4/20/21/45/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 |
| Text/clang-19/text_behavior_ResolveTextStyle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 128/100/1/14/14/30/0/23/22/9/9/0/0/2/18 | 57/0/0/5/0/8/0/3/3/2/7/0/0/2/0 |
| Text/clang-19/style_ResolveFont | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Text/clang-19/style_StyleFontValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Text/clang-19/theme_CurrentText | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/drawing_props_ColorFromPacked | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/surface_Opacity | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Text/clang-19/style_StyleOpacityValue | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphLineHeight | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_layout_TextDefaultLineGap | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 47/27/1/0/0/0/0/6/5/4/13/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Text/clang-19/text_behavior_TextWrapPolicy | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/text_layout_NextTextLine | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 699/635/27/132/244/441/16/68/41/26/20/12/1/0/57 | 345/109/17/14/7/108/42/57/40/5/19/10/0/13/13 |
| Text/clang-19/text_behavior_TextExtent | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 18/0/0/0/0/0/0/1/1/1/4/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/clang-19/text_layout_TextLayoutTotalHeight | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 |
| Text/clang-19/tree_TreeCurrentParent | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Text/clang-19/geometry_WorldToScreen | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 48/0/0/8/7/24/0/0/0/2/32/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 |
| Text/clang-19/tree_TreeEnsureSemantic | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Text/clang-19/tree_TreeStoreLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Text/clang-19/text_behavior_TextAlignmentOffset | 0 | 0/38 0% | calls/inlining candidates | 33/0/0/0/0/0/0/3/3/2/10/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 |
| Text/clang-19/text_layout_TextCharactersEnd | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 |
| Text/clang-19/text_TextSpan | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphWidth | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_TextEllipsisStart | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 55/23/3/2/2/9/3/7/4/1/0/0/0/0/2 |
| Text/clang-19/text_layout_TextLineXFor | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 |
| Text/clang-19/text_PaintTextLineRuns | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/323/7/109/109/237/12/16/9/13/36/0/0/0/14 | 301/250/14/75/45/157/7/25/11/16/80/2/0/1/20 |
| Text/clang-19/text_behavior_TextStrikethroughBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 45/17/1/3/9/14/0/3/2/1/18/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 |
| Text/clang-19/text_PaintTextRule | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 218/4/1/77/86/169/2/2/1/10/53/0/0/0/1 | 101/0/0/26/22/52/0/4/2/6/51/0/0/0/0 |
| Text/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Text/clang-19/paint_queue_ResolvePaintText | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_TreeModalLayer | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/tree_TreeLayerAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Text/clang-19/std_utf8_Continuation | 0 | 0/38 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/std_grapheme_data_Property | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Text/clang-19/std_grapheme_IsControl | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/session_WidgetPartKey | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/38 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_style_TextStyleFor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 |
| Text/clang-19/surface_Unit | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/38 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphMetricsRevision | 0 | 0/38 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_layout_NextTextLineUncached | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 377/335/6/115/53/189/6/42/36/22/20/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 |
| Text/clang-19/text_layout_TextLineCacheMatches | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 |
| Text/clang-19/tree_TreeLabelSpanAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Text/clang-19/text_TextLinkRunAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 121/32/2/37/20/69/1/12/10/4/0/0/0/0/3 | 57/27/7/2/0/10/5/14/7/0/0/0/0/0/2 |
| Text/clang-19/text_TextLinkColor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 |
| Text/clang-19/paint_queue_EmitLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Text/clang-19/paint_queue_ClipLabel | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Text/clang-19/text_TextRangeValid | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 29/0/0/0/0/2/2/3/3/0/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintClippedEmphasis | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 230/8/2/43/109/200/4/3/1/2/27/0/0/0/2 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 |
| Text/clang-19/paint_queue_PaintRoundedFill | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Text/clang-19/shape_paint_PaintTriangleShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 200/4/1/50/123/176/2/1/0/4/33/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 |
| Text/clang-19/paint_queue_PendingTextValue | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Text/clang-19/drawing_props_StyleTokenColorOr | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/theme_CurrentAccent | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_typeface_TextTypefaceUse | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/raster_text_PaintTextRotated | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Text/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Text/clang-19/raster_text_PaintTextClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang-19/raster_text_PaintText | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_EmitPaintNow | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Text/clang-19/tree_TreeOverlayLayerAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Text/clang-19/tree_TreeSubmittedGeometryAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Text/clang-19/paint_queue_KeepPaintMesh | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Text/clang-19/paint_queue_ClipShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeOffset | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/6/5/12/0/0/0/0/11/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/shape_paint_SubmitShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 181/12/3/35/115/152/6/3/0/1/28/0/0/0/3 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 |
| Text/clang-19/text_layout_TextIsSpace | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/paint_queue_KeepPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Text/clang-19/paint_queue_KeepMeshPoints | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Text/clang-19/paint_queue_ShapeVisible | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Text/clang-19/paint_queue_ShapeWhole | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Text/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_FillShapeClipped | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Text/clang-19/shape_paint_EmitShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 106/16/4/17/12/33/8/18/14/7/16/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 |
| Text/clang-19/paint_queue_SubmitPaint | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 |
| Text/clang-19/theme_DarkPalette | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/theme_LightPalette | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/theme_DefaultMetrics | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 171/4/1/4/71/78/2/1/0/0/132/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Text/clang-19/theme_DefaultMotion | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Text/clang-19/theme_DefaultFonts | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 88/4/1/22/41/74/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_layout_TextNoBreakAfter | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 |
| Text/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Text/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Text/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Text/clang-19/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Text/clang-19/paint_queue_ShapeRadius | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Text/clang-19/paint_queue_FillClippedSpan | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Text/clang-19/shape_paint_EmitLineShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 |
| Text/clang-19/shape_paint_EmitRingShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 208/59/3/35/28/67/4/26/23/18/62/0/0/0/11 | 182/57/3/32/13/80/9/21/18/9/103/0/0/0/0 |
| Text/clang-19/shape_paint_EmitTriangleShape | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 |
| Text/clang-19/paint_queue___zi_spec_9308732530dd822f | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 449/418/5/27/278/335/8/21/16/7/4/4/0/1/25 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/widget_benchmark_host_RecordBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeAt | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/5/12/0/0/0/0/9/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/shape_paint_LineCornersFor | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 199/0/0/44/40/105/0/9/9/7/82/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchBegin | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeHidden | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 |
| Text/clang-19/shape_paint_PointsBounds | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 124/109/5/20/18/46/1/11/6/5/45/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/3 |
| Text/clang-19/shape_paint_ShapeBatchFlush | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeArcDirections | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/56/2/10/12/30/3/7/5/3/25/0/0/0/5 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeFadeBands | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 143/128/10/15/3/18/0/24/14/19/7/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 |
| Text/clang-19/shape_paint_ShapeBatchSector | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchBand | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 |
| Text/clang-19/paint_queue_ShapeSquareRoot | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchAdd | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/68/1/11/6/41/5/9/8/6/12/0/0/0/12 | 63/0/0/6/4/21/3/6/6/2/29/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeClipSide | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/73/4/44/43/104/12/14/10/6/28/0/0/0/8 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 |
| Text/clang-19/raster_triangles_FillTriangles | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeChannelChange | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/std_math_CeilInteger | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 |
| Text/clang-19/paint_queue_GradientChannel | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 65/0/0/0/0/0/0/6/6/5/19/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 |
| Text/clang-19/shape_paint_ShapeArcPoint | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 27/0/0/6/6/16/1/0/0/0/13/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 |
| Text/clang-19/std_math_NotANumber | 0 | 0/38 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 |
| Text/clang-19/std_math_Finite64 | 0 | 0/38 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/6/6/1/7/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/std_math_ScaleByPowerOfTwo | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 89/35/5/0/0/15/4/22/17/0/34/0/0/1/4 |
| Text/clang-19/shape_paint_ShapeTurn | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeInside | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeCrossing | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/13/37/0/10/10/2/41/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_RasterTriangles | 0 | 0/38 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 |
| Input/cc/widget_benchmarks_InputCase | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 514/12/3/96/182/321/10/38/35/26/61/0/0/0/3 | 164/152/15/12/15/57/0/17/2/11/8/0/0/0/42 |
| Input/cc/widget_benchmarks_BeginCase | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Input/cc/tree_draw_BeginFrame | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Input/cc/textfield_Draw | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 195/136/7/50/17/84/3/18/11/9/16/0/0/3/19 |
| Input/cc/tree_draw_EndFrame | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Input/cc/tree_input_TreePointerUpdate | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 424/0/0/46/23/228/46/35/35/29/29/46/0/0/0 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 |
| Input/cc/tree_TreeNodeAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/81 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/81 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_request_TextRequestSend | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Input/cc/text_request_TextRequestClear | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Input/cc/std_grapheme_Boundary | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Input/cc/session_SessionOpen | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Input/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/81 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointerSample | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Input/cc/tree_input_SecondaryPointerSample | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Input/cc/session_SessionFlagSet | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Input/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Input/cc/tree_input_TreeKeyboardRoute | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Input/cc/text_editing_TextEditingFrameBegin | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_PaintClear | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Input/cc/tree_TreeStart | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Input/cc/tree_TreeFrameStatus | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Input/cc/textfield_TextFieldKey | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 7/0/0/2/0/2/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuTakeCommand | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/110/7/23/10/47/0/31/24/14/0/0/0/0/18 | 66/46/6/2/0/14/2/13/7/3/0/0/0/0/6 |
| Input/cc/text_context_menu_TextMenuPasteText | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuOpenRequested | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 63/22/1/6/5/13/1/4/3/4/11/0/0/1/1 |
| Input/cc/system_clipboard_SystemClipboardSet | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_platform_TextInputNoteFocused | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 83/0/0/17/17/53/4/3/3/6/0/4/0/0/0 | 54/20/3/0/0/19/6/5/2/3/0/0/0/0/3 |
| Input/cc/text_context_menu_TextMenuIsOpenFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 18/4/1/0/0/4/0/4/3/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuShow | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 17/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/cc/session_SessionFlagGet | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/tree_draw_CommitFrame | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Input/cc/paint_queue_PaintSnapshotEmit | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Input/cc/tree_TreeIsBuilding | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/tree_input_TreeHitAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 |
| Input/cc/tree_input_TreeTextFocused | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 |
| Input/cc/tree_input_TreeTextKind | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeFocusAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Input/cc/tree_TreeRead | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Input/cc/tree_TreeCount | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/std_utf8_Valid | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Input/cc/tree_TreeTextRange | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Input/cc/std_grapheme_Next | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/geometry_RectangleIntersection | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Input/cc/tree_TreePointerActivatable | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Input/cc/tree_input_TreeActivateAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 |
| Input/cc/cursor_CursorHoverFrameStart | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Input/cc/tree_TreeBuildOldIndex | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 |
| Input/cc/text_input_TextSelectionRangeFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/1/1/0/0/0/0/0/0 |
| Input/cc/system_clipboard_SystemClipboardText | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 73/46/8/2/1/11/6/14/6/2/0/0/0/0/6 |
| Input/cc/text_context_menu_TextMenuOpenAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 61/12/2/3/0/16/2/6/4/2/2/0/0/0/2 |
| Input/cc/tree_TreePrevious | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 |
| Input/cc/tree_TreeCurrentParent | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Input/cc/tree_input_TreeTakeDragAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 173/148/11/13/4/58/10/25/14/6/22/0/0/8/20 |
| Input/cc/tree_TreePreviousWidgetState | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 48/0/0/0/0/17/3/4/4/1/2/1/0/1/0 |
| Input/cc/tree_input_TreeHasFocus | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 101/71/3/11/8/35/6/14/11/8/0/3/0/0/14 | 71/45/3/5/4/20/7/9/6/3/0/1/0/0/7 |
| Input/cc/tree_input_TreeBlurAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 37/7/1/0/0/9/4/5/4/1/0/1/0/1/2 |
| Input/cc/style_ResolveFont | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Input/cc/style_StyleFontValue | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Input/cc/text_input_TextInputScaledMetric | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/38/1/0/0/0/0/7/6/5/11/0/0/0/8 | 14/0/0/0/0/2/0/3/1/0/7/0/0/0/0 |
| Input/cc/textfield_TextFieldSecureDisplay | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 49/20/2/1/0/5/1/5/3/2/0/0/0/0/3 |
| Input/cc/font_metrics_GlyphWidth | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldCursorAtX | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 66/38/2/8/10/20/1/4/2/3/0/0/0/0/2 |
| Input/cc/text_input_TextPointerOffset | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 72/0/0/0/0/0/0/7/7/7/21/0/0/0/0 | 37/0/0/0/0/6/0/6/3/0/15/0/0/0/0 |
| Input/cc/textfield_TextFieldCodepoints | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 29/7/1/0/0/5/2/2/1/1/0/0/0/0/3 |
| Input/cc/text_TextSpan | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldByteAfter | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 39/21/2/0/0/2/1/5/3/1/0/0/0/0/2 |
| Input/cc/text_input_CompositionTransitionFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 159/126/11/16/29/59/8/28/17/0/23/0/0/0/13 |
| Input/cc/textfield_TextFieldNext | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextSelectionAfterMove | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 15/11/1/2/3/6/1/2/1/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldInputTextAllowed | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 21/8/1/0/0/6/3/5/4/0/0/0/0/0/3 |
| Input/cc/textfield_TextFieldReplacementAllowed | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 133/98/8/12/3/35/9/24/16/4/0/0/0/0/9 |
| Input/cc/text_input_TextEditCommandDecisionFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/88/8/4/3/13/4/18/10/0/0/0/0/5/8 |
| Input/cc/textfield_TextFieldEditFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 106/75/9/16/12/37/1/14/5/5/12/0/0/0/6 |
| Input/cc/text_request_TextRequestTake | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 198/0/0/47/28/118/10/16/16/11/0/9/0/0/0 | 79/0/0/4/3/24/4/8/8/4/0/1/0/0/0 |
| Input/cc/text_request_TextRequestChangeFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 280/263/12/53/38/116/5/36/24/13/0/0/0/0/24 | 160/128/12/20/5/57/8/24/12/6/0/0/0/0/9 |
| Input/cc/font_metrics_GlyphCaret | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 34/0/0/0/0/5/1/2/2/1/0/0/0/0/0 |
| Input/cc/text_input_TextFieldRevealScroll | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 25/0/0/0/0/2/2/1/1/0/0/0/0/0/0 |
| Input/cc/style_StyleOpacityValue | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Input/cc/tree_TreeSetInteractive | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Input/cc/tree_TreeSetCaptureTab | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 49/36/5/0/0/7/1/8/3/4/0/1/0/1/4 |
| Input/cc/tree_input_TreeFocusSubmittedAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 227/4/1/39/26/96/10/28/27/14/10/7/0/0/1 | 158/139/17/8/16/61/7/23/6/6/25/3/0/5/15 |
| Input/cc/tree_TreeSetWidgetState | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 155/104/7/15/5/48/10/26/19/3/2/4/0/3/14 |
| Input/cc/tree_TreeSetSemantic | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 |
| Input/cc/tree_TreeSetLabel | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Input/cc/tree_TreeSetStyleClass | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Input/cc/tree_TreeSetStyleId | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Input/cc/tree_TreeSetSelected | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Input/cc/tree_TreeSetTextValue | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 172/64/6/12/9/59/8/28/22/4/0/3/0/1/8 |
| Input/cc/paint_queue_PaintRoundedFill | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Input/cc/surface_Opacity | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Input/cc/font_metrics_GlyphLineHeight | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldPaintFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 179/0/0/22/20/54/1/13/13/12/47/0/1/0/0 | 120/0/0/2/0/13/2/13/5/0/41/0/0/4/0 |
| Input/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineY | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineEndX | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeSubmittedNodeAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/button_Draw | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/cc/system_clipboard_host_SystemClipboardWriteHost | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 18/6/1/0/0/6/2/3/2/0/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputFocusFrameBegin | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/18/17/57/6/3/3/5/0/6/0/0/0 | 53/17/2/0/0/14/5/4/2/3/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputFocused | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 38/25/4/0/0/8/3/5/1/3/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputOwnerMatches | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 13/6/1/0/0/1/0/3/1/0/0/0/0/0/0 |
| Input/cc/tree_TreeDefer | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 49/6/1/0/0/10/5/3/2/1/0/0/0/2/4 |
| Input/cc/text_context_menu_TextMenuDraw | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 152/138/6/8/31/79/3/13/7/4/21/0/0/2/33 |
| Input/cc/tree_draw_FrameInputCommit | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Input/cc/paint_queue_PaintOverflow | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/cc/tree_TreeCancel | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Input/cc/paint_queue_PaintOrder | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Input/cc/paint_queue_SnapshotTextRead | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_SnapshotTextJoin | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Input/cc/paint_queue_PaintPointCount | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Input/cc/tree_input_TreeTopModalLayer | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeModalLayerAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeButtonLike | 0 | 0/81 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointInCircleBounds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/cc/std_utf8_Decode | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Input/cc/tree_TreeParentGeneration | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Input/cc/tree_TreeIdentityHash | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/tree_TreeBuildNewIndex | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Input/cc/tree_TreeFindOld | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Input/cc/system_clipboard_host_SystemClipboardOpenHost | 0 | 0/81 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/system_clipboard_host_SystemClipboardByteHost | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 11/0/0/0/0/4/2/2/2/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 |
| Input/cc/style_sheet_StyleControlRoleFacts | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldDefaultFace | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 63/0/0/10/13/35/0/0/0/6/26/0/0/0/0 |
| Input/cc/style_sheet_ResolveInstalledStyleRules | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 |
| Input/cc/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 |
| Input/cc/text_input_TextInputContentWidth | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 18/0/0/0/0/2/0/3/1/0/4/0/0/0/0 |
| Input/cc/text_input_TextUtf8Continuation | 0 | 0/81 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_utf8_Next | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/text_input_TextCursorForLength | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_CompositionBoundaryFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/15/2/0/0/2/2/6/4/0/0/0/0/0/3 |
| Input/cc/std_grapheme_Previous | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Input/cc/tree_TreeEnsureInteraction | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 |
| Input/cc/tree_TreeEnsureSemantic | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Input/cc/tree_TreeStoreLabel | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Input/cc/paint_queue_ClipShape | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Input/cc/surface_Unit | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Input/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/81 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldCursorHeightFor | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 24/0/0/0/0/2/0/3/1/0/4/0/0/0/0 |
| Input/cc/paint_queue_EmitLabel | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Input/cc/paint_queue_ClipLabel | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Input/cc/button_SubmitButton | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 |
| Input/cc/system_clipboard_SystemClipboardHasText | 0 | 0/81 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/contextmenu_Draw | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1034/999/36/259/267/614/28/93/57/58/87/16/0/0/95 | 570/548/32/102/94/285/46/65/33/18/103/1/0/4/61 |
| Input/cc/cursor_CursorHoverFrameCommit | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Input/cc/paint_queue_PaintMeshPointCount | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Input/cc/paint_queue_EmitPaintDirect | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Input/cc/paint_queue_ResolvePaintText | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Input/cc/tree_TreeModalLayer | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/std_utf8_Continuation | 0 | 0/81 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_grapheme_IsControl | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/session_WidgetPartKey | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/widget_key_WidgetRepeatedPart | 0 | 0/81 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleControlFacts | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleBucket | 0 | 0/81 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleFirstBit | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 |
| Input/cc/style_sheet_StyleRuleMatchesState | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleSelectorMatches | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Input/cc/style_MergeValues | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 |
| Input/cc/tree_TreeLabelSpanAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Input/cc/paint_queue_ShapeWhole | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Input/cc/raster_shape_FillRoundedRectangle | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_FillShapeClipped | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Input/cc/paint_queue_EmitPaintNow | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Input/cc/tree_TreeCurrentPaintGeometry | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Input/cc/tree_TreeSubmittedGeometryAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 142/0/0/32/36/105/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Input/cc/paint_queue_KeepPaintMesh | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Input/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_typeface_TextTypefaceUse | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Input/cc/raster_text_PaintTextRotated | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Input/cc/raster_text_PaintTextItalicClipped | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Input/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Input/cc/raster_text_PaintTextClipped | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Input/cc/raster_text_PaintText | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/raster_StrokeLine | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/style_ResolveFlags | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Input/cc/style_ResolveState | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Input/cc/tree_input_TreePressedAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 |
| Input/cc/tree_input_TreeHoveredAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 |
| Input/cc/button_ButtonDetailSize | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 |
| Input/cc/tree_TreeRootBounds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Input/cc/button_behavior_MeasureBounds | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 |
| Input/cc/style_StyleMinHeightValue | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/button_ButtonMinimumHeight | 0 | 0/81 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/button_ButtonFitContentFace | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 |
| Input/cc/tree_TreeSetCircleHit | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 |
| Input/cc/tree_TreeSetSemanticDetail | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Input/cc/button_PaintButtonForNode | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 |
| Input/cc/tree_TreeNextKey | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 65/9/1/0/0/11/5/5/4/2/0/5/0/4/1 |
| Input/cc/contextmenu_ContextMenuOverlayKey | 0 | 0/81 0% |  | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/cc/contextmenu_ContextMenuPanelKey | 0 | 0/81 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePreviousCommitted | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 46/9/1/0/0/6/2/4/3/2/0/1/0/1/1 |
| Input/cc/contextmenu_ContextMenuRowKey | 0 | 0/81 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 8/0/0/0/0/4/3/0/0/0/0/0/0/1/0 |
| Input/cc/contextmenu_ContextMenuOpens | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 25/7/2/0/0/7/3/6/4/0/0/0/0/0/2 |
| Input/cc/contextmenu_ContextMenuFind | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/2/5/3/0/0/0/0/0/3 |
| Input/cc/contextmenu_ContextMenuBounds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuSubmenuBounds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 115/43/1/32/26/64/1/3/2/6/46/1/0/0/1 | 85/0/0/11/8/21/2/2/1/4/48/0/0/0/0 |
| Input/cc/overlay_Draw | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 257/214/9/46/27/82/5/22/13/19/36/0/0/6/31 |
| Input/cc/contextmenu_ContextMenuLevel | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 1465/1248/10/377/376/810/10/72/62/142/347/1/0/0/114 | 1082/888/26/288/120/488/12/74/36/102/301/0/0/2/121 |
| Input/cc/paint_queue_PendingTextValue | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Input/cc/style_sheet_StyleDefaultFacts | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleMatchesOptional | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Input/cc/paint_queue_ShapeRadius | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Input/cc/paint_queue_FillShapeRows | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 275/259/10/54/29/83/0/26/16/12/145/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Input/cc/paint_queue_FillClippedSpan | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Input/cc/paint_queue_KeepPaintBytes | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Input/cc/paint_queue_KeepMeshPoints | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Input/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Input/cc/text_emphasis_RunTextItalicRaster | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterText | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Input/cc/button_ButtonDefaultFace | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 |
| Input/cc/button_ButtonFaceFacts | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 |
| Input/cc/button_ButtonDetailFont | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Input/cc/style_StyleLineHeightValue | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/style_FitHeight | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 |
| Input/cc/button_behavior_MeasureContent | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 |
| Input/cc/button_behavior_ShapeWidth | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 |
| Input/cc/theme_ControlHeightFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 |
| Input/cc/button_behavior_ButtonCornerRadius | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Input/cc/focus_FocusDefaultOutlineBounds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/20/2/6/5/4/33/0/0/0/6 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 |
| Input/cc/image_lookup_IconLookupImage | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 |
| Input/cc/button_behavior_ButtonPictureSize | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 |
| Input/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 |
| Input/cc/button_behavior_ContentLayout | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 |
| Input/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 372/317/4/71/71/157/3/27/23/22/113/4/1/0/36 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 |
| Input/cc/image_lookup_ImageLookupTexture | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 |
| Input/cc/image_behavior_ImageDrawPlanFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 |
| Input/cc/paint_queue_PaintImage | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 68/0/0/20/23/54/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelBounds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 446/409/22/88/61/185/12/48/26/33/63/11/0/0/41 | 261/197/11/38/27/100/8/30/19/19/54/0/0/2/32 |
| Input/cc/dpi_ScalePixels | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 42/0/0/0/0/7/0/7/4/0/19/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuRowTop | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 113/93/7/21/13/42/4/18/11/9/31/4/0/0/12 | 93/61/7/3/3/21/5/14/7/4/26/0/0/1/11 |
| Input/cc/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/14/1/0/0/1/1/2/1/0/0/0/0/2/4 |
| Input/cc/tree_input_TreeTakeDismissalAt | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/17/1/1/0/4/2/5/4/4/0/0/0/0/1 |
| Input/cc/tree_TreeSetPopup | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 38/0/0/0/0/8/1/5/5/2/0/1/0/1/0 |
| Input/cc/drawing_props_StyleTokenColorOr | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePushScope | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 81/64/5/0/0/13/9/9/4/8/0/8/0/0/12 |
| Input/cc/contextmenu_ContextMenuTextLeft | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/1/1/2/0/1/1/2/2/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelChecks | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 26/13/4/0/0/8/2/7/3/0/0/0/0/0/4 |
| Input/cc/contextmenu_ContextMenuInlineWidth | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/2/0/4/0/2/2/4/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuInlineEnds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 24/6/1/0/0/9/5/3/2/0/0/0/0/0/0 |
| Input/cc/svg_icon_behavior_PaintStroke | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 644/596/8/74/331/442/14/31/23/21/80/0/0/0/35 | 182/92/5/18/42/76/9/11/6/4/78/0/0/1/19 |
| Input/cc/tree_End | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RecordBounds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Input/cc/paint_queue_ShapeCornerInset | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 63/0/0/1/1/2/0/6/6/4/20/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Input/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Input/cc/paint_queue_KeepLongPaintBytes | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Input/cc/theme_DarkPalette | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/cc/theme_DefaultMotion | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 69/0/0/6/22/33/0/1/1/0/35/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Input/cc/theme_StateLayer | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 |
| Input/cc/card_behavior_CardDefaultFace | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 |
| Input/cc/theme_FontForRole | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Input/cc/theme_MotionDuration | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 |
| Input/cc/button_behavior_ButtonRoleFactsFor | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 |
| Input/cc/drawing_props_PaintPixelScale | 0 | 0/81 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 |
| Input/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 |
| Input/cc/image_behavior_ImageFitRect | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Input/cc/paint_queue_PaintImageWithPolygon | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 |
| Input/cc/svg_icon_behavior_StrokeAppendByte | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/cc/svg_icon_behavior_StrokeAppendNumber | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 230/194/7/10/16/40/7/36/29/13/11/0/6/0/35 | 122/102/6/0/6/38/14/13/5/0/6/4/0/7/26 |
| Input/cc/tree_TreePopScope | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 54/27/1/2/1/6/3/4/3/4/0/4/0/0/5 |
| Input/cc/paint_queue_ShapeSquareRoot | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Input/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Input/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Input/cc/svg_icon_behavior_SvgIconHash | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 |
| Input/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 |
| Input/cc/svg_shape_ShapeRaster | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 |
| Input/cc/svg_icon_behavior_SvgIconCells | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 345/311/29/30/34/97/9/61/32/9/0/8/1/2/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 |
| Input/cc/image_behavior_ImageFitBounds | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 |
| Input/cc/image_behavior_ImagePolygonValid | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 |
| Input/cc/paint_queue_EmitImage | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Input/cc/paint_queue_ClipImage | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeNumber | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 |
| Input/cc/svg_shape_ShapeBuildPath | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 568/544/41/129/90/238/7/87/46/35/160/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 |
| Input/cc/svg_shape_ShapeCoverEdges | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 |
| Input/cc/paint_queue_EmitImageBegin | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterImage | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 |
| Input/cc/paint_queue_EmitImageEnd | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsDigit | 0 | 0/81 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsSpace | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddPoint | 0 | 0/81 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddSpan | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 |
| Input/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 |
| Input/cc/svg_shape_ShapeFillSubpath | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeCollapse | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 |
| Input/cc/svg_shape_ShapeAddCircle | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 |
| Input/cc/svg_shape_ShapeSqrt | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 |
| Input/cc/svg_shape_ShapeQuad | 0 | 0/81 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeAbs | 0 | 0/81 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeTriangle | 0 | 0/81 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddEdge | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Input/cc/svg_shape_ShapeAddPolygon | 0 | 0/81 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 |
| Input/clang-19/widget_benchmarks_BeginCase | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/tree_draw_BeginFrame | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 283/0/0/45/28/117/12/40/40/27/10/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Input/clang-19/textfield_Draw | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 117/0/0/14/4/41/2/10/10/9/2/0/0/0/0 |
| Input/clang-19/tree_draw_EndFrame | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Input/clang-19/tree_TreeNodeAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/80 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/80 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_request_TextRequestSend | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Input/clang-19/text_request_TextRequestClear | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 79/15/4/10/16/42/8/8/4/4/8/1/0/0/4 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/80 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_PointerSample | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Input/clang-19/tree_input_SecondaryPointerSample | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Input/clang-19/session_SessionFlagSet | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 653/589/20/93/86/243/24/106/86/59/16/7/0/0/98 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 |
| Input/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintClear | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Input/clang-19/tree_TreeStart | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Input/clang-19/tree_TreeFrameStatus | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Input/clang-19/text_input_TextSelectionRangeForLength | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/text_context_menu_TextMenuTakeCommand | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/110/7/23/10/47/0/31/24/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 |
| Input/clang-19/text_context_menu_TextMenuPasteText | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/system_clipboard_SystemClipboardSet | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputNoteFocused | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 83/0/0/17/17/53/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuIsOpenFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuShow | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/session_SessionFlagGet | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/tree_draw_CommitFrame | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Input/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 633/557/10/126/167/421/21/27/17/29/36/5/0/0/53 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Input/clang-19/tree_TreeIsBuilding | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_input_TreeTextFocused | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Input/clang-19/tree_input_TreeTextKind | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeFocusAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Input/clang-19/tree_TreeRead | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 591/28/7/109/170/408/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Input/clang-19/tree_TreeCount | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/tree_TreeTextRange | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Input/clang-19/tree_input_TreeFocusedAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Input/clang-19/geometry_RectangleIntersection | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/72/4/18/13/33/0/12/8/4/52/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Input/clang-19/tree_TreePointerActivatable | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Input/clang-19/tree_input_TreeActivateAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Input/clang-19/cursor_CursorHoverFrameStart | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Input/clang-19/tree_TreeBuildOldIndex | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Input/clang-19/text_input_TextSelectionRangeFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/system_clipboard_SystemClipboardText | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 65/17/2/0/0/5/1/9/7/2/0/0/0/0/1 |
| Input/clang-19/input_protocol_PointInBounds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 77/0/0/8/0/8/0/15/15/4/20/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuOpenAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 55/14/1/3/0/16/1/5/4/3/2/0/0/0/2 |
| Input/clang-19/tree_TreePrevious | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeCurrentParent | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Input/clang-19/tree_input_TreeTakeActivationAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeTakeDragAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 155/35/3/8/1/45/10/27/24/6/8/6/0/0/6 |
| Input/clang-19/tree_TreePreviousWidgetState | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 |
| Input/clang-19/tree_input_TreeHasFocus | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 101/71/3/11/8/35/6/14/11/8/0/3/0/0/14 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 |
| Input/clang-19/tree_input_TreeBlurAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 |
| Input/clang-19/style_ResolveFont | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Input/clang-19/style_StyleFontValue | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 56/0/0/0/0/0/0/6/6/5/11/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Input/clang-19/textfield_TextFieldSecureDisplay | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 57/8/1/0/0/2/2/5/4/2/0/0/0/0/1 |
| Input/clang-19/font_metrics_GlyphWidth | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextFieldScrollFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 |
| Input/clang-19/textfield_TextFieldCursorAtX | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 |
| Input/clang-19/textfield_TextFieldCodepoints | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 |
| Input/clang-19/textfield_TextFieldByteAfter | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 |
| Input/clang-19/text_input_CompositionTransitionFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 313/113/2/106/64/215/0/18/16/11/0/0/0/0/3 | 110/98/3/0/0/37/4/14/11/0/19/0/0/0/11 |
| Input/clang-19/textfield_TextFieldNext | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextSelectionAfterMove | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 |
| Input/clang-19/textfield_TextFieldInputTextAllowed | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 25/12/1/0/0/3/2/4/3/0/0/0/0/0/3 |
| Input/clang-19/textfield_TextFieldReplacementAllowed | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 |
| Input/clang-19/text_input_TextEditCommandDecisionFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/0/0/2/0/2/0/12/12/0/0/0/0/6/0 |
| Input/clang-19/textfield_TextFieldEditFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/4/1/52/43/121/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 |
| Input/clang-19/text_request_TextRequestTake | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 198/0/0/47/28/118/10/16/16/11/0/9/0/0/0 | 76/0/0/1/0/18/2/9/9/4/3/1/0/0/0 |
| Input/clang-19/text_request_TextRequestChangeFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 280/263/12/53/38/116/5/36/24/13/0/0/0/0/24 | 145/55/6/6/10/39/8/22/16/5/0/0/0/2/0 |
| Input/clang-19/font_metrics_GlyphCaret | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 32/0/0/1/3/8/0/2/2/1/2/0/0/0/0 |
| Input/clang-19/text_input_TextFieldRevealScroll | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_StyleOpacityValue | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/4/4/3/14/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Input/clang-19/tree_TreeSetInteractive | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetCaptureTab | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 |
| Input/clang-19/tree_input_TreeFocusSubmittedAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 227/4/1/39/26/96/10/28/27/14/10/7/0/0/1 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 |
| Input/clang-19/tree_TreeSetWidgetState | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 |
| Input/clang-19/tree_TreeSetSemantic | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetLabel | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetStyleClass | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetStyleId | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetSelected | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetTextValue | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 138/86/3/8/4/47/12/22/19/5/0/1/0/2/6 |
| Input/clang-19/paint_queue_PaintRoundedFill | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 223/8/2/41/112/193/4/3/1/2/27/0/0/0/2 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Input/clang-19/drawing_props_ColorFromPacked | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/0/0/2/9/12/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/surface_Opacity | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Input/clang-19/paint_queue_PaintRoundedOutline | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 225/8/2/41/112/195/4/3/1/2/28/0/0/0/2 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Input/clang-19/theme_CurrentMuted | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentAccent | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintClippedLabel | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 228/8/2/43/109/198/4/3/1/2/27/0/0/0/2 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Input/clang-19/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/clang-19/text_input_TextCompositionUnderlineY | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextCompositionUnderlineEndX | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintLine | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 217/8/2/40/111/187/4/3/1/2/26/0/0/0/2 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Input/clang-19/tree_TreeSubmittedNodeAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/button_Draw | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputFocusFrameBegin | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/18/17/57/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputFocused | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputOwnerMatches | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreeDefer | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 |
| Input/clang-19/text_context_menu_TextMenuDraw | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 510/460/2/96/153/380/21/16/14/13/26/16/0/0/82 | 140/0/0/6/33/75/0/12/12/5/18/0/0/2/0 |
| Input/clang-19/tree_draw_FrameInputCommit | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Input/clang-19/paint_queue_PaintOverflow | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/clang-19/tree_TreeCancel | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_TreeFinish | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Input/clang-19/paint_queue_PaintOrder | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Input/clang-19/paint_queue_SnapshotTextRead | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Input/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Input/clang-19/paint_queue_PaintPointCount | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Input/clang-19/paint_queue_EmitPaint | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Input/clang-19/tree_input_TreeTopModalLayer | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreeButtonLike | 0 | 0/80 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_PointInCircleBounds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 53/0/0/8/4/12/0/1/1/2/29/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/clang-19/tree_TreeOverlayLayerAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/std_grapheme_Push | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 404/303/3/30/43/77/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Input/clang-19/tree_TreeParentGeneration | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Input/clang-19/tree_TreeIdentityHash | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang-19/tree_TreeBuildNewIndex | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Input/clang-19/tree_TreeExplicitExists | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Input/clang-19/tree_TreeFindOld | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Input/clang-19/system_clipboard_host_SystemClipboardOpenHost | 0 | 0/80 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/system_clipboard_host_SystemClipboardByteHost | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeTakePointerActivationAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 |
| Input/clang-19/style_sheet_StyleControlRoleFacts | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/textfield_TextFieldDefaultFace | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 78/4/1/6/41/50/2/1/0/6/28/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 |
| Input/clang-19/style_sheet_ResolveInstalledStyleRules | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 |
| Input/clang-19/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Input/clang-19/text_input_TextUtf8Continuation | 0 | 0/80 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextCursorForLength | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_CompositionBoundaryFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/9/1/0/0/3/2/3/2/0/0/0/0/0/2 |
| Input/clang-19/tree_TreeEnsureInteraction | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Input/clang-19/tree_TreeLayerAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Input/clang-19/tree_TreeEnsureSemantic | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Input/clang-19/tree_TreeStoreLabel | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 471/445/23/81/35/184/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Input/clang-19/paint_queue_ClipShape | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Input/clang-19/surface_Unit | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/3/3/2/12/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Input/clang-19/paint_queue_EmitRoundedOutline | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 107/8/2/29/22/52/4/8/6/7/16/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/80 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextFieldCursorHeightFor | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Input/clang-19/paint_queue_EmitLabel | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 175/0/0/82/58/142/0/8/8/7/5/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Input/clang-19/paint_queue_ClipLabel | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/8/2/28/21/56/4/6/4/2/8/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Input/clang-19/paint_queue_EmitLine | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 309/160/4/56/18/74/0/39/35/18/86/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Input/clang-19/button_SubmitButton | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 620/599/18/167/121/310/20/59/41/56/52/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 |
| Input/clang-19/system_clipboard_SystemClipboardHasText | 0 | 0/80 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/contextmenu_Draw | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1034/999/36/259/267/614/28/93/57/58/87/16/0/0/95 | 588/511/31/82/99/249/33/78/47/19/99/14/0/3/58 |
| Input/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Input/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang-19/paint_queue_ResolvePaintText | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/tree_TreeModalLayer | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/std_utf8_Continuation | 0 | 0/80 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/std_grapheme_data_Property | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Input/clang-19/std_grapheme_IsControl | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/session_WidgetPartKey | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/80 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleControlFacts | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Input/clang-19/theme_CurrentSunken | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentBorderStrong | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_ShapeSmall | 0 | 0/80 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/style_sheet_StyleBucket | 0 | 0/80 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleFirstBit | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Input/clang-19/style_sheet_StyleRuleMatchesState | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleSelectorMatches | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Input/clang-19/style_MergeValues | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Input/clang-19/tree_TreeLabelSpanAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Input/clang-19/paint_queue_ShapeVisible | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/22/19/48/0/6/6/4/22/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Input/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_FillShapeClipped | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 182/165/5/50/39/89/0/12/7/11/87/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Input/clang-19/paint_queue_EmitPaintNow | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Input/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/4/1/48/48/120/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Input/clang-19/paint_queue_KeepPaintMesh | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/64/3/16/14/51/6/7/4/5/14/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Input/clang-19/raster_shape_StrokeRoundedRectangle | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_typeface_TextTypefaceUse | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/raster_text_PaintTextRotated | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/10/8/18/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 121/0/0/33/19/52/0/7/7/7/8/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Input/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 114/22/1/21/13/34/0/11/10/7/8/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Input/clang-19/raster_text_PaintTextClipped | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/7/5/12/0/5/5/3/8/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang-19/raster_text_PaintText | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/raster_StrokeLine | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/style_ResolveFlags | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 42/0/0/3/7/12/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Input/clang-19/style_ResolveState | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeHoveredAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Input/clang-19/button_ButtonDetailSize | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 110/0/0/17/20/49/0/12/12/6/15/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 |
| Input/clang-19/tree_TreeRootBounds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Input/clang-19/button_behavior_MeasureBounds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 343/319/6/50/26/102/0/56/50/19/98/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 |
| Input/clang-19/style_StyleMinHeightValue | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang-19/button_ButtonMinimumHeight | 0 | 0/80 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/button_ButtonFitContentFace | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 379/296/13/41/19/86/4/61/48/25/91/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 |
| Input/clang-19/tree_TreeSetCircleHit | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Input/clang-19/tree_TreeSetSemanticDetail | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang-19/button_PaintButtonForNode | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1398/1360/16/266/215/548/12/135/119/93/375/0/1/0/134 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 |
| Input/clang-19/tree_TreeNextKey | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 |
| Input/clang-19/contextmenu_ContextMenuOverlayKey | 0 | 0/80 0% |  | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuPanelKey | 0 | 0/80 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreePreviousCommitted | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuRowKey | 0 | 0/80 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuOpens | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 |
| Input/clang-19/contextmenu_ContextMenuFind | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 |
| Input/clang-19/contextmenu_ContextMenuBounds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/overlay_Draw | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 392/311/1/115/78/209/0/28/27/28/23/0/0/0/28 | 200/82/1/15/5/38/0/16/15/19/34/0/0/1/9 |
| Input/clang-19/contextmenu_ContextMenuLevel | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1465/1248/10/377/376/810/10/72/62/142/347/1/0/0/114 | 940/723/13/211/90/371/26/63/50/101/289/2/0/1/99 |
| Input/clang-19/paint_queue_PendingTextValue | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Input/clang-19/style_sheet_StyleDefaultFacts | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Input/clang-19/style_sheet_StyleMatchesOptional | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_ShapeRadius | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 62/46/3/4/0/4/0/7/4/3/24/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Input/clang-19/paint_queue_FillClippedSpan | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 114/99/4/15/11/26/0/13/9/7/37/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Input/clang-19/paint_queue_KeepPaintBytes | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Input/clang-19/paint_queue_KeepMeshPoints | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 264/146/9/39/27/124/16/29/20/10/18/3/0/1/24 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Input/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/button_ButtonFaceFinish | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 42/33/2/5/3/11/2/9/7/2/7/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Input/clang-19/button_ButtonDefaultFace | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 296/201/4/55/79/150/6/36/32/19/52/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Input/clang-19/button_ButtonFaceFacts | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Input/clang-19/button_ButtonDetailFont | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Input/clang-19/style_StyleLineHeightValue | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 33/0/0/0/0/0/0/5/5/2/6/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang-19/style_FitHeight | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 77/53/4/0/0/0/0/9/5/5/20/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Input/clang-19/button_behavior_MeasureContent | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/151/5/4/13/19/0/28/23/9/57/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 |
| Input/clang-19/button_behavior_ShapeWidth | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 78/0/0/0/0/0/0/12/12/6/13/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Input/clang-19/theme_ControlHeightFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Input/clang-19/button_behavior_ButtonCornerRadius | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 35/0/0/0/0/0/0/4/4/2/10/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Input/clang-19/focus_FocusDefaultOutlineBounds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/1/7/9/20/2/6/5/4/33/0/0/0/6 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Input/clang-19/image_lookup_IconLookupImage | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 137/8/2/32/64/105/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Input/clang-19/button_behavior_ButtonPictureSize | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 59/43/3/0/0/0/0/10/7/5/11/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Input/clang-19/button_behavior_ButtonLabeledPictureSize | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 98/52/4/0/0/0/0/10/6/7/34/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Input/clang-19/button_behavior_ContentLayout | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 241/177/5/37/33/79/0/26/21/9/109/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_PaintSvgIcon | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 372/317/4/71/71/157/3/27/23/22/113/4/1/0/36 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 |
| Input/clang-19/image_lookup_ImageLookupTexture | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Input/clang-19/image_behavior_ImageDrawPlanFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 224/4/1/39/51/116/2/21/20/7/41/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuLevelBounds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 446/409/22/88/61/185/12/48/26/33/63/11/0/0/41 | 267/183/14/44/15/86/6/33/19/22/50/1/0/0/34 |
| Input/clang-19/dpi_ScalePixels | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 88/75/1/0/0/0/0/10/9/8/26/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuRowTop | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 113/93/7/21/13/42/4/18/11/9/31/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 |
| Input/clang-19/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/0/0/0/3/0 |
| Input/clang-19/tree_input_TreeTakeDismissalAt | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 |
| Input/clang-19/tree_TreeSetPopup | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 |
| Input/clang-19/drawing_props_StyleTokenColorOr | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentSurface | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentBorder | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentDisabled | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentSelection | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/tree_TreePushScope | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 |
| Input/clang-19/contextmenu_ContextMenuTextLeft | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuLevelChecks | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 30/16/4/0/0/6/1/7/3/0/0/0/0/0/5 |
| Input/clang-19/contextmenu_ContextMenuInlineWidth | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuInlineEnds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 |
| Input/clang-19/svg_icon_behavior_PaintStroke | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 644/596/8/74/331/442/14/31/23/21/80/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 |
| Input/clang-19/tree_End | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Input/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Input/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Input/clang-19/theme_DarkPalette | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang-19/theme_StateLayer | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Input/clang-19/card_behavior_CardDefaultFace | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 122/4/1/19/49/72/2/10/9/4/28/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 |
| Input/clang-19/theme_ShapeMedium | 0 | 0/80 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/theme_FontForRole | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Input/clang-19/theme_MotionDuration | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 95/44/2/17/8/26/0/16/14/4/21/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Input/clang-19/drawing_props_PaintPixelScale | 0 | 0/80 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconSlot | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 |
| Input/clang-19/svg_icon_behavior_SvgIconSnap | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 80/31/2/0/0/0/0/11/9/6/25/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconTexture | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 |
| Input/clang-19/paint_queue_PaintImageWithPolygon | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 290/8/2/63/116/242/4/7/5/5/32/0/0/0/2 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_StrokeAppendByte | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_StrokeAppendNumber | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 230/194/7/10/16/40/7/36/29/13/11/0/6/0/35 | 98/25/2/1/4/25/10/9/7/1/6/4/0/5/4 |
| Input/clang-19/tree_TreePopScope | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 52/10/1/0/0/6/4/6/5/4/0/4/0/0/2 |
| Input/clang-19/paint_queue_ShapeSquareRoot | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 59/32/2/0/0/0/0/5/3/2/20/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 |
| Input/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 247/197/9/31/26/110/15/28/19/10/12/4/0/1/36 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang-19/theme_ShapeSurface | 0 | 0/80 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconRetire | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeRaster | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 263/220/10/32/17/69/6/50/40/14/17/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 |
| Input/clang-19/image_behavior_ImageFitBounds | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 206/186/8/18/9/31/0/26/18/13/55/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 |
| Input/clang-19/image_behavior_ImagePolygonValid | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 107/91/3/9/1/12/1/19/16/7/16/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Input/clang-19/paint_queue_EmitImage | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Input/clang-19/paint_queue_ClipImage | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeNumber | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 355/314/20/55/19/93/15/60/40/19/60/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 |
| Input/clang-19/svg_shape_ShapeCoverEdges | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 354/333/22/25/21/75/15/63/41/14/65/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 |
| Input/clang-19/paint_queue_EmitImageBegin | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 75/0/0/12/5/30/0/6/6/3/8/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_RasterImage | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Input/clang-19/paint_queue_EmitImageEnd | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeIsDigit | 0 | 0/80 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeIsSpace | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/svg_shape_ShapeAddPoint | 0 | 0/80 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAddSpan | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 142/126/5/0/0/16/4/18/13/11/40/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 |
| Input/clang-19/svg_shape_ShapeStrokeSubpath | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 624/541/15/90/49/179/20/64/49/38/279/0/3/0/62 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 |
| Input/clang-19/svg_shape_ShapeFillSubpath | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 |
| Input/clang-19/svg_shape_ShapeCollapse | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 69/65/3/0/0/16/8/8/5/2/18/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Input/clang-19/svg_shape_ShapeAddCircle | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Input/clang-19/svg_shape_ShapeQuad | 0 | 0/80 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAbs | 0 | 0/80 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/1/1/1/8/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeTriangle | 0 | 0/80 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAddEdge | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 53/0/0/2/8/22/1/3/3/2/10/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Input/clang-19/svg_shape_ShapeAddPolygon | 0 | 0/80 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 112/98/8/4/5/27/12/22/14/4/26/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 |
| Hello/cc/hello_main | 0 | 0/0 unknown |  | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Hello/clang-19/hello_main | 0 | 0/0 unknown |  | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
