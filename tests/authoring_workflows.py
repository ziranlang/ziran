#!/usr/bin/env python3
"""End-to-end evidence for Ziran's checked authoring tools on the frozen L-01 corpus.

Runs the real `ziran api`, `ziran check --diagnostics=json`, `ziran ir`,
`ziran edit` and `ziran compact`/`expand` commands against the corpus programs,
then runs the resulting source through the L-01 checker/law/runtime pipeline.
No model, provider, network access or tokenizer download is used.
"""

import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile


REPO = Path(__file__).resolve().parent.parent
STD = REPO / "std"
SCHEMA = "ziran.authoring-workflows"
VERSION = 1
SCRATCH = REPO / "build/scratch/authoring-workflows"
HEADER = b"ZI-COMPACT-1\n"
API_BUDGET = {"max_symbols": 16, "max_bytes": 4096}
KINDS = ("imports", "constants", "globals", "types", "functions", "laws")
DECLARATIONS = ("constants", "globals", "types", "functions")
TOOLS = ("ziran", "zi-api", "zi-edit", "zi-fmt", "zi2zir", "zi2c", "zi2go", "zi2zx")
# Reference tokenizer implementations inspected through installed module metadata
# only. Nothing is imported, downloaded or read from account/cache directories.
TOKENIZER_MODULES = ("tiktoken", "tokenizers", "sentencepiece", "transformers")

sys.dont_write_bytecode = True  # Loading the shared L-01 helpers leaves no cache files.
_spec = importlib.util.spec_from_file_location("llm_authoring", REPO / "tests/llm_authoring.py")
L = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(L)
Invalid, require, digest = L.Invalid, L.require, L.digest

# Workflow plans for the frozen corpus 1.0.0 tasks. Edits target the entry
# document. `declaration` edits resolve their semantic_id through the checked
# API; `derived_span` takes the minimal token-aligned initial/solution span.
# A rollback substitutes text inside the planned replacements to make a
# candidate the canonical checker or law gate must reject.
PLANS = {
    "pure-law": {
        "symbols": [["main", "Answer"]],
        "edits": [{"insert_at": 0, "text": "Add :: (a: s32, b: s32) -> s32 { return a + b }\n"
                                            "#law Sum custom Add(20, 22) == 42;\n#law Pure effect Add;\n"},
                  {"declaration": "Answer", "original": "Answer :: () -> s32 { return 0 }",
                   "replacement": "Answer :: () -> s32 { return Add(20, 22) }"}],
        "rollback": {"replace": ["a + b", "a - b"], "code": "law.gate"}},
    "diagnostic-repair": {
        "symbols": [["main", "Answer"]],
        "edits": "derived_span",
        "unchecked_semantic_probe": "Answer",
        "rollback": {"replace": ["22", "\"23\""], "code": "check.type"}},
    "api-call": {
        "symbols": [["main", "Answer"], ["text", "StartsWithFoldASCII"]],
        "edits": [{"declaration": "Answer",
                   "original": "Answer :: () -> s32 { if Text.ContainsFoldASCII(\"abc\", \"bc\") { return 0 }; return 42 }",
                   "replacement": "Answer :: () -> s32 { if !Text.StartsWithFoldASCII(\"abc\", \"AB\") { return 1 }; "
                                  "if Text.StartsWithFoldASCII(\"abc\", \"bc\") { return 2 }; "
                                  "if Text.StartsWithFoldASCII(\"é\", \"É\") { return 3 }; return 42 }"}],
        "rollback": {"replace": ["return 42 }", "return \"42\" }"], "code": "check.type"}},
    "cross-file": {
        "symbols": [["main", "Answer"], ["arithmetic", "First"], ["arithmetic", "Second"]],
        "final_symbols": [["main", "Answer"], ["sums", "Add"]],
        "edits": None,
        "unsupported": {"path": "sums.zi", "text": "Add :: (a: s32, b: s32) -> s32 { return a + b }\n",
                        "reason": "single-document transaction: the goal creates sums.zi and deletes arithmetic.zi"}},
    "capability-portable": {
        "symbols": [["main", "Answer"], ["main", "Callback"]],
        "final_symbols": [["main", "Answer"], ["main", "Increment"]],
        "edits": [{"declaration": "Callback", "original": "Callback :: #type (value: s32) -> s32 #c_call;",
                   "replacement": "Increment :: (value: s32) -> s32 { return value + 1 }\n#law Portable abi Increment;"},
                  {"declaration": "Answer", "original": "Answer :: () -> s32 { return 41 }",
                   "replacement": "Answer :: () -> s32 { return Increment(41) }"}],
        "rollback": {"replace": ["value + 1", "value + \"1\""], "code": "check.type"}},
    "law-repair": {
        "symbols": [["main", "Add"]],
        "edits": "derived_span",
        "rollback": {"replace": ["+", "*"], "code": "law.gate"}},
    "span-edit": {
        "symbols": [["main", "Answer"]],
        "edits": "derived_span",
        "variant": [{"declaration": "Answer", "original": "Answer :: () -> s32 { return 41 }",
                     "replacement": "Answer :: () -> s32 { return 42 }"}],
        "rollback": {"replace": ["42", "\"42\""], "code": "check.type"}},
}


def utf8(text):
    return len(text.encode("utf-8"))


def measure(text):
    data = text.encode("utf-8") if isinstance(text, str) else text
    return {"utf8_bytes": len(data), "sha256": digest(data)}


def word(byte):
    return byte == 95 or 48 <= byte <= 57 or 65 <= byte <= 90 or 97 <= byte <= 122


def derived_span(old, new):
    """Minimal common-prefix/suffix span widened to whole identifier/number tokens."""
    require(old != new, "derived span needs different documents")
    low = 0
    while low < min(len(old), len(new)) and old[low] == new[low]:
        low += 1
    tail = 0
    while tail < min(len(old), len(new)) - low and old[-1 - tail] == new[-1 - tail]:
        tail += 1
    def inside(data, at):
        return at < len(data) and 0x80 <= data[at] < 0xC0
    while low > 0 and (word(old[low - 1]) or inside(old, low) or inside(new, low)):
        low -= 1
    while tail > 0 and (word(old[len(old) - tail]) or inside(old, len(old) - tail)):
        tail -= 1
    return {"start_byte": low, "end_byte": len(old) - tail,
            "original": old[low:len(old) - tail].decode("utf-8"),
            "replacement": new[low:len(new) - tail].decode("utf-8")}


def rows(value):
    return [(module, kind, row) for module in value["modules"] for kind in KINDS for row in module[kind]]


def parse_json(text, label):
    try:
        return json.loads(text, object_pairs_hook=L.unique)
    except (ValueError, Invalid) as error:
        raise Invalid(label + ": tool emitted invalid JSON") from error


def json_lines(text, label):
    return [parse_json(line, label) for line in text.splitlines() if line.strip()]


def offset_of(data, line, column):
    """One-based line and UTF-8 byte column to a zero-based document offset."""
    at = 0
    for _ in range(line - 1):
        found = data.find(b"\n", at)
        if found < 0:
            return None
        at = found + 1
    return at + column - 1


def validate_map(mapping, source, transport):
    problems = []
    keys = {"schema_version", "coordinate_space", "source_sha256", "transport_sha256", "source_bytes",
            "transport_bytes", "header_bytes", "segments"}
    if not isinstance(mapping, dict) or set(mapping) != keys:
        return ["map members differ"]
    if mapping["schema_version"] != 1 or mapping["coordinate_space"] != "utf8_bytes":
        problems.append("unsupported map version or coordinates")
    if (mapping["source_sha256"], mapping["transport_sha256"]) != (digest(source), digest(transport)):
        problems.append("map digests do not bind both documents")
    if (mapping["source_bytes"], mapping["transport_bytes"]) != (len(source), len(transport)):
        problems.append("map lengths differ")
    if mapping["header_bytes"] != len(HEADER) or not transport.startswith(HEADER):
        problems.append("transport header differs")
    a, b = 0, len(HEADER)
    for part in mapping["segments"]:
        if (part.get("source_start"), part.get("transport_start")) != (a, b):
            problems.append("segments are not consecutive")
            break
        a, b = part["source_end"], part["transport_end"]
        raw, packed = source[part["source_start"]:a], transport[part["transport_start"]:b]
        if part["kind"] == "literal":
            ok = raw == packed and raw
        elif part["kind"] == "escape":
            ok = raw == b"~" and packed == b"~~"
        elif part["kind"] == "repeat":
            fill = {115: 32, 116: 9, 110: 10, 114: 13}.get(packed[1] if len(packed) > 3 else -1)
            ok = (fill is not None and packed[:1] == b"~" and packed[-1:] == b";"
                  and packed[2:-1].isdigit() and raw == bytes([fill]) * int(packed[2:-1]) and len(raw) >= 8)
        else:
            ok = False
        if not ok:
            problems.append("segment does not reproduce its source bytes")
            break
    if not problems and (a, b) != (len(source), len(transport)):
        problems.append("segments do not cover both documents")
    return problems


def map_offset(mapping, offset):
    for part in mapping["segments"]:
        if part["source_start"] <= offset < part["source_end"]:
            if part["kind"] == "literal":
                return part["kind"], part["transport_start"] + offset - part["source_start"]
            return part["kind"], part["transport_start"]
    return None, None


class Workflows:
    def __init__(self, checker, manifest):
        self.checker = checker
        self.manifest = manifest
        self.ziran = checker.ziran
        self.problems = []

    def run(self, args, cwd=REPO):
        code, out, err = self.checker.command([str(x) for x in args], cwd)
        return code, out, err

    def fail(self, task, message):
        self.problems.append(task + ": " + message)

    def api(self, root, entry, *extra):
        return self.run([self.ziran, "api", "--json", "--root", root, "--module-path", STD, *extra, root / entry])

    def seed(self, root, files):
        root.mkdir(parents=True)
        for name, data in files.items():
            path = L.confined(root, name)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(0o640)

    def document_bytes(self, files, path):
        if path in files:
            return files[path]
        candidate = STD / path
        return L.bounded(candidate) if candidate.is_file() and not candidate.is_symlink() else None

    def check_identities(self, tid, value, files, label):
        """Every retrieved declaration binds its captured document hash, length and anchor."""
        seen = set()
        for module, kind, row in rows(value):
            name = row.get("name", "")
            if not re.fullmatch(r"zi:[0-9a-f]{64}", row.get("id", "")) or row["id"] in seen:
                self.fail(tid, label + ": invalid or duplicate semantic id for " + name)
            seen.add(row.get("id"))
            data = self.document_bytes(files, row["span"]["path"])
            doc, span = row["document"], row["span"]
            if data is None or not doc["available"] or doc["sha256"] != digest(data) or doc["bytes"] != len(data):
                self.fail(tid, label + ": document identity differs for " + name)
                continue
            if span["kind"] != "declaration_anchor" or span["coordinate_space"] != "source" \
               or not (0 <= span["start_byte"] <= span["end_byte"] <= len(data)):
                self.fail(tid, label + ": invalid source anchor for " + name)
            elif kind in DECLARATIONS and not data[span["start_byte"]:].startswith(name.encode()):
                self.fail(tid, label + ": anchor does not start at " + name)
        return seen

    def retrieve(self, tid, root, files, entry, symbols):
        code, out, err = self.api(root, entry)
        result = {"accepted": code == 0, "exit_code": code}
        if code:
            if out:
                self.fail(tid, "rejected API printed a partial projection")
            result["diagnostic"] = measure(err)
            return result, {}
        full = parse_json(out, "api")
        again = self.api(root, entry)
        if again != (code, out, err):
            self.fail(tid, "repeated API retrieval differs")
        if full["schema_version"] != 1 or not full["truncation"]["complete"]:
            self.fail(tid, "full API projection is incomplete or has an unknown schema")
        ids = self.check_identities(tid, full, files, "full")
        result.update(full=measure(out), modules=[m["name"] for m in full["modules"]],
                      symbols=len(rows(full)), repeatable=again == (code, out, err))
        queries, found = [], {}
        for module, symbol in symbols:
            budget = ["--max-symbols", str(API_BUDGET["max_symbols"]), "--max-bytes", str(API_BUDGET["max_bytes"])]
            code, out, err = self.api(root, entry, "--module", module, "--symbol", symbol, *budget)
            if code:
                self.fail(tid, "bounded retrieval rejected " + module + "." + symbol)
                continue
            value = parse_json(out, "api")
            emitted = [row for _, _, row in rows(value)]
            if not emitted or any(row["name"] != symbol for row in emitted) or utf8(out) > API_BUDGET["max_bytes"]:
                self.fail(tid, "bounded retrieval returned other declarations or exceeded its budget")
                continue
            self.check_identities(tid, value, files, "bounded")
            identity = emitted[0]["id"]
            code, by_id, _ = self.api(root, entry, "--module", module, "--symbol", identity, *budget)
            by_id_value = parse_json(by_id, "api") if code == 0 else None
            exact = (by_id_value is not None and by_id_value["truncation"]["matched_symbols"] == 1
                     and [row["id"] for _, _, row in rows(by_id_value)] == [identity])
            if not exact or identity not in ids:
                self.fail(tid, "semantic id retrieval is not exact for " + symbol)
            found[symbol] = {"id": identity, "document_sha256": emitted[0]["document"]["sha256"],
                             "path": emitted[0]["span"]["path"]}
            queries.append({"module": module, "symbol": symbol, "budget": dict(API_BUDGET),
                            "by_name": measure(out), "by_id": measure(by_id), "by_id_exact": exact,
                            "matched_symbols": value["truncation"]["matched_symbols"],
                            "emitted_symbols": value["truncation"]["emitted_symbols"],
                            "complete": value["truncation"]["complete"]})
        result["bounded"] = queries
        return result, found

    def saved_ir(self, tid, root, entry, source_ids):
        ir = root.parent / (root.name + ".ir")
        code, _, _ = self.run([self.ziran, "ir", "--root", root, "--module-path", STD, "-o", ir, root / entry])
        if code:
            self.fail(tid, "saved IR emission failed")
            return {"emitted": False}
        name = entry[:-3] + ".zir"
        code, out, _ = self.run([self.ziran, "api", "--json", "--root", ir, "--module-path", ir, ir / name])
        if code:
            self.fail(tid, "saved IR API rejected")
            return {"emitted": True, "accepted": False}
        value = parse_json(out, "api")
        ids = {row["id"] for _, _, row in rows(value)}
        unavailable = all(not row["document"]["available"] and row["document"]["sha256"] is None
                          for _, _, row in rows(value))
        if ids != source_ids or not unavailable:
            self.fail(tid, "saved IR semantic ids or document identity differ from source")
        return {"emitted": True, "accepted": True, "same_semantic_ids": ids == source_ids,
                "documents_unavailable": unavailable, "api": measure(out)}

    def diagnose(self, tid, root, files, entry):
        code, out, err = self.run([self.ziran, "check", "--target=zx", "--diagnostics=json",
                                   "--root", root, "--module-path", STD, root / entry])
        found = []
        for item in json_lines(err, "diagnostics"):
            if item.get("schema_version") != 2:
                self.fail(tid, "unexpected diagnostic schema")
                continue
            data = files.get(item.get("path"))
            document = item.get("document")
            entry_record = {"code": item["code"], "semantic_id": item.get("semantic_id"),
                            "document_bound": bool(document and data is not None
                                                   and document["sha256"] == digest(data)
                                                   and document["bytes"] == len(data))}
            if data is not None and item.get("line"):
                low = offset_of(data, item["line"], item["column"])
                high = offset_of(data, item["end_line"], item["end_column"])
                if low is not None and high is not None and low <= high <= len(data):
                    entry_record.update(start_byte=low, end_byte=high, span_text=data[low:high].decode("utf-8", "replace"))
            found.append(entry_record)
        laws = {x["law"]: x["status"] for x in json_lines(out, "laws") if "law" in x and x.get("kind") != "waiver"}
        return {"target": "zx", "exit_code": code, "diagnostics": found, "laws": laws, "stderr": measure(err)}, err

    def request(self, work, path, data, edits):
        value = {"schema_version": 1, "path": path, "document_sha256": digest(data), "edits": edits}
        text = json.dumps(value, ensure_ascii=False, sort_keys=True)
        file = work / "request.json"
        file.write_text(text, encoding="utf-8")
        return value, text, file

    def edit(self, root, file, apply):
        return self.run([self.ziran, "edit", "--root", root, "--module-path", STD, "--diagnostics=json",
                         *(["--apply"] if apply else []), file])

    def rejected(self, tid, label, root, path, before, result):
        code, out, err = result
        rows_ = json_lines(err, "diagnostics")
        siblings = sorted(p.name for p in root.glob(".zi-edit-*"))
        ok = code != 0 and not out and (root / path).read_bytes() == before and not siblings \
            and rows_ and all(x.get("schema_version") == 2 for x in rows_)
        if not ok:
            self.fail(tid, label + " was not rejected transactionally")
        return {"exit_code": code, "rejected": code != 0, "document_unchanged": (root / path).read_bytes() == before,
                "no_sibling_files": not siblings, "codes": sorted({x.get("code") for x in rows_}),
                "stderr": measure(err)}

    def plan_edits(self, tid, plan_edits, initial, solution, ids):
        if plan_edits == "derived_span":
            return [derived_span(initial, solution)], "span"
        edits, kinds = [], set()
        for item in plan_edits:
            if "insert_at" in item:
                edits.append({"start_byte": item["insert_at"], "end_byte": item["insert_at"],
                              "original": "", "replacement": item["text"]})
                kinds.add("span")
            else:
                require(item["declaration"] in ids, tid + ": no checked semantic id for " + item["declaration"])
                edits.append({"semantic_id": ids[item["declaration"]]["id"], "original": item["original"],
                              "replacement": item["replacement"]})
                kinds.add("semantic")
        return edits, "+".join(sorted(kinds))

    def transact(self, tid, root, work, path, initial, solution, edits, rollback):
        """Preview, rejected rollback, apply and stale replay of one request."""
        document = root / path
        value, text, file = self.request(work, path, initial, edits)
        code, out, err = self.edit(root, file, False)
        preview = {"exit_code": code, "response": measure(out), "document_unchanged": document.read_bytes() == initial}
        response = parse_json(out, "edit") if code == 0 else None
        if response is not None:
            preview.update(applied=response["applied"], source_matches_solution=response["source"].encode() == solution)
        if code or response["applied"] or response["source"].encode() != solution or document.read_bytes() != initial:
            self.fail(tid, "preview did not return the goal without writing")
        result = {"request": value, "request_bytes": measure(text), "preview": preview}
        if rollback is not None:
            old, new = rollback["replace"]
            broken = copy.deepcopy(edits)
            require(any(old in e["replacement"] for e in broken), tid + ": rollback substitution not found")
            for item in broken:
                item["replacement"] = item["replacement"].replace(old, new, 1)
            _, _, bad = self.request(work, path, initial, broken)
            result["rollback"] = self.rejected(tid, "rollback", root, path, initial, self.edit(root, bad, True))
            result["rollback"]["expected_code"] = rollback["code"]
            if rollback["code"] not in result["rollback"]["codes"]:
                self.fail(tid, "rollback rejection code differs: " + str(result["rollback"]["codes"]))
            file.write_text(text, encoding="utf-8")
        code, out, err = self.edit(root, file, True)
        applied = document.read_bytes()
        response = parse_json(out, "edit") if code == 0 else None
        result["apply"] = {"exit_code": code, "response": measure(out),
                           "applied": bool(response and response["applied"]),
                           "document_matches_solution": applied == solution,
                           "response_matches_document": bool(response and response["source"].encode() == applied),
                           "mode_preserved": stat.S_IMODE(document.stat().st_mode) == 0o640,
                           "no_sibling_files": not list(root.glob(".zi-edit-*"))}
        if not all(result["apply"][k] for k in ("applied", "document_matches_solution", "response_matches_document",
                                                "mode_preserved", "no_sibling_files")):
            self.fail(tid, "apply did not atomically write the goal")
        result["stale"] = self.rejected(tid, "stale replay", root, path, applied, self.edit(root, file, True))
        return result

    def transport(self, tid, work, label, path, data):
        stem = work / ("transport-" + label + "-" + path.replace("/", "_"))
        stem.mkdir(parents=True)
        source = stem / Path(path).name
        source.write_bytes(data)
        code, packed, _ = self.run([self.ziran, "compact", "--diagnostics=json", "--map", stem / "map.json", source])
        if code:
            self.fail(tid, "compact rejected canonical " + path)
            return {"path": path, "role": label, "accepted": False}, None, None, None
        encoded = packed.encode("utf-8")
        (stem / "source.zic").write_bytes(encoded)
        _, repeated, _ = self.run([self.ziran, "compact", source])
        code, restored, _ = self.run([self.ziran, "expand", "--diagnostics=json", "--map", stem / "expanded.json",
                                      stem / "source.zic"])
        mapping = parse_json((stem / "map.json").read_text(encoding="utf-8"), "map")
        problems = validate_map(mapping, data, encoded)
        exact = code == 0 and restored.encode("utf-8") == data
        same_map = (stem / "expanded.json").read_bytes() == (stem / "map.json").read_bytes()
        if problems or not exact or not same_map or repeated != packed:
            self.fail(tid, "transport did not round-trip " + path + ": " + "; ".join(problems))
        return ({"path": path, "role": label, "accepted": True, "canonical": measure(data), "transport": measure(encoded),
                 "map": measure((stem / "map.json").read_bytes()), "segments": len(mapping["segments"]),
                 "segment_kinds": sorted({p["kind"] for p in mapping["segments"]}),
                 "round_trip_exact": exact, "repeatable": repeated == packed, "expand_map_identical": same_map,
                 "map_problems": problems},
                restored.encode("utf-8") if exact else None, mapping, encoded)

    def observe(self, task, base):
        tid, plan = task["id"], PLANS[task["id"]]
        entry = task["entry"].split(":")[0] + ".zi"
        initial, solution = L.files_for(task["initial"]), L.files_for(task["solution"])
        root, work = base / tid / "source", base / tid
        work.mkdir(parents=True)
        self.seed(root, initial)
        out = {"task_id": tid, "genre": task["genre"], "entry": task["entry"], "plan": copy.deepcopy(plan),
               "initial_sha256": {k: digest(v) for k, v in sorted(initial.items())},
               "solution_sha256": {k: digest(v) for k, v in sorted(solution.items())}}
        diagnosis, diagnostics_text = self.diagnose(tid, root, initial, entry)
        out["initial_check"] = diagnosis
        retrieval, ids = self.retrieve(tid, root, initial, entry, plan["symbols"])
        out["initial_retrieval"] = retrieval
        context_modules = {name: L.bounded(L.confined(REPO, name)) for name in task["context"]}
        # Canonical: the agent reads every task document and listed context module.
        context = {"unit": "utf8_bytes", "tokens": None,
                   "canonical_source": {"documents": sorted(initial), "context_modules": sorted(context_modules),
                                        "utf8_bytes": sum(map(len, initial.values())) + sum(map(len, context_modules.values()))},
                   "checked_api_full": retrieval["full"]["utf8_bytes"] if retrieval["accepted"] else None,
                   "checked_api_bounded": sum(q["by_name"]["utf8_bytes"] for q in retrieval.get("bounded", [])) if retrieval["accepted"] else None,
                   "diagnostics": diagnosis["stderr"]["utf8_bytes"] if diagnosis["exit_code"] else None,
                   "whole_document_output": sum(len(solution[p]) for p in solution)}
        edit = {"supported": plan["edits"] is not None}
        final = dict(initial)
        if plan["edits"] is None:
            probe = plan["unsupported"]
            missing = root / probe["path"]
            value = {"schema_version": 1, "path": probe["path"], "document_sha256": digest(b""),
                     "edits": [{"start_byte": 0, "end_byte": 0, "original": "", "replacement": probe["text"]}]}
            file = work / "request.json"
            file.write_text(json.dumps(value, sort_keys=True), encoding="utf-8")
            code, stdout, err = self.edit(root, file, True)
            codes = sorted({x.get("code") for x in json_lines(err, "diagnostics")})
            edit.update(reason=probe["reason"], creation_probe={"exit_code": code, "codes": codes,
                                                                 "created": missing.exists()})
            if code == 0 or missing.exists() or stdout:
                self.fail(tid, "single-document edit unexpectedly created a document")
            final = dict(solution)
        else:
            name = plan.get("unchecked_semantic_probe")
            if name and diagnosis["exit_code"] and retrieval["accepted"] is False:
                # Diagnostics name the enclosing declaration; a semantic edit
                # still requires a checked initial graph and must be rejected.
                semantic = next((d["semantic_id"] for d in diagnosis["diagnostics"] if d.get("semantic_id")), None)
                require(semantic is not None, tid + ": diagnostic names no enclosing declaration")
                if semantic:
                    line = next(x for x in initial[entry].decode().splitlines() if x.startswith(name + " ::"))
                    goal = next(x for x in solution[entry].decode().splitlines() if x.startswith(name + " ::"))
                    _, _, file = self.request(work, entry, initial[entry],
                                              [{"semantic_id": semantic, "original": line, "replacement": goal}])
                    edit["semantic_on_unchecked"] = self.rejected(tid, "semantic edit on unchecked document", root, entry,
                                                                  initial[entry], self.edit(root, file, True))
            edits, mode = self.plan_edits(tid, plan["edits"], initial[entry], solution[entry], ids)
            edit["mode"] = mode
            edit.update(self.transact(tid, root, work, entry, initial[entry], solution[entry], edits, plan.get("rollback")))
            context["edit_request"] = edit["request_bytes"]["utf8_bytes"]
            context["edit_preview_response"] = edit["preview"]["response"]["utf8_bytes"]
            final[entry] = (root / entry).read_bytes()
            for item in diagnosis["diagnostics"]:
                if "start_byte" in item and edits and "start_byte" in edits[0]:
                    item["overlaps_repair_span"] = item["start_byte"] < edits[0]["end_byte"] and edits[0]["start_byte"] < item["end_byte"]
            if "variant" in plan:
                vroot = work / "variant"
                self.seed(vroot, initial)
                vedits, vmode = self.plan_edits(tid, plan["variant"], initial[entry], solution[entry], ids)
                value, text, file = self.request(work, entry, initial[entry], vedits)
                code, response, _ = self.edit(vroot, file, True)
                same = code == 0 and (vroot / entry).read_bytes() == final[entry]
                if not same:
                    self.fail(tid, "semantic variant did not reach the same canonical document")
                edit["variant"] = {"mode": vmode, "request": value, "request_bytes": measure(text),
                                   "response": measure(response), "same_document_as_primary": same}
            if set(final) != set(solution) or any(final[p] != solution[p] for p in solution):
                self.fail(tid, "edited documents differ from the frozen solution")
        out["edit"] = edit
        if edit["supported"]:
            after, after_ids = self.retrieve(tid, root, final, entry, plan.get("final_symbols", plan["symbols"]))
            stable, retired = {}, {}
            for name, before in sorted(ids.items()):
                if name not in after_ids:
                    # A replaced declaration's identity no longer resolves.
                    code, text, _ = self.api(root, entry, "--symbol", before["id"])
                    gone = code == 0 and parse_json(text, "api")["truncation"]["matched_symbols"] == 0
                    retired[name] = {"id": before["id"], "resolves": not gone}
                    if not gone:
                        self.fail(tid, "replaced declaration id still resolves: " + name)
                    continue
                now = after_ids[name]
                edited = before["path"] == entry
                version = (now["document_sha256"] == digest(final[entry]) != before["document_sha256"]) if edited \
                    else now["document_sha256"] == before["document_sha256"]
                stable[name] = {"id_stable": now["id"] == before["id"], "document": "edited" if edited else "unchanged",
                                "document_version_consistent": version}
                if not stable[name]["id_stable"] or not version:
                    self.fail(tid, "semantic id or document version is inconsistent after apply: " + name)
            semantic = [d["semantic_id"] for d in diagnosis["diagnostics"] if d.get("semantic_id")]
            out["final_retrieval"] = after
            out["identity_after_edit"] = {"symbols": stable, "retired": retired,
                                          "diagnostic_ids_resolve": all(any(v["id"] == s for v in after_ids.values())
                                                                        for s in semantic) if semantic else None}
            if semantic and not out["identity_after_edit"]["diagnostic_ids_resolve"]:
                self.fail(tid, "diagnostic semantic id is absent from the repaired checked API")
        else:
            final_root = work / "final"
            self.seed(final_root, final)
            out["final_retrieval"], _ = self.retrieve(tid, final_root, final, entry, plan["final_symbols"])
            root = final_root
        full_ids = set()
        code, api_text, _ = self.api(root, entry)
        if code == 0:
            full_ids = {row["id"] for _, _, row in rows(parse_json(api_text, "api"))}
        out["saved_ir"] = self.saved_ir(tid, root, entry, full_ids)
        transports, restored = [], {}
        for path in sorted(final):
            record, data, _, _ = self.transport(tid, work, "final", path, final[path])
            transports.append(record)
            restored[path] = data
        for name, data in sorted(context_modules.items()):
            record, _, _, _ = self.transport(tid, work, "context", name, data)
            transports.append(record)
        for path in sorted(initial):
            record, data, mapping, encoded = self.transport(tid, work, "initial", path, initial[path])
            transports.append(record)
            if path == entry and diagnosis["exit_code"] and data is not None:
                check_root = work / "restored-initial"
                restored_initial = dict(initial)
                restored_initial[entry] = data
                self.seed(check_root, restored_initial)
                again, again_text = self.diagnose(tid, check_root, restored_initial, entry)
                mapped = []
                for item in diagnosis["diagnostics"]:
                    if "start_byte" in item and item["start_byte"] < len(initial[entry]):
                        kind, at = map_offset(mapping, item["start_byte"])
                        # Literal offsets map byte-for-byte; a repeat marker
                        # stands for its whole run and keeps the run offset.
                        ok = (kind == "literal" and encoded[at] == initial[entry][item["start_byte"]]) or \
                             (kind in ("repeat", "escape") and encoded[at:at + 1] == b"~")
                        mapped.append({"source_offset": item["start_byte"], "segment": kind,
                                       "transport_offset": at, "same_byte": ok})
                same = again_text == diagnostics_text and again["exit_code"] == diagnosis["exit_code"]
                if not same or not all(m["same_byte"] for m in mapped):
                    self.fail(tid, "restored-source diagnostics or map coordinates differ")
                record["restored_diagnostics_identical"] = same
                record["diagnostic_offsets"] = mapped
        out["transport"] = transports
        def pair(roles):
            group = [t for t in transports if t["accepted"] and t["role"] in roles]
            return {"canonical": sum(t["canonical"]["utf8_bytes"] for t in group),
                    "compact": sum(t["transport"]["utf8_bytes"] for t in group)}
        # Read: initial documents and context modules. Written: final documents.
        context["transport"] = {"read": pair(("initial", "context")), "written": pair(("final",))}
        out["context_bytes"] = context
        return out, restored


def tokenization(manifest):
    inventory = {}
    for name in TOKENIZER_MODULES:
        try:
            found = importlib.util.find_spec(name) is not None
        except (ImportError, ValueError):
            found = False
        inventory[name] = "installed_unused" if found else "not_installed"
    return {"status": "unavailable", "adapters": [], "reference_modules": inventory,
            "families": {family: {"status": "unavailable", "token_counts": None}
                         for family in manifest["tokenizer_families"]},
            "reason": "no exact local tokenizer adapter is implemented or selected; no tokenizer data is "
                      "downloaded and no model is called",
            "policy": "utf8_bytes are byte counts and never token counts"}


def summary(workflows):
    edits = [w["edit"] for w in workflows]
    transports = [t for w in workflows for t in w["transport"] if t["accepted"]]
    canonical_docs = [t for w in workflows for t in w["transport"] if t["accepted"] and t["role"] == "final"]
    contexts = [w["context_bytes"] for w in workflows]
    def total(key):
        values = [c[key] for c in contexts if c.get(key) is not None]
        return {"tasks": len(values), "utf8_bytes": sum(values)}
    return {"tasks": len(workflows),
            "edits": {"applied": sum(bool(e.get("apply", {}).get("applied")) for e in edits),
                      "unsupported": sum(not e["supported"] for e in edits),
                      "modes": {w["task_id"]: w["edit"].get("mode") for w in workflows},
                      "rollbacks_rejected": sum(bool(e.get("rollback", {}).get("rejected")) for e in edits),
                      "stale_rejected": sum(bool(e.get("stale", {}).get("rejected")) for e in edits)},
            "context_utf8_bytes": {key: total(key) for key in ("checked_api_full", "checked_api_bounded", "diagnostics",
                                                               "edit_request", "edit_preview_response",
                                                               "whole_document_output")} | {
                "canonical_source": {"tasks": len(contexts), "utf8_bytes": sum(c["canonical_source"]["utf8_bytes"] for c in contexts)}},
            "transport": {"documents": len(transports),
                          "round_trip_exact": sum(t["round_trip_exact"] for t in transports),
                          "smaller": sum(t["transport"]["utf8_bytes"] < t["canonical"]["utf8_bytes"] for t in transports),
                          "larger": sum(t["transport"]["utf8_bytes"] > t["canonical"]["utf8_bytes"] for t in transports),
                          "by_role": {role: {"documents": len(group), "canonical_utf8_bytes": sum(t["canonical"]["utf8_bytes"] for t in group),
                                             "transport_utf8_bytes": sum(t["transport"]["utf8_bytes"] for t in group)}
                                      for role in ("final", "initial", "context")
                                      for group in [[t for t in transports if t["role"] == role]]},
                          "final_documents": {"count": len(canonical_docs),
                                               "canonical_utf8_bytes": sum(t["canonical"]["utf8_bytes"] for t in canonical_docs),
                                               "transport_utf8_bytes": sum(t["transport"]["utf8_bytes"] for t in canonical_docs)}},
            "token_counts": None}


def zia_gate(report):
    transport = report["summary"]["transport"]["final_documents"]
    criteria = {
        "exact_token_measurement": {"status": "met" if report["tokenization"]["status"] == "measured" else "unavailable"},
        "corpus_transport_reduction": {"status": "met" if transport["transport_utf8_bytes"] < transport["canonical_utf8_bytes"] else "not_met",
                                       "observed": transport},
        "round_trip_correctness": {"status": "met" if report["summary"]["transport"]["round_trip_exact"]
                                   == report["summary"]["transport"]["documents"] and report["passed"] else "not_met"},
        "model_authoring_quality": {"status": "unmeasured", "reason": "this evaluator makes no model calls"},
        "human_readability": {"status": "unmeasured", "reason": "no independent readability review is recorded"}}
    return {"decision": "eligible_for_design" if all(c["status"] == "met" for c in criteria.values()) else "deferred",
            "rule": "an LLM-first source view requires every criterion met; otherwise canonical .zi stays the only authoring syntax",
            "criteria": criteria}


def identity(checker):
    tool = Path(checker.ziran)
    tools = {name: L.file_digest(tool.parent / name) for name in TOOLS if (tool.parent / name).is_file()}
    code, version, _ = checker.command([checker.ziran, "--version"])
    return {"path": checker.identity["path"], "version": version.strip() if code == 0 else None, "sha256": tools}


def self_test(manifest_path, manifest, checker):
    require(set(PLANS) == {t["id"] for t in manifest["tasks"]}, "workflow plans do not cover the frozen corpus")
    negatives = 0
    # Validator negatives: a shifted map segment, a wrong digest and missing header fail.
    data = b"Answer :: () -> s32 { return 42 }\n"
    packed = HEADER + data
    good = {"schema_version": 1, "coordinate_space": "utf8_bytes", "source_sha256": digest(data),
            "transport_sha256": digest(packed), "source_bytes": len(data), "transport_bytes": len(packed),
            "header_bytes": len(HEADER), "segments": [{"source_start": 0, "source_end": len(data),
                                                     "transport_start": len(HEADER), "transport_end": len(packed),
                                                     "kind": "literal"}]}
    require(not validate_map(good, data, packed), "valid map rejected")
    for mutate in (lambda m: m["segments"][0].update(transport_start=0), lambda m: m.update(source_sha256="0" * 64),
                   lambda m: m["segments"][0].update(kind="repeat"), lambda m: m.update(extra=1)):
        bad = copy.deepcopy(good)
        mutate(bad)
        require(validate_map(bad, data, packed), "corrupt map accepted")
        negatives += 1
    require(derived_span(b"return 41 }", b"return 42 }")["original"] == "41", "span is not token aligned")
    require(derived_span("x := \"é\"".encode(), "x := \"ê\"".encode())["original"] == "é", "span split UTF-8")
    # Oracle negatives: workflow checks must notice a wrong goal and a document
    # identity that no longer matches the bytes on disk.
    task = next(t for t in manifest["tasks"] if t["id"] == "span-edit")
    probe = Workflows(checker, manifest)
    with tempfile.TemporaryDirectory(prefix="negative-", dir=checker.scratch) as name:
        base = Path(name)
        initial, solution = L.files_for(task["initial"]), L.files_for(task["solution"])
        probe.seed(base / "source", initial)
        edits = [derived_span(initial["main.zi"], solution["main.zi"])]
        probe.transact("negative", base / "source", base, "main.zi", initial["main.zi"], solution["main.zi"] + b"\n", edits, None)
        require(probe.problems, "wrong edit goal accepted")
        negatives += 1
        probe.problems.clear()
        probe.retrieve("negative", base / "source", initial, "main.zi", [])
        require(probe.problems, "stale document identity accepted")
        negatives += 1
    work = Workflows(checker, manifest)
    passes = []
    restored = {}
    for index in range(2):
        with tempfile.TemporaryDirectory(prefix="observe-", dir=checker.scratch) as name:
            base = Path(name)
            observed = []
            for task in manifest["tasks"]:
                record, files = work.observe(task, base)
                observed.append(record)
                restored[task["id"]] = files
            passes.append(checker.normalize(observed, base))
    if passes[0] != passes[1]:
        work.problems.append("repeated workflow observations differ")
    workflows = passes[0]
    for record, task in zip(workflows, manifest["tasks"]):
        files = restored[task["id"]]
        if any(data is None for data in files.values()):
            record["execution"] = None
            continue
        # Restored transport bytes run through the L-01 checker, saved IR,
        # bundle, explicit `zi2zx run --vm` and native C/Go parity pipeline.
        record["execution"] = checker.evaluate(task, files, task["expected"]["final"], task["portable_parity"])
        if not record["execution"]["passed"]:
            work.problems.append(task["id"] + ": restored final source failed: " + "; ".join(record["execution"]["problems"]))
    report = {"schema": SCHEMA, "schema_version": VERSION,
              "corpus": {"schema_version": manifest["schema_version"], "corpus_version": manifest["corpus_version"],
                         "manifest_sha256": L.file_digest(manifest_path), "tasks": [t["id"] for t in manifest["tasks"]]},
              "plans_sha256": digest(json.dumps(PLANS, sort_keys=True, ensure_ascii=False).encode()),
              "evaluator_sha256": L.file_digest(Path(__file__).resolve()),
              "tools": identity(checker),
              "budgets": {"bounded_api": dict(API_BUDGET), "full_api": "ziran api defaults",
                          "command_timeout_seconds": 90, "stream_limit_bytes": L.LIMIT},
              "tokenization": tokenization(manifest),
              "workflows": workflows, "llm_calls": 0, "model_results": None,
              "deterministic_observation_passes": len(passes), "validator_negative_cases": negatives}
    report["passed"] = not work.problems
    report["problems"] = work.problems
    report["summary"] = summary(workflows)
    report["zia_gate"] = zia_gate(report)
    report["compiler_tool_turns"] = checker.turns
    require(report["tokenization"]["status"] == "unavailable" and report["summary"]["token_counts"] is None,
            "token counts fabricated")
    require(json.loads(json.dumps(report, allow_nan=False)) == report, "report is not strict JSON")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ziran", type=Path, default=REPO / "build/bin/ziran")
    parser.add_argument("--manifest", type=Path, default=L.SURFACE / "tasks.json")
    parser.add_argument("--report", type=Path, help="also write the report to this git-ignored path")
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    try:
        manifest = L.load_manifest(args.manifest)
        require(set(PLANS) == {t["id"] for t in manifest["tasks"]}, "workflow plans do not cover the frozen corpus")
        if args.validate_only:
            result = {"schema": SCHEMA, "schema_version": VERSION, "passed": True, "tasks": len(manifest["tasks"])}
        else:
            for target in ("build/scratch/authoring-workflows/record.json",
                           *([str(args.report.resolve())] if args.report else [])):
                ignored = subprocess.run(["git", "check-ignore", "-q", target], cwd=REPO)
                require(ignored.returncode == 0, "output must be git-ignored: " + target)
            SCRATCH.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory(prefix="run-", dir=SCRATCH) as name:
                checker = L.Checker(args.ziran.resolve(), Path(name))
                result = self_test(args.manifest.resolve(), manifest, checker)
        text = json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True)
        if args.report and not args.validate_only:
            args.report.write_text(text + "\n", encoding="utf-8")
        print(text)
        return 0 if result["passed"] else 1
    except (Invalid, OSError, UnicodeError, json.JSONDecodeError, subprocess.TimeoutExpired, KeyError, TypeError) as error:
        print(json.dumps({"schema": SCHEMA, "schema_version": VERSION, "passed": False,
                          "error": type(error).__name__ + ": " + str(error)}, sort_keys=True), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
