# Feature registry

`ziran features` and `ziran features --json` expose tested feature contracts in
the installed compiler. `ziran features --id ID --json` returns one stable
entry. The checked-in [FEATURES.json](FEATURES.json) must be byte-identical to
the command output, and [tests/compiler_inventory_cases.zi](../tests/compiler_inventory_cases.zi) validates:

- stable, unique, sorted IDs;
- syntax, status, limits, rejection behavior, and target support,
  including explicit unsupported experimental targets;
- referenced tests, conformance IDs, and diagnostic codes;
- every accepted example from source and saved IR with `ziran check --target`
  for every advertised supported or experimental target; and
- rejection of every negative example.

The registry is intentionally incremental. Its 16 contracts cover compile-time
assertions, laws and execution, CPU parallelism and the explicit GPU device Gap,
checked printing, heap allocation, named imports, integer width conformance
and implicit widening, automatic vector cleanup, local `TextView` mutation
checks, record-field vector moves, procedure defaults, procedure values, and
architecture-specific assembly with a portable body.
ZX preserves that assembly through saved IR and supports optional native
selection and explicit target-only procedures. Supported native template
forms and target limits are listed under `native.assembly` in the registry.
The registry is intentionally incremental and does not claim complete
feature coverage; the language reference is docs/LANGUAGE_DIRECTION.md.
