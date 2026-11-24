# Check control

Two documented build modes, wired 2026-11-24:

- `--thorough`: `--checks` plus every optional compile-time verification.
- `--quickest`: skips the optional compile-time verification only.

Both modes keep every required gate, so the quickest build still refuses a
broken program at the points where a skipped check would hide it. Output is
identical between the default and `--quickest` modes; `--thorough` differs
only by the runtime checks recorded in ZX.

## Optional verification today

| Step | Where | Runs in quickest |
| --- | --- | --- |
| Optimizer input re-verification | `compiler_opt.zi` `LirOptBuild` | no; `compiler_zx_run.zi` `LowerBuild` verified the lowered IR first |
| Optimizer per-pass re-verification | `compiler_opt.zi` `VerifyPass`, `CloneProgram`; `compiler_opt_ssa.zi` `LirSsaSerialize` | no |
| Encoder allocated-program re-check | `compiler_x86_64.zi`, `compiler_aarch64.zi`, `compiler_armv7.zi` `Process`; `compiler_x86_image.zi` `MachineX64ImageMeasure` via `MachineVerifySelfBuilt` | no; the allocator verified the program it published |

## Required in both modes

- `compiler_zx_run.zi` `LowerBuild` verifies the lowered IR before the
  optimizer sees it.
- `compiler_regalloc.zi` `Attempt` verifies the input and fuel placement, and
  verifies the allocated program it publishes.
- The `.zx` reader and linker verify the bytes and structure they receive.
  The loader's verification is not a build switch.
- Law evaluation (`EvaluateEffectLaw`, `DeriveEffectClasses`) is part of
  checking in every build. Re-proving unchanged laws is not made optional:
  the rejected-research rule forbids trusting cached proof bits without a
  re-check (plans/open-work.md).

## Costs

Costs are to be measured on Alpha's physical host only, per the owner's rule,
and recorded in `bench/results/` with the host named. Nothing has been timed
yet. Procedure once Alpha is available:

1. Pick a release-sized program (for example a std-heavy bundle) and build it
   with `--quickest`, the default, and `--thorough`.
2. Run eleven interleaved samples per mode on the same host, collect wall
   time for the `zi2zx` lowering/optimization phase and the `zx2native`
   encoding phase separately (`ZIRAN_PROFILE` records both), and compare the
   medians.
3. Exclude anomalies with the symmetric 1.5xIQR rule and keep the raw
   samples.

Until those runs exist, do not quote cost numbers anywhere: only the
inventory above is stated.

## Tests

`tests/check_control.sh` builds one program in both modes and asserts:

- `--quickest` writes byte-identical ZX and native images to the default.
- `--thorough` records runtime checks and both bundles run the same result.
- The generated C route accepts both switches.
