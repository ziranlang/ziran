# Authoring with compiler tools

Canonical `.zi` is the only authoring source. The checked tools around it are:

1. Retrieve: `ziran api --json --module NAME --symbol NAME|ID --max-bytes N`
   ([API discovery](API_DISCOVERY.md)).
2. Diagnose: `ziran check --diagnostics=json` ([diagnostics](DIAGNOSTICS.md)).
3. Edit: `ziran edit --root DIR request.json` previews a change; `--apply`
   writes it atomically after the same checker and law gates
   ([checked source edits](SEMANTIC_EDIT.md)).
4. Optional transport: `ziran compact`/`expand --map` is an exact round trip
   ([compact transport](TRANSPORT_VIEW.md)).
5. Verify: `check`, `ir`, `bundle`, `run`, and native C/Go runs when the task
   asks for portable parity.

The retained suite exercises these production commands directly:

```sh
sh tests/zir_api.sh build/bin/ziran
sh tests/semantic_edit.sh build/bin/ziran
make CHECK_FILTER=transport_view check
make CHECK_FILTER=tool_diagnostics check
```

API, diagnostic, edit and transport commands share canonical `.zi` source and
the same checker. Apply exact edits only against their recorded document hash
and preimage. Run checking and relevant execution tests after applying an
edit. The transport view is optional and round-trips to canonical source.
