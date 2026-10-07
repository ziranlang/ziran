# Feature registry

`ziran features` and `ziran features --json` expose tested feature contracts in
the installed compiler. `ziran features --id ID --json` returns one stable
entry. The checked-in [FEATURES.json](FEATURES.json) must be byte-identical to
the command output, and [tests/features.sh](../tests/features.sh) validates:

- stable, unique, sorted IDs;
- syntax, status, limits, rejection behavior, and target support,
  including explicit unsupported experimental targets;
- referenced tests, conformance IDs, and diagnostic codes;
- every accepted example from source and saved IR with `ziran check --target`
  for every advertised supported or experimental target; and
- rejection of every negative example.

The registry is intentionally incremental. Its 15 contracts cover compile-time
assertions, laws and execution, CPU parallelism and explicit GPU fallback,
checked printing, heap allocation, named imports, integer width conformance
and implicit widening, automatic vector cleanup, local `TextView` mutation
checks, record-field vector moves, procedure defaults, and procedure values.
It does not yet replace the language reference or claim complete feature
coverage; the remaining expansion is tracked in the
[consistency roadmap](CONSISTENCY_ROADMAP.md).
