# C teacher: native assembly comparison

Base revision: 42dc1b0f41489e8affae19815a52aa3a875ff0d9
Host: omega
build/bin/zi2c SHA-256: 549a9b9f3600255c4b8af83947661f92ec8db8a97d80d26392b1674a8bc59f27
build/bin/zi2zx SHA-256: 65c25915eeef2444eabaf560bcb01493c89682afbcfcd7593c6952d2d3777527
build/bin/zx2native SHA-256: 126825f64142f927acb563489fe83977d682945a9dfc29ee9d5158c095f846c0
Learning off. Generated C compiled with -O2; native compiled from verified ZX. Function extents come from the native map; C symbols match module_name or exported name exactly. Absent symbols are not evidence of inlining. Metrics: instructions/loop-union/back-edges/stack-reads/stack-writes/memory/indexed/branches/forward-branches/calls/vector-register-uses/mul/div/shifts/loop-immediate-or-PC-relative candidates. Stack traffic includes locals and saves. Vector-register uses include scalar floating point. Loop constants are LICM candidates, not proven invariants. Back edges describe loop bodies, not execution frequency. SIGPROF uses a 1ms CPU timer, at most 30 runs; zero samples means unknown time share. Sampling includes process startup and runtime. Ranking weights relative instruction excess by observed function samples; it predicts neither cycles nor speedup.

## Sieve / cc
Native SHA-256: 81468f46fa5939aeec5abd2c18b72fb884295ee84ea0190e788663fed0d10ae9
C SHA-256: a90bdb344798838279f6a10871246589e12193ef0ab912e32c4b0994e021f267
ZX SHA-256: f1fccf1c68dab477a2d0e35a94d9fda6ed8c9873d34deb24c4de73e88f22d92e
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 120/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Sieve | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 34/19/3/0/0/7/6/6/3/1/0/1/0/0/9 | 120 | 421..594: 40 instructions, 113 samples |

## Sieve / clang-19
Native SHA-256: 81468f46fa5939aeec5abd2c18b72fb884295ee84ea0190e788663fed0d10ae9
C SHA-256: ffe0a6f6a17493a2edf3e8be1843da7ff239ae67174011b627c4b07e379a5f8a
ZX SHA-256: f1fccf1c68dab477a2d0e35a94d9fda6ed8c9873d34deb24c4de73e88f22d92e
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 119/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Sieve_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 66..167: 26 instructions, 0 samples |
| wasm_tier_Sieve | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 | 119 | 421..594: 40 instructions, 118 samples |

## Records / cc
Native SHA-256: 6653d136d7d21aaafb92a9255befdf5d125e49adebec03b7965a551c76753db8
C SHA-256: c1e5ee0de67ea0bb3254c26d8cba9d59504c91d226b277a243a97265bf234f30
ZX SHA-256: 4dc39f678013ae78a3783b0dc2cf95aa0cd66f3c79ed5b88250f42f8d9d3ea45
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 73/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..151: 26 instructions, 0 samples |
| wasm_tier_Records | 407/144/21/4/260/290/5/45/24/4/0/4/2/0/29 | 70/57/4/0/0/20/2/6/2/0/0/2/0/2/11 | 73 | 1936..2472: 127 instructions, 73 samples |

## Records / clang-19
Native SHA-256: 6653d136d7d21aaafb92a9255befdf5d125e49adebec03b7965a551c76753db8
C SHA-256: e0fb2ce977684ea9df7560d86ee8b7b931dc72fa33e8fe30b3f4e16c7a7a2782
ZX SHA-256: 4dc39f678013ae78a3783b0dc2cf95aa0cd66f3c79ed5b88250f42f8d9d3ea45
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 78/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Records_main | 37/28/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..151: 26 instructions, 0 samples |
| wasm_tier_Records | 407/144/21/4/260/290/5/45/24/4/0/4/2/0/29 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 | 78 | 1936..2472: 127 instructions, 78 samples |

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
SIGPROF samples/runs: 154/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Hash_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Hash | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 | 154 | 199..408: 52 instructions, 154 samples |

## Vectors / cc
Native SHA-256: 533cfc39d0908267889f1c8501bfe4cb4d3f850aa08cbaaf69c17104b6b3a1a6
C SHA-256: 80ce3f982e01a9e5571fe04dda3f94f871709933e51d40514bd28f4127f251f1
ZX SHA-256: 541c0b8413f48e82b2b148136f15a8f226ee7f9e6b4a82413f0df09fc64630d2
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 153/21

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Vectors | 149/137/15/22/23/47/2/31/16/3/0/1/0/1/15 | 101/90/7/7/8/24/5/16/9/2/7/0/0/0/9 | 133 | 218..755: 136 instructions, 133 samples |

## Vectors / clang-19
Native SHA-256: 533cfc39d0908267889f1c8501bfe4cb4d3f850aa08cbaaf69c17104b6b3a1a6
C SHA-256: 69731a1b48535b712c8be6562303fd06cdce49a7447024a162af4e25f7379c31
ZX SHA-256: 541c0b8413f48e82b2b148136f15a8f226ee7f9e6b4a82413f0df09fc64630d2
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 153/22

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Vectors_main | 38/29/5/4/2/6/0/9/4/2/0/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 50..157: 27 instructions, 0 samples |
| wasm_tier_Vectors | 149/137/15/22/23/47/2/31/16/3/0/1/0/1/15 | 123/101/10/3/3/22/15/17/7/2/26/1/0/1/13 | 136 | 218..755: 136 instructions, 136 samples |

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
Native SHA-256: a96f7661945af874c7f644ba8cb0058d1b634f95e0fe5f175f74d7b953320193
C SHA-256: cad0c34b670e2363e8361bcac8a7f903af61489b8faaa9fd0b3f8ea74bf59a1b
ZX SHA-256: 7a061d9bab1d21d133299121a434247fa17ae4848273f4f00de0813d3bebd853
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 31/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 87..158: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 359/8/2/48/142/207/5/26/24/25/48/0/0/0/2 | 158/147/9/4/10/45/4/19/10/12/8/0/0/0/36 | 1 | 1040..1059: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 0 | none |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 3526..3548: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3985..4004: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 | 0 | none |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 1 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 6639..8198: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 8401..8420: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 8599..8751: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 8831..8941: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 6 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 1 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 9841..9884: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 10866..13267: 519 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 0 | 14992..15251: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| button_SubmitButton | 619/598/18/167/121/310/20/59/41/56/51/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 | 1 | 15430..18248: 583 instructions, 1 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 19532..21912: 471 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 0 | 22018..22358: 67 instructions, 0 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 0 | 23003..23025: 4 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 | 0 | 25669..26749: 232 instructions, 0 samples |
| tree_input_TreeTextFocused | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 | 0 | 27380..27604: 57 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 1 | 28543..28559: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 28783..29096: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 29490..29582: 25 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 29953..30217: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 0 | none |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 30754..31004: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 | 0 | 32216..33044: 195 instructions, 0 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 0 | 34138..37012: 616 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 | 0 | 37362..37417: 17 instructions, 0 samples |
| style_ResolveFlags | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 266/172/4/89/71/181/2/9/5/15/144/0/0/0/7 | 0 | 38373..38395: 4 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 1 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| button_behavior_MeasureBounds | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 | 0 | 39983..41257: 300 instructions, 0 samples |
| style_StyleMinHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 1 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 371/290/13/41/19/86/4/61/48/25/83/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 | 0 | 41759..43033: 278 instructions, 0 samples |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| button_PaintButtonForNode | 1316/1279/16/257/184/508/12/135/119/93/320/0/1/0/133 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 | 2 | 45845..51983: 1260 instructions, 2 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 26/12/1/0/0/2/1/2/1/2/0/0/0/0/1 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 1 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 0 | 53180..53458: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 55699..56747: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 57603..59408: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 0 | 59575..59597: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 1 | 61024..61046: 4 instructions, 1 samples |
| input_protocol_PointInBounds | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 0 | 62165..62951: 209 instructions, 0 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 0 | 63374..64605: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 0 | 65369..66049: 170 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 78/21/2/2/2/18/7/5/3/2/0/3/0/3/2 | 1 | 66180..66363: 42 instructions, 1 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 0 | 66629..66958: 77 instructions, 0 samples |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 | 0 | none |
| button_ButtonFaceFinish | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 45/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 1 | 67793..67951: 31 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 | 0 | 68102..69368: 251 instructions, 0 samples |
| button_ButtonDefaultFace | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 | 0 | 69845..70832: 191 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 | 0 | 70877..71034: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 33/16/2/0/0/4/2/5/3/0/0/0/0/2/5 | 0 | 71157..71319: 47 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 | 0 | 71626..71788: 45 instructions, 0 samples |
| button_behavior_MeasureContent | 157/141/5/4/9/15/0/28/23/9/40/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 | 0 | 71873..72431: 136 instructions, 0 samples |
| button_behavior_ShapeWidth | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 | 0 | 72767..72789: 4 instructions, 0 samples |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 | 0 | 73239..73515: 69 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 0 | 73912..74219: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 0 | 74339..76025: 408 instructions, 0 samples |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 0 | 77360..77379: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 | 0 | 78233..78252: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 0 | 78600..78622: 4 instructions, 0 samples |
| focus_FocusDefaultOutlineBounds | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 | 0 | 78749..78961: 53 instructions, 0 samples |
| image_lookup_IconLookupImage | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 | 0 | 79247..79266: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 | 0 | 79475..79617: 39 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 91/52/4/0/0/0/0/10/6/7/27/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 | 0 | 79793..79964: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 224/168/5/37/26/72/0/26/21/9/92/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 | 0 | 80228..80920: 163 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 346/293/4/71/57/143/3/27/23/22/88/4/1/0/36 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 | 0 | 81165..82447: 289 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 | 0 | 82716..82738: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 | 0 | 83253..83275: 4 instructions, 0 samples |
| paint_queue_PaintImage | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 | 0 | 84416..84435: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 1 | 85280..85299: 4 instructions, 1 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 85910..85920: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 1 | 86104..86123: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 1 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 1 | 86701..87029: 82 instructions, 1 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 278/259/112/0/0/136/10/118/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 | 0 | 87925..88273: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 | 0 | 89964..89986: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | 90095..90114: 4 instructions, 0 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 | 0 | 90388..91049: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 | 0 | 91579..91598: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 1 | none |
| theme_MotionDuration | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 | 0 | 92348..92495: 40 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 | 1 | 92563..92642: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | none |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 0 | 93130..93152: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 93750..93772: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 1 | 93956..97411: 711 instructions, 1 samples |
| paint_queue_EmitRoundedOutline | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 132/113/2/24/6/35/0/11/8/2/63/0/0/6/11 | 0 | 97478..97500: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 | 0 | 98052..98461: 97 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 76/29/2/0/0/0/0/11/9/6/21/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 | 0 | 98981..99088: 28 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 | 0 | 99258..99460: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 | 0 | 100696..100715: 4 instructions, 0 samples |
| paint_queue_EmitLine | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 101775..102370: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 103574..103596: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 0 | 115230..115252: 4 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 116413..117086: 155 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 117233..118299: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 118935..118954: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 1 | 119999..120252: 56 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 | 0 | 120360..120418: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 | 0 | none |
| svg_shape_ShapeRaster | 260/218/10/32/17/69/6/50/40/14/14/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 | 0 | 120941..121779: 205 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 339/305/29/30/29/92/9/61/32/9/0/8/1/2/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 | 0 | 122034..123016: 256 instructions, 0 samples |
| image_behavior_ImageFitBounds | 198/179/8/18/9/31/0/26/18/13/47/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 | 0 | 123174..123844: 170 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 103/87/3/9/1/12/1/19/16/7/12/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 | 0 | 123960..124290: 87 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 124313..124335: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 124744..124766: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 126034..126112: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 1 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 126695..126714: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 127164..127183: 4 instructions, 0 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 0 | 127682..127701: 4 instructions, 0 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 0 | 128368..128384: 4 instructions, 0 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 1 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 128840..128856: 4 instructions, 0 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 1 | none |
| paint_queue_ShapeRadius | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 129227..129390: 40 instructions, 0 samples |
| paint_queue_FillShapeRows | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 129467..130434: 230 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 130596..130953: 92 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 | 0 | 131937..132492: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 349/308/20/55/19/93/15/60/40/19/54/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 | 0 | 132822..133985: 279 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 564/542/41/129/90/238/7/87/46/35/156/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 | 0 | 134198..136355: 477 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 346/326/22/25/21/75/15/63/41/14/57/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 | 0 | 136778..138126: 325 instructions, 0 samples |
| paint_queue_EmitImageBegin | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 22/0/0/0/0/4/0/2/0/0/12/0/0/3/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 139454..139499: 12 instructions, 0 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 1 | 140095..140319: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 140412..140646: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 140739..140943: 52 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 19/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 140/126/5/0/0/16/4/18/13/11/38/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 | 0 | 141342..141823: 123 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 141935..142041: 30 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 142159..144128: 410 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 144257..144304: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 144606..145462: 186 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 145591..145638: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 145940..146796: 186 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 146925..146972: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 612/530/15/90/49/179/20/64/49/38/268/0/3/0/61 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 | 0 | 147423..149771: 512 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | 149895..150018: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 68/64/3/0/0/16/8/8/5/2/17/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 | 0 | 150056..150279: 52 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 | 0 | 150358..150564: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 | 0 | 150667..150773: 30 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 151392..151408: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 151614..151630: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 110/97/8/4/5/27/12/22/14/4/24/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 | 0 | 151685..152040: 89 instructions, 0 samples |

## Button / clang-19
Native SHA-256: a96f7661945af874c7f644ba8cb0058d1b634f95e0fe5f175f74d7b953320193
C SHA-256: 9468ebf7a30babea7d8d481ece8d21f51a5fc9acafbeacc5d99c3c4e7c2abd6a
ZX SHA-256: 7a061d9bab1d21d133299121a434247fa17ae4848273f4f00de0813d3bebd853
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 30/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Button_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 87..158: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_ButtonCase | 359/8/2/48/142/207/5/26/24/25/48/0/0/0/2 | 145/0/0/5/5/38/0/13/13/14/8/0/0/0/0 | 2 | 1040..1059: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 1 | none |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 1 | 3526..3548: 4 instructions, 1 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3985..4004: 4 instructions, 0 samples |
| tree_input_TreePointerUpdate | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 | 0 | none |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 6639..8198: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 8401..8420: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 8599..8751: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 8831..8941: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 7 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 9841..9884: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 | 0 | 10866..13267: 519 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 14992..15251: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| button_SubmitButton | 619/598/18/167/121/310/20/59/41/56/51/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 | 1 | 15430..18248: 583 instructions, 1 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 19532..21912: 471 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 2 | 22018..22358: 67 instructions, 2 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 0 | 23003..23025: 4 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeHitAt | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 0 | 25669..26749: 232 instructions, 0 samples |
| tree_input_TreeTextFocused | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 0 | 27380..27604: 57 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 28543..28559: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 28783..29096: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 29490..29582: 25 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 29953..30217: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 30754..31004: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 1 | 32115..32197: 19 instructions, 1 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 0 | 34138..37012: 616 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 0 | 37362..37417: 17 instructions, 0 samples |
| style_ResolveFlags | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 1 | 38632..38654: 4 instructions, 1 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailSize | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 | 1 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 0 | none |
| button_behavior_MeasureBounds | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 | 0 | 39983..41257: 300 instructions, 0 samples |
| style_StyleMinHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 371/290/13/41/19/86/4/61/48/25/83/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 | 0 | 41759..43033: 278 instructions, 0 samples |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 1 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1316/1279/16/257/184/508/12/135/119/93/320/0/1/0/133 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 | 0 | 45845..51983: 1260 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 0 | 53180..53458: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 55699..56747: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 57603..59408: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 1 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 59575..59597: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 0 | 59988..60010: 4 instructions, 0 samples |
| input_protocol_PointInBounds | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 62165..62951: 209 instructions, 0 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 63374..64605: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 0 | 65369..66049: 170 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 66180..66363: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 1 | 66930..67125: 54 instructions, 1 samples |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 | 0 | none |
| button_ButtonFaceFinish | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 67793..67951: 31 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 | 0 | 68102..69368: 251 instructions, 0 samples |
| button_ButtonDefaultFace | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 0 | 69845..70832: 191 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 70877..71034: 41 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 0 | 71157..71319: 47 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 71626..71788: 45 instructions, 0 samples |
| button_behavior_MeasureContent | 157/141/5/4/9/15/0/28/23/9/40/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 | 0 | 71873..72431: 136 instructions, 0 samples |
| button_behavior_ShapeWidth | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 0 | 72767..72789: 4 instructions, 0 samples |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 0 | 73239..73515: 69 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 1 | 73912..74219: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 1 | 74483..76128: 397 instructions, 1 samples |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| button_behavior_ButtonCornerRadius | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 1 | none |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 77360..77379: 4 instructions, 0 samples |
| paint_queue_PaintRoundedOutline | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 0 | 78233..78252: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 0 | 78600..78622: 4 instructions, 0 samples |
| focus_FocusDefaultOutlineBounds | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 78749..78961: 53 instructions, 0 samples |
| image_lookup_IconLookupImage | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 79247..79266: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 79475..79617: 39 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 91/52/4/0/0/0/0/10/6/7/27/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 79793..79964: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 224/168/5/37/26/72/0/26/21/9/92/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 | 0 | 80228..80920: 163 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 346/293/4/71/57/143/3/27/23/22/88/4/1/0/36 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 | 0 | 81165..82447: 289 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 82716..82738: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 83253..83275: 4 instructions, 0 samples |
| paint_queue_PaintImage | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| paint_queue_PaintLine | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 84416..84435: 4 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 0 | 85280..85299: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 85910..85920: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 0 | 86050..86069: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 1 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 0 | 86701..87029: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 87925..88273: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 89964..89986: 4 instructions, 0 samples |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 0 | 90095..90114: 4 instructions, 0 samples |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 90388..91049: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 | 0 | 91579..91598: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 92348..92495: 40 instructions, 0 samples |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 92563..92642: 22 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | none |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 93130..93152: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 93750..93772: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 1 | 93956..97411: 711 instructions, 1 samples |
| paint_queue_EmitRoundedOutline | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 97478..97500: 4 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 | 0 | 98052..98461: 97 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 76/29/2/0/0/0/0/11/9/6/21/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 98981..99088: 28 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 | 0 | 99258..99460: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 100696..100715: 4 instructions, 0 samples |
| paint_queue_EmitLine | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 101775..102370: 157 instructions, 0 samples |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 103574..103596: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 2 | 115230..115252: 4 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 116413..117086: 155 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 117233..118299: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 118935..118954: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 1 | 119902..119921: 4 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 46/22/2/0/0/8/7/6/4/0/0/5/0/0/1 | 0 | 120360..120418: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 260/218/10/32/17/69/6/50/40/14/14/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 | 0 | 120941..121779: 205 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 339/305/29/30/29/92/9/61/32/9/0/8/1/2/41 | 375/321/22/23/20/119/48/57/35/1/32/4/0/4/51 | 0 | 122034..123016: 256 instructions, 0 samples |
| image_behavior_ImageFitBounds | 198/179/8/18/9/31/0/26/18/13/47/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 | 0 | 123174..123844: 170 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 103/87/3/9/1/12/1/19/16/7/12/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 123960..124290: 87 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 124313..124335: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 124744..124766: 4 instructions, 0 samples |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 126034..126112: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 126695..126714: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 1 | 127164..127183: 4 instructions, 0 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 0 | 127682..127701: 4 instructions, 0 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 0 | 128368..128384: 4 instructions, 0 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 128840..128856: 4 instructions, 0 samples |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 129227..129390: 40 instructions, 0 samples |
| paint_queue_FillShapeRows | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 129467..130434: 230 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 130596..130953: 92 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 131937..132492: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| svg_shape_ShapeNumber | 349/308/20/55/19/93/15/60/40/19/54/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 | 0 | 132822..133985: 279 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 564/542/41/129/90/238/7/87/46/35/156/0/0/0/51 | 669/640/30/61/31/225/53/103/72/15/142/0/0/3/94 | 0 | 134198..136355: 477 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 346/326/22/25/21/75/15/63/41/14/57/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 | 0 | 136778..138126: 325 instructions, 0 samples |
| paint_queue_EmitImageBegin | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 1 | 139454..139499: 12 instructions, 1 samples |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 140095..140319: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 140412..140646: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 140739..140943: 52 instructions, 0 samples |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 140/126/5/0/0/16/4/18/13/11/38/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 | 0 | 141342..141823: 123 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 141935..142041: 30 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 142159..144128: 410 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 144257..144304: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 144606..145462: 186 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 145591..145638: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 145940..146796: 186 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 146925..146972: 14 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 612/530/15/90/49/179/20/64/49/38/268/0/3/0/61 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 | 0 | 147423..149771: 512 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 | 0 | 149895..150018: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 68/64/3/0/0/16/8/8/5/2/17/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 150056..150279: 52 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 150358..150564: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 150667..150773: 30 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 151392..151408: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 151614..151630: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 110/97/8/4/5/27/12/22/14/4/24/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 | 0 | 151685..152040: 89 instructions, 0 samples |

## Text / cc
Native SHA-256: e93b493c5645f88ededabe73d8e7a6d4311c880e032b42054493c37bd6795b71
C SHA-256: 4abc4d6c8da1175b763ea53c29cb054974fffe4c2a5f06131c828109890de89c
ZX SHA-256: 6e473205235ad0e328c1e6f1d6db93d00be0ff754ac81e632c8ee85426be9ce6
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 44/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 67..138: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 236/158/9/19/59/102/3/30/21/16/29/0/0/0/27 | 140/128/10/16/20/66/4/16/6/7/32/0/0/1/29 | 1 | 588..1261: 157 instructions, 1 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 0 | none |
| text___zi_spec_6a19f7c98e79a734 | 21/4/1/6/6/12/2/1/0/1/2/0/0/0/1 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | 2775..2797: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 1 | 3249..3268: 4 instructions, 1 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 4071..5630: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 5833..5852: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 6031..6183: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 6263..6373: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 5 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 7273..7316: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 8298..10699: 519 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 0 | 12424..12683: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| text_DrawSession | 342/141/9/96/82/193/14/30/21/29/24/0/0/0/21 | 293/263/10/101/82/188/2/15/5/17/159/0/0/0/13 | 0 | 13985..14610: 121 instructions, 0 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 15603..17983: 471 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 0 | 18089..18429: 67 instructions, 0 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 0 | 19074..19096: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 21500..21813: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 22207..22299: 25 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 23050..23314: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 0 | 24114..24130: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 24422..24672: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 44/20/1/5/0/9/1/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 192/153/11/6/6/53/12/32/21/4/0/7/0/4/25 | 1 | 25884..26712: 195 instructions, 1 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 1 | 27806..30680: 616 instructions, 1 samples |
| std_math_Finite32 | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 11/0/0/0/0/3/0/2/2/0/4/0/0/0/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 57/47/1/10/12/28/0/2/1/2/14/0/0/1/10 | 0 | 31244..31266: 4 instructions, 0 samples |
| text_PrepareText | 247/81/2/54/57/153/0/13/11/15/16/0/0/0/2 | 172/91/3/37/23/69/2/7/4/14/32/0/0/3/4 | 1 | 32315..32500: 45 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 48/0/0/2/2/9/2/4/3/1/4/2/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 174/110/8/34/48/86/1/16/8/6/79/0/0/0/6 | 81/32/1/8/14/25/0/1/0/2/56/0/0/3/4 | 1 | 33009..33481: 105 instructions, 1 samples |
| text_TextOrigin | 23/0/0/2/3/14/0/0/0/0/9/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 1 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 1 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 | 1 | 34841..35124: 74 instructions, 0 samples |
| text_TextHiddenLines | 117/39/2/11/2/22/0/11/9/10/21/0/0/0/4 | 86/45/3/10/2/22/7/11/4/2/22/0/0/0/6 | 0 | 35503..35590: 26 instructions, 0 samples |
| tree_TreeSetHiddenLines | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 0 | none |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 49/0/0/15/15/33/0/1/1/1/30/0/0/0/0 | 0 | 36006..36028: 4 instructions, 0 samples |
| text_PaintTextLines | 691/609/7/184/149/360/10/49/42/45/83/0/0/0/42 | 585/494/12/195/126/348/8/37/15/15/222/0/0/8/37 | 0 | 36528..39713: 606 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 0 | 40745..41023: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 43264..44312: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 45168..46973: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 1 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 0 | 47140..47162: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 0 | 47553..47575: 4 instructions, 0 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 7 | 49163..49949: 209 instructions, 5 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 0 | 50372..51603: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 0 | 52367..53047: 170 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 80/21/2/2/2/20/10/5/3/2/0/3/0/3/2 | 0 | 53178..53361: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 0 | 53627..53956: 77 instructions, 0 samples |
| text_DefaultTextFace | 65/4/1/5/32/45/2/1/0/1/20/0/0/0/1 | 13/0/0/0/0/7/0/0/0/1/0/0/0/0/0 | 0 | 54372..54391: 4 instructions, 0 samples |
| text_TextFaceFor | 59/16/4/20/17/41/8/4/0/1/8/0/0/0/4 | 38/0/0/9/5/17/0/0/0/1/10/0/0/1/0 | 1 | 54701..54723: 4 instructions, 1 samples |
| text_behavior_ResolveTextStyle | 127/99/1/14/14/30/0/23/22/9/8/0/0/2/18 | 79/58/6/8/3/19/2/14/6/1/11/0/0/0/3 | 0 | 54862..55241: 99 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 0 | none |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 44/26/1/0/0/0/0/6/5/4/10/0/0/0/6 | 14/0/0/0/0/4/0/3/1/0/7/0/0/0/0 | 0 | 56194..56291: 26 instructions, 0 samples |
| text_behavior_TextWrapPolicy | 21/0/0/0/0/0/0/1/1/1/2/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 566/502/26/67/179/311/14/67/41/26/18/12/1/0/55 | 264/212/25/12/8/101/33/43/17/3/11/2/0/14/30 | 0 | 56639..59008: 467 instructions, 0 samples |
| text_behavior_TextExtent | 17/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 13/0/0/0/0/0/0/2/2/0/0/2/0/0/0 | 0 | 59304..59374: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| geometry_WorldToScreen | 46/0/0/8/5/22/0/0/0/2/30/0/0/0/0 | 42/0/0/8/3/11/0/0/0/2/35/0/0/0/0 | 1 | none |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 1 | 60164..60471: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 1 | 60591..62277: 408 instructions, 1 samples |
| text_behavior_TextAlignmentOffset | 31/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 18/0/0/0/0/3/1/2/2/0/9/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 31/8/1/0/0/1/0/4/3/1/0/0/0/0/1 | 0 | 62558..62617: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_TextEllipsisStart | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 49/33/3/2/2/8/2/6/3/1/0/0/0/0/5 | 0 | 62821..63146: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 15/0/0/0/0/1/1/1/1/0/0/0/0/2/0 | 1 | none |
| text_PaintTextLineRuns | 336/316/7/110/106/235/12/16/9/13/30/0/0/0/14 | 280/230/5/89/52/150/5/9/4/11/101/2/0/4/22 | 0 | 63366..65147: 316 instructions, 0 samples |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 1 | 65712..65731: 4 instructions, 1 samples |
| text_PaintTextEmphasis | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 239/187/5/55/36/103/8/16/11/6/37/2/0/7/21 | 0 | 66244..67152: 172 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 38/16/1/3/5/10/0/3/2/1/11/0/0/0/1 | 9/0/0/0/0/2/0/0/0/0/8/0/0/0/0 | 0 | 67283..67343: 16 instructions, 0 samples |
| text_PaintTextRule | 195/4/1/69/73/148/2/2/1/10/46/0/0/0/1 | 131/0/0/27/26/54/0/4/2/6/72/0/0/0/0 | 0 | 67510..67532: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 68613..68623: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 0 | 68753..68772: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 0 | 69404..69732: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 278/259/112/0/0/136/10/118/4/0/0/1/0/3/122 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 27/0/0/4/2/12/1/1/1/1/8/0/0/0/0 | 0 | 70578..70600: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 1 | 70905..70927: 4 instructions, 1 samples |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 375/334/6/115/53/189/6/42/36/22/18/0/0/0/22 | 352/318/8/47/23/84/6/28/20/6/12/0/0/13/29 | 0 | 71463..72584: 279 instructions, 0 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/2/0/2/1/0/0/1/0/0/1 | 0 | 72623..72675: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 25/21/6/1/0/10/0/9/3/0/1/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 207/114/4/0/0/0/0/21/17/12/66/0/0/0/23 | 95/49/3/0/0/28/2/16/11/0/55/0/0/0/0 | 1 | 73323..73770: 110 instructions, 0 samples |
| std_math_CosineDegrees | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/1/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 | 2 | 74139..74309: 44 instructions, 2 samples |
| text_TextLinkRunAt | 119/32/2/37/18/67/1/12/10/4/0/0/0/0/3 | 75/52/2/4/0/11/7/11/9/0/0/0/0/3/7 | 0 | 74492..74620: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 21/0/0/0/0/1/0/0/0/3/0/2/0/4/0 | 0 | none |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 76223..76245: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 1 | 76510..79965: 711 instructions, 1 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 32/13/3/0/0/3/2/4/1/0/0/0/0/2/3 | 0 | none |
| paint_queue_PaintClippedEmphasis | 177/4/1/18/84/150/2/2/1/2/25/0/0/0/1 | 45/0/0/4/16/23/0/0/0/1/6/0/0/0/0 | 0 | 80849..80868: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 0 | 81728..81747: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 141/4/1/21/93/117/2/1/0/4/32/0/0/0/1 | 44/0/0/0/7/10/0/0/0/1/18/0/0/2/0 | 0 | 82729..82748: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | 93414..93433: 4 instructions, 0 samples |
| text_layout_TextTokenNext | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 259/237/31/6/4/46/10/62/31/6/0/0/0/4/53 | 1 | 93743..94867: 277 instructions, 1 samples |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 95066..95219: 42 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 96443..96521: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 96858..97924: 253 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 98597..98616: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 0 | 99661..99914: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 0 | 99959..99981: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 100579..100601: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 19/0/0/6/3/10/0/0/0/0/9/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 123/8/2/10/85/97/4/2/0/1/26/0/0/0/2 | 30/0/0/0/4/11/0/0/0/1/0/0/0/0/0 | 0 | 101162..101181: 4 instructions, 0 samples |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 1 | 102262..102284: 4 instructions, 0 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 102/89/15/0/0/18/1/28/13/5/0/0/0/0/20 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 103539..103584: 12 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 178/148/12/9/11/52/15/26/14/4/21/0/0/6/20 | 0 | 104555..105110: 127 instructions, 0 samples |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 105810..106483: 155 instructions, 0 samples |
| shape_paint_EmitShape | 104/16/4/17/12/33/8/18/14/7/14/0/0/0/4 | 25/0/0/4/0/8/1/11/7/0/3/0/0/0/0 | 0 | 106689..106711: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 11/0/0/4/0/5/0/0/0/1/0/0/0/0/0 | 0 | 107134..107156: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 107693..107712: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 1 | 108162..108181: 4 instructions, 0 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 0 | 108680..108699: 4 instructions, 0 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 0 | 109366..109382: 4 instructions, 0 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 0 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 109838..109854: 4 instructions, 0 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 24/8/2/0/0/6/0/4/2/0/0/0/0/0/2 | 2 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 114/105/18/0/0/12/2/28/10/0/0/0/0/0/37 | 0 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 36/19/2/0/0/4/1/7/5/0/0/0/0/0/6 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 1 | 112813..113037: 58 instructions, 1 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 113130..113364: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 113457..113661: 52 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 0 | none |
| paint_queue_ShapeRadius | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 113874..114037: 40 instructions, 0 samples |
| paint_queue_FillShapeRows | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 114114..115081: 230 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 115243..115600: 92 instructions, 0 samples |
| shape_paint_EmitLineShape | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 102/16/1/19/0/39/5/5/4/5/44/0/0/2/1 | 0 | 115685..115707: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 220/120/6/50/37/89/6/22/16/17/53/0/0/0/16 | 176/124/9/25/14/65/1/20/11/8/53/0/0/3/14 | 0 | 116974..117450: 112 instructions, 0 samples |
| shape_paint_EmitRingShape | 198/59/3/35/24/63/4/26/23/18/52/0/0/0/11 | 108/82/9/20/6/42/1/14/5/8/45/0/0/0/9 | 0 | 118160..118338: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 37/12/1/7/0/19/1/3/2/2/9/0/0/0/3 | 0 | 118385..118407: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 4 | 118874..120843: 410 instructions, 4 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 120972..121019: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 121321..122177: 186 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 122306..122353: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 122655..123511: 186 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 123640..123687: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| shape_paint_ShapeAt | 19/0/0/2/3/10/0/0/0/0/7/0/0/0/0 | 5/0/0/1/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 164/0/0/32/24/73/0/9/9/7/71/0/0/0/0 | 96/0/0/8/6/19/0/5/5/3/63/0/0/2/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 31/20/3/0/0/6/0/5/2/0/18/0/0/2/2 | 0 | none |
| shape_paint_PointsBounds | 110/95/5/20/10/38/1/11/6/5/32/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 | 0 | 125418..125700: 73 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 349/43/5/70/57/178/14/33/28/28/72/0/0/0/7 | 215/178/9/33/33/78/6/16/6/12/113/0/0/2/11 | 0 | 126879..127004: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 19/0/0/0/0/6/0/2/2/1/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcDirections | 69/53/2/10/10/28/3/7/5/3/22/0/0/0/5 | 48/18/1/4/5/15/3/3/2/2/21/0/0/0/1 | 0 | 127608..127826: 53 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 142/127/10/15/3/18/0/24/14/19/6/1/2/0/23 | 77/17/1/0/0/3/2/2/1/2/3/2/0/9/3 | 0 | 127890..128323: 115 instructions, 0 samples |
| paint_queue_GradientColorAt | 63/46/2/13/6/20/0/5/3/6/14/0/0/0/4 | 153/120/12/0/0/25/9/29/13/0/73/0/0/6/14 | 0 | 128441..128619: 45 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 35/15/1/3/3/11/2/2/1/1/17/0/0/0/1 | 0 | 128668..128802: 35 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 59/36/1/7/5/15/1/2/1/2/37/0/0/0/1 | 0 | 128846..129081: 59 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 129178..129284: 30 instructions, 0 samples |
| std_math_Sqrt | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 100/80/5/0/0/24/1/17/11/0/57/0/0/2/10 | 0 | 129523..129922: 98 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 103/67/1/11/6/41/5/9/8/6/10/0/0/0/12 | 79/36/3/2/2/20/7/8/5/2/28/0/0/0/5 | 0 | 130107..130357: 67 instructions, 0 samples |
| shape_paint_ShapeClipSide | 150/69/3/35/34/86/10/13/10/6/26/0/0/0/7 | 272/247/28/13/7/60/14/61/33/0/111/0/0/1/21 | 0 | 130576..130840: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 131119..131128: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/3/1/0/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 62/0/0/0/0/0/0/6/6/5/16/0/0/0/0 | 33/0/0/0/0/7/3/4/3/0/17/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcPoint | 25/0/0/6/4/14/1/0/0/0/11/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/8/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| std_math_Finite64 | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 10/0/0/0/0/2/0/2/2/0/4/0/0/0/0 | 0 | none |
| std_math_MathSplit | 173/159/16/3/3/8/0/25/9/6/42/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 131827..132484: 148 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 74/48/7/0/0/15/4/17/10/0/21/0/0/0/6 | 0 | 132611..132972: 87 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/20/0/0/1/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 16/0/0/0/0/1/1/3/3/0/4/0/0/1/0 | 0 | none |
| shape_paint_ShapeCrossing | 98/0/0/22/9/33/0/10/10/2/37/0/0/0/0 | 44/20/1/0/0/2/1/6/5/0/30/0/0/2/1 | 0 | none |
| widget_benchmark_host_RasterTriangles | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/9/2/4/1/0/6/0/0/1/2 | 0 | 133694..133829: 34 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 133876..133978: 24 instructions, 0 samples |

## Text / clang-19
Native SHA-256: e93b493c5645f88ededabe73d8e7a6d4311c880e032b42054493c37bd6795b71
C SHA-256: 1843fc432bba8da9d944fbc07ff6f1c5107586deae3ebb97aff7ec269525218a
ZX SHA-256: 6e473205235ad0e328c1e6f1d6db93d00be0ff754ac81e632c8ee85426be9ce6
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 43/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Text_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 67..138: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_TextCase | 236/158/9/19/59/102/3/30/21/16/29/0/0/0/27 | 155/66/6/12/15/54/0/20/14/14/19/0/0/1/12 | 0 | 588..1261: 157 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 0 | none |
| text___zi_spec_6a19f7c98e79a734 | 21/4/1/6/6/12/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | 2775..2797: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 3249..3268: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 4071..5630: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 5833..5852: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 6031..6183: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 6263..6373: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 2 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 7273..7316: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 379/237/17/42/15/132/43/58/41/21/26/4/0/5/35 | 0 | 8298..10699: 519 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 1 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 12424..12683: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| text_DrawSession | 342/141/9/96/82/193/14/30/21/29/24/0/0/0/21 | 293/19/2/36/34/100/0/25/23/20/108/1/0/2/2 | 1 | 13114..13136: 4 instructions, 1 samples |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 15603..17983: 471 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 0 | 18089..18429: 67 instructions, 0 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 0 | 19074..19096: 4 instructions, 0 samples |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 21500..21813: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 22207..22299: 25 instructions, 0 samples |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | absent (inline/dead/renamed unknown) | 0 | 23050..23314: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 24114..24130: 4 instructions, 0 samples |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 24422..24672: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 0 | 25884..26712: 195 instructions, 0 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 0 | 27806..30680: 616 instructions, 0 samples |
| std_math_Finite32 | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 0 | none |
| text_TextFaceOf | 45/12/3/11/7/26/6/4/1/2/8/0/0/0/3 | 48/0/0/12/9/28/0/2/2/2/12/0/0/1/0 | 0 | 31244..31266: 4 instructions, 0 samples |
| text_PrepareText | 247/81/2/54/57/153/0/13/11/15/16/0/0/0/2 | 134/18/1/13/2/44/0/6/5/14/18/0/0/3/3 | 1 | 32315..32500: 45 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| geometry_RotatedRectangleBounds | 174/110/8/34/48/86/1/16/8/6/79/0/0/0/6 | 124/0/0/28/18/48/0/0/0/10/111/0/0/0/0 | 1 | 33009..33481: 105 instructions, 1 samples |
| text_TextOrigin | 23/0/0/2/3/14/0/0/0/0/9/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 | 1 | none |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 0 | 34841..35124: 74 instructions, 0 samples |
| text_TextHiddenLines | 117/39/2/11/2/22/0/11/9/10/21/0/0/0/4 | 112/19/2/2/1/17/1/11/9/3/51/1/0/0/2 | 0 | 35503..35590: 26 instructions, 0 samples |
| tree_TreeSetHiddenLines | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| text_selection_host_TextSelectionPresent | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 | 0 | 36006..36028: 4 instructions, 0 samples |
| text_PaintTextLines | 691/609/7/184/149/360/10/49/42/45/83/0/0/0/42 | 490/418/9/71/66/235/5/41/32/23/164/0/0/10/38 | 3 | 36528..39713: 606 instructions, 2 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 1 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 1 | 40745..41023: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 43264..44312: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 45168..46973: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 1 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 47140..47162: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 2 | 48589..48611: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 3 | 49163..49949: 209 instructions, 2 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 50372..51603: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 1 | 52367..53047: 170 instructions, 1 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 1 | 53178..53361: 42 instructions, 1 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 0 | 53627..53956: 77 instructions, 0 samples |
| text_DefaultTextFace | 65/4/1/5/32/45/2/1/0/1/20/0/0/0/1 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 | 0 | 54372..54391: 4 instructions, 0 samples |
| text_TextFaceFor | 59/16/4/20/17/41/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 | 0 | 54479..54498: 4 instructions, 0 samples |
| text_behavior_ResolveTextStyle | 127/99/1/14/14/30/0/23/22/9/8/0/0/2/18 | 57/0/0/5/0/8/0/3/3/2/7/0/0/2/0 | 0 | 54862..55241: 99 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_layout_TextDefaultLineGap | 44/26/1/0/0/0/0/6/5/4/10/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | 56194..56291: 26 instructions, 0 samples |
| text_behavior_TextWrapPolicy | 21/0/0/0/0/0/0/1/1/1/2/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| text_layout_NextTextLine | 566/502/26/67/179/311/14/67/41/26/18/12/1/0/55 | 345/109/17/14/7/108/42/57/40/5/19/10/0/13/13 | 0 | 56639..59008: 467 instructions, 0 samples |
| text_behavior_TextExtent | 17/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 | 0 | none |
| text_layout_TextLayoutTotalHeight | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 | 1 | 59304..59374: 24 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| geometry_WorldToScreen | 46/0/0/8/5/22/0/0/0/2/30/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 | 0 | none |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 0 | 60164..60471: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 0 | 60591..62277: 408 instructions, 0 samples |
| text_behavior_TextAlignmentOffset | 31/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 | 0 | none |
| text_layout_TextCharactersEnd | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 | 0 | 62558..62617: 16 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1 | none |
| text_TextEllipsisStart | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 55/23/3/2/2/9/3/7/4/1/0/0/0/0/2 | 0 | 62821..63146: 84 instructions, 0 samples |
| text_layout_TextLineXFor | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 | 0 | none |
| text_PaintTextLineRuns | 336/316/7/110/106/235/12/16/9/13/30/0/0/0/14 | 301/250/14/75/45/157/7/25/11/16/80/2/0/1/20 | 1 | 63366..65147: 316 instructions, 1 samples |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 1 | 65712..65731: 4 instructions, 0 samples |
| text_PaintTextEmphasis | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 221/165/17/28/27/87/10/29/12/5/13/2/0/3/17 | 0 | 66244..67152: 172 instructions, 0 samples |
| text_behavior_TextStrikethroughBounds | 38/16/1/3/5/10/0/3/2/1/11/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 | 0 | 67283..67343: 16 instructions, 0 samples |
| text_PaintTextRule | 195/4/1/69/73/148/2/2/1/10/46/0/0/0/1 | 101/0/0/26/22/52/0/4/2/6/51/0/0/0/0 | 0 | 67510..67532: 4 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 68613..68623: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 0 | 68753..68772: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 0 | 69404..69732: 82 instructions, 0 samples |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_style_TextStyleFor | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 | 0 | 70578..70600: 4 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 0 | 70863..70885: 4 instructions, 0 samples |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| font_metrics_GlyphMetricsRevision | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_layout_NextTextLineUncached | 375/334/6/115/53/189/6/42/36/22/18/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 | 3 | 71463..72584: 279 instructions, 3 samples |
| text_layout_TextLineHash | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 45/22/2/0/0/7/5/6/4/0/0/5/0/0/1 | 0 | 72623..72675: 13 instructions, 0 samples |
| text_layout_TextLineCacheMatches | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 | 0 | none |
| std_math_SineDegrees | 207/114/4/0/0/0/0/21/17/12/66/0/0/0/23 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 | 2 | 73323..73770: 110 instructions, 0 samples |
| std_math_CosineDegrees | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 80/0/0/0/0/24/0/12/12/1/63/0/0/0/0 | 0 | none |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | none |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 2 | 74139..74309: 44 instructions, 2 samples |
| text_TextLinkRunAt | 119/32/2/37/18/67/1/12/10/4/0/0/0/0/3 | 57/27/7/2/0/10/5/14/7/0/0/0/0/0/2 | 0 | 74492..74620: 32 instructions, 0 samples |
| text_TextLinkColor | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 | 0 | none |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 76223..76245: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 3 | 76510..79965: 711 instructions, 3 samples |
| text_TextRangeValid | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 29/0/0/0/0/2/2/3/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_PaintClippedEmphasis | 177/4/1/18/84/150/2/2/1/2/25/0/0/0/1 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 | 0 | 80849..80868: 4 instructions, 0 samples |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 0 | 81728..81747: 4 instructions, 0 samples |
| shape_paint_PaintTriangleShape | 141/4/1/21/93/117/2/1/0/4/32/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 | 0 | 82729..82748: 4 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 0 | 93414..93433: 4 instructions, 0 samples |
| text_layout_TextTokenNext | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 210/116/20/4/2/22/7/53/33/3/0/0/0/3/34 | 3 | 93743..94867: 277 instructions, 3 samples |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 95066..95219: 42 instructions, 0 samples |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 96443..96521: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 96858..97924: 253 instructions, 0 samples |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 98597..98616: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 0 | 99661..99914: 56 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 99959..99981: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 100579..100601: 4 instructions, 0 samples |
| shape_paint_ShapeOffset | 19/0/0/6/3/10/0/0/0/0/9/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_SubmitShape | 123/8/2/10/85/97/4/2/0/1/26/0/0/0/2 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 | 0 | 101162..101181: 4 instructions, 0 samples |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 2 | 102262..102284: 4 instructions, 0 samples |
| text_layout_TextIsSpace | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| text_layout_TextBreaksBetween | 42/0/0/4/2/6/0/5/5/6/0/0/0/0/0 | 82/46/12/0/0/8/0/24/12/1/0/0/0/0/16 | 0 | none |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 103539..103584: 12 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 104555..105110: 127 instructions, 0 samples |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 105810..106483: 155 instructions, 0 samples |
| shape_paint_EmitShape | 104/16/4/17/12/33/8/18/14/7/14/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 | 0 | 106689..106711: 4 instructions, 0 samples |
| paint_queue_SubmitPaint | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 | 0 | 107134..107156: 4 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 107693..107712: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 108162..108181: 4 instructions, 0 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 0 | 108680..108699: 4 instructions, 0 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 1 | 109366..109382: 4 instructions, 1 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 0 | 109838..109854: 4 instructions, 0 samples |
| text_layout_TextIdeographic | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 1 | none |
| text_layout_TextNoBreakBefore | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/10/7/0/0/0/0/0/7 | 2 | none |
| text_layout_TextNoBreakAfter | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 112813..113037: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 113130..113364: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 113457..113661: 52 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 113874..114037: 40 instructions, 0 samples |
| paint_queue_FillShapeRows | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 114114..115081: 230 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 115243..115600: 92 instructions, 0 samples |
| shape_paint_EmitLineShape | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 | 0 | 115685..115707: 4 instructions, 0 samples |
| shape_paint_EmitCircleShape | 220/120/6/50/37/89/6/22/16/17/53/0/0/0/16 | 280/234/11/36/19/99/9/21/10/10/108/2/0/8/25 | 0 | 116974..117450: 112 instructions, 0 samples |
| shape_paint_EmitRingShape | 198/59/3/35/24/63/4/26/23/18/52/0/0/0/11 | 182/57/3/32/13/80/9/21/18/9/103/0/0/0/0 | 0 | 118160..118338: 51 instructions, 0 samples |
| shape_paint_EmitTriangleShape | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 | 0 | 118385..118407: 4 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 118874..120843: 410 instructions, 0 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 120972..121019: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 121321..122177: 186 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 122306..122353: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 122655..123511: 186 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 123640..123687: 14 instructions, 0 samples |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| shape_paint_ShapeAt | 19/0/0/2/3/10/0/0/0/0/7/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 | 0 | none |
| shape_paint_LineCornersFor | 164/0/0/32/24/73/0/9/9/7/71/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 | 0 | none |
| shape_paint_ShapeBatchBegin | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 | 0 | none |
| shape_paint_ShapeHidden | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 | 0 | none |
| shape_paint_PointsBounds | 110/95/5/20/10/38/1/11/6/5/32/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/3 | 0 | 125418..125700: 73 instructions, 0 samples |
| shape_paint_ShapeBatchTriangle | 349/43/5/70/57/178/14/33/28/28/72/0/0/0/7 | 567/481/23/63/91/187/30/86/62/10/307/0/0/1/48 | 0 | 126879..127004: 27 instructions, 0 samples |
| shape_paint_ShapeBatchFlush | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeArcDirections | 69/53/2/10/10/28/3/7/5/3/22/0/0/0/5 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 | 0 | 127608..127826: 53 instructions, 0 samples |
| shape_paint_ShapeFadeBands | 142/127/10/15/3/18/0/24/14/19/6/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 | 0 | 127890..128323: 115 instructions, 0 samples |
| paint_queue_GradientColorAt | 63/46/2/13/6/20/0/5/3/6/14/0/0/0/4 | 143/0/0/0/0/17/0/16/16/1/72/0/0/13/0 | 0 | 128441..128619: 45 instructions, 0 samples |
| shape_paint_ShapeBatchSector | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 | 0 | 128668..128802: 35 instructions, 0 samples |
| shape_paint_ShapeBatchBand | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 | 0 | 128846..129081: 59 instructions, 0 samples |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 129178..129284: 30 instructions, 0 samples |
| std_math_Sqrt | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 228/43/7/0/0/40/6/32/25/0/139/0/0/2/6 | 0 | 129523..129922: 98 instructions, 0 samples |
| shape_paint_ShapeBatchAdd | 103/67/1/11/6/41/5/9/8/6/10/0/0/0/12 | 63/0/0/6/4/21/3/6/6/2/29/0/0/0/0 | 0 | 130107..130357: 67 instructions, 0 samples |
| shape_paint_ShapeClipSide | 150/69/3/35/34/86/10/13/10/6/26/0/0/0/7 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 | 0 | 130576..130840: 61 instructions, 0 samples |
| raster_triangles_FillTriangles | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| shape_paint_ShapeChannelChange | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 131119..131128: 5 instructions, 0 samples |
| std_math_CeilInteger | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 | 0 | none |
| paint_queue_GradientChannel | 62/0/0/0/0/0/0/6/6/5/16/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 | 0 | none |
| shape_paint_ShapeArcPoint | 25/0/0/6/4/14/1/0/0/0/11/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 | 0 | none |
| std_math_NotANumber | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 | 0 | none |
| std_math_Finite64 | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 | 0 | none |
| std_math_MathSplit | 173/159/16/3/3/8/0/25/9/6/42/0/0/0/28 | absent (inline/dead/renamed unknown) | 0 | 131827..132484: 148 instructions, 0 samples |
| std_math_ScaleByPowerOfTwo | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 89/35/5/0/0/15/4/22/17/0/34/0/0/1/4 | 0 | 132611..132972: 87 instructions, 0 samples |
| shape_paint_ShapeTurn | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 | 0 | none |
| shape_paint_ShapeInside | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 | 0 | none |
| shape_paint_ShapeCrossing | 98/0/0/22/9/33/0/10/10/2/37/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTriangles | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 | 0 | 133694..133829: 34 instructions, 0 samples |
| std_math_SmallPowerOfTwo | 34/24/2/0/0/0/0/6/4/1/6/0/0/0/6 | absent (inline/dead/renamed unknown) | 0 | 133876..133978: 24 instructions, 0 samples |

## Input / cc
Native SHA-256: 85df50eff17838d7c25334da776ffaa81600a9a045730ea5d1388e357b36386b
C SHA-256: c77c158bdc0e61b8c246fb0fb2efd8e62a5f537c6e0ef7fc7e573b897bd6d311
ZX SHA-256: 8d37d3c31614715048c09987fc38cf77ffd8a239d01b0b25504f892a8720bb54
Compiler: cc (Debian 14.2.0-19) 14.2.0
SIGPROF samples/runs: 58/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 92..163: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 394/8/2/53/124/218/8/37/35/26/35/0/0/0/2 | 164/152/15/12/15/57/0/17/2/11/8/0/0/0/42 | 0 | 873..892: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 10/0/0/0/0/8/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 | 0 | none |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 195/136/7/50/17/84/3/18/11/9/16/0/0/3/19 | 0 | 3639..3661: 4 instructions, 0 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 | 0 | none |
| tree_input_TreePointerUpdate | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 6844..6863: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 | 0 | 7666..9225: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 | 0 | 9428..9447: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 9626..9778: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 17/0/0/0/0/4/1/2/2/0/0/0/0/1/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 | 0 | 9858..9968: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 | 12 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 | 0 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 | 0 | 10868..10911: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 | 0 | 11893..14294: 519 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 | 0 | 16019..16278: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 7/0/0/2/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/1/1/0/0/0/0/0/0 | 0 | 16504..16642: 38 instructions, 0 samples |
| text_context_menu_TextMenuTakeCommand | 153/110/7/23/10/47/0/31/24/14/0/0/0/0/18 | 66/46/6/2/0/14/2/13/7/3/0/0/0/0/6 | 0 | 16842..17272: 108 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 63/22/1/6/5/13/1/4/3/4/11/0/0/1/1 | 0 | none |
| textfield_TextFieldCore | 3691/3557/51/746/1040/2101/26/399/348/251/250/1/1/0/263 | 2425/2344/104/678/397/1177/25/259/140/150/359/2/0/15/320 | 3 | 18276..36051: 3473 instructions, 3 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 79/0/0/15/15/49/4/3/3/6/0/4/0/0/0 | 54/20/3/0/0/19/6/5/2/3/0/0/0/0/3 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 18/4/1/0/0/4/0/4/3/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 17/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 | 0 | none |
| paint_queue_PaintSnapshotEmit | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 | 0 | 38090..40470: 471 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 | 1 | 40576..40916: 67 instructions, 1 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 1 | none |
| tree_input_TreeHitAt | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 | 0 | 41410..42490: 232 instructions, 0 samples |
| tree_input_TreeTextFocused | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 | 0 | 43121..43345: 57 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 | 0 | 44284..44300: 4 instructions, 0 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 | 0 | 44915..44937: 4 instructions, 0 samples |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 32/0/0/0/0/0/0/1/1/2/0/0/0/0/0 | 1 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 | 0 | 47406..47719: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 | 0 | 48113..48205: 25 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | 48576..48840: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 | 0 | none |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 | 0 | 49377..49627: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 | 1 | 50839..51667: 195 instructions, 1 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 | 2 | 52761..55635: 616 instructions, 1 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/1/1/0/0/0/0/0/0 | 0 | none |
| system_clipboard_SystemClipboardText | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 73/46/8/2/1/11/6/14/6/2/0/0/0/0/6 | 0 | 55874..56430: 140 instructions, 0 samples |
| input_protocol_PointInBounds | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 | 0 | none |
| text_context_menu_TextMenuOpenAt | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 61/12/2/3/0/16/2/6/4/2/2/0/0/0/2 | 0 | 56910..57126: 51 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 | 0 | 57441..57496: 17 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 12/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakeDragAt | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 173/148/11/13/4/58/10/25/14/6/22/0/0/8/20 | 0 | none |
| tree_TreePreviousWidgetState | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 48/0/0/0/0/17/3/4/4/1/2/1/0/1/0 | 0 | none |
| tree_input_TreeHasFocus | 101/71/3/11/8/35/6/14/11/8/0/3/0/0/14 | 71/45/3/5/4/20/7/9/6/3/0/1/0/0/7 | 0 | 59573..59852: 71 instructions, 0 samples |
| tree_input_TreeBlurAt | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 37/7/1/0/0/9/4/5/4/1/0/1/0/1/2 | 0 | none |
| textfield_TextFieldFaceFor | 145/134/4/24/61/91/4/9/5/8/28/0/0/0/10 | 121/74/1/30/15/63/1/2/1/10/52/0/0/0/4 | 2 | 60169..60887: 133 instructions, 2 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 52/38/1/0/0/0/0/7/6/5/9/0/0/0/8 | 14/0/0/0/0/2/0/3/1/0/7/0/0/0/0 | 0 | 61249..61384: 38 instructions, 0 samples |
| textfield_TextFieldSecureDisplay | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 49/20/2/1/0/5/1/5/3/2/0/0/0/0/3 | 0 | 61479..61606: 36 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 61/0/0/0/0/5/2/6/2/0/7/0/0/2/0 | 1 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 66/38/2/8/10/20/1/4/2/3/0/0/0/0/2 | 0 | 62072..62331: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 69/0/0/0/0/0/0/7/7/7/18/0/0/0/0 | 37/0/0/0/0/6/0/6/3/0/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 29/7/1/0/0/5/2/2/1/1/0/0/0/0/3 | 0 | 62622..62685: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 39/21/2/0/0/2/1/5/3/1/0/0/0/0/2 | 0 | 62828..62971: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 269/97/2/84/42/171/0/18/16/11/0/0/0/0/3 | 159/126/11/16/29/59/8/28/17/0/23/0/0/0/13 | 0 | 63645..64003: 97 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 15/11/1/2/3/6/1/2/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 21/8/1/0/0/6/3/5/4/0/0/0/0/0/3 | 0 | 64251..64328: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 133/98/8/12/3/35/9/24/16/4/0/0/0/0/9 | 0 | 64543..64688: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/88/8/4/3/13/4/18/10/0/0/0/0/5/8 | 0 | 65112..65773: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 174/4/1/48/39/113/2/18/17/7/2/0/0/0/1 | 106/75/9/16/12/37/1/14/5/5/12/0/0/0/6 | 0 | 65966..65988: 4 instructions, 0 samples |
| text_request_TextRequestTake | 190/0/0/43/24/110/10/16/16/11/0/9/0/0/0 | 79/0/0/4/3/24/4/8/8/4/0/1/0/0/0 | 0 | none |
| text_request_TextRequestChangeFor | 259/242/12/47/26/98/5/36/24/13/0/0/0/0/24 | 160/128/12/20/5/57/8/24/12/6/0/0/0/0/9 | 0 | 67504..68310: 217 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 67/50/5/0/0/7/3/8/3/4/0/0/0/0/5 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 34/0/0/0/0/5/1/2/2/1/0/0/0/0/0 | 0 | 68505..68734: 60 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 25/0/0/0/0/2/2/1/1/0/0/0/0/0/0 | 0 | 68766..68936: 52 instructions, 0 samples |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 48/0/0/2/2/9/2/4/3/1/4/2/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetCaptureTab | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 49/36/5/0/0/7/1/8/3/4/0/1/0/1/4 | 0 | none |
| tree_input_TreeFocusSubmittedAt | 225/4/1/39/26/96/10/28/27/14/8/7/0/0/1 | 158/139/17/8/16/61/7/23/6/6/25/3/0/5/15 | 0 | 70331..70350: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 155/104/7/15/5/48/10/26/19/3/2/4/0/3/14 | 0 | 71518..71903: 96 instructions, 0 samples |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 | 0 | 72109..72392: 74 instructions, 0 samples |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 | 0 | none |
| tree_TreeSetTextValue | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 172/64/6/12/9/59/8/28/22/4/0/3/0/1/8 | 0 | 74228..74937: 178 instructions, 0 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 36/0/0/5/0/8/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 | 1 | 75596..75615: 4 instructions, 0 samples |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 | 1 | none |
| paint_queue_PaintRoundedOutline | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 | 0 | 76812..76831: 4 instructions, 0 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 173/0/0/21/19/52/1/13/13/12/46/0/1/0/0 | 120/0/0/2/0/13/2/13/5/0/41/0/0/4/0 | 0 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 17/0/0/0/0/3/0/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 17/0/0/0/0/3/0/3/1/0/7/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 27/0/0/0/0/3/0/4/2/0/6/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 | 0 | 78559..78578: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 28/0/0/0/0/4/2/4/2/0/7/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 79026..79043: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 79073..79127: 18 instructions, 0 samples |
| paint_queue_PaintLine | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 | 1 | 79670..79689: 4 instructions, 1 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 80037..80056: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 80135..80157: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 18/6/1/0/0/6/2/3/2/0/0/0/0/0/1 | 0 | 80190..80235: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 86/0/0/16/15/53/6/3/3/5/0/6/0/0/0 | 53/17/2/0/0/14/5/4/2/3/0/0/0/0/1 | 0 | none |
| text_input_platform_TextInputFocused | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 38/25/4/0/0/8/3/5/1/3/0/0/0/0/1 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 13/6/1/0/0/1/0/3/1/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 49/6/1/0/0/10/5/3/2/1/0/0/0/2/4 | 0 | 81038..81105: 17 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 400/350/2/41/98/270/21/16/14/13/26/16/0/0/82 | 152/138/6/8/31/79/3/13/7/4/21/0/0/2/33 | 0 | 81466..83162: 350 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 | 0 | 84177..84455: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 | 0 | 86696..87744: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 | 0 | 88600..90405: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 55/0/0/5/0/21/0/3/3/2/2/0/0/0/0 | 0 | 90572..90594: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 | 2 | 92091..92113: 4 instructions, 2 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 | 0 | 92871..93657: 209 instructions, 0 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 426/377/47/24/48/100/11/91/44/1/2/0/0/10/106 | 1 | 94080..95311: 302 instructions, 1 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 | 1 | 96075..96755: 170 instructions, 1 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/21/2/2/2/22/12/5/3/2/0/3/0/3/2 | 0 | 96886..97069: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 | 2 | 97335..97664: 77 instructions, 2 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 11/0/0/0/0/4/2/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 | 0 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 71/4/1/4/39/46/2/1/0/6/25/0/0/0/1 | 63/0/0/10/13/35/0/0/0/6/26/0/0/0/0 | 1 | 98965..98984: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 | 0 | 99161..100427: 251 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 | 0 | 100553..100715: 47 instructions, 0 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 18/0/0/0/0/2/0/3/1/0/4/0/0/0/0 | 1 | 100861..100870: 5 instructions, 1 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 100962..101042: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/15/2/0/0/2/2/6/4/0/0/0/0/0/3 | 0 | 101095..101159: 17 instructions, 0 samples |
| std_grapheme_Previous | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 | 0 | 101269..101412: 41 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 | 0 | 101747..102023: 69 instructions, 0 samples |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 | 0 | 102152..102480: 82 instructions, 0 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 | 0 | 102877..103184: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 | 0 | 103304..104990: 408 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 127/114/2/23/5/34/2/11/8/2/57/0/0/6/12 | 0 | 105179..105201: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 | 0 | 105799..105821: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 | 1 | 106005..109460: 711 instructions, 1 samples |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 132/113/2/24/6/35/0/11/8/2/63/0/0/6/11 | 0 | 109659..109681: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 24/0/0/0/0/2/0/3/1/0/4/0/0/0/0 | 0 | 110230..110436: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 36/28/1/10/0/24/0/2/1/1/20/0/0/0/1 | 2 | 110509..110531: 4 instructions, 1 samples |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 | 0 | 111765..111787: 4 instructions, 0 samples |
| paint_queue_EmitLine | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 | 0 | 112504..113099: 157 instructions, 0 samples |
| button_SubmitButton | 619/598/18/167/121/310/20/59/41/56/51/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 | 0 | 113203..116021: 583 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 980/945/36/232/240/560/28/93/57/58/87/16/0/0/95 | 570/548/32/102/94/285/46/65/33/18/103/1/0/4/61 | 0 | 116300..120674: 904 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 | 0 | 121162..121172: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 | 0 | 121302..121321: 4 instructions, 0 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 | 1 | none |
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
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 | 0 | 123035..123383: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 | 0 | 125074..125096: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 35/0/0/0/2/14/4/3/3/1/0/2/0/0/0 | 0 | none |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 | 0 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 | 0 | 126139..126812: 155 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 | 0 | 126959..128025: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 56/37/3/0/3/9/3/7/4/3/4/0/0/4/6 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 | 0 | 128661..128680: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 | 1 | 129628..129647: 4 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 1 | 130096..130115: 4 instructions, 1 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 | 0 | 131538..131616: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 266/172/4/89/71/181/2/9/5/15/144/0/0/0/7 | 0 | 132798..132820: 4 instructions, 0 samples |
| button_ButtonDetailSize | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 | 0 | none |
| button_behavior_MeasureBounds | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 | 0 | 134026..135300: 300 instructions, 0 samples |
| style_StyleMinHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 371/290/13/41/19/86/4/61/48/25/83/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 | 0 | 135802..137076: 278 instructions, 0 samples |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1316/1279/16/257/184/508/12/135/119/93/320/0/1/0/133 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 | 0 | 138065..144203: 1260 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 26/12/1/0/0/2/1/2/1/2/0/0/0/0/1 | 0 | none |
| tree_TreeNextKey | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 65/9/1/0/0/11/5/5/4/2/0/5/0/4/1 | 0 | 144615..144677: 15 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 46/9/1/0/0/6/2/4/3/2/0/1/0/1/1 | 0 | none |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 8/0/0/0/0/4/3/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 25/7/2/0/0/7/3/6/4/0/0/0/0/0/2 | 0 | 145059..145141: 21 instructions, 0 samples |
| contextmenu_ContextMenuFind | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/2/5/3/0/0/0/0/0/3 | 0 | 145206..145293: 23 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 105/43/1/32/22/60/1/3/2/6/37/1/0/0/1 | 85/0/0/11/8/21/2/2/1/4/48/0/0/0/0 | 0 | 145681..145858: 43 instructions, 0 samples |
| overlay_Draw | 388/311/1/115/78/209/0/28/27/28/19/0/0/0/28 | 257/214/9/46/27/82/5/22/13/19/36/0/0/6/31 | 0 | 146177..147479: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1422/1205/10/372/349/778/10/72/62/142/317/1/0/0/114 | 1082/888/26/288/120/488/12/74/36/102/301/0/0/2/121 | 0 | 148569..154440: 1093 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 | 0 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 1 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 | 1 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 | 1 | none |
| paint_queue_ShapeRadius | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 | 0 | 165886..166049: 40 instructions, 0 samples |
| paint_queue_FillShapeRows | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 | 0 | 166126..167093: 230 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 | 0 | 167255..167612: 92 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 | 0 | 168596..169151: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 | 4 | 170210..170232: 4 instructions, 1 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 38/6/1/0/0/12/3/5/2/0/8/0/0/2/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 | 0 | 171230..171275: 12 instructions, 0 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 22/0/0/0/0/4/0/2/0/0/12/0/0/3/0 | 1 | none |
| button_ButtonFaceFinish | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 45/37/3/11/1/23/2/5/2/3/18/0/0/0/0 | 0 | 171368..171526: 31 instructions, 0 samples |
| button_ButtonDefaultFace | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 | 0 | 171995..172982: 191 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 | 0 | 173027..173184: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 | 0 | none |
| style_StyleLineHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 | 0 | 173479..173641: 45 instructions, 0 samples |
| button_behavior_MeasureContent | 157/141/5/4/9/15/0/28/23/9/40/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 | 0 | 173726..174284: 136 instructions, 0 samples |
| button_behavior_ShapeWidth | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 | 0 | 174620..174642: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 | 0 | 174969..175181: 53 instructions, 0 samples |
| image_lookup_IconLookupImage | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 | 0 | 175467..175486: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 | 0 | 175695..175837: 39 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 91/52/4/0/0/0/0/10/6/7/27/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 | 0 | 176013..176184: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 224/168/5/37/26/72/0/26/21/9/92/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 | 0 | 176448..177140: 163 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 346/293/4/71/57/143/3/27/23/22/88/4/1/0/36 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 | 0 | 177385..178667: 289 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 | 0 | 178936..178958: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 | 0 | 179473..179495: 4 instructions, 0 samples |
| paint_queue_PaintImage | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 442/405/22/88/57/181/12/48/26/33/59/11/0/0/41 | 261/197/11/38/27/100/8/30/19/19/54/0/0/2/32 | 0 | 180264..181472: 276 instructions, 0 samples |
| dpi_ScalePixels | 83/72/1/0/0/0/0/10/9/8/21/0/0/0/14 | 42/0/0/0/0/7/0/7/4/0/19/0/0/0/0 | 0 | 182097..182372: 72 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 113/93/7/21/13/42/4/18/11/9/31/4/0/0/12 | 93/61/7/3/3/21/5/14/7/4/26/0/0/1/11 | 0 | 182459..182852: 91 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/14/1/0/0/1/1/2/1/0/0/0/0/2/4 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/17/1/1/0/4/2/5/4/4/0/0/0/0/1 | 0 | none |
| tree_TreeSetPopup | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 38/0/0/0/0/8/1/5/5/2/0/1/0/1/0 | 0 | none |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 183864..184017: 42 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 17/0/0/2/0/5/0/1/1/1/0/0/0/0/0 | 0 | none |
| tree_TreePushScope | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 81/64/5/0/0/13/9/9/4/8/0/8/0/0/12 | 0 | none |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/1/1/2/0/1/1/2/2/0/0/0/0 | 0 | 184918..184931: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 26/13/4/0/0/8/2/7/3/0/0/0/0/0/4 | 0 | 184946..185109: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/2/0/4/0/2/2/4/4/0/0/0/0 | 0 | 185359..185368: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 24/6/1/0/0/9/5/3/2/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 635/588/8/74/327/438/14/31/23/21/71/0/0/0/35 | 182/92/5/18/42/76/9/11/6/4/78/0/0/1/19 | 0 | 185908..188733: 554 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 189250..189474: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 | 0 | 189567..189801: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 | 0 | 189894..190098: 52 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 190456..190475: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 2 | 190925..190944: 4 instructions, 0 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 | 1 | 191443..191462: 4 instructions, 0 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 | 3 | 192129..192145: 4 instructions, 2 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 | 0 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 2 | 192601..192617: 4 instructions, 1 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 | 0 | 192685..193346: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 | 0 | 193876..193895: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 | 0 | 194601..194748: 40 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 | 0 | 194808..194887: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 | 0 | 195003..195412: 97 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 76/29/2/0/0/0/0/11/9/6/21/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 | 0 | 195932..196039: 28 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 | 0 | 196209..196411: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 | 0 | 197647..197666: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 228/194/7/10/16/40/7/36/29/13/9/0/6/0/35 | 122/102/6/0/6/38/14/13/5/0/6/4/0/7/26 | 0 | 198331..198979: 193 instructions, 0 samples |
| tree_TreePopScope | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 54/27/1/2/1/6/3/4/3/4/0/4/0/0/5 | 0 | none |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 | 0 | 199463..199569: 30 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 1 | 199687..201656: 410 instructions, 1 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 201785..201832: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 | 0 | 202134..202990: 186 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 203119..203166: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 | 0 | 203468..204324: 186 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 78/40/5/0/0/10/3/19/14/1/0/0/0/3/8 | 0 | 204453..204500: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 13/0/0/1/0/4/1/1/1/1/2/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 | 0 | 204688..204746: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 | 0 | none |
| svg_shape_ShapeRaster | 260/218/10/32/17/69/6/50/40/14/14/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 | 0 | 205269..206107: 205 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 339/305/29/30/29/92/9/61/32/9/0/8/1/2/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 | 0 | 206362..207344: 256 instructions, 0 samples |
| image_behavior_ImageFitBounds | 198/179/8/18/9/31/0/26/18/13/47/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 | 0 | 207502..208172: 170 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 103/87/3/9/1/12/1/19/16/7/12/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 | 0 | 208288..208618: 87 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 | 0 | 208641..208663: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 | 0 | 209072..209094: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 349/308/20/55/19/93/15/60/40/19/54/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 | 0 | 209311..210474: 279 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 564/542/41/129/90/238/7/87/46/35/156/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 | 0 | 210687..212844: 477 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 346/326/22/25/21/75/15/63/41/14/57/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 | 0 | 213267..214615: 325 instructions, 0 samples |
| paint_queue_EmitImageBegin | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 19/0/0/0/0/4/1/2/2/2/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 140/126/5/0/0/16/4/18/13/11/38/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 | 0 | 215603..216084: 123 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 612/530/15/90/49/179/20/64/49/38/268/0/3/0/61 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 | 0 | 216473..218821: 512 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 | 0 | 218945..219068: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 68/64/3/0/0/16/8/8/5/2/17/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 | 0 | 219106..219329: 52 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 | 0 | 219408..219614: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 | 0 | 219717..219823: 30 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 220442..220458: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 220664..220680: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 110/97/8/4/5/27/12/22/14/4/24/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 | 0 | 220735..221090: 89 instructions, 0 samples |

## Input / clang-19
Native SHA-256: 85df50eff17838d7c25334da776ffaa81600a9a045730ea5d1388e357b36386b
C SHA-256: 508d91ebbc6bf9126f61f6932319a62a4847f7cba48106af5b72f913a5b9aceb
ZX SHA-256: 8d37d3c31614715048c09987fc38cf77ffd8a239d01b0b25504f892a8720bb54
Compiler: Debian clang version 19.1.7 (3+b1)
SIGPROF samples/runs: 56/30

| Function | Native metrics | C metrics | Samples | Native loop with most samples (text offsets) |
| --- | --- | --- | ---: | --- |
| teacher_Input_main | 28/20/3/2/1/3/0/6/3/2/0/0/0/0/5 | absent (inline/dead/renamed unknown) | 0 | 92..163: 20 instructions, 0 samples |
| text_boundary___global_init_0_0 | 11/0/0/1/1/5/0/0/0/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| widget_benchmarks_InputCase | 394/8/2/53/124/218/8/37/35/26/35/0/0/0/2 | 161/0/0/13/24/70/0/19/19/12/23/0/0/0/0 | 0 | 873..892: 4 instructions, 0 samples |
| text_request_TextRequestUseBoundary | 5/0/0/0/0/4/0/1/1/0/0/0/0/0/0 | 12/0/0/0/0/10/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_boundary_GraphemeBoundary | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 33/9/1/0/0/1/0/5/4/1/0/0/0/0/0 | 0 | none |
| widget_benchmarks_BeginCase | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_draw_BeginFrame | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 | 0 | none |
| textfield_Draw | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 117/0/0/14/4/41/2/10/10/9/2/0/0/0/0 | 1 | 4116..4138: 4 instructions, 1 samples |
| tree_draw_EndFrame | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 | 0 | none |
| tree_input_TreePointerUpdate | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 | 0 | none |
| tree_TreeNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 6844..6863: 4 instructions, 0 samples |
| widget_benchmark_host_WidgetTextCount | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_WidgetPaintChecksum | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_editing_TextEditingUseRequests | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 6/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_request_TextRequestSend | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 | 0 | 7666..9225: 355 instructions, 0 samples |
| text_request_TextRequestClear | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 | 0 | 9428..9447: 4 instructions, 0 samples |
| std_grapheme_Boundary | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 68/40/4/5/2/10/1/9/5/2/0/0/0/1/4 | 0 | 9626..9778: 45 instructions, 0 samples |
| session_SessionValid | 13/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 0 | none |
| session_SessionOpen | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 104/0/0/0/0/12/1/15/15/0/0/0/0/7/0 | 0 | 9858..9968: 30 instructions, 0 samples |
| widget_benchmark_host_WidgetPaintReset | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_SessionIndex | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 | 7 | none |
| tree_input_PointerSample | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 | 1 | none |
| tree_input_SecondaryPointerSample | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 | 0 | none |
| session_SessionFlagSet | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | 10868..10911: 18 instructions, 0 samples |
| paint_queue_PaintSnapshotInvalidate | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 | 0 | none |
| tree_input_TreeKeyboardRoute | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 | 0 | 11893..14294: 519 instructions, 0 samples |
| text_editing_TextEditingFrameBegin | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClear | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 | 0 | none |
| tree_TreeStart | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 | 0 | 16019..16278: 61 instructions, 0 samples |
| tree_TreeFrameStatus | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 | 0 | none |
| textfield_TextFieldKey | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionRangeForLength | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | 16504..16642: 38 instructions, 0 samples |
| text_context_menu_TextMenuTakeCommand | 153/110/7/23/10/47/0/31/24/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 | 0 | 16842..17272: 108 instructions, 0 samples |
| text_context_menu_TextMenuPasteText | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenRequested | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 85/30/2/7/4/25/2/9/7/6/12/0/0/0/3 | 1 | none |
| textfield_TextFieldCore | 3691/3557/51/746/1040/2101/26/399/348/251/250/1/1/0/263 | 2155/1176/35/302/166/909/19/256/221/148/299/1/0/13/120 | 2 | 18276..36051: 3473 instructions, 2 samples |
| system_clipboard_SystemClipboardSet | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputNoteFocused | 79/0/0/15/15/49/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuIsOpenFor | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuShow | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 | 0 | none |
| session_SessionFlagGet | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| tree_draw_CommitFrame | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 | 0 | none |
| paint_queue_PaintSnapshotEmit | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 | 0 | 38090..40470: 471 instructions, 0 samples |
| paint_queue_PaintFlush | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 141/75/4/5/1/48/14/12/8/13/6/1/0/6/13 | 0 | 40576..40916: 67 instructions, 0 samples |
| tree_TreeIsBuilding | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 1 | none |
| tree_input_TreeHitAt | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 | 1 | 41410..42490: 232 instructions, 1 samples |
| tree_input_TreeTextFocused | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 | 0 | 43121..43345: 57 instructions, 0 samples |
| tree_input_TreeTextKind | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeFocusAt | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 | 0 | none |
| tree_TreeInputAt | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 | 0 | 44284..44300: 4 instructions, 0 samples |
| tree_TreeRead | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 | 2 | 44915..44937: 4 instructions, 1 samples |
| tree_TreeCount | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeInputAllowedAt | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 | 0 | none |
| std_utf8_Valid | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 118/105/2/0/0/13/4/21/19/0/0/0/0/3/30 | 0 | 47406..47719: 80 instructions, 0 samples |
| tree_TreeTextRange | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 | 0 | 48113..48205: 25 instructions, 0 samples |
| std_grapheme_Next | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | 48576..48840: 73 instructions, 0 samples |
| tree_input_TreeFocusedAt | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 | 0 | none |
| geometry_RectangleIntersection | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 | 0 | 49377..49627: 65 instructions, 0 samples |
| tree_TreePointerActivatable | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 | 0 | none |
| tree_input_TreeActivateAt | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 | 0 | none |
| cursor_CursorHoverFrameStart | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 | 0 | none |
| tree_TreeBuildOldIndex | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 | 1 | 50839..51667: 195 instructions, 1 samples |
| tree_TreeSubmit | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 | 0 | 52761..55635: 616 instructions, 0 samples |
| text_input_TextSelectionRangeFor | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| system_clipboard_SystemClipboardText | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 65/17/2/0/0/5/1/9/7/2/0/0/0/0/1 | 0 | 55874..56430: 140 instructions, 0 samples |
| input_protocol_PointInBounds | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 | 0 | none |
| text_context_menu_TextMenuOpenAt | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 55/14/1/3/0/16/1/5/4/3/2/0/0/0/2 | 0 | 56910..57126: 51 instructions, 0 samples |
| tree_TreePrevious | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 | 0 | 57441..57496: 17 instructions, 0 samples |
| tree_TreeCurrentParent | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 | 0 | none |
| tree_input_TreeTakeActivationAt | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakeDragAt | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 155/35/3/8/1/45/10/27/24/6/8/6/0/0/6 | 0 | none |
| tree_TreePreviousWidgetState | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 | 0 | none |
| tree_input_TreeHasFocus | 101/71/3/11/8/35/6/14/11/8/0/3/0/0/14 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 | 0 | 59573..59852: 71 instructions, 0 samples |
| tree_input_TreeBlurAt | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 | 0 | none |
| textfield_TextFieldFaceFor | 145/134/4/24/61/91/4/9/5/8/28/0/0/0/10 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 | 0 | 60169..60887: 133 instructions, 0 samples |
| style_ResolveFont | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 | 0 | none |
| style_StyleFontValue | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 | 0 | none |
| text_input_TextInputScaledMetric | 52/38/1/0/0/0/0/7/6/5/9/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 1 | 61249..61384: 38 instructions, 0 samples |
| textfield_TextFieldSecureDisplay | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 57/8/1/0/0/2/2/5/4/2/0/0/0/0/1 | 0 | 61479..61606: 36 instructions, 0 samples |
| font_metrics_GlyphWidth | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldScrollFor | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 | 0 | none |
| textfield_TextFieldCursorAtX | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 | 0 | 62072..62331: 68 instructions, 0 samples |
| text_input_TextPointerOffset | 69/0/0/0/0/0/0/7/7/7/18/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 | 0 | none |
| textfield_TextFieldCodepoints | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 | 0 | 62622..62685: 17 instructions, 0 samples |
| text_TextSpan | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldByteAfter | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 | 0 | 62828..62971: 42 instructions, 0 samples |
| text_input_CompositionTransitionFor | 269/97/2/84/42/171/0/18/16/11/0/0/0/0/3 | 110/98/3/0/0/37/4/14/11/0/19/0/0/0/11 | 0 | 63645..64003: 97 instructions, 0 samples |
| textfield_TextFieldPrevious | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 32/9/1/0/0/1/0/4/3/1/0/0/0/0/0 | 0 | none |
| textfield_TextFieldNext | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextSelectionAfterMove | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 | 0 | none |
| text_input_TextContextCommandCopy | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandCut | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextContextCommandPaste | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldInputTextAllowed | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 25/12/1/0/0/3/2/4/3/0/0/0/0/0/3 | 0 | 64251..64328: 19 instructions, 0 samples |
| textfield_TextFieldReplacementAllowed | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 | 0 | 64543..64688: 29 instructions, 0 samples |
| text_input_TextEditCommandDecisionFor | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/0/0/2/0/2/0/12/12/0/0/0/0/6/0 | 0 | 65112..65773: 163 instructions, 0 samples |
| textfield_TextFieldEditFor | 174/4/1/48/39/113/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 | 0 | 65966..65988: 4 instructions, 0 samples |
| text_request_TextRequestTake | 190/0/0/43/24/110/10/16/16/11/0/9/0/0/0 | 76/0/0/1/0/18/2/9/9/4/3/1/0/0/0 | 0 | none |
| text_request_TextRequestChangeFor | 259/242/12/47/26/98/5/36/24/13/0/0/0/0/24 | 145/55/6/6/10/39/8/22/16/5/0/0/0/2/0 | 0 | 67504..68310: 217 instructions, 0 samples |
| textfield_TextFieldDisplayByteFor | 41/0/0/9/7/17/0/1/1/5/0/0/0/0/0 | 68/18/2/0/0/2/2/8/6/4/0/0/0/0/1 | 0 | none |
| font_metrics_GlyphCaret | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 32/0/0/1/3/8/0/2/2/1/2/0/0/0/0 | 0 | 68505..68734: 60 instructions, 0 samples |
| text_input_TextFieldRevealScroll | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 68766..68936: 52 instructions, 0 samples |
| style_StyleOpacityValue | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 | 0 | none |
| tree_TreeSubmitCurrent | 40/0/0/10/8/18/0/1/1/3/0/0/0/0/0 | 44/0/0/2/2/8/2/4/3/1/4/1/0/0/0 | 0 | none |
| tree_TreeSetInteractive | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetCaptureTab | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 | 0 | none |
| tree_input_TreeFocusSubmittedAt | 225/4/1/39/26/96/10/28/27/14/8/7/0/0/1 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 | 0 | 70331..70350: 4 instructions, 0 samples |
| tree_TreeSetWidgetState | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 | 0 | 71518..71903: 96 instructions, 0 samples |
| tree_TreeSetSemantic | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 | 0 | 72109..72392: 74 instructions, 0 samples |
| tree_TreeSetLabel | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleClass | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetStyleId | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetSelected | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 | 0 | none |
| tree_TreeSetTextValue | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 138/86/3/8/4/47/12/22/19/5/0/1/0/2/6 | 0 | 74228..74937: 178 instructions, 0 samples |
| text_host_TextHostPresent | 26/0/0/5/5/14/0/2/2/2/0/0/0/0/0 | 32/0/0/5/0/8/0/1/1/1/0/0/0/0/0 | 0 | none |
| paint_queue_PaintRoundedFill | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 | 2 | 75596..75615: 4 instructions, 1 samples |
| drawing_props_ColorFromPacked | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 1 | none |
| surface_Opacity | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 | 0 | none |
| paint_queue_PaintRoundedOutline | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 | 0 | 76812..76831: 4 instructions, 0 samples |
| font_metrics_GlyphLineHeight | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldPaintFor | 173/0/0/21/19/52/1/13/13/12/46/0/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 | 0 | none |
| text_input_TextFieldMinCursorHeight | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorVerticalPadding | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextFieldClipGuard | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 22/0/0/0/0/3/0/2/2/1/11/0/0/0/0 | 0 | none |
| theme_CurrentMuted | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentAccent | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_PaintClippedLabel | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 | 0 | 78559..78578: 4 instructions, 0 samples |
| paint_queue_zi_default_7a97e54d75f71213 | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 | 0 | none |
| text_input_TextInputStrokeWidth | 19/0/0/0/0/0/0/1/1/2/2/0/0/0/0 | 23/0/0/0/0/3/0/2/2/1/12/0/0/0/0 | 0 | none |
| text_input_TextCompositionUnderlineY | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 | 0 | 79026..79043: 8 instructions, 0 samples |
| text_input_TextCompositionUnderlineEndX | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 79073..79127: 18 instructions, 0 samples |
| paint_queue_PaintLine | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 | 0 | 79670..79689: 4 instructions, 0 samples |
| tree_TreeSubmittedNodeAt | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | 80037..80056: 4 instructions, 0 samples |
| button_Draw | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 | 0 | 80135..80157: 4 instructions, 0 samples |
| system_clipboard_host_SystemClipboardWriteHost | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 82/28/4/0/0/21/19/15/11/0/4/0/0/0/3 | 0 | 80190..80235: 12 instructions, 0 samples |
| text_editing_TextEditingUseFocus | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 5/0/0/0/0/4/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocusFrameBegin | 86/0/0/16/15/53/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 | 0 | none |
| text_input_platform_TextInputFocused | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 | 0 | none |
| text_input_platform_TextInputOwnerMatches | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeDefer | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 | 0 | 81038..81105: 17 instructions, 0 samples |
| text_context_menu_TextMenuDraw | 400/350/2/41/98/270/21/16/14/13/26/16/0/0/82 | 140/0/0/6/33/75/0/12/12/5/18/0/0/2/0 | 0 | 81466..83162: 350 instructions, 0 samples |
| tree_draw_FrameInputCommit | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintOverflow | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 | 0 | none |
| tree_TreeCancel | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 | 0 | none |
| tree_TreeFinish | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 | 0 | 84177..84455: 58 instructions, 0 samples |
| paint_queue_PaintOrder | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 | 0 | 86696..87744: 225 instructions, 0 samples |
| paint_queue_SnapshotTextRead | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 | 0 | none |
| paint_queue_SnapshotTextJoin | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 | 0 | 88600..90405: 409 instructions, 0 samples |
| paint_queue_PaintPointCount | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 | 0 | none |
| paint_queue_EmitPaint | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 | 0 | 90572..90594: 4 instructions, 0 samples |
| paint_queue_PendingPaintAt | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 | 1 | 90985..91007: 4 instructions, 1 samples |
| tree_input_TreeTopModalLayer | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayerAt | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 53/15/1/0/0/11/4/7/6/3/0/1/0/1/2 | 0 | none |
| tree_TreeButtonLike | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_input_PointInCircleBounds | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 | 0 | none |
| tree_TreeOverlayLayerAt | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| std_utf8_Decode | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 | 0 | 92871..93657: 209 instructions, 0 samples |
| std_grapheme_Push | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 | 0 | 94080..95311: 302 instructions, 0 samples |
| tree_TreeParentGeneration | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 | 0 | none |
| tree_TreeIdentityHash | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeBuildNewIndex | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 | 1 | 96075..96755: 170 instructions, 0 samples |
| tree_TreeExplicitExists | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 | 0 | 96886..97069: 42 instructions, 0 samples |
| widget_key_WidgetRepeatedKey | 12/0/0/1/1/2/0/1/1/1/0/0/0/0/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| tree_TreeFindOld | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 | 1 | 97335..97664: 77 instructions, 1 samples |
| system_clipboard_host_SystemClipboardOpenHost | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 | 0 | none |
| system_clipboard_host_SystemClipboardByteHost | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreeTakePointerActivationAt | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 | 1 | none |
| style_sheet_StyleControlRoleFacts | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindTextField | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| textfield_TextFieldDefaultFace | 71/4/1/4/39/46/2/1/0/6/25/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 | 0 | 98965..98984: 4 instructions, 0 samples |
| style_sheet_ResolveInstalledStyleRules | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 | 0 | 99161..100427: 251 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphWidth | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 | 0 | 100553..100715: 47 instructions, 0 samples |
| text_input_TextInputContentWidth | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 1 | 100861..100870: 5 instructions, 0 samples |
| text_input_TextUtf8Continuation | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_utf8_Next | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 86/33/2/0/0/10/5/17/15/0/0/0/0/3/10 | 0 | none |
| text_input_TextCursorForLength | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 100962..101042: 26 instructions, 0 samples |
| text_input_CompositionBoundaryFor | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/9/1/0/0/3/2/3/2/0/0/0/0/0/2 | 0 | 101095..101159: 17 instructions, 0 samples |
| std_grapheme_Previous | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 65/38/2/6/2/11/2/8/6/2/0/0/0/1/4 | 0 | 101269..101412: 41 instructions, 0 samples |
| text_input_TextContextCommandSelectAll | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreeEnsureInteraction | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 | 0 | 101747..102023: 69 instructions, 0 samples |
| tree_TreeLayerAt | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 | 1 | 102152..102480: 82 instructions, 1 samples |
| tree_TreeEnsureSemantic | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 | 0 | 102877..103184: 70 instructions, 0 samples |
| tree_TreeStoreLabel | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 | 0 | 103304..104990: 408 instructions, 0 samples |
| paint_queue_EmitRoundedFill | 99/8/2/26/22/52/4/6/4/7/10/0/0/0/2 | 153/0/0/20/12/44/0/17/16/3/109/0/0/0/0 | 0 | 105179..105201: 4 instructions, 0 samples |
| paint_queue_ClipShape | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 | 0 | 105799..105821: 4 instructions, 0 samples |
| paint_queue_QueuePaint | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 | 2 | 106005..109460: 711 instructions, 2 samples |
| surface_Unit | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 | 0 | none |
| paint_queue_EmitRoundedOutline | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 | 0 | 109659..109681: 4 instructions, 0 samples |
| widget_benchmark_host_MeasureGlyphLineHeight | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| text_input_TextFieldCursorHeightFor | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 | 0 | 110230..110436: 53 instructions, 0 samples |
| theme_CurrentPalette | 19/8/2/3/2/7/4/2/0/1/4/0/0/0/2 | 35/0/0/0/0/31/0/2/2/0/30/0/0/0/0 | 1 | 110551..110573: 4 instructions, 1 samples |
| paint_queue_EmitLabel | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 | 0 | none |
| paint_queue_ClipLabel | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 | 0 | 111765..111787: 4 instructions, 0 samples |
| paint_queue_EmitLine | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 | 0 | 112504..113099: 157 instructions, 0 samples |
| button_SubmitButton | 619/598/18/167/121/310/20/59/41/56/51/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 | 0 | 113203..116021: 583 instructions, 0 samples |
| text_context_menu_text_menu_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| system_clipboard_SystemClipboardHasText | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 0 | none |
| contextmenu_Draw | 980/945/36/232/240/560/28/93/57/58/87/16/0/0/95 | 588/511/31/82/99/249/33/78/47/19/99/14/0/3/58 | 0 | 116300..120674: 904 instructions, 0 samples |
| cursor_CursorHoverFrameCommit | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 | 0 | none |
| paint_queue_PaintMeshPointCount | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | 121162..121172: 5 instructions, 0 samples |
| paint_queue_EmitPaintDirect | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 | 1 | 121356..121375: 4 instructions, 1 samples |
| paint_queue_ResolvePaintText | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 | 0 | none |
| tree_TreeModalLayer | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 | 0 | none |
| std_utf8_Continuation | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| std_grapheme_data_Property | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 | 0 | none |
| std_grapheme_IsControl | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| session_WidgetPartKey | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 | 0 | none |
| widget_key_WidgetRepeatedPart | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleControlFacts | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 | 0 | none |
| theme_CurrentSunken | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentText | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorderStrong | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 1 | none |
| theme_ShapeSmall | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_TypeLabel | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| style_sheet_StyleBucket | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleFirstBit | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 | 0 | 123035..123383: 93 instructions, 0 samples |
| style_sheet_StyleRuleMatchesState | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleSelectorMatches | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 | 0 | none |
| style_MergeValues | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 | 0 | 125074..125096: 4 instructions, 0 samples |
| tree_TreeLabelSpanAt | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 | 0 | none |
| tree_TreeSetLabelSpan | 30/0/0/4/0/18/4/2/2/0/0/4/0/0/0 | 32/0/0/0/0/11/4/4/4/2/0/1/0/1/0 | 0 | none |
| paint_queue_ShapeVisible | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 | 1 | none |
| paint_queue_ShapeWhole | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 | 2 | none |
| raster_shape_FillRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| paint_queue_FillShapeClipped | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 | 0 | 126139..126812: 155 instructions, 0 samples |
| paint_queue_EmitPaintNow | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 | 0 | 126959..128025: 253 instructions, 0 samples |
| paint_queue_KeepPaintText | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| paint_queue_KeepPaintAsset | 44/0/0/10/3/22/0/3/3/3/0/0/0/0/0 | 47/0/0/0/0/6/1/10/10/3/0/0/0/2/0 | 0 | none |
| tree_TreeCurrentPaintGeometry | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 | 0 | 128661..128680: 4 instructions, 0 samples |
| tree_TreeSubmittedGeometryAt | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 | 0 | none |
| paint_queue_KeepPaintMesh | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 | 1 | 129628..129647: 4 instructions, 1 samples |
| raster_shape_StrokeRoundedRectangle | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentTheme | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 3 | 130096..130115: 4 instructions, 3 samples |
| text_typeface_TextTypefaceUse | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| raster_text_PaintTextRotated | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintTextItalicClipped | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 | 0 | none |
| raster_text_PaintTextEmphasisClipped | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 | 0 | 131538..131616: 22 instructions, 0 samples |
| raster_text_PaintTextClipped | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 | 0 | none |
| raster_text_PaintText | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| raster_StrokeLine | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| style_ResolveFlags | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 | 0 | none |
| style_ResolveState | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 | 0 | none |
| tree_input_TreePressedAt | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 69/0/0/0/0/11/7/7/7/4/0/4/0/1/0 | 0 | none |
| tree_input_TreeHoveredAt | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 | 0 | none |
| button_ButtonInstalledFaceFor | 75/24/6/20/17/44/12/6/0/4/12/0/0/0/6 | 124/0/0/36/22/76/0/4/4/8/44/0/0/0/0 | 0 | 132798..132820: 4 instructions, 0 samples |
| button_ButtonDetailSize | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 | 0 | none |
| tree_TreeRootBounds | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 | 0 | none |
| button_behavior_MeasureBounds | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 | 0 | 134026..135300: 300 instructions, 0 samples |
| style_StyleMinHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| button_ButtonMinimumHeight | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 0 | none |
| button_ButtonFitContentFace | 371/290/13/41/19/86/4/61/48/25/83/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 | 0 | 135802..137076: 278 instructions, 0 samples |
| tree_TreeSetCircleHit | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 | 0 | none |
| tree_TreeSetSemanticDetail | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 | 0 | none |
| button_PaintButtonForNode | 1316/1279/16/257/184/508/12/135/119/93/320/0/1/0/133 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 | 0 | 138065..144203: 1260 instructions, 0 samples |
| tree_input_TreeFocusVisibleAt | 26/0/0/5/5/12/0/2/2/3/0/0/0/0/0 | 64/0/0/0/0/8/4/7/7/4/0/1/0/2/0 | 0 | none |
| tree_TreeNextKey | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 | 0 | 144615..144677: 15 instructions, 0 samples |
| contextmenu_ContextMenuOverlayKey | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 0 | none |
| overlay_OverlayPanelKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuPanelKey | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| tree_TreePreviousCommitted | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 | 0 | none |
| contextmenu_ContextMenuRowKey | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 | 0 | none |
| contextmenu_ContextMenuOpens | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 | 0 | 145059..145141: 21 instructions, 0 samples |
| contextmenu_ContextMenuFind | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 | 0 | 145206..145293: 23 instructions, 0 samples |
| contextmenu_ContextMenuBounds | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuSubmenuBounds | 105/43/1/32/22/60/1/3/2/6/37/1/0/0/1 | 122/32/8/25/13/45/3/12/3/5/50/2/0/0/9 | 0 | 145681..145858: 43 instructions, 0 samples |
| overlay_Draw | 388/311/1/115/78/209/0/28/27/28/19/0/0/0/28 | 200/82/1/15/5/38/0/16/15/19/34/0/0/1/9 | 0 | 146177..147479: 311 instructions, 0 samples |
| contextmenu_ContextMenuLevel | 1422/1205/10/372/349/778/10/72/62/142/317/1/0/0/114 | 940/723/13/211/90/371/26/63/50/101/289/2/0/1/99 | 0 | 148569..154440: 1093 instructions, 0 samples |
| paint_queue_PendingTextValue | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 | 1 | none |
| std_grapheme_data_Block | 184/0/0/30/21/89/9/32/32/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| std_grapheme_data_Page | 1966/0/0/315/211/944/104/389/389/1/0/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| style_sheet_StyleDefaultFacts | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 | 0 | none |
| style_sheet_StyleStateAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleMatchesOptional | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterRoundedRectangle | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 | 0 | none |
| paint_queue_ShapeRadius | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 | 0 | 165886..166049: 40 instructions, 0 samples |
| paint_queue_FillShapeRows | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 328/265/6/37/24/70/2/22/16/2/289/0/0/0/2 | 0 | 166126..167093: 230 instructions, 0 samples |
| paint_queue_FillClippedSpan | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 | 0 | 167255..167612: 92 instructions, 0 samples |
| paint_queue_KeepPaintBytes | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 | 0 | none |
| paint_queue_KeepMeshPoints | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 | 0 | 168596..169151: 127 instructions, 0 samples |
| widget_benchmark_host_RasterRoundedRectangleOutline | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 37/0/0/0/0/9/0/6/6/1/16/0/0/1/0 | 0 | none |
| theme_DefaultTheme | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 | 3 | 170477..170496: 4 instructions, 2 samples |
| widget_benchmark_host_RasterTextRotated | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 79/21/2/3/0/23/6/10/8/1/15/0/0/1/1 | 0 | none |
| text_emphasis_RunTextItalicRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterTextClipped | 38/0/0/5/3/11/0/3/3/4/8/0/0/0/0 | 61/21/2/0/0/18/6/8/6/1/7/0/0/1/1 | 0 | none |
| text_emphasis_RunTextEmphasisRaster | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterText | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 47/21/2/0/0/15/6/6/4/0/0/0/0/1/1 | 0 | 171230..171275: 12 instructions, 0 samples |
| widget_benchmark_host_RasterLine | 15/0/0/3/2/8/0/0/0/1/0/0/0/0/0 | 19/0/0/0/0/4/0/2/2/1/10/0/0/1/0 | 0 | none |
| button_ButtonFaceFinish | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 | 0 | 171368..171526: 31 instructions, 0 samples |
| button_ButtonDefaultFace | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 | 0 | 171995..172982: 191 instructions, 0 samples |
| button_ButtonFaceFacts | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 | 0 | 173027..173184: 41 instructions, 0 samples |
| button_ButtonDetailFont | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 | 0 | none |
| style_StyleLineHeightValue | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 | 0 | none |
| style_FitHeight | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 | 0 | 173479..173641: 45 instructions, 0 samples |
| button_behavior_MeasureContent | 157/141/5/4/9/15/0/28/23/9/40/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 | 0 | 173726..174284: 136 instructions, 0 samples |
| button_behavior_ShapeWidth | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 | 0 | none |
| theme_ControlHeightFor | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 | 0 | 174620..174642: 4 instructions, 0 samples |
| button_behavior_ButtonCornerRadius | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 | 0 | none |
| focus_FocusDefaultOutlineBounds | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 | 0 | 174969..175181: 53 instructions, 0 samples |
| image_lookup_IconLookupImage | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 | 0 | 175467..175486: 4 instructions, 0 samples |
| button_behavior_ButtonPictureSize | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 | 0 | 175695..175837: 39 instructions, 0 samples |
| button_behavior_ButtonLabeledPictureSize | 91/52/4/0/0/0/0/10/6/7/27/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 | 0 | 176013..176184: 49 instructions, 0 samples |
| button_behavior_ContentLayout | 224/168/5/37/26/72/0/26/21/9/92/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 | 0 | 176448..177140: 163 instructions, 0 samples |
| svg_icon_behavior_PaintSvgIcon | 346/293/4/71/57/143/3/27/23/22/88/4/1/0/36 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 | 0 | 177385..178667: 289 instructions, 0 samples |
| image_lookup_ImageLookupTexture | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 | 0 | 178936..178958: 4 instructions, 0 samples |
| image_behavior_ImageDrawPlanFor | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 | 0 | 179473..179495: 4 instructions, 0 samples |
| paint_queue_PaintImage | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 77/0/0/14/30/49/0/0/0/2/31/0/0/0/0 | 0 | none |
| contextmenu_ContextMenuLevelBounds | 442/405/22/88/57/181/12/48/26/33/59/11/0/0/41 | 267/183/14/44/15/86/6/33/19/22/50/1/0/0/34 | 0 | 180264..181472: 276 instructions, 0 samples |
| dpi_ScalePixels | 83/72/1/0/0/0/0/10/9/8/21/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 | 0 | 182097..182372: 72 instructions, 0 samples |
| contextmenu_ContextMenuRowTop | 113/93/7/21/13/42/4/18/11/9/31/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 | 0 | 182459..182852: 91 instructions, 0 samples |
| overlay_OverlayScrimKey | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| overlay_behavior_DismissibleOverlayPolicyFor | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/0/0/0/3/0 | 0 | none |
| tree_input_TreeTakeDismissalAt | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 | 0 | none |
| tree_TreeSetPopup | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 | 0 | none |
| drawing_props_StyleTokenColorOr | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | 183864..184017: 42 instructions, 0 samples |
| theme_CurrentSurface | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentBorder | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentDisabled | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| theme_CurrentSelection | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 | 0 | none |
| tree_TreePushScope | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 | 0 | none |
| contextmenu_ContextMenuTextLeft | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 | 0 | 184918..184931: 6 instructions, 0 samples |
| contextmenu_ContextMenuLevelChecks | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 30/16/4/0/0/6/1/7/3/0/0/0/0/0/5 | 0 | 184946..185109: 39 instructions, 0 samples |
| contextmenu_ContextMenuInlineWidth | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 | 0 | 185359..185368: 5 instructions, 0 samples |
| contextmenu_ContextMenuInlineEnds | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 | 0 | none |
| svg_icon_behavior_PaintStroke | 635/588/8/74/327/438/14/31/23/21/71/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 | 0 | 185908..188733: 554 instructions, 0 samples |
| tree_End | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleAny | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| widget_benchmark_host_RecordBounds | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 | 0 | none |
| paint_queue_ShapeCornerInset | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 70/0/0/0/0/2/0/4/4/0/65/0/0/0/0 | 0 | none |
| paint_queue_KeepShortPaintBytes | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 189250..189474: 58 instructions, 0 samples |
| paint_queue_KeepMediumPaintBytes | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 | 0 | 189567..189801: 60 instructions, 0 samples |
| paint_queue_KeepLongPaintBytes | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 | 0 | 189894..190098: 52 instructions, 0 samples |
| theme_DarkPalette | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 0 | 190456..190475: 4 instructions, 0 samples |
| theme_LightPalette | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 | 9 | 190925..190944: 4 instructions, 5 samples |
| theme_DefaultMetrics | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 | 0 | 191443..191462: 4 instructions, 0 samples |
| theme_DefaultShadows | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 | 2 | 192129..192145: 4 instructions, 2 samples |
| theme_DefaultMotion | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 | 0 | none |
| theme_DefaultFonts | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 | 1 | 192601..192617: 4 instructions, 1 samples |
| style_sheet_StyleKindButton | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindModal | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| style_sheet_StyleKindSegment | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| theme_StateLayer | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 | 0 | 192685..193346: 200 instructions, 0 samples |
| style_sheet_StyleKindCard | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| card_behavior_CardDefaultFace | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 | 0 | 193876..193895: 4 instructions, 0 samples |
| theme_ShapeMedium | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| theme_FontForRole | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 | 0 | none |
| theme_MotionDuration | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 | 0 | 194601..194748: 40 instructions, 0 samples |
| button_behavior_ButtonRoleFactsFor | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 41/0/0/5/0/5/0/1/1/2/0/0/0/0/0 | 0 | 194808..194887: 22 instructions, 0 samples |
| drawing_props_PaintPixelScale | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | 0 | none |
| svg_icon_behavior_SvgIconSlot | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 | 0 | 195003..195412: 97 instructions, 0 samples |
| svg_icon_behavior_SvgIconSnap | 76/29/2/0/0/0/0/11/9/6/21/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 | 0 | 195932..196039: 28 instructions, 0 samples |
| svg_icon_behavior_SvgIconTexture | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 | 0 | 196209..196411: 54 instructions, 0 samples |
| image_behavior_ImageFitRect | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 46/0/0/5/0/8/0/9/9/0/32/0/0/0/0 | 0 | none |
| paint_queue_PaintImageWithPolygon | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 | 0 | 197647..197666: 4 instructions, 0 samples |
| svg_icon_behavior_StrokeAppendByte | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 | 0 | none |
| svg_icon_behavior_StrokeAppendNumber | 228/194/7/10/16/40/7/36/29/13/9/0/6/0/35 | 98/25/2/1/4/25/10/9/7/1/6/4/0/5/4 | 0 | 198331..198979: 193 instructions, 0 samples |
| tree_TreePopScope | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 52/10/1/0/0/6/4/6/5/4/0/4/0/0/2 | 0 | none |
| paint_queue_ShapeSquareRoot | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 56/0/0/0/0/2/0/1/1/0/54/0/0/0/0 | 0 | 199463..199569: 30 instructions, 0 samples |
| paint_queue___zi_spec_9308732530dd822f | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 1 | 199687..201656: 410 instructions, 1 samples |
| paint_queue___zi_spec_27099373fae5fe98 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 201785..201832: 14 instructions, 0 samples |
| paint_queue___zi_spec_27a25ba88f17713a | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 202134..202990: 186 instructions, 0 samples |
| paint_queue___zi_spec_17277838aa4fe0e7 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 203119..203166: 14 instructions, 0 samples |
| paint_queue___zi_spec_542e1e54e11043bf | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 | 0 | 203468..204324: 186 instructions, 0 samples |
| paint_queue___zi_spec_b997e17bf3928592 | 64/14/1/9/7/26/1/8/7/2/0/0/0/0/2 | 75/6/1/0/0/7/2/15/14/1/0/0/0/2/1 | 0 | 204453..204500: 14 instructions, 0 samples |
| theme_ShapeSurface | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 | 0 | none |
| drawing_props_paint_pixel_scale.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| drawing_props_paint_device_density.initializer | 5/0/0/0/0/2/0/0/0/0/2/0/0/0/0 | absent (inline/dead/renamed unknown) | 0 | none |
| svg_icon_behavior_SvgIconHash | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 46/22/2/0/0/8/7/6/4/0/0/5/0/0/1 | 0 | 204688..204746: 15 instructions, 0 samples |
| svg_icon_behavior_SvgIconRetire | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeRaster | 260/218/10/32/17/69/6/50/40/14/14/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 | 0 | 205269..206107: 205 instructions, 0 samples |
| svg_icon_behavior_SvgIconCells | 339/305/29/30/29/92/9/61/32/9/0/8/1/2/41 | 375/321/22/23/20/119/48/57/35/1/32/4/0/4/51 | 0 | 206362..207344: 256 instructions, 0 samples |
| image_behavior_ImageFitBounds | 198/179/8/18/9/31/0/26/18/13/47/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 | 0 | 207502..208172: 170 instructions, 0 samples |
| image_behavior_ImagePolygonValid | 103/87/3/9/1/12/1/19/16/7/12/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 | 0 | 208288..208618: 87 instructions, 0 samples |
| paint_queue_EmitImage | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 | 0 | 208641..208663: 4 instructions, 0 samples |
| paint_queue_ClipImage | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 | 0 | 209072..209094: 4 instructions, 0 samples |
| svg_shape_ShapeNumber | 349/308/20/55/19/93/15/60/40/19/54/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 | 0 | 209311..210474: 279 instructions, 0 samples |
| svg_shape_ShapeBuildPath | 564/542/41/129/90/238/7/87/46/35/156/0/0/0/51 | 669/640/30/61/31/225/53/103/72/15/142/0/0/3/94 | 0 | 210687..212844: 477 instructions, 0 samples |
| svg_shape_ShapeCoverEdges | 346/326/22/25/21/75/15/63/41/14/57/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 | 0 | 213267..214615: 325 instructions, 0 samples |
| paint_queue_EmitImageBegin | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 | 0 | none |
| widget_benchmark_host_RasterImage | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 | 0 | none |
| paint_queue_EmitImageEnd | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsDigit | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 0 | none |
| svg_shape_ShapeIsSpace | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 | 0 | none |
| svg_shape_ShapeFinishSubpath | 14/0/0/0/0/2/0/2/2/2/0/0/0/0/0 | 55/44/4/0/0/18/8/9/5/1/9/0/0/1/11 | 0 | none |
| svg_shape_ShapeAddPoint | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddSpan | 140/126/5/0/0/16/4/18/13/11/38/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 | 0 | 215603..216084: 123 instructions, 0 samples |
| svg_shape_ShapeStrokeSubpath | 612/530/15/90/49/179/20/64/49/38/268/0/3/0/61 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 | 0 | 216473..218821: 512 instructions, 0 samples |
| svg_shape_ShapeFillSubpath | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 | 0 | 218945..219068: 32 instructions, 0 samples |
| svg_shape_ShapeCollapse | 68/64/3/0/0/16/8/8/5/2/17/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 | 0 | 219106..219329: 52 instructions, 0 samples |
| svg_shape_ShapeAddCircle | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 | 0 | 219408..219614: 52 instructions, 0 samples |
| svg_shape_ShapeSqrt | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 104/0/0/0/0/2/0/1/1/0/102/0/0/0/0 | 0 | 219717..219823: 30 instructions, 0 samples |
| svg_shape_ShapeQuad | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 | 0 | none |
| svg_shape_ShapeAbs | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 | 0 | none |
| svg_shape_ShapeTriangle | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 | 0 | none |
| svg_shape_ShapeAddEdge | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 | 0 | none |
| svg_shape_circle_x.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 220442..220458: 4 instructions, 0 samples |
| svg_shape_circle_y.initializer | 45/4/1/1/16/19/2/1/0/0/27/0/0/0/1 | absent (inline/dead/renamed unknown) | 0 | 220664..220680: 4 instructions, 0 samples |
| svg_shape_ShapeAddPolygon | 110/97/8/4/5/27/12/22/14/4/24/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 | 0 | 220735..221090: 89 instructions, 0 samples |

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
| Records/cc/wasm_tier_Records | 82800 | 73/73 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 407/144/21/4/260/290/5/45/24/4/0/4/2/0/29 | 70/57/4/0/0/20/2/6/2/0/0/2/0/2/11 |
| Records/clang-19/wasm_tier_Records | 82063 | 78/78 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 407/144/21/4/260/290/5/45/24/4/0/4/2/0/29 | 73/61/5/12/4/25/20/9/4/0/0/2/0/2/15 |
| Sieve/cc/wasm_tier_Sieve | 62222 | 120/120 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 34/19/3/0/0/7/6/6/3/1/0/1/0/0/9 |
| Sieve/clang-19/wasm_tier_Sieve | 60000 | 119/119 100% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/53/14/2/2/24/7/22/8/5/6/1/0/0/13 | 36/20/4/1/0/4/2/6/2/1/0/1/0/0/6 |
| Hash/cc/wasm_tier_Hash | 58064 | 149/149 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 26/12/2/0/0/5/1/2/0/0/0/4/0/1/4 |
| Hash/clang-19/wasm_tier_Hash | 37096 | 154/154 100% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 62/56/10/0/0/3/2/18/8/3/0/1/1/0/13 | 39/26/2/0/0/9/7/2/0/0/0/8/0/1/3 |
| Vectors/cc/wasm_tier_Vectors | 28003 | 133/153 86% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/137/15/22/23/47/2/31/16/3/0/1/0/1/15 | 101/90/7/7/8/24/5/16/9/2/7/0/0/0/9 |
| Button/clang-19/session_SessionIndex | 17916 | 7/30 23% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Vectors/clang-19/wasm_tier_Vectors | 15510 | 136/153 88% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/137/15/22/23/47/2/31/16/3/0/1/0/1/15 | 123/101/10/3/3/22/15/17/7/2/26/1/0/1/13 |
| Input/cc/session_SessionIndex | 13300 | 12/58 20% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Input/clang-19/theme_LightPalette | 13049 | 9/56 16% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/session_SessionIndex | 12442 | 6/31 19% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Text/cc/std_utf8_Decode | 11273 | 7/44 15% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Input/clang-19/session_SessionIndex | 9598 | 7/56 12% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Text/cc/session_SessionIndex | 7305 | 5/44 11% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 20/11/2/0/0/5/1/4/2/0/0/0/0/1/1 |
| Text/cc/paint_queue___zi_spec_9308732530dd822f | 5589 | 4/44 9% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Text/clang-19/std_utf8_Decode | 4504 | 3/43 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Text/clang-19/text_layout_TextNoBreakBefore | 4199 | 2/43 4% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 43/22/2/0/0/5/1/10/7/0/0/0/0/0/7 |
| Button/clang-19/widget_benchmarks_ButtonCase | 3974 | 2/30 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 359/8/2/48/142/207/5/26/24/25/48/0/0/0/2 | 145/0/0/5/5/38/0/13/13/14/8/0/0/0/0 |
| Input/clang-19/theme_CurrentTheme | 3826 | 3/56 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/session_SessionIndex | 3571 | 2/43 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/6/0/10/1/11/11/5/0/0/0/1/0 | 13/0/0/0/0/4/1/2/2/0/0/0/0/0/0 |
| Text/clang-19/text_layout_NextTextLineUncached | 3460 | 3/43 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 375/334/6/115/53/189/6/42/36/22/18/0/0/0/22 | 189/62/5/29/16/59/2/24/19/6/11/0/0/4/7 |
| Input/cc/theme_DefaultShadows | 3422 | 3/58 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Text/cc/text_layout_TextIdeographic | 3397 | 2/44 4% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 24/8/2/0/0/6/0/4/2/0/0/0/0/0/2 |
| Text/clang-19/std_math_SineDegrees | 2876 | 2/43 4% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 207/114/4/0/0/0/0/21/17/12/66/0/0/0/23 | 79/0/0/0/0/23/0/12/12/1/62/0/0/0/0 |
| Input/cc/theme_LightPalette | 2799 | 2/58 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/paint_queue_PendingPaintAt | 2717 | 2/43 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Button/clang-19/theme_LightPalette | 2706 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/paint_queue_QueuePaint | 2705 | 3/43 6% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Input/clang-19/theme_DefaultShadows | 2604 | 2/56 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphWidth | 2511 | 2/44 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 |
| Input/clang-19/paint_queue_PaintRoundedFill | 2457 | 2/56 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Button/cc/theme_FontForRole | 2436 | 1/31 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Button/cc/paint_queue_PaintClippedLabel | 2414 | 1/31 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Input/clang-19/paint_queue_ShapeWhole | 2318 | 2/56 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Button/cc/raster_text_PaintTextClipped | 2304 | 1/31 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Text/clang-19/text_layout_TextTokenNext | 2265 | 3/43 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 210/116/20/4/2/22/7/53/33/3/0/0/0/3/34 |
| Button/cc/theme_DefaultMotion | 2258 | 1/31 3% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Input/cc/theme_DefaultFonts | 2227 | 2/58 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_StyleMinHeightValue | 2217 | 1/31 3% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphWidth | 2159 | 1/43 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_PaintTextLines | 2029 | 3/43 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 691/609/7/184/149/360/10/49/42/45/83/0/0/0/42 | 490/418/9/71/66/235/5/41/32/23/164/0/0/10/38 |
| Button/cc/button_PaintButtonForNode | 1931 | 2/31 6% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1316/1279/16/257/184/508/12/135/119/93/320/0/1/0/133 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 |
| Text/clang-19/text_TextOrigin | 1921 | 1/43 2% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 23/0/0/2/3/14/0/0/0/0/9/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Text/cc/text_TextOrigin | 1877 | 1/44 2% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 23/0/0/2/3/14/0/0/0/0/9/0/0/0/0 | 4/0/0/2/0/2/0/0/0/0/3/0/0/0/0 |
| Button/clang-19/button_ButtonDetailSize | 1862 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 |
| Text/clang-19/paint_queue_PaintClear | 1855 | 1/43 2% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Button/cc/style_StyleFontValue | 1851 | 1/31 3% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Input/clang-19/tree_TreeRead | 1849 | 2/56 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Button/clang-19/surface_Opacity | 1845 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Text/cc/theme_LightPalette | 1845 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/widget_benchmarks_ButtonCase | 1806 | 1/31 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 359/8/2/48/142/207/5/26/24/25/48/0/0/0/2 | 158/147/9/4/10/45/4/19/10/12/8/0/0/0/36 |
| Button/cc/paint_queue_EmitPaintDirect | 1774 | 1/31 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Input/cc/textfield_TextFieldCore | 1774 | 3/58 5% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3691/3557/51/746/1040/2101/26/399/348/251/250/1/1/0/263 | 2425/2344/104/678/397/1177/25/259/140/150/359/2/0/15/320 |
| Text/clang-19/text_layout_TextIdeographic | 1762 | 1/43 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 95/0/0/0/0/0/0/27/27/6/0/0/0/0/0 | 23/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Text/cc/paint_queue_PaintClippedLabel | 1701 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Text/clang-19/theme_DefaultShadows | 1696 | 1/43 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Button/clang-19/tree_TreeModalLayer | 1666 | 1/30 3% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/drawing_props_ColorFromPacked | 1666 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextIsSpace | 1660 | 1/44 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_ResolvePaintText | 1657 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Button/cc/paint_queue_PaintOverflow | 1612 | 1/31 3% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintClippedLabel | 1541 | 1/43 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Text/cc/tree_TreeNodeAt | 1515 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/cc/text_layout_TextLineXFor | 1515 | 1/44 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 15/0/0/0/0/1/1/1/1/0/0/0/0/2/0 |
| Button/clang-19/paint_queue_KeepPaintMesh | 1505 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Text/clang-19/text_layout_TextLayoutTotalHeight | 1495 | 1/43 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 15/0/0/0/0/1/0/1/1/0/0/2/0/0/0 |
| Input/clang-19/textfield_TextFieldCore | 1486 | 2/56 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 3691/3557/51/746/1040/2101/26/399/348/251/250/1/1/0/263 | 2155/1176/35/302/166/909/19/256/221/148/299/1/0/13/120 |
| Button/clang-19/tree_TreeSetSelected | 1416 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/cc/paint_queue_KeepPaintMesh | 1387 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Input/clang-19/paint_queue_QueuePaint | 1384 | 2/56 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Input/cc/theme_DefaultMetrics | 1365 | 1/58 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Input/cc/paint_queue_PaintLine | 1345 | 1/58 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 |
| Input/cc/paint_queue_PaintRoundedFill | 1338 | 1/58 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Input/cc/style_sheet_StyleDefaultFacts | 1313 | 1/58 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_QueuePaint | 1292 | 1/30 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 461/88/7/22/11/150/11/79/72/28/71/3/0/7/15 |
| Button/clang-19/button_SubmitButton | 1287 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 619/598/18/167/121/310/20/59/41/56/51/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 |
| Button/cc/paint_queue_QueuePaint | 1280 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Button/clang-19/tree_draw_BeginFrame | 1276 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Button/clang-19/button_Draw | 1269 | 1/30 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Button/clang-19/tree_TreeEnsureSemantic | 1255 | 1/30 3% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Input/clang-19/paint_queue___zi_spec_9308732530dd822f | 1246 | 1/56 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/cc/theme_CurrentTheme | 1231 | 1/58 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/std_math_SineDegrees | 1229 | 1/44 2% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 207/114/4/0/0/0/0/21/17/12/66/0/0/0/23 | 95/49/3/0/0/28/2/16/11/0/55/0/0/0/0 |
| Text/cc/geometry_RotatedRectangleBounds | 1214 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 174/110/8/34/48/86/1/16/8/6/79/0/0/0/6 | 81/32/1/8/14/25/0/1/0/2/56/0/0/3/4 |
| Input/clang-19/text_input_TextInputContentWidth | 1190 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 14/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Input/clang-19/paint_queue_ShapeVisible | 1190 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Text/clang-19/tree_TreeFinish | 1179 | 1/43 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Input/clang-19/theme_DefaultFonts | 1153 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Input/cc/paint_queue_PendingPaintAt | 1149 | 2/58 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Input/cc/tree_TreeFindOld | 1099 | 2/58 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Input/clang-19/text_input_TextInputScaledMetric | 1098 | 1/56 1% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 52/38/1/0/0/0/0/7/6/5/9/0/0/0/8 | 20/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Button/cc/tree_TreeInputAt | 1082 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Button/cc/widget_benchmark_host_WidgetPaintChecksum | 1075 | 1/31 3% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/paint_queue_PendingPaintAt | 1075 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Input/clang-19/tree_input_PointerSample | 1065 | 1/56 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Text/clang-19/text_PrepareText | 1063 | 1/43 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 247/81/2/54/57/153/0/13/11/15/16/0/0/0/2 | 134/18/1/13/2/44/0/6/5/14/18/0/0/3/3 |
| Input/cc/paint_queue___zi_spec_9308732530dd822f | 1060 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Input/clang-19/paint_queue_PendingPaintAt | 1043 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Input/clang-19/tree_input_TreeHitAt | 1035 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Button/cc/tree_input_SecondaryPointerSample | 1029 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Input/cc/text_input_TextInputContentWidth | 985 | 1/58 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 42/5/1/1/1/3/1/5/4/4/5/0/0/0/0 | 18/0/0/0/0/2/0/3/1/0/4/0/0/0/0 |
| Button/clang-19/tree_TreeBuildOldIndex | 970 | 1/30 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Text/cc/tree_TreeSetSemantic | 965 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 |
| Button/cc/button_SubmitButton | 948 | 1/31 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 619/598/18/167/121/310/20/59/41/56/51/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 |
| Text/cc/widget_benchmarks_TextCase | 924 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 236/158/9/19/59/102/3/30/21/16/29/0/0/0/27 | 140/128/10/16/20/66/4/16/6/7/32/0/0/1/29 |
| Text/cc/tree_TreeSetStyleClass | 909 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Text/cc/paint_queue_QueuePaint | 902 | 1/44 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Input/clang-19/tree_TreeIsBuilding | 892 | 1/56 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/paint_queue_ResolvePaintText | 886 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Input/clang-19/textfield_Draw | 877 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 117/0/0/14/4/41/2/10/10/9/2/0/0/0/0 |
| Button/clang-19/paint_queue_PaintPointCount | 869 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Input/clang-19/paint_queue_EmitPaintDirect | 863 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Input/cc/tree_TreeIsBuilding | 862 | 1/58 1% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/tree_TreeSubmit | 810 | 2/58 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Text/cc/text_TextFaceFor | 808 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 59/16/4/20/17/41/8/4/0/1/8/0/0/0/4 | 38/0/0/9/5/17/0/0/0/1/10/0/0/1/0 |
| Input/clang-19/paint_queue_KeepPaintMesh | 806 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Button/clang-19/tree_TreeFindOld | 770 | 1/30 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Input/cc/paint_queue_KeepPaintMesh | 741 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Text/clang-19/tree_draw_FrameInputCommit | 715 | 1/43 2% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangle | 691 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Text/cc/text_PrepareText | 690 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 247/81/2/54/57/153/0/13/11/15/16/0/0/0/2 | 172/91/3/37/23/69/2/7/4/14/32/0/0/3/4 |
| Input/cc/paint_queue_QueuePaint | 684 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 753/718/15/93/89/386/25/71/56/47/104/5/0/1/78 | 454/433/24/40/24/174/15/72/44/19/74/0/0/9/56 |
| Button/cc/tree_TreeLayerAt | 680 | 1/31 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Input/clang-19/theme_CurrentBorderStrong | 669 | 1/56 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/geometry_RotatedRectangleBounds | 668 | 1/43 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/110/8/34/48/86/1/16/8/6/79/0/0/0/6 | 124/0/0/28/18/48/0/0/0/10/111/0/0/0/0 |
| Input/clang-19/paint_queue_PendingTextValue | 652 | 1/56 1% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Button/cc/tree_TreeExplicitExists | 631 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 78/21/2/2/2/18/7/5/3/2/0/3/0/3/2 |
| Input/cc/surface_Opacity | 615 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Text/clang-19/paint_queue_PaintPointCount | 606 | 1/43 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Text/cc/tree_TreeStoreLabel | 603 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Text/cc/tree_TreeBuildOldIndex | 600 | 1/44 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 192/153/11/6/6/53/12/32/21/4/0/7/0/4/25 |
| Input/clang-19/tree_TreeLayerAt | 573 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Input/clang-19/tree_input_TreeTakePointerActivationAt | 571 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 |
| Input/cc/textfield_TextFieldFaceFor | 570 | 2/58 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 145/134/4/24/61/91/4/9/5/8/28/0/0/0/10 | 121/74/1/30/15/63/1/2/1/10/52/0/0/0/4 |
| Button/cc/button_behavior_ButtonRoleFactsFor | 552 | 1/31 3% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 |
| Text/clang-19/widget_benchmark_host_MeasureGlyphWidth | 550 | 2/43 4% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Text/cc/tree_TreeSubmit | 533 | 1/44 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Input/clang-19/tree_TreeBuildOldIndex | 519 | 1/56 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Text/clang-19/tree_TreeBuildNewIndex | 501 | 1/43 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Text/cc/paint_queue_PaintPointCount | 494 | 1/44 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Input/cc/theme_DefaultTheme | 468 | 4/58 6% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Input/cc/tree_TreeBuildOldIndex | 449 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 |
| Input/cc/paint_queue_PaintFlush | 445 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Text/cc/tree_TreeEnsureSemantic | 420 | 1/44 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Input/clang-19/tree_TreeFindOld | 412 | 1/56 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Input/clang-19/tree_TreeBuildNewIndex | 384 | 1/56 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Text/cc/text_layout_TextTokenNext | 380 | 1/44 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 311/288/12/48/29/92/6/60/48/27/0/0/0/0/45 | 259/237/31/6/4/46/10/62/31/6/0/0/0/4/53 |
| Input/cc/text_input_TextFieldScrollFor | 375 | 1/58 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 61/0/0/0/0/5/2/6/2/0/7/0/0/2/0 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangle | 369 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Text/clang-19/tree_TreeExplicitExists | 359 | 1/43 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Button/clang-19/theme_DefaultTheme | 355 | 2/30 6% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Text/clang-19/text_DrawSession | 333 | 1/43 2% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 342/141/9/96/82/193/14/30/21/29/24/0/0/0/21 | 293/19/2/36/34/100/0/25/23/20/108/1/0/2/2 |
| Input/cc/tree_TreeBuildNewIndex | 289 | 1/58 1% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Input/clang-19/theme_DefaultTheme | 286 | 3/56 5% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Text/clang-19/theme_DefaultTheme | 248 | 2/43 4% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 195/4/1/0/0/115/0/7/6/1/101/0/0/0/0 |
| Text/clang-19/text_PaintTextLineRuns | 242 | 1/43 2% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/316/7/110/106/235/12/16/9/13/30/0/0/0/14 | 301/250/14/75/45/157/7/25/11/16/80/2/0/1/20 |
| Button/cc/paint_queue_KeepShortPaintBytes | 241 | 1/31 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Button/clang-19/tree_TreeStoreLabel | 214 | 1/30 3% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Text/cc/geometry_WorldToScreen | 197 | 1/44 2% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/0/0/8/5/22/0/0/0/2/30/0/0/0/0 | 42/0/0/8/3/11/0/0/0/2/35/0/0/0/0 |
| Input/cc/textfield_TextFieldDefaultFace | 194 | 1/58 1% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 71/4/1/4/39/46/2/1/0/6/25/0/0/0/1 | 63/0/0/10/13/35/0/0/0/6/26/0/0/0/0 |
| Text/cc/paint_queue_KeepShortPaintBytes | 170 | 1/44 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Text/cc/theme_DefaultTheme | 154 | 1/44 2% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Calls/clang-19/wasm_tier_Fib | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/4/2/6/0/1/1/3/0/0/0/0/0 | 21/7/1/0/0/2/0/2/1/1/0/0/0/0/1 |
| Printing/cc/wasm_tier_Printing | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 31/14/1/0/0/4/0/1/0/1/0/2/0/0/3 |
| Printing/clang-19/wasm_tier_Printing | 0 | 0/0 unknown | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 63/52/3/5/5/12/0/8/5/7/0/1/1/0/9 | 43/26/2/0/0/7/4/4/2/1/0/1/0/3/3 |
| Button/cc/widget_benchmarks_BeginCase | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Button/cc/tree_draw_BeginFrame | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Button/cc/button_Draw | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Button/cc/tree_draw_EndFrame | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Button/cc/tree_TreeCount | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeNodeAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/cc/tree_input_TreePointerUpdate | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 |
| Button/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/31 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_WidgetPaintCount | 0 | 0/31 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/text_request_TextRequestSend | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Button/cc/text_request_TextRequestClear | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Button/cc/std_grapheme_Boundary | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Button/cc/session_SessionOpen | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Button/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/31 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_PointerSample | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Button/cc/session_SessionFlagSet | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Button/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Button/cc/tree_input_TreeKeyboardRoute | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Button/cc/text_editing_TextEditingFrameBegin | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_PaintClear | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Button/cc/tree_TreeStart | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Button/cc/tree_TreeFrameStatus | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/cc/tree_TreeCurrentParent | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Button/cc/session_SessionFlagGet | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/tree_draw_CommitFrame | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Button/cc/paint_queue_PaintSnapshotEmit | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Button/cc/paint_queue_PaintFlush | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Button/cc/tree_TreeRead | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Button/cc/tree_TreeIsBuilding | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/tree_input_TreeHitAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 |
| Button/cc/tree_input_TreeTextFocused | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 |
| Button/cc/tree_input_TreeTextKind | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_TreeFocusAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Button/cc/std_utf8_Valid | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Button/cc/tree_TreeTextRange | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Button/cc/tree_input_TreeFocusedAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Button/cc/geometry_RectangleIntersection | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Button/cc/tree_TreePointerActivatable | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Button/cc/tree_input_TreeActivateAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 |
| Button/cc/cursor_CursorHoverFrameStart | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Button/cc/tree_TreeBuildOldIndex | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 193/153/11/6/6/54/14/32/21/4/0/7/0/4/25 |
| Button/cc/tree_TreeSubmit | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 648/560/26/89/60/272/33/85/59/8/140/22/0/20/101 |
| Button/cc/tree_TreePrevious | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 |
| Button/cc/style_ResolveFlags | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Button/cc/style_ResolveState | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Button/cc/tree_input_TreePressedAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 |
| Button/cc/tree_input_TreeHoveredAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 |
| Button/cc/style_ResolveFont | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Button/cc/font_metrics_GlyphWidth | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/font_metrics_GlyphLineHeight | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/button_ButtonDetailSize | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 |
| Button/cc/tree_TreeRootBounds | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Button/cc/button_behavior_MeasureBounds | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 |
| Button/cc/button_ButtonMinimumHeight | 0 | 0/31 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/button_ButtonFitContentFace | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 371/290/13/41/19/86/4/61/48/25/83/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 |
| Button/cc/tree_TreeSetInteractive | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Button/cc/tree_TreeSetCircleHit | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 |
| Button/cc/tree_TreeSetSelected | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Button/cc/tree_TreeSetLabel | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeSetSemanticDetail | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Button/cc/tree_TreeSetStyleClass | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Button/cc/tree_TreeSetStyleId | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Button/cc/tree_draw_FrameInputCommit | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Button/cc/tree_TreeCancel | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Button/cc/tree_TreeFinish | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Button/cc/paint_queue_PaintOrder | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Button/cc/paint_queue_SnapshotTextRead | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_SnapshotTextJoin | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Button/cc/paint_queue_PaintPointCount | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Button/cc/input_protocol_PointInBounds | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 |
| Button/cc/tree_input_TreeTopModalLayer | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/cc/tree_TreeModalLayerAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/cc/tree_TreeButtonLike | 0 | 0/31 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/tree_input_PointInCircleBounds | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/cc/std_utf8_Decode | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Button/cc/tree_TreeParentGeneration | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Button/cc/tree_TreeIdentityHash | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/tree_TreeBuildNewIndex | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Button/cc/tree_TreeFindOld | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Button/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 |
| Button/cc/style_sheet_ResolveInstalledStyleRules | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 |
| Button/cc/button_ButtonDefaultFace | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 |
| Button/cc/button_ButtonFaceFacts | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 |
| Button/cc/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 33/16/2/0/0/4/2/5/3/0/0/0/0/2/5 |
| Button/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/31 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/button_ButtonDetailFont | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Button/cc/style_StyleLineHeightValue | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/cc/style_FitHeight | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 |
| Button/cc/button_behavior_MeasureContent | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 157/141/5/4/9/15/0/28/23/9/40/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 |
| Button/cc/button_behavior_ShapeWidth | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 |
| Button/cc/theme_ControlHeightFor | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 |
| Button/cc/tree_TreeEnsureInteraction | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 |
| Button/cc/tree_TreeEnsureSemantic | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Button/cc/tree_TreeStoreLabel | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Button/cc/style_StyleOpacityValue | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Button/cc/button_behavior_ButtonCornerRadius | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Button/cc/drawing_props_ColorFromPacked | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/surface_Opacity | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Button/cc/paint_queue_PaintRoundedFill | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Button/cc/paint_queue_PaintRoundedOutline | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 |
| Button/cc/focus_FocusDefaultOutlineBounds | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 |
| Button/cc/image_lookup_IconLookupImage | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 |
| Button/cc/button_behavior_ButtonPictureSize | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 |
| Button/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 91/52/4/0/0/0/0/10/6/7/27/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 |
| Button/cc/button_behavior_ContentLayout | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 224/168/5/37/26/72/0/26/21/9/92/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 |
| Button/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/293/4/71/57/143/3/27/23/22/88/4/1/0/36 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 |
| Button/cc/image_lookup_ImageLookupTexture | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 |
| Button/cc/image_behavior_ImageDrawPlanFor | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 |
| Button/cc/paint_queue_PaintImage | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 |
| Button/cc/paint_queue_PaintLine | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 36/0/0/0/9/13/0/0/0/1/2/0/0/0/0 |
| Button/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/cc/cursor_CursorHoverFrameCommit | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Button/cc/paint_queue_PaintMeshPointCount | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Button/cc/tree_TreeModalLayer | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/cc/std_utf8_Continuation | 0 | 0/31 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/std_grapheme_IsControl | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/session_WidgetPartKey | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/cc/widget_key_WidgetRepeatedPart | 0 | 0/31 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleBucket | 0 | 0/31 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleFirstBit | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 |
| Button/cc/style_sheet_StyleRuleMatchesState | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleSelectorMatches | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Button/cc/style_MergeValues | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 |
| Button/cc/theme_CurrentTheme | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Button/cc/theme_StateLayer | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 |
| Button/cc/card_behavior_CardDefaultFace | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 |
| Button/cc/theme_MotionDuration | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 |
| Button/cc/tree_TreeLabelSpanAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Button/cc/surface_Unit | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Button/cc/paint_queue_ClipShape | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Button/cc/drawing_props_PaintPixelScale | 0 | 0/31 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 |
| Button/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 76/29/2/0/0/0/0/11/9/6/21/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 |
| Button/cc/image_behavior_ImageFitRect | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Button/cc/paint_queue_PaintImageWithPolygon | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 |
| Button/cc/paint_queue_EmitLine | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Button/cc/paint_queue_EmitLabel | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Button/cc/paint_queue_ClipLabel | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Button/cc/paint_queue_PendingTextValue | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Button/cc/style_sheet_StyleMatchesOptional | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/theme_DefaultTheme | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 206/8/2/8/139/161/4/6/4/6/68/0/0/0/2 | 192/113/2/30/74/148/2/6/4/2/124/0/0/0/23 |
| Button/cc/style_sheet_StyleControlRoleFacts | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Button/cc/paint_queue_ShapeVisible | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Button/cc/paint_queue_ShapeWhole | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Button/cc/raster_shape_FillRoundedRectangle | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/paint_queue_FillShapeClipped | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Button/cc/paint_queue_EmitPaintNow | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Button/cc/tree_TreeCurrentPaintGeometry | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Button/cc/tree_TreeSubmittedGeometryAt | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Button/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_icon_behavior_SvgIconHash | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 |
| Button/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 |
| Button/cc/svg_shape_ShapeRaster | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 260/218/10/32/17/69/6/50/40/14/14/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 |
| Button/cc/svg_icon_behavior_SvgIconCells | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 339/305/29/30/29/92/9/61/32/9/0/8/1/2/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 |
| Button/cc/image_behavior_ImageFitBounds | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 198/179/8/18/9/31/0/26/18/13/47/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 |
| Button/cc/image_behavior_ImagePolygonValid | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 103/87/3/9/1/12/1/19/16/7/12/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 |
| Button/cc/paint_queue_EmitImage | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Button/cc/paint_queue_ClipImage | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Button/cc/raster_StrokeLine | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/text_typeface_TextTypefaceUse | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Button/cc/raster_text_PaintTextRotated | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Button/cc/raster_text_PaintTextItalicClipped | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Button/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Button/cc/raster_text_PaintText | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/theme_DarkPalette | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/theme_LightPalette | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/cc/theme_DefaultMetrics | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Button/cc/theme_DefaultShadows | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Button/cc/theme_DefaultFonts | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/cc/style_sheet_StyleControlFacts | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 |
| Button/cc/paint_queue_ShapeRadius | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Button/cc/paint_queue_FillShapeRows | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Button/cc/paint_queue_FillClippedSpan | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Button/cc/paint_queue_KeepPaintBytes | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Button/cc/paint_queue_KeepMeshPoints | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 |
| Button/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Button/cc/svg_shape_ShapeNumber | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 349/308/20/55/19/93/15/60/40/19/54/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 |
| Button/cc/svg_shape_ShapeBuildPath | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 564/542/41/129/90/238/7/87/46/35/156/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 |
| Button/cc/svg_shape_ShapeCoverEdges | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 346/326/22/25/21/75/15/63/41/14/57/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 |
| Button/cc/paint_queue_EmitImageBegin | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterImage | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 |
| Button/cc/paint_queue_EmitImageEnd | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Button/cc/text_emphasis_RunTextItalicRaster | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RasterText | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Button/cc/style_sheet_StyleDefaultFacts | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 10/0/0/0/0/7/0/0/0/0/0/0/0/0/0 |
| Button/cc/widget_benchmark_host_RecordBounds | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Button/cc/paint_queue_ShapeCornerInset | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Button/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Button/cc/paint_queue_KeepLongPaintBytes | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Button/cc/svg_shape_ShapeIsDigit | 0 | 0/31 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeIsSpace | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddPoint | 0 | 0/31 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddSpan | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 140/126/5/0/0/16/4/18/13/11/38/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 |
| Button/cc/paint_queue_ShapeSquareRoot | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Button/cc/paint_queue___zi_spec_9308732530dd822f | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Button/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Button/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Button/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 612/530/15/90/49/179/20/64/49/38/268/0/3/0/61 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 |
| Button/cc/svg_shape_ShapeFillSubpath | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Button/cc/svg_shape_ShapeCollapse | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 68/64/3/0/0/16/8/8/5/2/17/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 |
| Button/cc/svg_shape_ShapeAddCircle | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 |
| Button/cc/svg_shape_ShapeSqrt | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 |
| Button/cc/svg_shape_ShapeQuad | 0 | 0/31 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeAbs | 0 | 0/31 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Button/cc/svg_shape_ShapeTriangle | 0 | 0/31 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Button/cc/svg_shape_ShapeAddEdge | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Button/cc/svg_shape_ShapeAddPolygon | 0 | 0/31 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/97/8/4/5/27/12/22/14/4/24/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 |
| Button/clang-19/widget_benchmarks_BeginCase | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/tree_draw_EndFrame | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Button/clang-19/tree_TreeCount | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang-19/tree_TreeNodeAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang-19/tree_input_TreePointerUpdate | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 |
| Button/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/30 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintCount | 0 | 0/30 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/30 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/text_request_TextRequestSend | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Button/clang-19/text_request_TextRequestClear | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Button/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/30 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_PointerSample | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Button/clang-19/tree_input_SecondaryPointerSample | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Button/clang-19/session_SessionFlagSet | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 |
| Button/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_PaintClear | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Button/clang-19/tree_TreeStart | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Button/clang-19/tree_TreeFrameStatus | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Button/clang-19/tree_TreeCurrentParent | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Button/clang-19/session_SessionFlagGet | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang-19/tree_draw_CommitFrame | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Button/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Button/clang-19/tree_TreeRead | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Button/clang-19/tree_TreeIsBuilding | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Button/clang-19/tree_input_TreeHitAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 176/121/12/20/13/65/4/23/11/13/40/1/0/2/13 |
| Button/clang-19/tree_input_TreeTextFocused | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Button/clang-19/tree_input_TreeTextKind | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeFocusAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Button/clang-19/tree_TreeInputAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Button/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/tree_TreeTextRange | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Button/clang-19/tree_input_TreeFocusedAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Button/clang-19/geometry_RectangleIntersection | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Button/clang-19/tree_TreePointerActivatable | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Button/clang-19/tree_input_TreeActivateAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Button/clang-19/cursor_CursorHoverFrameStart | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Button/clang-19/tree_TreeSubmit | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Button/clang-19/tree_TreePrevious | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Button/clang-19/style_ResolveFlags | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Button/clang-19/tree_input_TreeTakeActivationAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Button/clang-19/style_ResolveState | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Button/clang-19/tree_input_TreeHoveredAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Button/clang-19/style_ResolveFont | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Button/clang-19/style_StyleFontValue | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Button/clang-19/font_metrics_GlyphWidth | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/font_metrics_GlyphLineHeight | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/tree_TreeRootBounds | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Button/clang-19/button_behavior_MeasureBounds | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 |
| Button/clang-19/style_StyleMinHeightValue | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang-19/button_ButtonMinimumHeight | 0 | 0/30 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/button_ButtonFitContentFace | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 371/290/13/41/19/86/4/61/48/25/83/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 |
| Button/clang-19/tree_TreeSetInteractive | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetCircleHit | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Button/clang-19/tree_TreeSetLabel | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetSemanticDetail | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetStyleClass | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/tree_TreeSetStyleId | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Button/clang-19/button_PaintButtonForNode | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1316/1279/16/257/184/508/12/135/119/93/320/0/1/0/133 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 |
| Button/clang-19/tree_draw_FrameInputCommit | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Button/clang-19/paint_queue_PaintOverflow | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Button/clang-19/tree_TreeCancel | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Button/clang-19/tree_TreeFinish | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Button/clang-19/paint_queue_PaintOrder | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Button/clang-19/paint_queue_SnapshotTextRead | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Button/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Button/clang-19/paint_queue_EmitPaint | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Button/clang-19/paint_queue_PendingPaintAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 106/67/3/0/0/41/7/12/9/7/0/2/0/2/11 |
| Button/clang-19/input_protocol_PointInBounds | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Button/clang-19/tree_input_TreeTopModalLayer | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Button/clang-19/tree_TreeButtonLike | 0 | 0/30 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/tree_input_PointInCircleBounds | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Button/clang-19/tree_TreeOverlayLayerAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/std_utf8_Decode | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Button/clang-19/std_grapheme_Push | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Button/clang-19/tree_TreeParentGeneration | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Button/clang-19/tree_TreeIdentityHash | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang-19/tree_TreeBuildNewIndex | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 182/157/9/2/2/47/17/33/24/5/0/4/0/5/24 |
| Button/clang-19/tree_TreeExplicitExists | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Button/clang-19/tree_input_TreeTakePointerActivationAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 102/18/2/6/0/16/4/17/15/6/5/3/0/2/5 |
| Button/clang-19/button_ButtonFaceFinish | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Button/clang-19/style_sheet_ResolveInstalledStyleRules | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 |
| Button/clang-19/button_ButtonDefaultFace | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Button/clang-19/button_ButtonFaceFacts | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Button/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/30 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/button_ButtonDetailFont | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Button/clang-19/style_StyleLineHeightValue | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Button/clang-19/style_FitHeight | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Button/clang-19/button_behavior_MeasureContent | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 157/141/5/4/9/15/0/28/23/9/40/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 |
| Button/clang-19/button_behavior_ShapeWidth | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Button/clang-19/theme_ControlHeightFor | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Button/clang-19/tree_TreeEnsureInteraction | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Button/clang-19/style_StyleOpacityValue | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonCornerRadius | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Button/clang-19/drawing_props_ColorFromPacked | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_PaintRoundedFill | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Button/clang-19/paint_queue_PaintRoundedOutline | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Button/clang-19/focus_FocusDefaultOutlineBounds | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Button/clang-19/image_lookup_IconLookupImage | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonPictureSize | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Button/clang-19/button_behavior_ButtonLabeledPictureSize | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 91/52/4/0/0/0/0/10/6/7/27/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Button/clang-19/button_behavior_ContentLayout | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 224/168/5/37/26/72/0/26/21/9/92/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_PaintSvgIcon | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 346/293/4/71/57/143/3/27/23/22/88/4/1/0/36 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 |
| Button/clang-19/image_lookup_ImageLookupTexture | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Button/clang-19/image_behavior_ImageDrawPlanFor | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Button/clang-19/paint_queue_PaintLine | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Button/clang-19/paint_queue_PaintClippedLabel | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Button/clang-19/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Button/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Button/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Button/clang-19/paint_queue_EmitPaintDirect | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Button/clang-19/paint_queue_ResolvePaintText | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/tree_TreeLayerAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Button/clang-19/std_utf8_Continuation | 0 | 0/30 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/std_grapheme_data_Property | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Button/clang-19/std_grapheme_IsControl | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/session_WidgetPartKey | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Button/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/30 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleBucket | 0 | 0/30 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleFirstBit | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Button/clang-19/style_sheet_StyleRuleMatchesState | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleSelectorMatches | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Button/clang-19/style_MergeValues | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Button/clang-19/theme_CurrentTheme | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Button/clang-19/theme_StateLayer | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Button/clang-19/card_behavior_CardDefaultFace | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 |
| Button/clang-19/theme_ShapeMedium | 0 | 0/30 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/theme_TypeLabel | 0 | 0/30 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/theme_FontForRole | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Button/clang-19/theme_MotionDuration | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Button/clang-19/tree_TreeLabelSpanAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Button/clang-19/surface_Unit | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Button/clang-19/paint_queue_ClipShape | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Button/clang-19/paint_queue_EmitRoundedOutline | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Button/clang-19/drawing_props_PaintPixelScale | 0 | 0/30 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconSlot | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 |
| Button/clang-19/svg_icon_behavior_SvgIconSnap | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 76/29/2/0/0/0/0/11/9/6/21/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconTexture | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 |
| Button/clang-19/paint_queue_PaintImageWithPolygon | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Button/clang-19/paint_queue_EmitLine | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Button/clang-19/paint_queue_EmitLabel | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Button/clang-19/paint_queue_ClipLabel | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Button/clang-19/paint_queue_PendingTextValue | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Button/clang-19/style_sheet_StyleMatchesOptional | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/theme_ShapeSurface | 0 | 0/30 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Button/clang-19/style_sheet_StyleControlRoleFacts | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_ShapeVisible | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Button/clang-19/paint_queue_ShapeWhole | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Button/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/paint_queue_FillShapeClipped | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Button/clang-19/paint_queue_EmitPaintNow | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Button/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Button/clang-19/tree_TreeSubmittedGeometryAt | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Button/clang-19/raster_shape_StrokeRoundedRectangle | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/svg_icon_behavior_SvgIconRetire | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeRaster | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 260/218/10/32/17/69/6/50/40/14/14/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 |
| Button/clang-19/image_behavior_ImageFitBounds | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 198/179/8/18/9/31/0/26/18/13/47/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 |
| Button/clang-19/image_behavior_ImagePolygonValid | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 103/87/3/9/1/12/1/19/16/7/12/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Button/clang-19/paint_queue_EmitImage | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Button/clang-19/paint_queue_ClipImage | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Button/clang-19/raster_StrokeLine | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/text_typeface_TextTypefaceUse | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/raster_text_PaintTextRotated | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Button/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Button/clang-19/raster_text_PaintTextClipped | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Button/clang-19/raster_text_PaintText | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Button/clang-19/theme_DarkPalette | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Button/clang-19/theme_DefaultMetrics | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Button/clang-19/theme_DefaultShadows | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 36/0/0/0/0/30/0/2/2/0/23/0/0/0/0 |
| Button/clang-19/theme_DefaultMotion | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Button/clang-19/theme_DefaultFonts | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleControlFacts | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Button/clang-19/paint_queue_ShapeRadius | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Button/clang-19/paint_queue_FillClippedSpan | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Button/clang-19/paint_queue_KeepPaintBytes | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Button/clang-19/paint_queue_KeepMeshPoints | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Button/clang-19/svg_shape_ShapeNumber | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 349/308/20/55/19/93/15/60/40/19/54/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 |
| Button/clang-19/svg_shape_ShapeCoverEdges | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/326/22/25/21/75/15/63/41/14/57/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 |
| Button/clang-19/paint_queue_EmitImageBegin | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RasterImage | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Button/clang-19/paint_queue_EmitImageEnd | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Button/clang-19/style_sheet_StyleDefaultFacts | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Button/clang-19/widget_benchmark_host_RecordBounds | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Button/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Button/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Button/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Button/clang-19/svg_shape_ShapeIsDigit | 0 | 0/30 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeIsSpace | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Button/clang-19/svg_shape_ShapeAddPoint | 0 | 0/30 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAddSpan | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 140/126/5/0/0/16/4/18/13/11/38/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 |
| Button/clang-19/paint_queue___zi_spec_9308732530dd822f | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Button/clang-19/svg_shape_ShapeStrokeSubpath | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 612/530/15/90/49/179/20/64/49/38/268/0/3/0/61 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 |
| Button/clang-19/svg_shape_ShapeFillSubpath | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 |
| Button/clang-19/svg_shape_ShapeCollapse | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 68/64/3/0/0/16/8/8/5/2/17/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Button/clang-19/svg_shape_ShapeAddCircle | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Button/clang-19/svg_shape_ShapeQuad | 0 | 0/30 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAbs | 0 | 0/30 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeTriangle | 0 | 0/30 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Button/clang-19/svg_shape_ShapeAddEdge | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Button/clang-19/svg_shape_ShapeAddPolygon | 0 | 0/30 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 110/97/8/4/5/27/12/22/14/4/24/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 |
| Text/cc/widget_benchmarks_BeginCase | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Text/cc/tree_draw_BeginFrame | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Text/cc/text___zi_spec_6a19f7c98e79a734 | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/6/6/12/2/1/0/1/2/0/0/0/1 | 12/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_draw_EndFrame | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Text/cc/tree_TreeCount | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/44 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/44 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_request_TextRequestSend | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Text/cc/text_request_TextRequestClear | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Text/cc/std_grapheme_Boundary | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Text/cc/session_SessionOpen | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Text/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/44 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/cc/tree_input_PointerSample | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Text/cc/tree_input_SecondaryPointerSample | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Text/cc/session_SessionFlagSet | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Text/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Text/cc/tree_input_TreeKeyboardRoute | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Text/cc/text_editing_TextEditingFrameBegin | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_PaintClear | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Text/cc/tree_TreeStart | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Text/cc/tree_TreeFrameStatus | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Text/cc/text_DrawSession | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 342/141/9/96/82/193/14/30/21/29/24/0/0/0/21 | 293/263/10/101/82/188/2/15/5/17/159/0/0/0/13 |
| Text/cc/session_SessionFlagGet | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/tree_draw_CommitFrame | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Text/cc/paint_queue_PaintSnapshotEmit | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Text/cc/paint_queue_PaintFlush | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/67/3/25/27/70/7/9/6/11/4/3/0/0/10 | 89/48/2/0/0/13/5/9/6/8/0/0/0/5/9 |
| Text/cc/tree_TreeRead | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Text/cc/std_utf8_Valid | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Text/cc/tree_TreeTextRange | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Text/cc/tree_input_TreeFocusAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Text/cc/tree_input_TreeFocusedAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Text/cc/tree_TreeInputAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Text/cc/geometry_RectangleIntersection | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Text/cc/tree_TreePointerActivatable | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Text/cc/tree_input_TreeActivateAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 44/20/1/5/0/9/1/2/1/3/0/0/0/1/1 |
| Text/cc/cursor_CursorHoverFrameStart | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Text/cc/std_math_Finite32 | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 11/0/0/0/0/3/0/2/2/0/4/0/0/0/0 |
| Text/cc/tree_TreeRootBounds | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Text/cc/tree_TreeIsBuilding | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeSetLabel | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Text/cc/tree_TreeSetStyleId | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Text/cc/text_TextHiddenLines | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 117/39/2/11/2/22/0/11/9/10/21/0/0/0/4 | 86/45/3/10/2/22/7/11/4/2/22/0/0/0/6 |
| Text/cc/tree_TreeSetHiddenLines | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Text/cc/text_PaintTextLines | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 691/609/7/184/149/360/10/49/42/45/83/0/0/0/42 | 585/494/12/195/126/348/8/37/15/15/222/0/0/8/37 |
| Text/cc/tree_draw_FrameInputCommit | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Text/cc/paint_queue_PaintOverflow | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/cc/tree_TreeCancel | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeFinish | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Text/cc/paint_queue_PaintOrder | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Text/cc/paint_queue_SnapshotTextRead | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_SnapshotTextJoin | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Text/cc/paint_queue_PendingPaintAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 255/28/7/24/90/168/21/14/7/7/36/5/0/0/7 | 170/108/7/16/21/75/8/17/10/4/0/1/0/4/12 |
| Text/cc/tree_input_TreeTopModalLayer | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_TreeModalLayerAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/cc/tree_TreeParentGeneration | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Text/cc/tree_TreeIdentityHash | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/tree_TreeBuildNewIndex | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 232/204/8/32/19/102/14/25/17/8/0/8/0/1/34 | 193/162/18/3/3/50/14/34/16/6/0/5/0/4/25 |
| Text/cc/tree_TreeExplicitExists | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 80/21/2/2/2/20/10/5/3/2/0/3/0/3/2 |
| Text/cc/tree_TreeFindOld | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 109/40/5/6/5/30/7/13/8/4/0/4/0/3/3 |
| Text/cc/text_DefaultTextFace | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 65/4/1/5/32/45/2/1/0/1/20/0/0/0/1 | 13/0/0/0/0/7/0/0/0/1/0/0/0/0/0 |
| Text/cc/text_behavior_ResolveTextStyle | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 127/99/1/14/14/30/0/23/22/9/8/0/0/2/18 | 79/58/6/8/3/19/2/14/6/1/11/0/0/0/3 |
| Text/cc/style_ResolveFont | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Text/cc/style_StyleFontValue | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Text/cc/drawing_props_ColorFromPacked | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/surface_Opacity | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 36/16/1/0/0/6/1/6/2/0/21/0/0/0/1 |
| Text/cc/style_StyleOpacityValue | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Text/cc/font_metrics_GlyphLineHeight | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_layout_TextDefaultLineGap | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 44/26/1/0/0/0/0/6/5/4/10/0/0/0/6 | 14/0/0/0/0/4/0/3/1/0/7/0/0/0/0 |
| Text/cc/text_behavior_TextWrapPolicy | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 21/0/0/0/0/0/0/1/1/1/2/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/text_layout_NextTextLine | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 566/502/26/67/179/311/14/67/41/26/18/12/1/0/55 | 264/212/25/12/8/101/33/43/17/3/11/2/0/14/30 |
| Text/cc/text_behavior_TextExtent | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 17/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/cc/text_layout_TextLayoutTotalHeight | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 42/24/1/0/0/0/0/5/4/3/0/2/0/0/4 | 13/0/0/0/0/0/0/2/2/0/0/2/0/0/0 |
| Text/cc/tree_TreeCurrentParent | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Text/cc/text_behavior_TextAlignmentOffset | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 31/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 18/0/0/0/0/3/1/2/2/0/9/0/0/0/0 |
| Text/cc/text_layout_TextCharactersEnd | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 31/8/1/0/0/1/0/4/3/1/0/0/0/0/1 |
| Text/cc/text_TextSpan | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/cc/font_metrics_GlyphWidth | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/text_TextEllipsisStart | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 49/33/3/2/2/8/2/6/3/1/0/0/0/0/5 |
| Text/cc/text_PaintTextLineRuns | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/316/7/110/106/235/12/16/9/13/30/0/0/0/14 | 280/230/5/89/52/150/5/9/4/11/101/2/0/4/22 |
| Text/cc/text_behavior_TextStrikethroughBounds | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 38/16/1/3/5/10/0/3/2/1/11/0/0/0/1 | 9/0/0/0/0/2/0/0/0/0/8/0/0/0/0 |
| Text/cc/text_PaintTextRule | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/4/1/69/73/148/2/2/1/10/46/0/0/0/1 | 131/0/0/27/26/54/0/4/2/6/72/0/0/0/0 |
| Text/cc/cursor_CursorHoverFrameCommit | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Text/cc/paint_queue_PaintMeshPointCount | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Text/cc/paint_queue_EmitPaintDirect | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Text/cc/paint_queue_ResolvePaintText | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 35/14/1/1/1/20/0/3/2/2/0/0/0/0/2 |
| Text/cc/tree_TreeModalLayer | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/cc/tree_TreeLayerAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Text/cc/std_utf8_Continuation | 0 | 0/44 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_grapheme_IsControl | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/session_WidgetPartKey | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/cc/widget_key_WidgetRepeatedPart | 0 | 0/44 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_style_TextStyleFor | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 27/0/0/4/2/12/1/1/1/1/8/0/0/0/0 |
| Text/cc/surface_Unit | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Text/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/44 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/font_metrics_GlyphMetricsRevision | 0 | 0/44 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_NextTextLineUncached | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 375/334/6/115/53/189/6/42/36/22/18/0/0/0/22 | 352/318/8/47/23/84/6/28/20/6/12/0/0/13/29 |
| Text/cc/text_layout_TextLineHash | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 19/13/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/2/0/2/1/0/0/1/0/0/1 |
| Text/cc/text_layout_TextLineCacheMatches | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 25/21/6/1/0/10/0/9/3/0/1/0/0/0/0 |
| Text/cc/std_math_CosineDegrees | 0 | 0/44 0% |  | 4/0/0/0/0/0/0/1/0/0/2/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/1/0/0/0/0 |
| Text/cc/tree_TreeLabelSpanAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Text/cc/text_TextLinkRunAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 119/32/2/37/18/67/1/12/10/4/0/0/0/0/3 | 75/52/2/4/0/11/7/11/9/0/0/0/0/3/7 |
| Text/cc/text_TextLinkColor | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 21/0/0/0/0/1/0/0/0/3/0/2/0/4/0 |
| Text/cc/paint_queue_EmitLabel | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Text/cc/paint_queue_ClipLabel | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Text/cc/text_TextRangeValid | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 32/13/3/0/0/3/2/4/1/0/0/0/0/2/3 |
| Text/cc/paint_queue_PaintClippedEmphasis | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 177/4/1/18/84/150/2/2/1/2/25/0/0/0/1 | 45/0/0/4/16/23/0/0/0/1/6/0/0/0/0 |
| Text/cc/paint_queue_PaintRoundedFill | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 38/0/0/0/11/15/0/0/0/1/3/0/0/0/0 |
| Text/cc/shape_paint_PaintTriangleShape | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 141/4/1/21/93/117/2/1/0/4/32/0/0/0/1 | 44/0/0/0/7/10/0/0/0/1/18/0/0/2/0 |
| Text/cc/paint_queue_PendingTextValue | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Text/cc/theme_CurrentTheme | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/drawing_props_StyleTokenColorOr | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_typeface_TextTypefaceUse | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Text/cc/raster_text_PaintTextRotated | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Text/cc/raster_text_PaintTextItalicClipped | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Text/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Text/cc/raster_text_PaintTextClipped | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Text/cc/raster_text_PaintText | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_EmitPaintNow | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Text/cc/tree_TreeCurrentPaintGeometry | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Text/cc/tree_TreeSubmittedGeometryAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Text/cc/paint_queue_KeepPaintMesh | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 53/33/2/1/1/18/5/8/6/1/4/1/0/1/7 |
| Text/cc/paint_queue_ClipShape | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Text/cc/shape_paint_ShapeOffset | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 19/0/0/6/3/10/0/0/0/0/9/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_SubmitShape | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 123/8/2/10/85/97/4/2/0/1/26/0/0/0/2 | 30/0/0/0/4/11/0/0/0/1/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Text/cc/text_emphasis_RunTextItalicRaster | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Text/cc/widget_benchmark_host_RasterText | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Text/cc/paint_queue_KeepPaintBytes | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Text/cc/paint_queue_KeepMeshPoints | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 178/148/12/9/11/52/15/26/14/4/21/0/0/6/20 |
| Text/cc/paint_queue_ShapeVisible | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Text/cc/paint_queue_ShapeWhole | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Text/cc/raster_shape_FillRoundedRectangle | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/paint_queue_FillShapeClipped | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Text/cc/shape_paint_EmitShape | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 104/16/4/17/12/33/8/18/14/7/14/0/0/0/4 | 25/0/0/4/0/8/1/11/7/0/3/0/0/0/0 |
| Text/cc/paint_queue_SubmitPaint | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 11/0/0/4/0/5/0/0/0/1/0/0/0/0/0 |
| Text/cc/theme_DarkPalette | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/cc/theme_DefaultMetrics | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/16/0/0/0/0 |
| Text/cc/theme_DefaultShadows | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 133/4/1/2/57/68/2/3/2/0/85/0/0/0/1 | 45/32/1/0/0/36/0/2/1/0/27/0/0/0/6 |
| Text/cc/theme_DefaultMotion | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Text/cc/theme_DefaultFonts | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/cc/text_layout_TextNoBreakBefore | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 443/0/0/0/0/0/0/155/155/54/0/0/0/0/0 | 114/105/18/0/0/12/2/28/10/0/0/0/0/0/37 |
| Text/cc/text_layout_TextNoBreakAfter | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 36/19/2/0/0/4/1/7/5/0/0/0/0/0/6 |
| Text/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Text/cc/paint_queue_KeepLongPaintBytes | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Text/cc/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 33/0/0/0/0/7/0/4/0/0/15/0/0/3/0 |
| Text/cc/paint_queue_ShapeRadius | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Text/cc/paint_queue_FillShapeRows | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Text/cc/paint_queue_FillClippedSpan | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Text/cc/shape_paint_EmitLineShape | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 102/16/1/19/0/39/5/5/4/5/44/0/0/2/1 |
| Text/cc/shape_paint_EmitCircleShape | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 220/120/6/50/37/89/6/22/16/17/53/0/0/0/16 | 176/124/9/25/14/65/1/20/11/8/53/0/0/3/14 |
| Text/cc/shape_paint_EmitRingShape | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 198/59/3/35/24/63/4/26/23/18/52/0/0/0/11 | 108/82/9/20/6/42/1/14/5/8/45/0/0/0/9 |
| Text/cc/shape_paint_EmitTriangleShape | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 37/12/1/7/0/19/1/3/2/2/9/0/0/0/3 |
| Text/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Text/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Text/cc/widget_benchmark_host_RecordBounds | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Text/cc/paint_queue_ShapeCornerInset | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Text/cc/shape_paint_ShapeAt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 19/0/0/2/3/10/0/0/0/0/7/0/0/0/0 | 5/0/0/1/0/1/0/0/0/0/4/0/0/0/0 |
| Text/cc/shape_paint_LineCornersFor | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 164/0/0/32/24/73/0/9/9/7/71/0/0/0/0 | 96/0/0/8/6/19/0/5/5/3/63/0/0/2/0 |
| Text/cc/shape_paint_ShapeBatchBegin | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/cc/shape_paint_ShapeHidden | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 31/20/3/0/0/6/0/5/2/0/18/0/0/2/2 |
| Text/cc/shape_paint_PointsBounds | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/5/20/10/38/1/11/6/5/32/0/0/0/6 | 46/29/2/2/2/15/6/4/2/0/29/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchTriangle | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 349/43/5/70/57/178/14/33/28/28/72/0/0/0/7 | 215/178/9/33/33/78/6/16/6/12/113/0/0/2/11 |
| Text/cc/shape_paint_ShapeBatchFlush | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 19/0/0/0/0/6/0/2/2/1/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcDirections | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 69/53/2/10/10/28/3/7/5/3/22/0/0/0/5 | 48/18/1/4/5/15/3/3/2/2/21/0/0/0/1 |
| Text/cc/shape_paint_ShapeFadeBands | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 142/127/10/15/3/18/0/24/14/19/6/1/2/0/23 | 77/17/1/0/0/3/2/2/1/2/3/2/0/9/3 |
| Text/cc/shape_paint_ShapeBatchSector | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 35/15/1/3/3/11/2/2/1/1/17/0/0/0/1 |
| Text/cc/shape_paint_ShapeBatchBand | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 59/36/1/7/5/15/1/2/1/2/37/0/0/0/1 |
| Text/cc/paint_queue_ShapeSquareRoot | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Text/cc/std_math_Sqrt | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 162/102/4/8/5/14/0/20/16/10/48/0/2/0/16 | 100/80/5/0/0/24/1/17/11/0/57/0/0/2/10 |
| Text/cc/shape_paint_ShapeBatchAdd | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 103/67/1/11/6/41/5/9/8/6/10/0/0/0/12 | 79/36/3/2/2/20/7/8/5/2/28/0/0/0/5 |
| Text/cc/raster_triangles_FillTriangles | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/cc/shape_paint_ShapeChannelChange | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/cc/std_math_CeilInteger | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 20/0/0/0/0/2/0/3/1/0/7/0/0/0/0 |
| Text/cc/paint_queue_GradientChannel | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 62/0/0/0/0/0/0/6/6/5/16/0/0/0/0 | 33/0/0/0/0/7/3/4/3/0/17/0/0/0/0 |
| Text/cc/shape_paint_ShapeArcPoint | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/6/4/14/1/0/0/0/11/0/0/0/0 | 11/0/0/0/0/2/1/0/0/0/8/0/0/0/0 |
| Text/cc/std_math_NotANumber | 0 | 0/44 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/cc/std_math_Finite64 | 0 | 0/44 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 10/0/0/0/0/2/0/2/2/0/4/0/0/0/0 |
| Text/cc/std_math_ScaleByPowerOfTwo | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 74/48/7/0/0/15/4/17/10/0/21/0/0/0/6 |
| Text/cc/shape_paint_ShapeInside | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 16/0/0/0/0/1/1/3/3/0/4/0/0/1/0 |
| Text/cc/shape_paint_ShapeCrossing | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 98/0/0/22/9/33/0/10/10/2/37/0/0/0/0 | 44/20/1/0/0/2/1/6/5/0/30/0/0/2/1 |
| Text/cc/widget_benchmark_host_RasterTriangles | 0 | 0/44 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/9/2/4/1/0/6/0/0/1/2 |
| Text/clang-19/widget_benchmarks_TextCase | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 236/158/9/19/59/102/3/30/21/16/29/0/0/0/27 | 155/66/6/12/15/54/0/20/14/14/19/0/0/1/12 |
| Text/clang-19/widget_benchmarks_BeginCase | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_draw_BeginFrame | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Text/clang-19/text___zi_spec_6a19f7c98e79a734 | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/6/6/12/2/1/0/1/2/0/0/0/1 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang-19/tree_draw_EndFrame | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Text/clang-19/tree_TreeCount | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/tree_TreeNodeAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/43 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/43 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_request_TextRequestSend | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Text/clang-19/text_request_TextRequestClear | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Text/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/43 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/tree_input_PointerSample | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 23/0/0/0/0/4/3/2/2/2/2/0/0/0/0 |
| Text/clang-19/tree_input_SecondaryPointerSample | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Text/clang-19/session_SessionFlagSet | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Text/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 379/237/17/42/15/132/43/58/41/21/26/4/0/5/35 |
| Text/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Text/clang-19/tree_TreeStart | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Text/clang-19/tree_TreeFrameStatus | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Text/clang-19/session_SessionFlagGet | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang-19/tree_draw_CommitFrame | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Text/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Text/clang-19/tree_TreeRead | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 258/0/0/12/12/144/53/18/18/1/43/8/0/6/0 |
| Text/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_TreeTextRange | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Text/clang-19/tree_input_TreeFocusAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Text/clang-19/tree_input_TreeFocusedAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Text/clang-19/tree_TreeInputAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Text/clang-19/geometry_RectangleIntersection | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Text/clang-19/tree_TreePointerActivatable | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Text/clang-19/tree_input_TreeActivateAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/0/0/1/0 |
| Text/clang-19/cursor_CursorHoverFrameStart | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Text/clang-19/tree_TreeBuildOldIndex | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 261/214/9/38/23/123/19/26/17/8/2/12/0/1/43 | 185/119/6/3/3/47/14/29/23/5/0/5/0/5/12 |
| Text/clang-19/tree_TreeSubmit | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Text/clang-19/std_math_Finite32 | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/tree_TreeRootBounds | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Text/clang-19/tree_TreeIsBuilding | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/tree_TreeSetLabel | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Text/clang-19/tree_TreeSetStyleClass | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang-19/tree_TreeSetStyleId | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang-19/tree_TreeSetSemantic | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Text/clang-19/text_TextHiddenLines | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/39/2/11/2/22/0/11/9/10/21/0/0/0/4 | 112/19/2/2/1/17/1/11/9/3/51/1/0/0/2 |
| Text/clang-19/tree_TreeSetHiddenLines | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Text/clang-19/text_selection_host_TextSelectionPresent | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/4/1/12/12/28/2/2/1/2/2/0/0/0/1 | 30/0/0/0/4/14/0/1/1/1/8/0/0/0/0 |
| Text/clang-19/paint_queue_PaintOverflow | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Text/clang-19/tree_TreeCancel | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Text/clang-19/paint_queue_PaintOrder | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Text/clang-19/paint_queue_SnapshotTextRead | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Text/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Text/clang-19/paint_queue_EmitPaint | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Text/clang-19/tree_input_TreeTopModalLayer | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Text/clang-19/std_grapheme_Push | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Text/clang-19/tree_TreeParentGeneration | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Text/clang-19/tree_TreeIdentityHash | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang-19/tree_TreeFindOld | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 160/122/2/24/18/71/7/17/15/9/0/5/0/0/20 | 123/54/5/9/7/37/10/15/10/4/0/5/0/4/3 |
| Text/clang-19/text_DefaultTextFace | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 65/4/1/5/32/45/2/1/0/1/20/0/0/0/1 | 13/0/0/0/0/6/0/0/0/1/3/0/0/0/0 |
| Text/clang-19/text_TextFaceFor | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 59/16/4/20/17/41/8/4/0/1/8/0/0/0/4 | 23/0/0/8/2/13/0/0/0/1/8/0/0/1/0 |
| Text/clang-19/text_behavior_ResolveTextStyle | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 127/99/1/14/14/30/0/23/22/9/8/0/0/2/18 | 57/0/0/5/0/8/0/3/3/2/7/0/0/2/0 |
| Text/clang-19/style_ResolveFont | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Text/clang-19/style_StyleFontValue | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Text/clang-19/theme_CurrentText | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/drawing_props_ColorFromPacked | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/surface_Opacity | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Text/clang-19/style_StyleOpacityValue | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphLineHeight | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_layout_TextDefaultLineGap | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 44/26/1/0/0/0/0/6/5/4/10/0/0/0/6 | 17/0/0/0/0/3/0/2/2/1/12/0/0/0/0 |
| Text/clang-19/text_behavior_TextWrapPolicy | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 21/0/0/0/0/0/0/1/1/1/2/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/text_layout_NextTextLine | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 566/502/26/67/179/311/14/67/41/26/18/12/1/0/55 | 345/109/17/14/7/108/42/57/40/5/19/10/0/13/13 |
| Text/clang-19/text_behavior_TextExtent | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 17/0/0/0/0/0/0/1/1/1/3/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/5/0/0/0/0 |
| Text/clang-19/tree_TreeCurrentParent | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Text/clang-19/geometry_WorldToScreen | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 46/0/0/8/5/22/0/0/0/2/30/0/0/0/0 | 31/0/0/8/3/12/0/0/0/2/26/0/0/0/0 |
| Text/clang-19/tree_TreeEnsureSemantic | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Text/clang-19/tree_TreeStoreLabel | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Text/clang-19/text_behavior_TextAlignmentOffset | 0 | 0/43 0% | calls/inlining candidates | 31/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 12/0/0/0/0/1/0/3/3/0/6/0/0/0/0 |
| Text/clang-19/text_layout_TextCharactersEnd | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 36/20/2/5/4/9/0/4/2/2/0/0/0/0/2 | 29/9/1/0/0/1/1/3/2/1/0/0/0/0/1 |
| Text/clang-19/text_TextSpan | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_TextEllipsisStart | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 96/84/6/23/14/42/1/10/4/4/0/0/0/0/11 | 55/23/3/2/2/9/3/7/4/1/0/0/0/0/2 |
| Text/clang-19/text_layout_TextLineXFor | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates | 45/0/0/0/0/0/0/6/6/3/0/0/1/0/0 | 14/0/0/0/0/0/0/2/2/0/0/0/0/2/0 |
| Text/clang-19/text_PaintTextEmphasis | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 231/212/11/69/59/145/7/21/10/11/7/0/0/0/13 | 221/165/17/28/27/87/10/29/12/5/13/2/0/3/17 |
| Text/clang-19/text_behavior_TextStrikethroughBounds | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 38/16/1/3/5/10/0/3/2/1/11/0/0/0/1 | 8/0/0/0/0/2/0/0/0/0/7/0/0/0/0 |
| Text/clang-19/text_PaintTextRule | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/4/1/69/73/148/2/2/1/10/46/0/0/0/1 | 101/0/0/26/22/52/0/4/2/6/51/0/0/0/0 |
| Text/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Text/clang-19/paint_queue_EmitPaintDirect | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 31/0/0/4/1/17/0/1/1/1/6/0/0/0/0 |
| Text/clang-19/paint_queue_ResolvePaintText | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/tree_TreeModalLayer | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Text/clang-19/tree_TreeLayerAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 74/27/1/0/0/15/5/12/11/3/0/1/0/1/3 |
| Text/clang-19/std_utf8_Continuation | 0 | 0/43 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/std_grapheme_data_Property | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Text/clang-19/std_grapheme_IsControl | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/session_WidgetPartKey | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Text/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/43 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_style_TextStyleFor | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 54/12/3/14/7/32/6/4/1/2/6/0/0/0/3 | 24/0/0/0/2/12/0/2/2/1/8/0/0/0/0 |
| Text/clang-19/surface_Unit | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/43 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/font_metrics_GlyphMetricsRevision | 0 | 0/43 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_layout_TextLineCacheMatches | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 68/0/0/1/0/15/0/18/18/5/2/0/0/0/0 | 20/0/0/1/0/8/0/7/7/0/2/0/0/0/0 |
| Text/clang-19/tree_TreeLabelSpanAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Text/clang-19/text_TextLinkRunAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 119/32/2/37/18/67/1/12/10/4/0/0/0/0/3 | 57/27/7/2/0/10/5/14/7/0/0/0/0/0/2 |
| Text/clang-19/text_TextLinkColor | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 48/0/0/7/7/19/1/3/3/4/0/1/1/0/0 | 19/0/0/0/0/1/0/0/0/3/0/2/0/2/0 |
| Text/clang-19/paint_queue_EmitLabel | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Text/clang-19/paint_queue_ClipLabel | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Text/clang-19/text_TextRangeValid | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 69/0/0/14/0/16/2/14/14/5/0/0/0/0/0 | 29/0/0/0/0/2/2/3/3/0/0/0/0/1/0 |
| Text/clang-19/paint_queue_PaintClippedEmphasis | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 177/4/1/18/84/150/2/2/1/2/25/0/0/0/1 | 60/0/0/8/19/31/0/0/0/2/17/0/0/0/0 |
| Text/clang-19/paint_queue_PaintRoundedFill | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 170/4/1/16/87/143/2/2/1/2/25/0/0/0/1 | 53/0/0/4/16/24/0/0/0/2/14/0/0/0/0 |
| Text/clang-19/shape_paint_PaintTriangleShape | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 141/4/1/21/93/117/2/1/0/4/32/0/0/0/1 | 48/0/0/0/26/30/0/0/0/1/30/0/0/0/0 |
| Text/clang-19/paint_queue_PendingTextValue | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 59/17/1/0/0/18/10/10/9/2/0/0/0/3/3 |
| Text/clang-19/theme_CurrentTheme | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 56/12/3/10/5/31/6/4/1/2/6/0/0/0/3 | 16/0/0/0/0/2/0/2/2/2/0/0/0/0/0 |
| Text/clang-19/drawing_props_StyleTokenColorOr | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/theme_CurrentAccent | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/text_typeface_TextTypefaceUse | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/raster_text_PaintTextRotated | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Text/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Text/clang-19/raster_text_PaintTextClipped | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Text/clang-19/raster_text_PaintText | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_EmitPaintNow | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Text/clang-19/tree_TreeOverlayLayerAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Text/clang-19/tree_TreeSubmittedGeometryAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Text/clang-19/paint_queue_KeepPaintMesh | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 93/64/3/16/14/51/6/7/4/5/10/2/0/0/12 | 51/0/0/0/0/14/3/5/5/1/6/1/0/1/0 |
| Text/clang-19/paint_queue_ClipShape | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeOffset | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 19/0/0/6/3/10/0/0/0/0/9/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/shape_paint_SubmitShape | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 123/8/2/10/85/97/4/2/0/1/26/0/0/0/2 | 30/0/0/0/5/10/0/0/0/2/4/0/0/0/0 |
| Text/clang-19/text_layout_TextIsSpace | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/0/0/9/9/2/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Text/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Text/clang-19/paint_queue_KeepPaintBytes | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Text/clang-19/paint_queue_KeepMeshPoints | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Text/clang-19/paint_queue_ShapeVisible | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 30/0/0/1/0/8/0/3/3/1/20/0/0/0/0 |
| Text/clang-19/paint_queue_ShapeWhole | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 20/0/0/2/0/5/0/4/4/0/8/0/0/0/0 |
| Text/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/paint_queue_FillShapeClipped | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Text/clang-19/shape_paint_EmitShape | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 104/16/4/17/12/33/8/18/14/7/14/0/0/0/4 | 49/0/0/1/0/22/1/10/6/2/12/0/0/0/0 |
| Text/clang-19/paint_queue_SubmitPaint | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 58/4/1/12/11/43/2/2/1/2/2/0/0/0/1 | 9/0/0/3/1/5/0/0/0/1/2/0/0/0/0 |
| Text/clang-19/theme_DarkPalette | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/theme_LightPalette | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 117/4/1/2/81/84/2/1/0/0/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Text/clang-19/theme_DefaultMetrics | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Text/clang-19/theme_DefaultMotion | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Text/clang-19/theme_DefaultFonts | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 48/4/1/2/21/34/7/1/0/0/2/0/0/0/1 | 17/0/0/0/0/15/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/text_layout_TextNoBreakAfter | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 96/0/0/0/0/0/0/33/33/10/0/0/0/0/0 | 27/14/6/0/0/0/0/11/5/0/0/0/0/0/6 |
| Text/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Text/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Text/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Text/clang-19/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Text/clang-19/paint_queue_ShapeRadius | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Text/clang-19/paint_queue_FillClippedSpan | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Text/clang-19/shape_paint_EmitLineShape | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 171/12/3/41/35/122/13/7/4/12/11/0/0/0/3 | 133/0/0/19/9/46/0/10/10/6/97/0/0/0/0 |
| Text/clang-19/shape_paint_EmitRingShape | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 198/59/3/35/24/63/4/26/23/18/52/0/0/0/11 | 182/57/3/32/13/80/9/21/18/9/103/0/0/0/0 |
| Text/clang-19/shape_paint_EmitTriangleShape | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 71/16/4/18/18/49/8/5/1/5/14/0/0/0/4 | 27/0/0/7/0/16/0/2/2/2/9/0/0/0/0 |
| Text/clang-19/paint_queue___zi_spec_9308732530dd822f | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 444/413/4/26/277/333/6/20/16/7/2/4/0/1/23 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Text/clang-19/widget_benchmark_host_RecordBounds | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeAt | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 19/0/0/2/3/10/0/0/0/0/7/0/0/0/0 | 4/0/0/1/0/1/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/shape_paint_LineCornersFor | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 164/0/0/32/24/73/0/9/9/7/71/0/0/0/0 | 66/0/0/9/6/22/0/3/3/3/47/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchBegin | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 23/0/0/6/2/18/0/0/0/0/0/0/0/0/0 | 7/0/0/2/0/6/0/0/0/0/2/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeHidden | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 73/0/0/12/2/19/0/10/10/3/20/0/0/0/0 | 26/0/0/0/0/5/0/4/4/0/16/0/0/0/0 |
| Text/clang-19/shape_paint_PointsBounds | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 110/95/5/20/10/38/1/11/6/5/32/0/0/0/6 | 61/25/1/0/0/7/4/6/5/0/38/0/0/0/3 |
| Text/clang-19/shape_paint_ShapeBatchFlush | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/3/6/22/1/5/5/3/0/0/0/0/0 | 12/0/0/0/0/5/0/2/2/1/0/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeArcDirections | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 69/53/2/10/10/28/3/7/5/3/22/0/0/0/5 | 47/17/1/6/5/18/3/3/2/2/21/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeFadeBands | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 142/127/10/15/3/18/0/24/14/19/6/1/2/0/23 | 66/0/0/1/1/3/0/1/1/2/3/2/0/6/0 |
| Text/clang-19/shape_paint_ShapeBatchSector | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 46/35/2/9/8/19/0/5/3/3/5/0/0/0/2 | 34/13/1/3/3/10/3/2/1/1/15/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchBand | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/59/2/17/14/35/0/5/3/6/9/0/0/0/3 | 52/27/1/8/8/21/3/2/1/2/31/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeBatchAdd | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 103/67/1/11/6/41/5/9/8/6/10/0/0/0/12 | 63/0/0/6/4/21/3/6/6/2/29/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeClipSide | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 150/69/3/35/34/86/10/13/10/6/26/0/0/0/7 | 115/77/2/1/0/20/10/27/25/0/45/0/0/0/8 |
| Text/clang-19/raster_triangles_FillTriangles | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeChannelChange | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/5/1/1/1/2/0/2/1/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Text/clang-19/std_math_CeilInteger | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 42/0/0/0/0/0/0/4/4/4/7/0/0/0/0 | 15/0/0/0/0/2/0/2/2/1/7/0/0/0/0 |
| Text/clang-19/paint_queue_GradientChannel | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 62/0/0/0/0/0/0/6/6/5/16/0/0/0/0 | 29/0/0/0/0/4/0/4/4/1/15/0/0/1/0 |
| Text/clang-19/shape_paint_ShapeArcPoint | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 25/0/0/6/4/14/1/0/0/0/11/0/0/0/0 | 7/0/0/0/0/2/1/0/0/0/4/0/0/0/0 |
| Text/clang-19/std_math_NotANumber | 0 | 0/43 0% |  | 6/0/0/0/0/0/0/0/0/0/4/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/1/0/0/0/0 |
| Text/clang-19/std_math_Finite64 | 0 | 0/43 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 35/0/0/0/0/0/0/6/6/1/5/0/0/0/0 | 5/0/0/0/0/2/0/0/0/0/3/0/0/0/0 |
| Text/clang-19/std_math_ScaleByPowerOfTwo | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 111/101/8/12/15/27/0/20/12/6/39/0/0/0/8 | 89/35/5/0/0/15/4/22/17/0/34/0/0/1/4 |
| Text/clang-19/shape_paint_ShapeTurn | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 17/0/0/6/0/6/0/0/0/0/13/0/0/0/0 | 10/0/0/0/0/0/0/0/0/0/9/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeInside | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/4/0/4/0/4/4/3/8/0/0/0/0 | 14/0/0/0/0/0/0/3/3/0/4/0/0/0/0 |
| Text/clang-19/shape_paint_ShapeCrossing | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 98/0/0/22/9/33/0/10/10/2/37/0/0/0/0 | 38/0/0/0/0/0/0/6/6/0/29/0/0/0/0 |
| Text/clang-19/widget_benchmark_host_RasterTriangles | 0 | 0/43 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 48/34/1/4/0/16/1/5/4/3/8/0/0/0/7 | 25/12/1/0/0/8/2/4/3/1/7/0/0/1/0 |
| Input/cc/widget_benchmarks_InputCase | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 394/8/2/53/124/218/8/37/35/26/35/0/0/0/2 | 164/152/15/12/15/57/0/17/2/11/8/0/0/0/42 |
| Input/cc/widget_benchmarks_BeginCase | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 19/13/1/0/0/11/0/2/1/2/0/0/0/0/5 |
| Input/cc/tree_draw_BeginFrame | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 200/150/6/13/6/82/20/26/19/11/11/1/0/3/28 |
| Input/cc/textfield_Draw | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 230/12/3/51/35/115/6/28/25/16/6/0/0/0/3 | 195/136/7/50/17/84/3/18/11/9/16/0/0/3/19 |
| Input/cc/tree_draw_EndFrame | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 57/42/4/0/0/3/2/7/3/7/0/0/0/1/3 |
| Input/cc/tree_input_TreePointerUpdate | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 265/206/14/21/10/117/35/27/13/9/17/5/0/14/50 |
| Input/cc/tree_TreeNodeAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_WidgetTextCount | 0 | 0/58 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/58 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_request_TextRequestSend | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 217/177/7/30/7/68/5/31/24/14/0/6/0/0/23 |
| Input/cc/text_request_TextRequestClear | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 18/0/0/0/0/1/0/1/1/2/0/1/0/0/0 |
| Input/cc/std_grapheme_Boundary | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 63/46/4/12/4/16/0/11/7/4/0/0/0/0/3 | 38/8/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Input/cc/session_SessionOpen | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/30/1/6/5/27/3/5/4/3/0/0/0/1/9 | 28/10/1/0/0/7/4/3/2/0/0/0/0/1/3 |
| Input/cc/widget_benchmark_host_WidgetPaintReset | 0 | 0/58 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointerSample | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/14/14/39/1/1/1/3/3/1/0/0/0 | 31/19/1/2/5/13/3/2/1/2/1/0/0/0/2 |
| Input/cc/tree_input_SecondaryPointerSample | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 32/21/1/2/4/11/2/2/1/2/0/0/0/1/3 |
| Input/cc/session_SessionFlagSet | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 14/0/0/0/0/0/0/1/1/0/0/0/0/1/0 |
| Input/cc/paint_queue_PaintSnapshotInvalidate | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 46/0/0/0/0/18/7/2/2/2/0/0/0/1/0 |
| Input/cc/tree_input_TreeKeyboardRoute | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 443/406/37/68/58/194/23/56/19/16/31/3/0/8/57 |
| Input/cc/text_editing_TextEditingFrameBegin | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 12/0/0/0/0/3/0/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_PaintClear | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 24/0/0/0/0/14/6/0/0/1/0/0/0/2/0 |
| Input/cc/tree_TreeStart | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 91/66/1/1/1/29/1/3/2/4/4/1/0/0/27 |
| Input/cc/tree_TreeFrameStatus | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 17/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Input/cc/textfield_TextFieldKey | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 7/0/0/2/0/2/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextSelectionRangeForLength | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 24/0/0/3/2/5/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuTakeCommand | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/110/7/23/10/47/0/31/24/14/0/0/0/0/18 | 66/46/6/2/0/14/2/13/7/3/0/0/0/0/6 |
| Input/cc/text_context_menu_TextMenuPasteText | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuOpenRequested | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 70/0/0/16/9/27/0/9/9/8/4/0/0/0/0 | 63/22/1/6/5/13/1/4/3/4/11/0/0/1/1 |
| Input/cc/system_clipboard_SystemClipboardSet | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_platform_TextInputNoteFocused | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 79/0/0/15/15/49/4/3/3/6/0/4/0/0/0 | 54/20/3/0/0/19/6/5/2/3/0/0/0/0/3 |
| Input/cc/text_context_menu_TextMenuIsOpenFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 18/4/1/0/0/4/0/4/3/0/0/0/0/0/0 |
| Input/cc/text_context_menu_TextMenuShow | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 17/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/cc/session_SessionFlagGet | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 6/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/tree_draw_CommitFrame | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 112/100/3/1/1/23/4/10/7/12/0/0/0/3/14 |
| Input/cc/paint_queue_PaintSnapshotEmit | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 252/239/8/35/50/126/9/23/15/12/5/2/0/3/35 |
| Input/cc/tree_input_TreeHitAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 419/350/6/59/55/188/7/70/64/37/10/7/0/0/58 | 195/163/15/27/12/74/3/28/13/13/41/1/0/2/17 |
| Input/cc/tree_input_TreeTextFocused | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 47/10/1/0/0/11/3/7/6/1/0/1/0/1/2 |
| Input/cc/tree_input_TreeTextKind | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeFocusAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 60/40/5/3/0/7/3/9/4/5/0/0/0/0/3 |
| Input/cc/tree_TreeInputAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 93/69/2/5/12/41/2/9/7/2/19/1/0/5/7 |
| Input/cc/tree_TreeRead | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 535/28/7/81/142/352/55/51/44/7/31/33/0/0/7 | 483/417/24/31/91/254/24/43/19/1/92/7/0/17/64 |
| Input/cc/tree_TreeCount | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/std_utf8_Valid | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 90/84/7/17/5/25/2/17/10/7/0/0/0/0/13 | 71/49/5/0/2/10/4/12/7/1/0/0/0/1/6 |
| Input/cc/tree_TreeTextRange | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 94/18/2/5/3/25/7/13/11/2/0/3/0/2/2 |
| Input/cc/std_grapheme_Next | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 86/76/3/19/18/39/0/9/6/5/0/0/0/0/3 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/tree_input_TreeFocusedAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 65/25/2/0/0/9/3/7/5/3/0/1/0/2/1 |
| Input/cc/geometry_RectangleIntersection | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 41/0/0/0/0/0/0/0/0/0/38/0/0/2/0 |
| Input/cc/tree_TreePointerActivatable | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/0/0/0/0/2/1/5/5/0/0/0/0/1/0 |
| Input/cc/tree_input_TreeActivateAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 46/20/1/5/0/10/2/2/1/3/0/0/0/1/1 |
| Input/cc/cursor_CursorHoverFrameStart | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 23/0/0/0/0/5/1/2/2/1/2/0/0/2/0 |
| Input/cc/text_input_TextSelectionRangeFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 13/0/0/3/2/5/0/1/1/0/0/0/0/0/0 |
| Input/cc/system_clipboard_SystemClipboardText | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 73/46/8/2/1/11/6/14/6/2/0/0/0/0/6 |
| Input/cc/input_protocol_PointInBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 27/0/0/0/0/0/0/5/5/0/15/0/0/2/0 |
| Input/cc/text_context_menu_TextMenuOpenAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 61/12/2/3/0/16/2/6/4/2/2/0/0/0/2 |
| Input/cc/tree_TreePrevious | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/25/2/0/0/10/2/8/6/3/0/1/0/1/4 |
| Input/cc/tree_TreeCurrentParent | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 22/0/0/0/0/6/3/2/2/1/0/2/0/0/0 |
| Input/cc/tree_input_TreeTakeDragAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 173/148/11/13/4/58/10/25/14/6/22/0/0/8/20 |
| Input/cc/tree_TreePreviousWidgetState | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 48/0/0/0/0/17/3/4/4/1/2/1/0/1/0 |
| Input/cc/tree_input_TreeHasFocus | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 101/71/3/11/8/35/6/14/11/8/0/3/0/0/14 | 71/45/3/5/4/20/7/9/6/3/0/1/0/0/7 |
| Input/cc/tree_input_TreeBlurAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 37/7/1/0/0/9/4/5/4/1/0/1/0/1/2 |
| Input/cc/style_ResolveFont | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 10/0/0/0/0/0/0/2/2/0/0/0/0/0/0 |
| Input/cc/style_StyleFontValue | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 23/0/0/0/0/4/0/5/3/0/7/0/0/0/0 |
| Input/cc/text_input_TextInputScaledMetric | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 52/38/1/0/0/0/0/7/6/5/9/0/0/0/8 | 14/0/0/0/0/2/0/3/1/0/7/0/0/0/0 |
| Input/cc/textfield_TextFieldSecureDisplay | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 49/20/2/1/0/5/1/5/3/2/0/0/0/0/3 |
| Input/cc/font_metrics_GlyphWidth | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldCursorAtX | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 66/38/2/8/10/20/1/4/2/3/0/0/0/0/2 |
| Input/cc/text_input_TextPointerOffset | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 69/0/0/0/0/0/0/7/7/7/18/0/0/0/0 | 37/0/0/0/0/6/0/6/3/0/15/0/0/0/0 |
| Input/cc/textfield_TextFieldCodepoints | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 29/7/1/0/0/5/2/2/1/1/0/0/0/0/3 |
| Input/cc/text_TextSpan | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 10/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldByteAfter | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 39/21/2/0/0/2/1/5/3/1/0/0/0/0/2 |
| Input/cc/text_input_CompositionTransitionFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 269/97/2/84/42/171/0/18/16/11/0/0/0/0/3 | 159/126/11/16/29/59/8/28/17/0/23/0/0/0/13 |
| Input/cc/textfield_TextFieldNext | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextSelectionAfterMove | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 15/11/1/2/3/6/1/2/1/0/0/0/0/0/0 |
| Input/cc/textfield_TextFieldInputTextAllowed | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 21/8/1/0/0/6/3/5/4/0/0/0/0/0/3 |
| Input/cc/textfield_TextFieldReplacementAllowed | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 133/98/8/12/3/35/9/24/16/4/0/0/0/0/9 |
| Input/cc/text_input_TextEditCommandDecisionFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/88/8/4/3/13/4/18/10/0/0/0/0/5/8 |
| Input/cc/textfield_TextFieldEditFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/4/1/48/39/113/2/18/17/7/2/0/0/0/1 | 106/75/9/16/12/37/1/14/5/5/12/0/0/0/6 |
| Input/cc/text_request_TextRequestTake | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 190/0/0/43/24/110/10/16/16/11/0/9/0/0/0 | 79/0/0/4/3/24/4/8/8/4/0/1/0/0/0 |
| Input/cc/text_request_TextRequestChangeFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 259/242/12/47/26/98/5/36/24/13/0/0/0/0/24 | 160/128/12/20/5/57/8/24/12/6/0/0/0/0/9 |
| Input/cc/font_metrics_GlyphCaret | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 34/0/0/0/0/5/1/2/2/1/0/0/0/0/0 |
| Input/cc/text_input_TextFieldRevealScroll | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 25/0/0/0/0/2/2/1/1/0/0/0/0/0/0 |
| Input/cc/style_StyleOpacityValue | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 16/0/0/0/0/3/1/2/2/0/10/0/0/0/0 |
| Input/cc/tree_TreeSetInteractive | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 53/0/0/1/1/10/0/6/6/3/0/1/0/1/0 |
| Input/cc/tree_TreeSetCaptureTab | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 49/36/5/0/0/7/1/8/3/4/0/1/0/1/4 |
| Input/cc/tree_input_TreeFocusSubmittedAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 225/4/1/39/26/96/10/28/27/14/8/7/0/0/1 | 158/139/17/8/16/61/7/23/6/6/25/3/0/5/15 |
| Input/cc/tree_TreeSetWidgetState | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 155/104/7/15/5/48/10/26/19/3/2/4/0/3/14 |
| Input/cc/tree_TreeSetSemantic | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 69/0/0/1/1/13/2/9/9/3/0/2/0/1/0 |
| Input/cc/tree_TreeSetLabel | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Input/cc/tree_TreeSetStyleClass | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Input/cc/tree_TreeSetStyleId | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 45/18/1/0/0/8/2/7/6/3/0/1/0/1/1 |
| Input/cc/tree_TreeSetSelected | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 43/0/0/0/0/7/2/6/6/3/0/1/0/1/0 |
| Input/cc/tree_TreeSetTextValue | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 172/64/6/12/9/59/8/28/22/4/0/3/0/1/8 |
| Input/cc/drawing_props_ColorFromPacked | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 45/0/0/2/5/8/0/3/3/3/0/0/0/3/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/paint_queue_PaintRoundedOutline | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 39/0/0/0/12/16/0/0/0/1/4/0/0/0/0 |
| Input/cc/font_metrics_GlyphLineHeight | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldPaintFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 173/0/0/21/19/52/1/13/13/12/46/0/1/0/0 | 120/0/0/2/0/13/2/13/5/0/41/0/0/4/0 |
| Input/cc/paint_queue_PaintClippedLabel | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 44/0/0/4/15/22/0/0/0/1/6/0/0/0/0 |
| Input/cc/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineY | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextCompositionUnderlineEndX | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeSubmittedNodeAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/button_Draw | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/cc/system_clipboard_host_SystemClipboardWriteHost | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 31/12/1/5/0/10/2/3/2/1/0/0/0/0/2 | 18/6/1/0/0/6/2/3/2/0/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputFocusFrameBegin | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 86/0/0/16/15/53/6/3/3/5/0/6/0/0/0 | 53/17/2/0/0/14/5/4/2/3/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputFocused | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 38/25/4/0/0/8/3/5/1/3/0/0/0/0/1 |
| Input/cc/text_input_platform_TextInputOwnerMatches | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 13/6/1/0/0/1/0/3/1/0/0/0/0/0/0 |
| Input/cc/tree_TreeDefer | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 49/6/1/0/0/10/5/3/2/1/0/0/0/2/4 |
| Input/cc/text_context_menu_TextMenuDraw | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 400/350/2/41/98/270/21/16/14/13/26/16/0/0/82 | 152/138/6/8/31/79/3/13/7/4/21/0/0/2/33 |
| Input/cc/tree_draw_FrameInputCommit | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 35/0/0/0/0/14/3/0/0/1/0/0/0/2/0 |
| Input/cc/paint_queue_PaintOverflow | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/cc/tree_TreeCancel | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 21/0/0/0/0/14/0/0/0/1/0/1/0/0/0 |
| Input/cc/tree_TreeFinish | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 213/167/8/4/4/112/23/20/12/3/20/6/0/3/20 |
| Input/cc/paint_queue_PaintOrder | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 188/170/12/7/6/65/15/32/20/2/5/0/0/8/30 |
| Input/cc/paint_queue_SnapshotTextRead | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 8/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_SnapshotTextJoin | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 272/250/21/6/6/71/22/54/33/4/0/0/0/9/34 |
| Input/cc/paint_queue_PaintPointCount | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 18/0/0/3/0/4/0/3/3/0/0/1/0/0/0 |
| Input/cc/tree_input_TreeTopModalLayer | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeModalLayerAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 11/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/tree_TreeButtonLike | 0 | 0/58 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_input_PointInCircleBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 31/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/cc/std_utf8_Decode | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 88/66/5/2/3/24/5/17/12/0/0/0/0/1/15 |
| Input/cc/tree_TreeParentGeneration | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 45/13/1/0/0/7/5/4/3/1/0/1/0/3/2 |
| Input/cc/tree_TreeIdentityHash | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/tree_TreeExplicitExists | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/21/2/2/2/22/12/5/3/2/0/3/0/3/2 |
| Input/cc/system_clipboard_host_SystemClipboardOpenHost | 0 | 0/58 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/cc/system_clipboard_host_SystemClipboardByteHost | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 11/0/0/0/0/4/2/2/2/0/0/0/0/0/0 |
| Input/cc/tree_input_TreeTakePointerActivationAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 150/0/0/29/19/72/6/20/20/12/7/6/0/0/0 | 82/64/7/8/3/21/3/11/4/6/1/0/0/1/7 |
| Input/cc/style_sheet_StyleControlRoleFacts | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_ResolveInstalledStyleRules | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 185/151/5/31/40/118/13/10/5/3/76/0/0/5/15 |
| Input/cc/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 34/16/2/0/0/5/3/5/3/0/0/0/0/2/5 |
| Input/cc/text_input_TextUtf8Continuation | 0 | 0/58 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_utf8_Next | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/1/0 |
| Input/cc/text_input_TextCursorForLength | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_CompositionBoundaryFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/15/2/0/0/2/2/6/4/0/0/0/0/0/3 |
| Input/cc/std_grapheme_Previous | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 61/46/5/10/4/14/0/12/7/4/0/0/0/0/3 | 31/9/1/0/0/0/0/4/3/1/0/0/0/0/0 |
| Input/cc/tree_TreeEnsureInteraction | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 119/97/8/1/2/26/6/22/14/3/0/5/0/7/20 |
| Input/cc/tree_TreeLayerAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 109/82/3/12/5/41/6/18/15/6/0/6/0/0/14 | 86/28/2/0/0/17/1/13/11/3/0/2/0/1/3 |
| Input/cc/tree_TreeEnsureSemantic | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 132/110/8/1/2/39/10/22/14/3/4/5/0/4/25 |
| Input/cc/tree_TreeStoreLabel | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 343/307/25/42/35/140/30/54/29/4/13/9/0/6/40 |
| Input/cc/paint_queue_ClipShape | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 29/19/1/6/4/12/0/2/1/1/8/0/0/0/2 |
| Input/cc/surface_Unit | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 14/0/0/0/0/2/1/1/1/0/10/0/0/0/0 |
| Input/cc/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/58 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/text_input_TextFieldCursorHeightFor | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 24/0/0/0/0/2/0/3/1/0/4/0/0/0/0 |
| Input/cc/paint_queue_EmitLabel | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 100/49/3/38/2/42/2/10/7/6/16/0/0/0/3 |
| Input/cc/paint_queue_ClipLabel | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/24/2/8/6/17/0/5/3/1/12/0/0/0/3 |
| Input/cc/paint_queue_EmitLine | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 82/48/2/13/0/17/1/20/18/0/53/0/0/0/0 |
| Input/cc/button_SubmitButton | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 619/598/18/167/121/310/20/59/41/56/51/0/0/0/60 | 437/407/9/117/73/208/3/21/10/35/124/0/0/2/36 |
| Input/cc/system_clipboard_SystemClipboardHasText | 0 | 0/58 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/cc/contextmenu_Draw | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 980/945/36/232/240/560/28/93/57/58/87/16/0/0/95 | 570/548/32/102/94/285/46/65/33/18/103/1/0/4/61 |
| Input/cc/cursor_CursorHoverFrameCommit | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 19/0/0/0/0/4/0/2/2/1/0/0/0/1/0 |
| Input/cc/paint_queue_PaintMeshPointCount | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 15/0/0/0/0/1/0/2/2/0/0/1/0/0/0 |
| Input/cc/paint_queue_EmitPaintDirect | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 60/8/2/13/11/42/4/4/2/2/4/0/0/0/2 | 27/0/0/5/0/15/0/1/1/1/2/0/0/0/0 |
| Input/cc/tree_TreeModalLayer | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/cc/std_utf8_Continuation | 0 | 0/58 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/std_grapheme_IsControl | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/session_WidgetPartKey | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 18/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/cc/widget_key_WidgetRepeatedPart | 0 | 0/58 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleControlFacts | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/13/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleBucket | 0 | 0/58 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleFirstBit | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 25/0/0/0/0/1/0/5/5/0/0/0/0/5/0 |
| Input/cc/style_sheet_StyleRuleMatchesState | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/style_sheet_StyleSelectorMatches | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 84/77/10/24/0/26/1/15/5/0/0/0/0/0/8 |
| Input/cc/style_MergeValues | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 182/166/15/59/14/87/2/40/25/0/73/0/0/0/24 |
| Input/cc/tree_TreeLabelSpanAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 30/0/0/4/4/20/6/1/1/0/0/2/0/0/0 |
| Input/cc/paint_queue_ShapeVisible | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 90/0/0/22/15/44/0/6/6/4/11/0/0/0/0 | 28/0/0/7/0/7/0/4/3/1/13/0/0/1/0 |
| Input/cc/paint_queue_ShapeWhole | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 57/0/0/4/0/10/0/9/9/2/12/0/0/0/0 | 23/0/0/4/0/5/1/6/6/0/8/0/0/2/0 |
| Input/cc/raster_shape_FillRoundedRectangle | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/paint_queue_FillShapeClipped | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 137/34/1/32/23/57/1/4/2/4/117/0/0/0/1 |
| Input/cc/paint_queue_EmitPaintNow | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 143/125/10/2/8/21/6/28/16/5/0/0/0/0/15 |
| Input/cc/tree_TreeCurrentPaintGeometry | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 70/27/3/0/0/19/5/8/5/2/11/3/0/1/2 |
| Input/cc/tree_TreeSubmittedGeometryAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 56/0/0/4/8/26/1/5/5/2/16/1/0/1/0 |
| Input/cc/raster_shape_StrokeRoundedRectangle | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/text_typeface_TextTypefaceUse | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 12/0/0/0/0/5/0/3/2/0/0/0/0/0/0 |
| Input/cc/raster_text_PaintTextRotated | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 36/0/0/3/3/7/1/3/3/0/10/0/0/0/0 |
| Input/cc/raster_text_PaintTextItalicClipped | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 72/0/0/4/3/8/0/6/5/1/10/0/0/1/0 |
| Input/cc/raster_text_PaintTextEmphasisClipped | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 64/31/1/2/2/7/2/6/5/1/10/1/0/3/5 |
| Input/cc/raster_text_PaintTextClipped | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 14/0/0/0/0/1/0/3/3/0/7/0/0/1/0 |
| Input/cc/raster_text_PaintText | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/raster_StrokeLine | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/style_ResolveFlags | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 16/0/0/1/2/3/0/0/0/0/0/0/0/1/0 |
| Input/cc/style_ResolveState | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 24/0/0/1/0/1/0/6/6/0/0/0/0/0/0 |
| Input/cc/tree_input_TreePressedAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 44/0/0/6/5/21/3/1/1/3/0/3/0/0/0 | 32/0/0/0/0/6/3/1/1/2/0/1/0/3/0 |
| Input/cc/tree_input_TreeHoveredAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 61/41/4/0/0/9/6/8/4/3/0/2/0/2/5 |
| Input/cc/button_ButtonDetailSize | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 47/0/0/9/0/10/1/4/4/2/8/2/0/2/0 |
| Input/cc/tree_TreeRootBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 38/15/1/3/0/7/4/2/1/3/4/3/0/0/1 |
| Input/cc/button_behavior_MeasureBounds | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 276/239/33/59/17/87/2/53/20/5/141/0/0/0/16 |
| Input/cc/style_StyleMinHeightValue | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/button_ButtonMinimumHeight | 0 | 0/58 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/button_ButtonFitContentFace | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 371/290/13/41/19/86/4/61/48/25/83/0/0/0/41 | 204/182/15/35/11/66/4/31/14/1/113/0/0/1/12 |
| Input/cc/tree_TreeSetCircleHit | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 43/0/0/0/0/9/2/6/6/2/0/1/0/2/0 |
| Input/cc/tree_TreeSetSemanticDetail | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 63/0/0/1/1/11/1/9/9/4/0/1/0/1/0 |
| Input/cc/button_PaintButtonForNode | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1316/1279/16/257/184/508/12/135/119/93/320/0/1/0/133 | 922/904/51/269/130/455/13/101/44/42/387/2/0/9/88 |
| Input/cc/tree_TreeNextKey | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 65/9/1/0/0/11/5/5/4/2/0/5/0/4/1 |
| Input/cc/contextmenu_ContextMenuOverlayKey | 0 | 0/58 0% |  | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/cc/contextmenu_ContextMenuPanelKey | 0 | 0/58 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePreviousCommitted | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 46/9/1/0/0/6/2/4/3/2/0/1/0/1/1 |
| Input/cc/contextmenu_ContextMenuRowKey | 0 | 0/58 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 8/0/0/0/0/4/3/0/0/0/0/0/0/1/0 |
| Input/cc/contextmenu_ContextMenuOpens | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 25/7/2/0/0/7/3/6/4/0/0/0/0/0/2 |
| Input/cc/contextmenu_ContextMenuFind | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 18/8/2/0/0/4/2/5/3/0/0/0/0/0/3 |
| Input/cc/contextmenu_ContextMenuBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuSubmenuBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/43/1/32/22/60/1/3/2/6/37/1/0/0/1 | 85/0/0/11/8/21/2/2/1/4/48/0/0/0/0 |
| Input/cc/overlay_Draw | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 388/311/1/115/78/209/0/28/27/28/19/0/0/0/28 | 257/214/9/46/27/82/5/22/13/19/36/0/0/6/31 |
| Input/cc/contextmenu_ContextMenuLevel | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 1422/1205/10/372/349/778/10/72/62/142/317/1/0/0/114 | 1082/888/26/288/120/488/12/74/36/102/301/0/0/2/121 |
| Input/cc/paint_queue_PendingTextValue | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 93/0/0/9/7/46/12/3/3/2/0/6/0/0/0 | 82/0/0/0/0/19/9/8/8/1/0/0/0/7/0 |
| Input/cc/style_sheet_StyleMatchesOptional | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/paint_queue_ShapeRadius | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 18/0/0/0/0/1/0/0/0/0/16/0/0/1/0 |
| Input/cc/paint_queue_FillShapeRows | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 267/251/10/54/29/83/0/26/16/12/137/0/0/0/11 | 252/192/7/32/21/68/6/22/15/3/165/0/0/8/9 |
| Input/cc/paint_queue_FillClippedSpan | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 35/0/0/0/0/1/0/3/3/0/26/0/0/2/0 |
| Input/cc/paint_queue_KeepPaintBytes | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 21/0/0/1/1/7/1/6/3/0/0/0/0/1/0 |
| Input/cc/paint_queue_KeepMeshPoints | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 175/145/12/9/11/49/12/26/14/4/21/0/0/6/20 |
| Input/cc/widget_benchmark_host_RasterRoundedRectangleOutline | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 37/0/0/4/4/11/0/3/3/4/7/0/0/0/0 | 33/0/0/2/2/10/0/4/0/1/11/0/0/1/0 |
| Input/cc/widget_benchmark_host_RasterTextRotated | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 62/0/0/12/9/24/0/3/3/4/13/0/0/0/0 | 41/0/0/4/1/9/0/2/0/1/13/0/0/1/0 |
| Input/cc/text_emphasis_RunTextItalicRaster | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 21/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/cc/text_emphasis_RunTextEmphasisRaster | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/0/0/3/0/1/1/1/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterText | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 36/12/1/4/0/17/1/2/1/0/0/0/0/0/2 | 25/6/1/0/0/9/2/2/1/0/0/0/0/1/1 |
| Input/cc/button_ButtonDefaultFace | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 217/173/12/35/36/93/9/26/14/15/37/0/0/0/25 |
| Input/cc/button_ButtonFaceFacts | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 38/27/1/7/0/8/1/2/1/3/0/0/0/0/2 |
| Input/cc/button_ButtonDetailFont | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 10/0/0/0/0/0/0/1/1/0/0/2/0/2/0 |
| Input/cc/style_StyleLineHeightValue | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/cc/style_FitHeight | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 22/0/0/0/0/0/0/1/1/0/20/0/0/0/0 |
| Input/cc/button_behavior_MeasureContent | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 157/141/5/4/9/15/0/28/23/9/40/0/0/0/14 | 79/38/3/0/0/5/1/9/6/0/50/0/0/0/0 |
| Input/cc/button_behavior_ShapeWidth | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 26/0/0/0/0/2/1/4/4/0/12/0/0/0/0 |
| Input/cc/theme_ControlHeightFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 38/14/2/3/0/11/1/7/5/1/10/0/0/0/1 |
| Input/cc/button_behavior_ButtonCornerRadius | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 8/0/0/0/0/4/1/2/2/0/3/0/0/0/0 |
| Input/cc/focus_FocusDefaultOutlineBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 46/23/2/2/1/10/2/6/2/0/30/0/0/0/2 |
| Input/cc/image_lookup_IconLookupImage | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 19/0/0/0/0/4/1/1/1/1/0/0/0/0/0 |
| Input/cc/button_behavior_ButtonPictureSize | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 19/0/0/0/0/2/1/2/2/0/12/0/0/0/0 |
| Input/cc/button_behavior_ButtonLabeledPictureSize | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 91/52/4/0/0/0/0/10/6/7/27/0/0/0/7 | 22/15/1/0/0/5/0/3/2/0/16/0/0/0/0 |
| Input/cc/button_behavior_ContentLayout | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 224/168/5/37/26/72/0/26/21/9/92/0/0/0/12 | 123/87/8/2/0/14/3/17/9/1/76/0/0/1/1 |
| Input/cc/svg_icon_behavior_PaintSvgIcon | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/293/4/71/57/143/3/27/23/22/88/4/1/0/36 | 224/196/7/32/25/84/9/17/8/7/88/1/0/6/22 |
| Input/cc/image_lookup_ImageLookupTexture | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 51/0/0/17/13/35/0/3/3/1/28/0/0/0/0 |
| Input/cc/image_behavior_ImageDrawPlanFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 95/56/2/15/3/27/2/10/8/1/32/0/0/2/8 |
| Input/cc/paint_queue_PaintImage | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/0/0/19/22/52/1/1/1/2/3/0/0/0/0 | 27/0/0/5/4/10/0/0/0/1/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 442/405/22/88/57/181/12/48/26/33/59/11/0/0/41 | 261/197/11/38/27/100/8/30/19/19/54/0/0/2/32 |
| Input/cc/dpi_ScalePixels | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 83/72/1/0/0/0/0/10/9/8/21/0/0/0/14 | 42/0/0/0/0/7/0/7/4/0/19/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuRowTop | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 113/93/7/21/13/42/4/18/11/9/31/4/0/0/12 | 93/61/7/3/3/21/5/14/7/4/26/0/0/1/11 |
| Input/cc/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/14/1/0/0/1/1/2/1/0/0/0/0/2/4 |
| Input/cc/tree_input_TreeTakeDismissalAt | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 55/17/1/1/0/4/2/5/4/4/0/0/0/0/1 |
| Input/cc/tree_TreeSetPopup | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 38/0/0/0/0/8/1/5/5/2/0/1/0/1/0 |
| Input/cc/drawing_props_StyleTokenColorOr | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/tree_TreePushScope | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 81/64/5/0/0/13/9/9/4/8/0/8/0/0/12 |
| Input/cc/contextmenu_ContextMenuTextLeft | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 25/0/0/1/1/2/0/1/1/2/2/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuLevelChecks | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 26/13/4/0/0/8/2/7/3/0/0/0/0/0/4 |
| Input/cc/contextmenu_ContextMenuInlineWidth | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/2/0/4/0/2/2/4/4/0/0/0/0 |
| Input/cc/contextmenu_ContextMenuInlineEnds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 24/6/1/0/0/9/5/3/2/0/0/0/0/0/0 |
| Input/cc/svg_icon_behavior_PaintStroke | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 635/588/8/74/327/438/14/31/23/21/71/0/0/0/35 | 182/92/5/18/42/76/9/11/6/4/78/0/0/1/19 |
| Input/cc/tree_End | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 3/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/cc/widget_benchmark_host_RecordBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 18/0/0/0/0/4/0/2/0/0/11/0/0/2/0 |
| Input/cc/paint_queue_ShapeCornerInset | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 60/0/0/1/1/2/0/6/6/4/17/0/0/0/0 | 33/6/1/0/0/4/2/5/4/0/23/0/0/0/1 |
| Input/cc/paint_queue_KeepShortPaintBytes | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 74/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Input/cc/paint_queue_KeepMediumPaintBytes | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 75/12/1/4/3/18/4/8/7/4/0/0/0/3/1 |
| Input/cc/paint_queue_KeepLongPaintBytes | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 69/12/1/5/2/19/5/8/7/4/0/0/0/3/1 |
| Input/cc/theme_DarkPalette | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/cc/theme_DefaultMotion | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 18/13/1/0/0/11/1/2/1/0/8/0/0/0/1 |
| Input/cc/theme_StateLayer | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 58/0/0/0/0/2/2/1/1/0/0/10/0/12/0 |
| Input/cc/card_behavior_CardDefaultFace | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 108/81/6/19/12/51/1/12/6/4/32/0/0/0/10 |
| Input/cc/theme_FontForRole | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 23/0/0/8/0/11/0/4/4/0/0/0/0/0/0 |
| Input/cc/theme_MotionDuration | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 59/47/5/6/0/21/6/9/3/1/28/0/0/0/3 |
| Input/cc/button_behavior_ButtonRoleFactsFor | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 35/22/1/9/3/19/0/3/2/2/0/0/0/0/0 | 29/24/1/7/4/11/0/2/1/2/0/0/0/0/2 |
| Input/cc/drawing_props_PaintPixelScale | 0 | 0/58 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconSlot | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 106/75/7/3/11/38/8/11/4/3/4/1/0/1/18 |
| Input/cc/svg_icon_behavior_SvgIconSnap | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 76/29/2/0/0/0/0/11/9/6/21/0/0/0/5 | 35/10/1/0/0/5/0/7/4/0/16/0/0/0/0 |
| Input/cc/svg_icon_behavior_SvgIconTexture | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 122/62/5/4/2/50/15/10/5/1/5/2/0/3/10 |
| Input/cc/image_behavior_ImageFitRect | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 34/0/0/14/9/26/0/0/0/1/0/0/0/0/0 | 8/0/0/7/0/7/0/1/0/0/4/0/0/0/0 |
| Input/cc/paint_queue_PaintImageWithPolygon | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 100/72/2/17/31/54/0/3/1/2/24/0/0/0/8 |
| Input/cc/svg_icon_behavior_StrokeAppendByte | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/cc/svg_icon_behavior_StrokeAppendNumber | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 228/194/7/10/16/40/7/36/29/13/9/0/6/0/35 | 122/102/6/0/6/38/14/13/5/0/6/4/0/7/26 |
| Input/cc/tree_TreePopScope | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 54/27/1/2/1/6/3/4/3/4/0/4/0/0/5 |
| Input/cc/paint_queue_ShapeSquareRoot | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 22/6/1/0/0/5/2/2/1/0/12/0/0/0/1 |
| Input/cc/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 171/146/15/4/5/48/8/26/11/2/0/0/0/8/29 |
| Input/cc/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 164/139/15/3/6/34/6/26/11/7/0/0/0/7/24 |
| Input/cc/svg_icon_behavior_SvgIconHash | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); loop constants/LICM candidates; memory/address folding candidates | 21/15/1/3/0/4/1/2/1/0/0/1/0/0/2 | 13/6/1/0/0/3/1/2/1/0/0/1/0/0/1 |
| Input/cc/svg_icon_behavior_SvgIconRetire | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 45/28/2/3/4/24/4/4/2/1/5/0/0/1/7 |
| Input/cc/svg_shape_ShapeRaster | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 260/218/10/32/17/69/6/50/40/14/14/2/0/0/39 | 133/94/6/8/5/35/6/18/12/4/9/1/0/0/22 |
| Input/cc/svg_icon_behavior_SvgIconCells | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 339/305/29/30/29/92/9/61/32/9/0/8/1/2/41 | 240/212/21/10/9/61/22/43/22/1/0/2/0/5/34 |
| Input/cc/image_behavior_ImageFitBounds | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 198/179/8/18/9/31/0/26/18/13/47/0/0/0/18 | 96/79/6/6/2/16/2/16/10/0/60/0/0/1/7 |
| Input/cc/image_behavior_ImagePolygonValid | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 103/87/3/9/1/12/1/19/16/7/12/0/0/0/12 | 33/13/1/0/0/7/2/8/7/0/11/0/0/0/1 |
| Input/cc/paint_queue_EmitImage | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 44/0/0/20/1/30/0/3/3/1/17/0/0/0/0 |
| Input/cc/paint_queue_ClipImage | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/2/8/0/0/0/1/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeNumber | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 349/308/20/55/19/93/15/60/40/19/54/1/0/0/48 | 157/144/12/0/0/48/24/31/19/0/20/0/0/0/21 |
| Input/cc/svg_shape_ShapeBuildPath | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 564/542/41/129/90/238/7/87/46/35/156/0/0/0/51 | 462/435/37/29/17/115/22/72/34/13/93/0/0/0/88 |
| Input/cc/svg_shape_ShapeCoverEdges | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 346/326/22/25/21/75/15/63/41/14/57/3/0/0/51 | 250/225/16/21/15/99/23/38/21/2/56/0/0/2/35 |
| Input/cc/paint_queue_EmitImageBegin | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 19/0/0/7/0/11/0/3/3/0/7/0/0/0/0 |
| Input/cc/widget_benchmark_host_RasterImage | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 45/0/0/2/1/7/0/2/0/1/17/0/0/1/0 |
| Input/cc/paint_queue_EmitImageEnd | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsDigit | 0 | 0/58 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeIsSpace | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddPoint | 0 | 0/58 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 14/0/0/0/0/8/3/1/1/0/2/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddSpan | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 140/126/5/0/0/16/4/18/13/11/38/0/0/0/22 | 85/56/3/0/0/18/12/13/6/0/39/0/0/0/5 |
| Input/cc/svg_shape_ShapeStrokeSubpath | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 612/530/15/90/49/179/20/64/49/38/268/0/3/0/61 | 384/358/20/16/12/95/23/46/25/7/185/0/3/0/38 |
| Input/cc/svg_shape_ShapeFillSubpath | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 5/0/0/0/0/1/0/2/1/0/0/0/0/0/0 |
| Input/cc/svg_shape_ShapeCollapse | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 68/64/3/0/0/16/8/8/5/2/17/0/0/0/15 | 52/34/3/0/0/21/8/7/4/0/14/0/0/0/4 |
| Input/cc/svg_shape_ShapeAddCircle | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 52/35/3/0/0/13/6/8/5/0/19/0/0/1/10 |
| Input/cc/svg_shape_ShapeSqrt | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 54/30/2/0/0/0/0/5/3/2/15/0/0/0/5 | 20/6/1/0/0/4/1/2/1/0/12/0/0/0/1 |
| Input/cc/svg_shape_ShapeQuad | 0 | 0/58 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/2/0/1/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeAbs | 0 | 0/58 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 9/0/0/0/0/1/0/0/0/0/8/0/0/0/0 |
| Input/cc/svg_shape_ShapeTriangle | 0 | 0/58 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/4/0/1/0/0/6/0/0/0/0 |
| Input/cc/svg_shape_ShapeAddEdge | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/6/1/3/3/0/5/0/0/1/0 |
| Input/cc/svg_shape_ShapeAddPolygon | 0 | 0/58 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/97/8/4/5/27/12/22/14/4/24/0/2/0/14 | 90/68/5/0/0/23/15/15/10/0/23/0/2/2/13 |
| Input/clang-19/widget_benchmarks_InputCase | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 394/8/2/53/124/218/8/37/35/26/35/0/0/0/2 | 161/0/0/13/24/70/0/19/19/12/23/0/0/0/0 |
| Input/clang-19/widget_benchmarks_BeginCase | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 46/0/0/8/8/31/0/2/2/4/0/0/0/0/0 | 21/0/0/0/0/13/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/tree_draw_BeginFrame | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 282/0/0/45/28/117/12/40/40/27/9/7/0/0/0 | 174/73/2/6/4/44/15/20/17/10/16/0/0/2/13 |
| Input/clang-19/tree_draw_EndFrame | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/18/12/32/0/6/6/9/0/0/0/0/0 | 49/0/0/0/0/1/0/6/6/6/0/0/0/2/0 |
| Input/clang-19/tree_input_TreePointerUpdate | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 420/0/0/46/20/225/46/35/35/29/27/46/0/0/0 | 206/111/2/8/1/67/23/30/28/7/18/6/0/5/23 |
| Input/clang-19/tree_TreeNodeAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetTextCount | 0 | 0/56 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetPaintChecksum | 0 | 0/56 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_request_TextRequestSend | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 517/385/7/99/42/226/20/75/68/40/6/12/0/0/72 | 201/164/14/26/7/59/4/31/17/13/2/1/0/0/15 |
| Input/clang-19/text_request_TextRequestClear | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 70/11/3/7/13/36/6/7/4/4/6/1/0/0/3 | 15/0/0/0/0/1/0/2/1/1/0/1/0/0/0 |
| Input/clang-19/widget_benchmark_host_WidgetPaintReset | 0 | 0/56 0% | memory/address folding candidates | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/3/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_SecondaryPointerSample | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 47/0/0/12/11/31/1/1/1/3/0/1/0/0/0 | 24/0/0/0/0/4/3/1/1/2/0/0/0/1/0 |
| Input/clang-19/session_SessionFlagSet | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 53/20/2/1/1/2/0/9/7/4/0/0/0/1/1 | 9/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintSnapshotInvalidate | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 114/0/0/22/23/87/6/1/1/5/0/4/0/0/0 | 37/0/0/0/0/16/8/1/1/2/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeKeyboardRoute | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 646/582/19/92/85/241/22/105/86/59/12/7/0/0/96 | 378/237/17/42/15/132/43/58/41/21/26/5/0/4/35 |
| Input/clang-19/text_editing_TextEditingFrameBegin | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 24/0/0/2/2/8/0/2/2/2/0/0/0/0/0 | 8/0/0/0/0/2/0/2/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintClear | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 104/0/0/17/22/89/8/0/0/1/0/8/0/0/0 | 21/0/0/0/0/13/8/0/0/1/0/0/0/2/0 |
| Input/clang-19/tree_TreeStart | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/61/1/47/46/210/26/6/5/8/0/25/0/0/14 | 73/0/0/2/2/30/22/4/4/4/6/1/0/0/0 |
| Input/clang-19/tree_TreeFrameStatus | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/3/2/8/1/1/1/2/0/1/0/0/0 | 13/0/0/0/0/2/1/1/1/1/0/1/0/0/0 |
| Input/clang-19/textfield_TextFieldKey | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 25/0/0/4/0/4/0/3/3/2/0/0/0/0/0 | 8/0/0/2/0/2/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextSelectionRangeForLength | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 51/38/1/10/7/20/0/8/7/2/0/0/0/0/3 | 20/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/text_context_menu_TextMenuTakeCommand | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 153/110/7/23/10/47/0/31/24/14/0/0/0/0/18 | 64/21/1/2/0/12/1/12/11/3/0/0/0/0/2 |
| Input/clang-19/text_context_menu_TextMenuPasteText | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 23/0/0/5/5/15/1/0/0/1/0/0/0/0/0 | 3/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/system_clipboard_SystemClipboardSet | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputNoteFocused | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 79/0/0/15/15/49/4/3/3/6/0/4/0/0/0 | 42/0/0/0/0/18/4/4/4/3/2/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuIsOpenFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 43/0/0/2/0/10/0/12/12/3/0/0/0/0/0 | 12/0/0/0/0/4/0/4/4/0/0/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuShow | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/2/2/15/0/0/0/1/0/0/0/0/0 | 9/0/0/1/0/7/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/session_SessionFlagGet | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 37/0/0/0/0/0/0/6/6/3/0/0/0/1/0 | 9/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/tree_draw_CommitFrame | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 139/0/0/38/28/71/0/6/6/20/0/0/0/0/0 | 87/44/2/0/0/18/10/9/7/11/0/0/0/2/7 |
| Input/clang-19/paint_queue_PaintSnapshotEmit | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 547/471/8/86/137/341/17/25/17/29/32/5/0/0/49 | 352/263/4/42/41/166/29/36/32/15/48/1/0/6/40 |
| Input/clang-19/tree_input_TreeTextFocused | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 85/57/2/2/2/25/7/13/11/6/0/5/0/0/13 | 41/9/1/0/0/10/5/5/4/1/0/1/0/1/1 |
| Input/clang-19/tree_input_TreeTextKind | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 7/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeFocusAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/16/9/28/1/18/18/11/0/0/0/0/0 | 56/0/0/4/0/6/1/9/9/5/0/0/0/0/0 |
| Input/clang-19/tree_TreeInputAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 140/8/2/10/38/97/9/9/7/3/13/5/0/0/2 | 63/0/0/0/0/31/10/5/5/3/11/1/0/2/0 |
| Input/clang-19/tree_TreeCount | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_input_TreeInputAllowedAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 30/0/0/6/6/12/0/2/2/2/0/0/0/0/0 | 26/0/0/0/0/0/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/tree_TreeTextRange | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 195/29/2/37/18/85/9/24/22/11/2/5/0/0/4 | 117/48/4/0/0/32/21/16/12/2/0/2/0/2/3 |
| Input/clang-19/tree_input_TreeFocusedAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 73/0/0/6/5/25/5/13/13/6/0/3/0/0/0 | 54/8/1/0/0/7/4/7/6/3/0/1/0/1/2 |
| Input/clang-19/geometry_RectangleIntersection | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 118/68/4/18/13/33/0/12/8/4/48/0/0/0/4 | 38/0/0/0/0/0/0/3/3/0/34/0/0/0/0 |
| Input/clang-19/tree_TreePointerActivatable | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 125/0/0/0/0/0/0/36/36/13/0/0/0/0/0 | 29/18/1/0/0/0/0/6/5/0/0/0/0/0/5 |
| Input/clang-19/tree_input_TreeActivateAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 57/0/0/13/5/26/2/3/3/5/0/2/0/0/0 | 35/0/0/5/0/9/2/3/3/3/0/1/0/0/0 |
| Input/clang-19/cursor_CursorHoverFrameStart | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 48/0/0/6/6/23/3/3/3/3/0/3/0/0/0 | 16/0/0/0/0/6/2/2/2/1/0/0/0/1/0 |
| Input/clang-19/tree_TreeSubmit | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 847/626/10/105/114/442/62/83/73/33/27/56/0/1/145 | 413/367/6/42/32/165/45/52/46/12/38/7/0/15/66 |
| Input/clang-19/text_input_TextSelectionRangeFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 29/0/0/5/9/16/0/2/2/0/0/0/0/0/0 | 8/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/system_clipboard_SystemClipboardText | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 168/146/16/22/8/37/3/36/20/14/0/0/0/0/25 | 65/17/2/0/0/5/1/9/7/2/0/0/0/0/1 |
| Input/clang-19/input_protocol_PointInBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 75/0/0/8/0/8/0/15/15/4/18/0/0/0/0 | 22/0/0/0/0/0/0/5/5/0/13/0/0/0/0 |
| Input/clang-19/text_context_menu_TextMenuOpenAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 88/51/3/14/9/44/0/10/7/7/2/0/0/0/6 | 55/14/1/3/0/16/1/5/4/3/2/0/0/0/2 |
| Input/clang-19/tree_TreePrevious | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 89/17/1/10/7/33/4/10/9/6/0/4/0/0/0 | 61/0/0/0/1/12/2/6/6/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeCurrentParent | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 45/0/0/2/2/16/5/5/5/3/0/4/0/0/0 | 16/0/0/0/0/4/2/2/2/1/0/1/0/0/0 |
| Input/clang-19/tree_input_TreeTakeActivationAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 12/0/0/3/2/6/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeTakeDragAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 289/0/0/41/28/147/21/33/33/17/17/21/0/0/0 | 155/35/3/8/1/45/10/27/24/6/8/6/0/0/6 |
| Input/clang-19/tree_TreePreviousWidgetState | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 108/0/0/17/11/60/6/9/9/5/0/6/0/0/0 | 41/0/0/0/0/14/6/5/5/1/4/1/0/1/0 |
| Input/clang-19/tree_input_TreeHasFocus | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 101/71/3/11/8/35/6/14/11/8/0/3/0/0/14 | 68/27/2/2/2/13/5/9/7/3/0/1/0/0/4 |
| Input/clang-19/tree_input_TreeBlurAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 59/0/0/3/3/20/5/11/11/5/0/3/0/0/0 | 26/0/0/0/0/8/6/3/3/1/0/1/0/1/0 |
| Input/clang-19/textfield_TextFieldFaceFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 145/134/4/24/61/91/4/9/5/8/28/0/0/0/10 | 97/0/0/6/25/48/1/1/1/12/30/0/0/0/0 |
| Input/clang-19/style_ResolveFont | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/4/4/3/0/0/0/0/0 | 11/4/1/0/0/0/0/2/1/0/0/0/0/0/0 |
| Input/clang-19/style_StyleFontValue | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 54/0/0/0/0/0/0/6/6/5/9/0/0/0/0 | 16/0/0/0/0/3/0/4/4/1/7/0/0/0/0 |
| Input/clang-19/textfield_TextFieldSecureDisplay | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 64/38/4/16/5/27/1/10/6/3/0/0/0/0/5 | 57/8/1/0/0/2/2/5/4/2/0/0/0/0/1 |
| Input/clang-19/font_metrics_GlyphWidth | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 14/0/0/4/4/8/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextFieldScrollFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 78/0/0/10/16/28/0/8/8/4/8/0/0/0/0 | 34/0/0/0/0/3/0/4/4/1/7/0/0/2/0 |
| Input/clang-19/textfield_TextFieldCursorAtX | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 100/68/4/16/15/31/0/9/5/9/0/0/0/0/7 | 57/22/1/4/3/8/1/2/1/3/0/0/0/0/0 |
| Input/clang-19/text_input_TextPointerOffset | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 69/0/0/0/0/0/0/7/7/7/18/0/0/0/0 | 27/0/0/0/0/6/0/5/5/1/15/0/0/0/0 |
| Input/clang-19/textfield_TextFieldCodepoints | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 34/26/2/5/2/8/1/4/2/2/0/0/0/0/4 | 30/8/1/0/0/2/1/3/2/1/0/0/0/0/1 |
| Input/clang-19/text_TextSpan | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/5/3/11/1/0/0/0/0/0/0/0/0 | 9/0/0/0/0/2/1/0/0/0/0/0/0/0/0 |
| Input/clang-19/textfield_TextFieldByteAfter | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 54/42/1/10/4/14/0/5/4/4/0/0/0/0/4 | 26/8/1/0/0/1/1/4/3/1/0/0/0/0/0 |
| Input/clang-19/text_input_CompositionTransitionFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 269/97/2/84/42/171/0/18/16/11/0/0/0/0/3 | 110/98/3/0/0/37/4/14/11/0/19/0/0/0/11 |
| Input/clang-19/textfield_TextFieldNext | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextSelectionAfterMove | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 34/0/0/5/10/17/0/5/5/0/0/0/0/0/0 | 8/0/0/0/0/1/1/0/0/0/0/0/0/1/0 |
| Input/clang-19/textfield_TextFieldInputTextAllowed | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 53/19/1/7/0/10/2/9/8/3/0/0/0/0/3 | 25/12/1/0/0/3/2/4/3/0/0/0/0/0/3 |
| Input/clang-19/textfield_TextFieldReplacementAllowed | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 157/29/4/26/11/46/2/23/19/15/0/0/0/0/4 | 120/41/5/5/4/22/8/17/12/4/0/0/0/0/7 |
| Input/clang-19/text_input_TextEditCommandDecisionFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 189/165/4/28/19/62/0/47/43/13/0/0/0/0/20 | 106/0/0/2/0/2/0/12/12/0/0/0/0/6/0 |
| Input/clang-19/textfield_TextFieldEditFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/4/1/48/39/113/2/18/17/7/2/0/0/0/1 | 113/31/3/3/0/25/2/17/14/4/5/0/0/3/3 |
| Input/clang-19/text_request_TextRequestTake | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 190/0/0/43/24/110/10/16/16/11/0/9/0/0/0 | 76/0/0/1/0/18/2/9/9/4/3/1/0/0/0 |
| Input/clang-19/text_request_TextRequestChangeFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 259/242/12/47/26/98/5/36/24/13/0/0/0/0/24 | 145/55/6/6/10/39/8/22/16/5/0/0/0/2/0 |
| Input/clang-19/font_metrics_GlyphCaret | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 72/61/2/16/12/32/0/6/4/5/0/0/0/0/6 | 32/0/0/1/3/8/0/2/2/1/2/0/0/0/0 |
| Input/clang-19/text_input_TextFieldRevealScroll | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 61/54/3/0/0/0/0/9/6/5/0/0/0/0/6 | 20/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_StyleOpacityValue | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 49/0/0/0/0/0/0/4/4/3/9/0/0/0/0 | 11/0/0/0/0/2/0/2/2/0/6/0/0/0/0 |
| Input/clang-19/tree_TreeSetInteractive | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 108/0/0/19/9/56/8/9/9/6/0/8/0/0/0 | 54/0/0/1/1/13/4/5/5/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetCaptureTab | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 90/0/0/15/9/38/4/10/10/8/0/4/0/0/0 | 50/0/0/0/0/9/2/6/6/5/0/1/0/1/0 |
| Input/clang-19/tree_input_TreeFocusSubmittedAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 225/4/1/39/26/96/10/28/27/14/8/7/0/0/1 | 127/15/1/3/3/34/15/20/19/6/8/2/0/3/2 |
| Input/clang-19/tree_TreeSetWidgetState | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 186/96/1/37/14/101/12/15/14/7/0/12/0/1/18 | 112/43/4/0/0/38/13/19/15/4/4/1/0/2/3 |
| Input/clang-19/tree_TreeSetSemantic | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 120/75/2/16/8/45/6/15/13/9/0/6/0/0/15 | 64/0/0/0/0/12/4/7/7/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetLabel | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetStyleClass | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetStyleId | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 75/0/0/10/7/31/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetSelected | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 80/0/0/13/7/34/4/9/9/6/0/4/0/0/0 | 46/0/0/0/0/10/3/5/5/4/0/1/0/1/0 |
| Input/clang-19/tree_TreeSetTextValue | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 285/227/4/47/34/166/24/19/15/10/0/22/0/1/38 | 138/86/3/8/4/47/12/22/19/5/0/1/0/2/6 |
| Input/clang-19/surface_Opacity | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 56/0/0/2/2/4/0/5/5/6/9/0/0/2/0 | 25/0/0/0/0/3/0/2/2/1/13/0/0/1/0 |
| Input/clang-19/paint_queue_PaintRoundedOutline | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 172/4/1/16/87/145/2/2/1/2/26/0/0/0/1 | 56/0/0/5/18/27/0/0/0/2/17/0/0/0/0 |
| Input/clang-19/font_metrics_GlyphLineHeight | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextFieldPaintFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 173/0/0/21/19/52/1/13/13/12/46/0/1/0/0 | 73/0/0/2/0/14/0/8/8/1/32/0/0/2/0 |
| Input/clang-19/theme_CurrentMuted | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentAccent | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintClippedLabel | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 175/4/1/18/84/148/2/2/1/2/25/0/0/0/1 | 59/0/0/8/18/30/0/0/0/2/17/0/0/0/0 |
| Input/clang-19/paint_queue_zi_default_7a97e54d75f71213 | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 13/0/0/2/3/6/0/0/0/0/3/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/1/0/0/0/0 |
| Input/clang-19/text_input_TextCompositionUnderlineY | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 19/8/1/0/0/0/0/3/2/1/0/0/0/0/0 | 6/0/0/0/0/1/1/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextCompositionUnderlineEndX | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 25/18/1/0/0/0/0/4/3/2/0/0/0/0/2 | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_PaintLine | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 164/4/1/15/86/137/2/2/1/2/24/0/0/0/1 | 46/0/0/3/13/20/0/0/0/2/11/0/0/0/0 |
| Input/clang-19/tree_TreeSubmittedNodeAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/3/11/2/1/0/1/2/0/0/0/1 | 7/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/button_Draw | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 21/4/1/5/5/10/2/1/0/2/2/0/0/0/1 | 13/0/0/0/0/0/0/1/0/1/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputFocusFrameBegin | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 86/0/0/16/15/53/6/3/3/5/0/6/0/0/0 | 37/0/0/0/0/12/4/5/5/3/2/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputFocused | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 65/0/0/11/11/32/3/5/5/5/0/3/0/0/0 | 39/0/0/0/0/7/4/6/6/3/0/0/0/0/0 |
| Input/clang-19/text_input_platform_TextInputOwnerMatches | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/6/2/8/0/6/6/2/0/0/0/0/0 | 9/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreeDefer | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 64/17/1/7/3/23/6/4/3/3/0/6/0/0/6 | 41/0/0/0/0/9/5/9/9/1/0/0/0/1/0 |
| Input/clang-19/text_context_menu_TextMenuDraw | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 400/350/2/41/98/270/21/16/14/13/26/16/0/0/82 | 140/0/0/6/33/75/0/12/12/5/18/0/0/2/0 |
| Input/clang-19/tree_draw_FrameInputCommit | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 39/0/0/7/5/30/5/0/0/1/0/3/0/0/0 | 27/0/0/0/0/15/10/0/0/1/0/0/0/1/0 |
| Input/clang-19/paint_queue_PaintOverflow | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/0/0/1/0 |
| Input/clang-19/tree_TreeCancel | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; memory/address folding candidates | 148/0/0/23/30/128/13/0/0/1/0/13/0/0/0 | 20/0/0/0/0/14/13/0/0/1/0/1/0/0/0 |
| Input/clang-19/tree_TreeFinish | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 495/115/2/45/53/319/56/33/31/16/0/55/0/0/41 | 244/186/8/10/10/137/25/42/34/6/30/1/0/3/30 |
| Input/clang-19/paint_queue_PaintOrder | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 278/231/8/41/34/149/17/29/21/10/4/13/0/1/43 | 149/96/6/12/8/59/11/22/16/4/6/3/0/2/11 |
| Input/clang-19/paint_queue_SnapshotTextRead | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 164/0/0/15/11/56/10/22/22/9/0/4/2/0/0 | 55/21/1/0/0/13/4/7/6/2/0/0/0/2/4 |
| Input/clang-19/paint_queue_SnapshotTextJoin | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 461/409/28/64/72/178/14/91/63/13/0/4/2/2/43 | 192/151/10/6/4/37/5/34/24/4/0/0/0/4/15 |
| Input/clang-19/paint_queue_PaintPointCount | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates | 23/0/0/5/0/5/0/5/5/2/0/0/0/0/0 | 17/0/0/1/0/5/0/1/1/0/0/1/0/0/0 |
| Input/clang-19/paint_queue_EmitPaint | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 55/8/2/10/10/24/4/6/4/4/4/0/0/0/2 | 46/0/0/1/1/20/0/3/3/2/6/0/0/0/0 |
| Input/clang-19/tree_input_TreeTopModalLayer | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreeButtonLike | 0 | 0/56 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_input_PointInCircleBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/8/4/12/0/1/1/2/27/0/0/0/0 | 34/0/0/4/4/9/0/1/1/1/24/0/0/0/0 |
| Input/clang-19/tree_TreeOverlayLayerAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 11/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/std_utf8_Decode | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 302/216/5/65/19/100/2/42/37/15/0/0/0/1/27 | 107/38/2/0/0/10/5/24/22/0/0/0/0/4/12 |
| Input/clang-19/std_grapheme_Push | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 397/303/3/27/39/70/0/103/100/27/0/0/1/0/69 | 179/150/9/0/0/12/0/24/15/1/0/0/0/6/48 |
| Input/clang-19/tree_TreeParentGeneration | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 97/0/0/12/4/44/8/6/6/5/0/8/0/0/0 | 36/0/0/0/0/7/6/5/5/1/0/1/0/2/0 |
| Input/clang-19/tree_TreeIdentityHash | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 33/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 15/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang-19/tree_TreeExplicitExists | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 97/42/1/14/9/45/7/4/3/5/0/5/0/0/9 | 82/34/2/4/3/21/11/10/8/2/0/4/0/4/3 |
| Input/clang-19/system_clipboard_host_SystemClipboardOpenHost | 0 | 0/56 0% | memory/address folding candidates | 3/0/0/0/0/2/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/1/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/system_clipboard_host_SystemClipboardByteHost | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/4/1/5/5/2/0/0/0/0/0 | 9/0/0/0/0/3/1/2/2/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleControlRoleFacts | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 30/0/0/10/4/21/0/0/0/1/0/0/0/0/0 | 16/0/0/3/0/14/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/textfield_TextFieldDefaultFace | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 71/4/1/4/39/46/2/1/0/6/25/0/0/0/1 | 29/0/0/0/0/17/0/0/0/6/12/0/0/0/0 |
| Input/clang-19/style_sheet_ResolveInstalledStyleRules | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 286/253/16/59/45/146/24/35/19/11/14/2/0/4/38 | 252/184/4/17/17/100/14/32/28/4/0/0/0/12/35 |
| Input/clang-19/widget_benchmark_host_MeasureGlyphWidth | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 76/70/6/3/0/4/1/19/13/6/0/0/1/0/12 | 67/48/3/0/0/5/5/14/11/0/0/0/0/2/8 |
| Input/clang-19/text_input_TextUtf8Continuation | 0 | 0/56 0% |  | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextCursorForLength | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 33/26/1/0/0/0/0/5/4/3/0/0/0/0/4 | 8/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_CompositionBoundaryFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 40/21/2/5/1/7/1/7/5/4/0/0/0/0/2 | 27/9/1/0/0/3/2/3/2/0/0/0/0/0/2 |
| Input/clang-19/tree_TreeEnsureInteraction | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 149/69/1/29/21/85/8/8/7/4/0/8/0/1/13 | 96/44/4/0/0/22/6/15/11/4/2/1/0/6/5 |
| Input/clang-19/tree_TreeEnsureSemantic | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 162/70/2/28/32/94/10/9/7/4/2/8/0/1/15 | 101/51/4/0/0/28/12/15/11/4/7/1/0/4/5 |
| Input/clang-19/tree_TreeStoreLabel | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 467/441/23/81/32/181/25/64/41/20/0/16/1/2/70 | 437/199/19/14/9/123/56/71/52/4/4/2/0/16/26 |
| Input/clang-19/paint_queue_ClipShape | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 52/4/1/19/16/39/2/3/2/1/2/0/0/0/1 | 30/0/0/1/0/13/0/2/2/2/8/0/0/0/0 |
| Input/clang-19/surface_Unit | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/3/3/2/8/0/0/0/0 | 6/0/0/0/0/1/0/0/0/0/5/0/0/0/0 |
| Input/clang-19/paint_queue_EmitRoundedOutline | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 105/8/2/29/22/52/4/8/6/7/14/0/0/0/2 | 58/0/0/5/2/15/0/7/6/1/34/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_MeasureGlyphLineHeight | 0 | 0/56 0% |  | 5/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/text_input_TextFieldCursorHeightFor | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 86/60/7/0/0/0/0/18/11/13/10/0/0/0/12 | 19/0/0/0/0/2/0/2/2/1/4/0/0/0/0 |
| Input/clang-19/paint_queue_EmitLabel | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 174/0/0/82/58/142/0/8/8/7/4/0/0/0/0 | 85/0/0/3/0/49/0/10/10/6/14/0/0/0/0 |
| Input/clang-19/paint_queue_ClipLabel | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 89/8/2/28/21/56/4/6/4/2/7/0/0/0/2 | 38/0/0/1/0/15/0/5/5/2/13/0/0/0/0 |
| Input/clang-19/paint_queue_EmitLine | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 306/160/4/56/18/74/0/39/35/18/83/0/0/0/16 | 86/0/0/1/0/14/0/21/21/0/58/0/0/0/0 |
| Input/clang-19/button_SubmitButton | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 619/598/18/167/121/310/20/59/41/56/51/0/0/0/60 | 380/11/1/47/27/125/0/21/20/38/35/2/0/3/1 |
| Input/clang-19/system_clipboard_SystemClipboardHasText | 0 | 0/56 0% |  | 8/0/0/0/0/0/0/0/0/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/1/0/0/0/0/0 |
| Input/clang-19/contextmenu_Draw | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 980/945/36/232/240/560/28/93/57/58/87/16/0/0/95 | 588/511/31/82/99/249/33/78/47/19/99/14/0/3/58 |
| Input/clang-19/cursor_CursorHoverFrameCommit | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 39/0/0/3/2/14/3/5/5/3/0/3/0/0/0 | 15/0/0/0/0/4/1/2/2/1/0/0/0/1/0 |
| Input/clang-19/paint_queue_PaintMeshPointCount | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/5/1/1/1/2/0/7/6/3/0/1/0/0/0 | 11/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang-19/paint_queue_ResolvePaintText | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 72/0/0/11/7/48/0/3/3/4/0/0/0/0/0 | 31/0/0/0/0/17/0/2/2/2/0/0/0/0/0 |
| Input/clang-19/tree_TreeModalLayer | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 16/0/0/3/3/9/1/0/0/1/0/1/0/0/0 | 8/0/0/0/0/2/1/0/0/1/0/1/0/0/0 |
| Input/clang-19/std_utf8_Continuation | 0 | 0/56 0% | branch layout/loop rotation (SSA lane) | 8/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/std_grapheme_data_Property | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 142/0/0/13/3/26/7/18/18/10/0/0/3/2/0 | 38/0/0/0/0/11/6/4/4/0/0/1/0/3/0 |
| Input/clang-19/std_grapheme_IsControl | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 22/0/0/0/0/0/0/6/6/1/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/session_WidgetPartKey | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 36/0/0/0/0/0/0/2/2/2/0/3/0/2/0 | 19/0/0/0/0/0/0/0/0/0/0/3/0/2/0 |
| Input/clang-19/widget_key_WidgetRepeatedPart | 0 | 0/56 0% |  | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleControlFacts | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 43/0/0/13/12/32/0/0/0/1/0/0/0/0/0 | 15/0/0/2/0/12/0/0/0/0/3/0/0/0/0 |
| Input/clang-19/theme_CurrentSunken | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentText | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_ShapeSmall | 0 | 0/56 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/theme_TypeLabel | 0 | 0/56 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/style_sheet_StyleBucket | 0 | 0/56 0% |  | 9/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleFirstBit | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 117/99/6/0/0/0/0/18/12/11/0/0/0/5/33 | 35/0/0/0/0/4/0/0/0/0/0/0/0/6/0 |
| Input/clang-19/style_sheet_StyleRuleMatchesState | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/3/3/2/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/style_sheet_StyleSelectorMatches | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 158/0/0/28/0/28/0/17/17/24/0/0/0/0/0 | 66/0/0/2/0/26/0/22/22/0/0/0/0/0/0 |
| Input/clang-19/style_MergeValues | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 209/4/1/61/31/95/2/27/26/0/32/0/0/0/1 | 161/152/25/1/0/54/0/50/25/1/30/0/0/0/44 |
| Input/clang-19/tree_TreeLabelSpanAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 46/0/0/6/5/29/4/1/1/1/0/4/0/0/0 | 21/0/0/0/0/10/7/2/2/0/0/1/0/0/0 |
| Input/clang-19/raster_shape_FillRoundedRectangle | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/paint_queue_FillShapeClipped | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 179/162/5/50/39/89/0/12/7/11/84/0/0/0/7 | 132/0/0/24/13/39/0/10/9/3/109/0/0/0/0 |
| Input/clang-19/paint_queue_EmitPaintNow | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 297/271/23/44/36/91/7/60/37/8/2/0/0/2/27 | 166/39/3/6/1/33/7/30/26/7/6/0/0/5/5 |
| Input/clang-19/tree_TreeCurrentPaintGeometry | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 125/4/1/34/34/92/4/7/6/3/15/2/0/0/1 | 60/0/0/0/0/19/9/5/5/2/10/2/0/1/0 |
| Input/clang-19/tree_TreeSubmittedGeometryAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 118/0/0/20/24/81/4/8/8/4/13/4/0/0/0 | 50/0/0/0/0/21/6/4/4/3/10/1/0/1/0 |
| Input/clang-19/raster_shape_StrokeRoundedRectangle | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/text_typeface_TextTypefaceUse | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 56/0/0/10/5/25/0/7/7/3/0/0/0/0/0 | 15/0/0/0/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/raster_text_PaintTextRotated | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 55/0/0/10/8/18/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang-19/raster_text_PaintTextItalicClipped | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 119/0/0/33/19/52/0/7/7/7/6/0/0/0/0 | 75/21/1/7/4/11/0/6/5/2/11/1/0/2/1 |
| Input/clang-19/raster_text_PaintTextEmphasisClipped | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 112/22/1/21/13/34/0/11/10/7/6/0/1/0/0 | 58/0/0/2/2/4/0/4/4/1/9/1/0/2/0 |
| Input/clang-19/raster_text_PaintTextClipped | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 49/0/0/7/5/12/0/5/5/3/6/0/0/0/0 | 8/0/0/0/0/0/0/2/2/0/5/0/0/0/0 |
| Input/clang-19/raster_text_PaintText | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/raster_StrokeLine | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 12/0/0/3/3/6/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/style_ResolveFlags | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); memory/address folding candidates | 38/0/0/3/4/9/0/6/6/0/0/0/0/0/0 | 17/0/0/0/0/0/0/0/0/0/0/0/0/2/0 |
| Input/clang-19/style_ResolveState | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 64/0/0/1/0/1/0/8/8/7/0/0/0/0/0 | 19/0/0/1/0/2/1/2/2/0/0/0/0/0/0 |
| Input/clang-19/tree_input_TreeHoveredAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/8/6/30/5/13/13/6/0/5/0/0/0 | 57/0/0/0/0/7/4/6/6/3/0/2/0/1/0 |
| Input/clang-19/button_ButtonDetailSize | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 102/0/0/17/16/45/0/12/12/6/8/0/0/0/0 | 45/0/0/2/0/8/0/4/4/2/5/2/0/1/0 |
| Input/clang-19/tree_TreeRootBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 74/0/0/14/14/45/3/5/5/5/5/3/0/0/0 | 37/0/0/0/0/6/3/3/3/3/4/3/0/0/0 |
| Input/clang-19/button_behavior_MeasureBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 336/314/6/50/26/102/0/56/50/19/91/0/0/0/27 | 188/0/0/17/10/49/0/25/25/2/115/0/0/0/0 |
| Input/clang-19/style_StyleMinHeightValue | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang-19/button_ButtonMinimumHeight | 0 | 0/56 0% |  | 5/0/0/0/0/0/0/1/1/0/0/0/0/0/0 | 1/0/0/0/0/0/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/button_ButtonFitContentFace | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 371/290/13/41/19/86/4/61/48/25/83/0/0/0/41 | 151/17/1/9/4/41/0/19/18/3/79/0/0/0/2 |
| Input/clang-19/tree_TreeSetCircleHit | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 78/0/0/8/4/33/6/9/9/5/0/6/0/0/0 | 40/0/0/0/0/12/4/5/5/3/0/1/0/2/0 |
| Input/clang-19/tree_TreeSetSemanticDetail | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 101/0/0/15/8/44/6/10/10/8/0/6/0/0/0 | 62/0/0/0/0/12/4/7/7/5/0/1/0/1/0 |
| Input/clang-19/button_PaintButtonForNode | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 1316/1279/16/257/184/508/12/135/119/93/320/0/1/0/133 | 801/492/2/168/115/396/0/58/56/42/340/4/0/7/47 |
| Input/clang-19/tree_TreeNextKey | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 69/15/1/6/5/32/6/5/3/4/0/6/0/0/4 | 60/0/0/0/0/10/8/5/5/2/0/4/0/4/0 |
| Input/clang-19/contextmenu_ContextMenuOverlayKey | 0 | 0/56 0% |  | 4/0/0/0/0/0/0/0/0/0/0/1/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/1/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuPanelKey | 0 | 0/56 0% |  | 7/0/0/0/0/0/0/0/0/0/0/0/0/0/0 | 3/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/tree_TreePreviousCommitted | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 69/0/0/10/7/27/3/8/8/5/0/3/0/0/0 | 44/0/0/0/1/6/3/4/4/2/0/1/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuRowKey | 0 | 0/56 0% | strength reduction candidates | 13/0/0/0/0/0/0/0/0/0/0/2/0/0/0 | 9/0/0/0/0/2/2/0/0/0/0/0/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuOpens | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 45/21/1/5/0/9/2/5/4/3/0/2/0/0/4 | 22/7/2/0/0/3/0/5/3/0/0/0/0/0/2 |
| Input/clang-19/contextmenu_ContextMenuFind | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 39/23/1/5/0/9/2/4/3/2/0/2/0/0/4 | 17/8/2/0/0/3/1/5/3/0/0/0/0/0/2 |
| Input/clang-19/contextmenu_ContextMenuBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 22/0/0/7/5/15/0/0/0/1/0/0/0/0/0 | 2/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/overlay_Draw | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 388/311/1/115/78/209/0/28/27/28/19/0/0/0/28 | 200/82/1/15/5/38/0/16/15/19/34/0/0/1/9 |
| Input/clang-19/contextmenu_ContextMenuLevel | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 1422/1205/10/372/349/778/10/72/62/142/317/1/0/0/114 | 940/723/13/211/90/371/26/63/50/101/289/2/0/1/99 |
| Input/clang-19/style_sheet_StyleDefaultFacts | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/7/22/35/0/0/0/0/0/0/0/0/0 | 8/0/0/0/0/5/0/0/0/0/3/0/0/0/0 |
| Input/clang-19/style_sheet_StyleMatchesOptional | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 15/0/0/0/0/0/0/1/1/1/0/0/0/0/0 | 6/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_RasterRoundedRectangle | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 42/0/0/5/4/12/0/3/3/4/7/0/0/0/0 | 30/0/0/0/0/7/0/4/4/1/13/0/0/1/0 |
| Input/clang-19/paint_queue_ShapeRadius | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 56/42/3/4/0/4/0/7/4/3/18/0/0/0/7 | 10/0/0/0/0/1/0/0/0/0/9/0/0/0/0 |
| Input/clang-19/paint_queue_FillClippedSpan | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 110/95/4/15/7/22/0/13/9/7/33/0/0/0/6 | 22/0/0/0/0/0/0/3/3/0/15/0/0/0/0 |
| Input/clang-19/paint_queue_KeepPaintBytes | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 88/0/0/25/11/51/1/4/4/6/0/1/0/0/0 | 24/0/0/0/0/2/1/3/3/3/0/0/0/1/0 |
| Input/clang-19/paint_queue_KeepMeshPoints | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 259/142/8/38/26/122/14/28/20/10/16/3/0/1/23 | 151/97/5/2/2/31/11/21/16/4/8/0/0/6/9 |
| Input/clang-19/text_emphasis_RunTextItalicRaster | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 19/0/0/1/0/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/text_emphasis_RunTextEmphasisRaster | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 29/0/0/5/5/14/0/1/1/2/0/0/0/0/0 | 18/0/0/0/1/4/0/1/1/1/0/0/0/0/0 |
| Input/clang-19/button_ButtonFaceFinish | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 40/31/2/5/3/11/2/9/7/2/5/0/0/0/6 | 26/0/0/1/0/3/0/3/3/4/0/0/0/0/0 |
| Input/clang-19/button_ButtonDefaultFace | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 282/191/4/51/75/142/6/36/32/19/46/0/0/0/36 | 175/91/2/24/12/79/1/18/15/14/28/0/0/0/15 |
| Input/clang-19/button_ButtonFaceFacts | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 53/42/2/15/3/25/0/5/3/3/0/0/0/0/4 | 37/0/0/1/0/8/0/1/1/3/0/0/0/0/0 |
| Input/clang-19/button_ButtonDetailFont | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 31/0/0/0/0/0/0/4/4/2/0/1/1/0/0 | 8/0/0/0/0/0/0/1/1/0/0/2/0/1/0 |
| Input/clang-19/style_StyleLineHeightValue | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/5/5/2/5/0/0/0/0 | 10/0/0/0/0/0/0/1/1/0/7/0/0/0/0 |
| Input/clang-19/style_FitHeight | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 71/50/4/0/0/0/0/9/5/5/14/0/0/0/6 | 11/0/0/0/0/0/0/1/1/0/9/0/0/0/0 |
| Input/clang-19/button_behavior_MeasureContent | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 157/141/5/4/9/15/0/28/23/9/40/0/0/0/14 | 42/0/0/0/0/1/0/7/7/0/30/0/0/0/0 |
| Input/clang-19/button_behavior_ShapeWidth | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 76/0/0/0/0/0/0/12/12/6/11/0/0/0/0 | 18/0/0/0/0/0/0/2/2/0/10/0/0/0/0 |
| Input/clang-19/theme_ControlHeightFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 41/4/1/9/4/14/2/4/3/3/5/0/0/0/1 | 24/4/1/0/0/7/0/6/5/0/6/0/0/0/1 |
| Input/clang-19/button_behavior_ButtonCornerRadius | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 32/0/0/0/0/0/0/4/4/2/7/0/0/0/0 | 13/0/0/0/0/3/0/0/0/0/12/0/0/0/0 |
| Input/clang-19/focus_FocusDefaultOutlineBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 73/53/1/7/5/16/2/6/5/4/26/0/0/0/6 | 28/0/0/0/0/3/0/2/2/1/22/0/0/0/0 |
| Input/clang-19/image_lookup_IconLookupImage | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 93/8/2/10/42/61/4/3/1/2/19/0/0/0/2 | 29/0/0/0/0/15/0/1/1/1/14/0/0/0/0 |
| Input/clang-19/button_behavior_ButtonPictureSize | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 56/40/3/0/0/0/0/10/7/5/8/0/0/0/6 | 9/0/0/0/0/1/0/1/1/0/5/0/0/0/0 |
| Input/clang-19/button_behavior_ButtonLabeledPictureSize | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 91/52/4/0/0/0/0/10/6/7/27/0/0/0/7 | 21/0/0/0/0/5/0/4/4/0/15/0/0/0/0 |
| Input/clang-19/button_behavior_ContentLayout | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 224/168/5/37/26/72/0/26/21/9/92/0/0/0/12 | 116/0/0/1/0/21/0/14/14/0/88/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_PaintSvgIcon | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 346/293/4/71/57/143/3/27/23/22/88/4/1/0/36 | 253/35/1/23/20/70/7/17/16/6/128/1/0/2/3 |
| Input/clang-19/image_lookup_ImageLookupTexture | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 81/4/1/13/6/38/2/11/10/4/2/0/0/0/1 | 32/13/1/1/0/11/0/3/2/1/2/0/0/0/2 |
| Input/clang-19/image_behavior_ImageDrawPlanFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 183/4/1/27/31/84/2/21/20/7/30/0/0/0/1 | 93/0/0/2/0/24/0/16/16/0/57/0/0/1/0 |
| Input/clang-19/contextmenu_ContextMenuLevelBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 442/405/22/88/57/181/12/48/26/33/59/11/0/0/41 | 267/183/14/44/15/86/6/33/19/22/50/1/0/0/34 |
| Input/clang-19/dpi_ScalePixels | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 83/72/1/0/0/0/0/10/9/8/21/0/0/0/14 | 34/0/0/0/0/7/0/5/5/1/22/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuRowTop | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 113/93/7/21/13/42/4/18/11/9/31/4/0/0/12 | 74/34/8/4/4/18/3/11/3/2/16/1/0/0/9 |
| Input/clang-19/overlay_behavior_DismissibleOverlayPolicyFor | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 51/0/0/2/10/13/0/15/15/2/0/0/0/0/0 | 22/0/0/0/0/0/0/0/0/0/0/0/0/3/0 |
| Input/clang-19/tree_input_TreeTakeDismissalAt | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 88/0/0/18/11/36/3/12/12/9/0/0/0/0/0 | 49/0/0/1/0/4/2/6/6/4/0/0/0/0/0 |
| Input/clang-19/tree_TreeSetPopup | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/0/0/4/3/28/6/8/8/4/0/6/0/0/0 | 31/0/0/0/0/11/4/4/4/3/0/1/0/1/0 |
| Input/clang-19/drawing_props_StyleTokenColorOr | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 78/70/3/8/1/25/5/11/8/5/0/3/0/0/14 | 2/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentSurface | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentBorder | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentDisabled | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/theme_CurrentSelection | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates | 8/0/0/1/0/2/0/0/0/1/0/0/0/0/0 | 5/0/0/0/0/2/0/1/1/0/0/0/0/0/0 |
| Input/clang-19/tree_TreePushScope | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 148/0/0/24/19/72/10/18/18/15/0/9/0/0/0 | 77/28/1/0/0/10/8/8/7/8/0/8/0/0/4 |
| Input/clang-19/contextmenu_ContextMenuTextLeft | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 30/6/1/3/3/6/0/3/2/3/4/0/0/0/0 | 18/0/0/1/1/2/0/1/1/2/2/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuLevelChecks | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 61/39/1/7/7/22/1/10/9/4/0/1/0/0/4 | 30/16/4/0/0/6/1/7/3/0/0/0/0/0/5 |
| Input/clang-19/contextmenu_ContextMenuInlineWidth | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 50/5/1/10/10/20/0/3/2/6/6/0/0/0/0 | 34/0/0/5/1/7/0/2/2/4/4/0/0/0/0 |
| Input/clang-19/contextmenu_ContextMenuInlineEnds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 58/0/0/7/0/15/4/9/9/2/0/4/0/0/0 | 17/0/0/0/0/5/3/3/3/0/0/2/0/0/0 |
| Input/clang-19/svg_icon_behavior_PaintStroke | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 635/588/8/74/327/438/14/31/23/21/71/0/0/0/35 | 197/162/7/14/36/70/7/19/12/4/85/0/0/0/26 |
| Input/clang-19/tree_End | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; memory/address folding candidates | 10/0/0/2/2/4/0/0/0/1/0/0/0/0/0 | 1/0/0/0/0/0/0/1/0/0/0/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_RecordBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 38/0/0/4/0/10/0/3/3/3/12/0/0/0/0 | 17/0/0/0/0/4/0/2/2/1/10/0/0/0/0 |
| Input/clang-19/paint_queue_KeepShortPaintBytes | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 80/58/3/16/3/36/6/7/4/5/0/4/0/0/10 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Input/clang-19/paint_queue_KeepMediumPaintBytes | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 82/60/3/16/3/36/6/7/4/5/0/4/0/0/11 | 67/12/1/0/0/12/5/6/5/4/0/0/0/2/0 |
| Input/clang-19/paint_queue_KeepLongPaintBytes | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 74/52/3/15/2/34/6/7/4/5/0/4/0/0/9 | 60/12/1/0/0/12/5/5/4/4/0/0/0/2/0 |
| Input/clang-19/theme_DarkPalette | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 87/4/1/2/41/45/2/1/0/1/2/0/0/0/1 | 22/0/0/0/0/20/0/0/0/0/20/0/0/0/0 |
| Input/clang-19/theme_DefaultMetrics | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; memory/address folding candidates | 101/4/1/4/36/43/2/1/0/0/62/0/0/0/1 | 21/0/0/0/0/19/0/0/0/0/18/0/0/0/0 |
| Input/clang-19/theme_DefaultMotion | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 60/0/0/6/22/33/0/1/1/0/26/0/0/0/0 | 12/0/0/0/0/8/0/1/1/0/4/0/0/0/0 |
| Input/clang-19/theme_StateLayer | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 211/200/1/8/4/12/0/17/16/15/0/7/4/9/40 | 60/0/0/0/0/0/0/1/1/0/0/10/0/12/0 |
| Input/clang-19/card_behavior_CardDefaultFace | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 115/4/1/17/47/68/2/10/9/4/25/0/0/0/1 | 74/0/0/7/0/31/1/9/8/3/9/0/0/0/0 |
| Input/clang-19/theme_ShapeMedium | 0 | 0/56 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/theme_FontForRole | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 94/0/0/15/11/56/0/5/5/4/0/0/0/0/0 | 15/0/0/0/0/7/4/2/2/0/0/0/0/0/0 |
| Input/clang-19/theme_MotionDuration | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 92/41/2/17/8/26/0/16/14/4/18/0/0/0/3 | 40/0/0/0/0/15/1/10/9/0/21/0/0/0/0 |
| Input/clang-19/drawing_props_PaintPixelScale | 0 | 0/56 0% | memory/address folding candidates | 6/0/0/0/0/4/0/0/0/0/3/0/0/0/0 | 3/0/0/0/0/2/0/0/0/0/2/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconSlot | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 204/182/3/27/24/110/15/16/13/11/0/14/0/0/42 | 166/44/4/6/7/62/19/20/16/3/10/5/0/0/4 |
| Input/clang-19/svg_icon_behavior_SvgIconSnap | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 76/29/2/0/0/0/0/11/9/6/21/0/0/0/5 | 31/0/0/0/0/5/0/3/3/1/24/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconTexture | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 195/98/4/26/16/97/16/13/9/3/0/12/1/0/19 | 180/96/8/7/3/68/30/19/11/1/9/2/0/7/26 |
| Input/clang-19/paint_queue_PaintImageWithPolygon | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 237/4/1/38/91/192/2/6/5/5/30/0/0/0/1 | 87/0/0/13/30/55/0/2/2/3/33/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_StrokeAppendByte | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; memory/address folding candidates | 21/0/0/3/0/8/1/2/2/1/0/0/0/0/0 | 6/0/0/0/0/3/1/1/1/0/0/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_StrokeAppendNumber | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 228/194/7/10/16/40/7/36/29/13/9/0/6/0/35 | 98/25/2/1/4/25/10/9/7/1/6/4/0/5/4 |
| Input/clang-19/tree_TreePopScope | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 84/0/0/12/9/39/5/9/9/8/0/5/0/0/0 | 52/10/1/0/0/6/4/6/5/4/0/4/0/0/2 |
| Input/clang-19/paint_queue___zi_spec_27a25ba88f17713a | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang-19/paint_queue___zi_spec_542e1e54e11043bf | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 242/192/8/30/25/108/13/27/19/10/10/4/0/1/34 | 134/89/4/1/2/31/8/21/17/6/0/0/0/7/14 |
| Input/clang-19/theme_ShapeSurface | 0 | 0/56 0% | calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 10/0/0/0/0/4/0/0/0/1/1/0/0/0/0 | 6/0/0/0/0/3/0/1/1/0/2/0/0/0/0 |
| Input/clang-19/svg_icon_behavior_SvgIconRetire | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 78/0/0/8/6/34/2/9/9/5/0/2/1/0/0 | 44/0/0/4/4/26/6/3/3/1/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeRaster | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 260/218/10/32/17/69/6/50/40/14/14/2/0/0/39 | 122/107/8/3/3/27/9/16/8/4/8/1/0/0/19 |
| Input/clang-19/image_behavior_ImageFitBounds | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 198/179/8/18/9/31/0/26/18/13/47/0/0/0/18 | 44/0/0/0/0/3/0/9/9/0/31/0/0/0/0 |
| Input/clang-19/image_behavior_ImagePolygonValid | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 103/87/3/9/1/12/1/19/16/7/12/0/0/0/12 | 39/33/2/0/0/5/3/3/1/0/13/0/0/0/3 |
| Input/clang-19/paint_queue_EmitImage | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 66/4/1/26/23/49/2/3/2/3/4/0/0/0/1 | 45/0/0/2/1/33/0/3/3/1/17/0/0/0/0 |
| Input/clang-19/paint_queue_ClipImage | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); loop constants/LICM candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 38/4/1/14/11/29/2/1/0/1/2/0/0/0/1 | 16/0/0/4/1/6/0/0/0/2/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeNumber | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 349/308/20/55/19/93/15/60/40/19/54/1/0/0/48 | 180/141/8/0/0/31/15/39/31/0/33/0/0/0/24 |
| Input/clang-19/svg_shape_ShapeCoverEdges | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; memory/address folding candidates | 346/326/22/25/21/75/15/63/41/14/57/3/0/0/51 | 191/157/16/10/9/59/22/28/12/3/44/0/0/3/12 |
| Input/clang-19/paint_queue_EmitImageBegin | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 73/0/0/12/5/30/0/6/6/3/6/0/0/0/0 | 25/0/0/2/0/15/0/2/2/0/11/0/0/0/0 |
| Input/clang-19/widget_benchmark_host_RasterImage | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses); memory/address folding candidates | 53/0/0/10/5/18/0/3/3/4/16/0/0/0/0 | 36/0/0/1/0/8/0/4/4/1/18/0/0/1/0 |
| Input/clang-19/paint_queue_EmitImageEnd | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); memory/address folding candidates | 21/0/0/2/2/14/0/0/0/0/0/0/0/0/0 | 7/0/0/0/0/6/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeIsDigit | 0 | 0/56 0% | branch layout/loop rotation (SSA lane) | 13/0/0/0/0/0/0/2/2/0/0/0/0/0/0 | 4/0/0/0/0/0/0/0/0/0/0/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeIsSpace | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 38/0/0/0/0/0/0/12/12/3/0/0/0/0/0 | 7/0/0/0/0/0/0/0/0/0/0/0/0/1/0 |
| Input/clang-19/svg_shape_ShapeAddPoint | 0 | 0/56 0% | calls/inlining candidates; memory/address folding candidates | 26/0/0/0/0/13/2/1/1/1/2/0/0/0/0 | 12/0/0/0/0/7/2/1/1/0/2/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAddSpan | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates | 140/126/5/0/0/16/4/18/13/11/38/0/0/0/22 | 90/16/2/0/0/24/17/13/11/1/37/0/0/0/1 |
| Input/clang-19/svg_shape_ShapeStrokeSubpath | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; vector idioms (inspect scalar SSE uses) | 612/530/15/90/49/179/20/64/49/38/268/0/3/0/61 | 582/426/24/44/26/194/49/61/37/8/322/0/3/10/73 |
| Input/clang-19/svg_shape_ShapeFillSubpath | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates; vector idioms (inspect scalar SSE uses) | 51/32/1/2/1/15/4/6/5/3/4/0/1/0/8 | 47/31/3/0/0/17/8/7/4/0/9/0/0/1/6 |
| Input/clang-19/svg_shape_ShapeCollapse | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates; loop constants/LICM candidates; memory/address folding candidates | 68/64/3/0/0/16/8/8/5/2/17/0/0/0/15 | 38/24/3/0/0/14/7/6/3/0/14/0/0/1/1 |
| Input/clang-19/svg_shape_ShapeAddCircle | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 64/52/1/4/4/14/4/5/4/2/29/0/1/0/8 | 44/33/3/0/0/12/6/6/3/0/18/0/0/2/9 |
| Input/clang-19/svg_shape_ShapeQuad | 0 | 0/56 0% | memory/address folding candidates | 16/0/0/0/0/10/8/1/1/0/8/0/0/0/0 | 10/0/0/0/0/8/0/1/0/0/8/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAbs | 0 | 0/56 0% | branch layout/loop rotation (SSA lane); calls/inlining candidates | 25/0/0/0/0/0/0/1/1/1/7/0/0/0/0 | 5/0/0/0/0/1/0/0/0/0/4/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeTriangle | 0 | 0/56 0% | memory/address folding candidates | 13/0/0/0/0/8/6/1/1/0/6/0/0/0/0 | 8/0/0/0/0/6/0/1/0/0/6/0/0/0/0 |
| Input/clang-19/svg_shape_ShapeAddEdge | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); calls/inlining candidates; strength reduction candidates; memory/address folding candidates | 49/0/0/2/4/18/1/3/3/2/6/1/0/0/0 | 19/0/0/0/0/8/4/3/3/0/5/0/0/1/0 |
| Input/clang-19/svg_shape_ShapeAddPolygon | 0 | 0/56 0% | stack traffic (regalloc/aggregate lane); branch layout/loop rotation (SSA lane); calls/inlining candidates; strength reduction candidates; loop constants/LICM candidates | 110/97/8/4/5/27/12/22/14/4/24/0/2/0/14 | 87/58/6/0/0/29/19/14/8/0/23/0/0/2/8 |
| Hello/cc/hello_main | 0 | 0/0 unknown |  | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
| Hello/clang-19/hello_main | 0 | 0/0 unknown |  | 4/0/0/0/0/1/0/1/1/0/0/0/0/0/0 | 2/0/0/0/0/1/0/1/0/0/0/0/0/0/0 |
