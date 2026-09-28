# Feature registry

`ziran features` and `ziran features --json` expose tested feature contracts in
the installed compiler. `ziran features --id ID --json` returns one stable
entry. The checked-in [FEATURES.json](FEATURES.json) must be byte-identical to
the command output, and [tests/features.sh](../tests/features.sh) validates:

- stable, unique, sorted IDs;
- syntax, status, limits, rejection behavior, and target support,
  including explicit unsupported experimental targets;
- referenced tests, conformance IDs, and diagnostic codes;
- every accepted example with `ziran check`; and
- rejection of every negative example.

The registry is intentionally incremental. It currently covers compile-time
assertions, integer width conformance, local `TextView` mutation checking, and
plain record-field `Vec` moves. It does not yet replace the language reference
or claim complete feature coverage; the remaining expansion is tracked in the
[consistency roadmap](CONSISTENCY_ROADMAP.md).
