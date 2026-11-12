# Inspect direct assembly in VS Code

The repository tasks run the experimental direct backend's real supported
inspection path without rebuilding the compiler or falling back to C:

```text
source -> checked .zir -> lowered listing + .lirb
       -> optimized listing + optimization counters
       -> final ELF + native map -> text-relative disassembly
```

Use **Terminal: Run Build Task** and select **Ziran: Inspect assembly (scalar
fixture)**. It uses `tests/spec/assembly_scalar.zi` with entry
`assembly_scalar:Answer`. The **choose source and entry** task prompts for a
different source, entry and module root. The same operation is available at the
repository root:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY \
    -u DBUS_SESSION_BUS_ADDRESS HARMONY_DESKTOP_RECOVERY=0 \
    build-queue -- python3 scripts/inspect_assembly.py \
    --source tests/spec/assembly_scalar.zi \
    --entry assembly_scalar:Answer \
    --root tests/spec
```

Every successful run publishes only a complete stable
`build/vscode-assembly/inspection/` set:

| Artifact | Contents |
| --- | --- |
| `checked/*.zir` | canonical checked compiler output |
| `lowered.lir` / `lowered.lirb` | verified instruction listing and binary |
| `optimized.lir` | the selected `--opt` instruction listing |
| `optimization.json` | deterministic optimization transformation counters |
| `final.elf` / `final.map.json` | optimized native image and its byte map |
| `final.disasm` | actual final ELF disassembly with text-relative addresses |
| `manifest.json` | source, tool and artifact hashes plus exact commands |

The helper verifies that `final.map.json` identifies the complete SHA-256 of
`final.elf`. It reads `image.text_virtual_address` from that map and passes its
negative value to `objdump --adjust-vma`; no ELF base or scalar-fixture offset is
hardcoded. `final.disasm` records the same ELF identity and adjustment, while the
manifest hashes every advertised artifact.

`objdump` (normally from GNU binutils) is an explicit inspection dependency. It
does not assemble or link the program: `zi2asm` directly encodes the `.lirb` and
ELF. Pass `--objdump PATH` when a compatible tool has another name. Missing
tools, unsupported source, compiler errors, identity mismatches and disassembly
errors stop at the failing stage. A failed run does not replace the last complete
artifact set, and there is no fallback compiler.

The tasks require the existing `build/bin/zi2zir` and `build/bin/zi2asm`. If
native compiler work makes them stale, finish that work and rebuild through the
shared queue before inspecting:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY \
    -u DBUS_SESSION_BUS_ADDRESS HARMONY_DESKTOP_RECOVERY=0 \
    build-queue -- make assembly
```

This view exposes only the currently implemented checked IR, verified lowered
instructions, basic optimization counters, native range map, ELF and
disassembly. It does not claim machine IR, register-allocation state,
source/unwind debug metadata, profiler/JIT mapping, or support for examples
outside the direct backend's documented lowering boundary.
