#!/usr/bin/env python3
"""Offline L-01 corpus validation and deterministic, scratch-only edit replay."""

import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import signal
import subprocess
import sys
import tempfile


REPO = Path(__file__).resolve().parent.parent
SURFACE = REPO / "tests/llm_authoring"
FIXTURES = SURFACE / "fixtures"
VERSION = 1
LIMIT = 4 * 1024 * 1024
CATEGORIES = ("prompt", "cached_prompt", "tool", "completion", "reasoning", "total")
SECRET_ENV = ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_SESSION_ID",
              "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER", "DESKTOP_SESSION",
              "XDG_CURRENT_DESKTOP", "XDG_SESSION_TYPE", "XDG_RUNTIME_DIR",
              "DESKTOP_STARTUP_ID", "ICEAUTHORITY", "SSH_AUTH_SOCK")


class Invalid(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Invalid(message)


def shape(value, required, optional=()):
    require(isinstance(value, dict), "expected an object")
    require(set(required) <= value.keys(), "missing fields: " + str(set(required) - value.keys()))
    require(value.keys() <= set(required) | set(optional), "unknown fields: " + str(value.keys() - set(required) - set(optional)))


def string(value):
    require(isinstance(value, str) and bool(value), "expected a nonempty string")
    try:
        value.encode("utf-8")
    except UnicodeEncodeError as error:
        raise Invalid("text must be valid UTF-8") from error


def integer(value):
    require(type(value) is int and value >= 0, "expected a nonnegative integer")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(value):
    require(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value), "invalid sha256")


def relative(value, suffix=None):
    string(value)
    p = PurePosixPath(value)
    require(not p.is_absolute() and ".." not in p.parts and "\\" not in value
            and str(p) == value and value != ".", "unsafe relative path: " + value)
    if suffix:
        require(p.suffix == suffix, "expected a " + suffix + " artifact")
    return p


def confined(root, name):
    p = root / relative(name)
    require(not any(part.is_symlink() for part in (p, *p.parents)), "symlinks are not allowed")
    require(p.resolve().is_relative_to(root.resolve()), "path leaves artifact root")
    return p


def bounded(path):
    with path.open("rb") as source:
        data = source.read(LIMIT + 1)
    require(len(data) <= LIMIT, "artifact exceeds 4 MiB: " + str(path))
    return data


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate JSON key: " + key)
        result[key] = value
    return result


def read_json(path):
    def finite_float(value):
        number = float(value)
        require(math.isfinite(number), "nonfinite JSON number")
        return number
    try:
        return json.loads(bounded(path), object_pairs_hook=unique,
                          parse_float=finite_float,
                          parse_constant=lambda x: (_ for _ in ()).throw(Invalid("nonfinite JSON number")))
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise Invalid("invalid UTF-8 JSON: " + str(path)) from error


def provenance(value):
    shape(value, ("kind", "source"), ("locator",))
    require(value["kind"] in ("local_fixture", "synthetic_fixture", "provider_export", "user_supplied"), "unknown provenance kind")
    string(value["source"])
    if "locator" in value:
        string(value["locator"])


def state(value):
    require(isinstance(value, dict) and value, "empty source state")
    for path, artifact in value.items():
        relative(path, ".zi")
        shape(artifact, ("fixture", "sha256"))
        relative(artifact["fixture"], ".zi")
        sha(artifact["sha256"])
        data = bounded(confined(FIXTURES, artifact["fixture"]))
        data.decode("utf-8")
        require(digest(data) == artifact["sha256"], "fixture digest mismatch: " + path)


def expectation(value):
    shape(value, ("checks",), ("run_stdout",))
    require(isinstance(value["checks"], dict) and value["checks"], "missing checker targets")
    for target, outcome in value["checks"].items():
        require(target in ("c", "go", "zx"), "unknown target")
        shape(outcome, ("exit_code", "laws"), ("diagnostics",))
        require(type(outcome["exit_code"]) is int and outcome["exit_code"] in (0, 1), "expected exit_code 0 or 1")
        require(isinstance(outcome["laws"], dict), "laws must be an object")
        for name, status in outcome["laws"].items():
            string(name)
            require(status in ("proved", "disproved", "unknown", "waived"), "unknown law status")
        require(isinstance(outcome.get("diagnostics", []), list), "diagnostics must be an array")
        for item in outcome.get("diagnostics", []):
            shape(item, (), ("code", "expected_type", "actual_type", "capability", "target", "message"))
            require(item, "empty diagnostic expectation")
            for text in item.values():
                string(text)
    if "run_stdout" in value:
        string(value["run_stdout"])
        require(all(x["exit_code"] == 0 for x in value["checks"].values()), "cannot run rejected code")


def load_manifest(path):
    value = read_json(path)
    shape(value, ("schema_version", "corpus_version", "provenance", "tokenizer_families", "experiments", "tasks"))
    require(type(value["schema_version"]) is int and value["schema_version"] == VERSION, "unsupported manifest schema")
    require(isinstance(value["corpus_version"], str) and re.fullmatch(r"\d+\.\d+\.\d+", value["corpus_version"]), "invalid corpus version")
    provenance(value["provenance"])
    families = value["tokenizer_families"]
    require(isinstance(families, list) and families and all(isinstance(x, str) and x for x in families)
            and len(set(families)) == len(families), "invalid tokenizer families")
    require(isinstance(value["experiments"], dict) and set(value["experiments"]) == {"baseline", "compact", "semantic-edit"}, "missing experiment arms")
    for name, arm in value["experiments"].items():
        shape(arm, ("status", "source_surface"), ("requires",))
        require(arm["status"] == ("available" if name == "baseline" else "planned"), "only canonical baseline is implemented")
        string(arm["source_surface"])
        require(isinstance(arm.get("requires", []), list), "requires must be an array")
        for item in arm.get("requires", []):
            string(item)
    require(isinstance(value["tasks"], list) and value["tasks"], "no tasks")
    ids = set()
    for task in value["tasks"]:
        shape(task, ("id", "genre", "prompt", "context", "entry", "retry_limit", "repair_budget",
                     "initial", "solution", "expected", "portable_parity"))
        require(isinstance(task["id"], str) and re.fullmatch(r"[a-z][a-z0-9-]*", task["id"]), "invalid task id")
        require(task["id"] not in ids, "duplicate task id")
        ids.add(task["id"])
        string(task["genre"])
        string(task["prompt"])
        require(isinstance(task["entry"], str) and re.fullmatch(r"[a-z][a-z0-9_]*:[A-Za-z][A-Za-z0-9_]*", task["entry"]), "invalid entry")
        for key in ("retry_limit", "repair_budget"):
            integer(task[key])
        require(type(task["portable_parity"]) is bool, "portable_parity must be boolean")
        require(isinstance(task["context"], list), "context must be an array")
        for name in task["context"]:
            relative(name, ".zi")
            require(name.startswith("std/") and confined(REPO, name).is_file(), "context must name an existing std module")
        state(task["initial"])
        state(task["solution"])
        module = task["entry"].split(":")[0] + ".zi"
        require(module in task["initial"] and module in task["solution"], "missing entry source")
        shape(task["expected"], ("initial", "final"))
        for item in task["expected"].values():
            expectation(item)
        if task["portable_parity"]:
            require(set(task["expected"]["final"]["checks"]) == {"c", "go", "zx"}
                    and "run_stdout" in task["expected"]["final"], "parity requires all targets and a runtime oracle")
    return value


def load_transcript(value, manifest, artifacts):
    shape(value, ("schema_version", "corpus_version", "task_id", "experiment", "provenance", "attempts"),
          ("model", "experiment_metadata"))
    require(type(value["schema_version"]) is int and value["schema_version"] == VERSION, "unsupported transcript schema")
    require(value["corpus_version"] == manifest["corpus_version"], "corpus version mismatch")
    task = next((x for x in manifest["tasks"] if x["id"] == value["task_id"]), None)
    require(task is not None, "unknown task id")
    require(value["experiment"] == "baseline",
            "corpus 1.0.0 replays only the canonical baseline; tests/authoring_workflows.py runs the edit and transport tools")
    provenance(value["provenance"])
    if "model" in value:
        shape(value["model"], ("provider", "name", "version"), ("temperature", "seed"))
        for key in ("provider", "name", "version"):
            if value["model"][key] is not None:
                string(value["model"][key])
        if "temperature" in value["model"]:
            require(type(value["model"]["temperature"]) in (int, float) and value["model"]["temperature"] >= 0, "invalid temperature")
        if "seed" in value["model"]:
            integer(value["model"]["seed"])
    if "experiment_metadata" in value:
        shape(value["experiment_metadata"], (), ("canonical_sha256", "round_trip", "source_maps", "document_versions", "semantic_ids"))
        # Reserved observations only; these never enable planned compiler commands.
        require(len(json.dumps(value["experiment_metadata"])) < 65536, "oversized experiment metadata")
    attempts = value["attempts"]
    require(isinstance(attempts, list) and 1 <= len(attempts) <= min(task["retry_limit"], task["repair_budget"]) + 1, "attempt/retry/repair limit exceeded")
    for index, attempt in enumerate(attempts):
        shape(attempt, ("index", "events", "edits"), ("usage", "provider_usage"))
        require(type(attempt["index"]) is int and attempt["index"] == index, "nonsequential attempt index")
        require(isinstance(attempt["events"], list), "events must be an array")
        for event in attempt["events"]:
            shape(event, ("kind", "text"), ("provenance",))
            require(event["kind"] in ("prompt", "tool", "completion", "reasoning"), "unknown transcript event")
            string(event["text"])
            if "provenance" in event:
                provenance(event["provenance"])
        require(isinstance(attempt["edits"], list) and attempt["edits"], "empty edit attempt")
        for edit in attempt["edits"]:
            shape(edit, ("path", "base_sha256"), ("start_byte", "end_byte", "replacement", "replacement_file", "delete"))
            relative(edit["path"], ".zi")
            require(edit["path"] in task["initial"] or edit["path"] in task["solution"],
                    "edit names an unintended file: " + edit["path"])
            if edit["base_sha256"] is not None:
                sha(edit["base_sha256"])
            if "delete" in edit:
                require(edit["delete"] is True and set(edit) == {"path", "base_sha256", "delete"}
                        and edit["base_sha256"] is not None, "invalid deletion")
                continue
            require("start_byte" in edit and "end_byte" in edit, "missing byte span")
            integer(edit["start_byte"])
            integer(edit["end_byte"])
            require(edit["start_byte"] <= edit["end_byte"], "reversed byte span")
            require(("replacement" in edit) != ("replacement_file" in edit), "supply exactly one replacement source")
            if "replacement_file" in edit:
                relative(edit["replacement_file"], ".zi")
                bounded(confined(artifacts, edit["replacement_file"])).decode("utf-8")
            else:
                require(isinstance(edit["replacement"], str), "replacement must be text")
                edit["replacement"].encode("utf-8")
        require(isinstance(attempt.get("usage", []), list), "usage must be an array")
        if "provider_usage" in attempt:
            shape(attempt["provider_usage"], ("provenance", "payload"))
            provenance(attempt["provider_usage"]["provenance"])
            require(isinstance(attempt["provider_usage"]["payload"], dict), "raw provider usage must be an object")
        seen = set()
        for usage in attempt.get("usage", []):
            shape(usage, ("category", "count", "unit", "tokenizer", "provenance", "cache_policy"))
            require(usage["category"] in CATEGORIES and usage["category"] not in seen, "unknown/duplicate usage category")
            seen.add(usage["category"])
            integer(usage["count"])
            require(usage["unit"] == "tokens", "usage unit must be tokens")
            shape(usage["tokenizer"], ("family", "name", "version"))
            for item in usage["tokenizer"].values():
                if item is not None:
                    string(item)
            provenance(usage["provenance"])
            require(usage["provenance"]["kind"] in ("provider_export", "user_supplied", "synthetic_fixture"), "usage requires supplied provenance")
            require(usage["cache_policy"] in ("included", "excluded", "not_reported"), "invalid cache policy")
    return task


def files_for(state_value):
    return {path: bounded(confined(FIXTURES, item["fixture"])) for path, item in state_value.items()}


def apply_edits(files, edits, artifacts):
    """Atomic in-memory byte edits; all spans refer to the same pre-attempt version."""
    groups = {}
    for edit in edits:
        groups.setdefault(edit["path"], []).append(edit)
    candidate = dict(files)
    for path, group in sorted(groups.items()):
        original = files.get(path)
        version = digest(original) if original is not None else None
        require(all(e["base_sha256"] == version for e in group), "stale document: " + path)
        if any(e.get("delete") for e in group):
            require(len(group) == 1 and original is not None, "conflicting deletion: " + path)
            del candidate[path]
            continue
        data = original if original is not None else b""
        spans = sorted(group, key=lambda e: (e["start_byte"], e["end_byte"]))
        end = -1
        start = -1
        for edit in spans:
            lo, hi = edit["start_byte"], edit["end_byte"]
            require(hi <= len(data), "span outside document: " + path)
            require(lo >= end and lo != start, "overlapping edits: " + path)
            data[:lo].decode("utf-8")
            data[:hi].decode("utf-8")
            start, end = lo, hi
        for edit in reversed(spans):
            replacement = (bounded(confined(artifacts, edit["replacement_file"]))
                           if "replacement_file" in edit else edit["replacement"].encode("utf-8"))
            data = data[:edit["start_byte"]] + replacement + data[edit["end_byte"]:]
        data.decode("utf-8")
        candidate[path] = data
    return candidate


def scrubbed_env(scratch):
    env = {k: v for k, v in os.environ.items() if k not in SECRET_ENV
           and not k.startswith(("HARMONY_DESKTOP", "DESKTOP_RECOVERY"))}
    env.update(TMPDIR=str(scratch), XDG_CACHE_HOME=str(scratch / "cache"),
               GOPATH=str(scratch / "go-path"),
               GOTMPDIR=str(scratch), GOTELEMETRY="off",
               ZIRAN_DESKTOP_RECOVERY="0", HARMONY_DESKTOP_RECOVERY="0", GO111MODULE="off",
               GOPROXY="off", GOSUMDB="off", GOTOOLCHAIN="local",
               GOMAXPROCS="2", GOFLAGS="-p=2", YUE_DESKTOP_RECOVERY="0")
    return env


class Checker:
    def __init__(self, ziran, scratch):
        self.ziran = str(ziran)
        self.scratch = scratch
        self.env = scrubbed_env(scratch)
        self.turns = 0
        tool = Path(self.ziran)
        self.vm = tool.parent / "zi2zx"
        require(self.vm.is_file(), "missing bundle runner: " + str(self.vm))
        self.identity = {"path": "$REPO/build/bin/ziran" if tool == REPO / "build/bin/ziran" else str(tool),
                         "sha256": file_digest(tool),
                         "tools_sha256": {name: file_digest(tool.parent / name)
                                          for name in ("zi2zir", "zi2c", "zi2go", "zi2zx")
                                          if (tool.parent / name).is_file()}}

    def command(self, args, cwd=REPO):
        self.turns += 1
        out, err = self.scratch / "stdout", self.scratch / "stderr"
        with out.open("wb") as stdout, err.open("wb") as stderr:
            with subprocess.Popen([str(x) for x in args], cwd=cwd, env=self.env,
                                  stdout=stdout, stderr=stderr, start_new_session=True) as process:
                try:
                    code = process.wait(timeout=90)
                except subprocess.TimeoutExpired:
                    # Only this invocation's isolated process group is ours.
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                    raise
        return code, bounded(out).decode("utf-8"), bounded(err).decode("utf-8")

    def normalize(self, value, root):
        # No wall time, random scratch path or account information enters a report.
        if isinstance(value, str):
            return value.replace(str(root), "$TASK").replace(str(self.scratch), "$SCRATCH").replace(str(REPO), "$REPO")
        if isinstance(value, list):
            return [self.normalize(x, root) for x in value]
        if isinstance(value, dict):
            return {k: self.normalize(v, root) for k, v in value.items()}
        return value

    def evaluate(self, task, files, expected, parity=False):
        root = self.scratch / "task"
        if root.exists():
            shutil.rmtree(root)
        root.mkdir()
        for name, data in files.items():
            path = confined(root, name)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        entry, symbol = task["entry"].split(":")
        source = root / (entry + ".zi")
        base = ["--root", root, "--module-path", REPO / "std"]
        observations, problems = {}, []
        for target, wanted in expected["checks"].items():
            code, out, err = self.command([self.ziran, "check", "--target=" + target,
                                          "--diagnostics=json", *base, source])
            laws = [json.loads(line) for line in out.splitlines()]
            diagnostics = [json.loads(line) for line in err.splitlines()]
            require(all(x.get("schema_version") == 2 and "code" in x for x in diagnostics), "unexpected compiler diagnostic schema")
            statuses = {x["law"]: x["status"] for x in laws if "law" in x and x.get("kind") != "waiver"}
            if code != wanted["exit_code"]:
                problems.append(target + ": unexpected checker exit " + str(code))
            if statuses != wanted["laws"]:
                problems.append(target + ": law closure differs: " + str(statuses))
            if code == 0 and any(x.get("severity") == "error" for x in diagnostics):
                problems.append(target + ": accepted with error diagnostics")
            if code == 0 and any(x != "proved" for x in statuses.values()):
                problems.append(target + ": unproved law")
            wanted_diagnostics = wanted.get("diagnostics", [])
            if wanted_diagnostics and len(diagnostics) != len(wanted_diagnostics):
                problems.append(target + ": diagnostic count differs")
            for item in wanted_diagnostics:
                if not any(all(actual.get(k) == v for k, v in item.items()) for actual in diagnostics):
                    problems.append(target + ": missing diagnostic " + str(item))
            observations[target] = {"exit_code": code, "laws": laws, "diagnostics": diagnostics}
        run = {}
        if "run_stdout" in expected and all(x["exit_code"] == 0 for x in observations.values()):
            ir, bundle = root / "ir", root / "source.zx"
            commands = [
                ("ir", [self.ziran, "ir", *base, "-o", ir, source]),
                ("check_saved", [self.ziran, "check", "--target=zx", "--diagnostics=json", "--root", ir,
                                 "--module-path", ir, ir / (entry + ".zir")]),
                ("bundle_source", [self.ziran, "bundle", *base, "--entry", task["entry"], "-o", bundle, source]),
                ("bundle_saved", [self.ziran, "bundle", "--root", ir, "--module-path", ir, "--entry", task["entry"], "-o", root / "saved.zx", ir / (entry + ".zir")]),
                ("source", [self.ziran, "run", bundle]),
                ("saved", [self.ziran, "run", root / "saved.zx"]),
                ("source_vm", [self.vm, "run", "--vm", bundle]),
                ("saved_vm", [self.vm, "run", "--vm", root / "saved.zx"])]
            for name, command in commands:
                code, out, err = self.command(command)
                run[name] = {"exit_code": code, "stdout": out, "stderr": err}
                if code or (name in ("source", "saved", "source_vm", "saved_vm") and out != expected["run_stdout"]):
                    problems.append(name + ": output/exit differs")
                    break
                if name == "check_saved":
                    saved_laws = {x["law"]: x["status"] for x in map(json.loads, out.splitlines())
                                  if "law" in x and x.get("kind") != "waiver"}
                    wanted = expected["checks"].get("zx", next(iter(expected["checks"].values())))["laws"]
                    if saved_laws != wanted:
                        problems.append("saved IR law closure differs")
            if not problems:
                same = bounded(bundle) == bounded(root / "saved.zx")
                run["source_saved_bundle_equal"] = same
                if not same:
                    problems.append("source/saved bundle differs")
            if parity and not problems:
                driver = bounded(FIXTURES / "runner.zi")
                (root / "runner.zi").write_bytes(driver)
                for target in ("c", "go"):
                    if target == "c":
                        code, out, err = self.command([self.ziran, "run", "--target=c",
                                                      "--no-cache", *base, root / "runner.zi"])
                    else:
                        code, out, err = self.command([self.ziran, "build", "--target=go", "--pkg", "main",
                                                      "--exe", "--entry", "runner:main", *base,
                                                      "-o", root / "go", root / "runner.zi"])
                        if code == 0:
                            code, out, err = self.command(["go", "run", "."], cwd=root / "go")
                    run[target] = {"exit_code": code, "stdout": out, "stderr": err}
                    if code or out != expected["run_stdout"]:
                        problems.append(target + ": native parity differs")
        return self.normalize({"passed": not problems, "problems": problems,
                               "checks": observations, "execution": run}, root)


def usage_report(transcript, manifest):
    observations = [{"attempt": a["index"], **u} for a in transcript["attempts"] for u in a.get("usage", [])]
    real = [u for u in observations if u["provenance"]["kind"] != "synthetic_fixture"
            and transcript["provenance"]["kind"] != "synthetic_fixture"]
    # Categories can overlap (reasoning inside completion, cache inside prompt).
    # Never add categories, subtract cache, or fill absent categories with zero.
    return {"observations": observations,
            "categories": {category: {"status": "supplied" if any(x["category"] == category for x in real) else "unmeasured"}
                           for category in CATEGORIES},
            "tokenizer_families": {family: {"status": "supplied" if any(x["tokenizer"]["family"] == family for x in real) else "unmeasured",
                                               "local_tokenization": "not_run"} for family in manifest["tokenizer_families"]},
            "total_tokens": None, "total_policy": "per-attempt supplied totals are retained verbatim; no inferred aggregate"}


def replay(transcript, manifest, artifacts, checker):
    task = load_transcript(transcript, manifest, artifacts)
    files, results = files_for(task["initial"]), []
    first_pass = None
    starting_turns = checker.turns
    for attempt in transcript["attempts"]:
        result = {"index": attempt["index"], "events": attempt["events"], "edits": attempt["edits"],
                  "provider_usage": attempt.get("provider_usage")}
        try:
            candidate = apply_edits(files, attempt["edits"], artifacts)
        except (Invalid, UnicodeError) as error:
            result.update(edit_accepted=False, edit_error=str(error), correctness=None, changed_files=[])
        else:
            changed = sorted(name for name in files.keys() | candidate.keys() if files.get(name) != candidate.get(name))
            files = candidate
            outcome = checker.evaluate(task, files, task["expected"]["final"], task["portable_parity"])
            if set(files) != set(task["solution"]):
                outcome["passed"] = False
                outcome["problems"].append("source file set differs from task goal")
            result.update(edit_accepted=True, correctness=outcome, changed_files=changed)
            if outcome["passed"] and first_pass is None:
                first_pass = attempt["index"]
        result["source_sha256"] = {name: digest(data) for name, data in sorted(files.items())}
        results.append(result)
    last = results[-1]
    passed = bool(last["edit_accepted"] and last["correctness"]["passed"])
    return {"schema_version": VERSION, "corpus_version": manifest["corpus_version"],
            "task_id": task["id"], "experiment": transcript["experiment"],
            "provenance": transcript["provenance"], "model": transcript.get("model"),
            "compiler": checker.identity,
            "experiment_metadata": transcript.get("experiment_metadata"),
            "passed": passed, "attempts": results, "usage": usage_report(transcript, manifest),
            "metrics": {"attempt_count": len(results), "retry_count": len(results) - 1,
                        "first_correct_attempt": first_pass, "attempts_before_first_pass": first_pass,
                        "edit_accepted_first_attempt": results[0]["edit_accepted"],
                        "repair_budget_exhausted": first_pass is None and len(results) - 1 >= task["repair_budget"],
                        "compiler_tool_turns": checker.turns - starting_turns,
                        "text_units": {"unit": "utf8_bytes", "events": sum(len(e["text"].encode("utf-8")) for a in results for e in a["events"])},
                        "context_quality": None, "readability": None, "repair_tokens": None}}


def file_digest(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for data in iter(lambda: source.read(65536), b""):
            h.update(data)
    return h.hexdigest()


def solution_transcript(task, manifest):
    edits = []
    initial, final = files_for(task["initial"]), files_for(task["solution"])
    for path in sorted(initial.keys() | final.keys()):
        old, new = initial.get(path), final.get(path)
        if old == new:
            continue
        edit = {"path": path, "base_sha256": digest(old) if old is not None else None}
        if new is None:
            edit["delete"] = True
        else:
            edit.update(start_byte=0, end_byte=len(old or b""), replacement_file=task["solution"][path]["fixture"])
        edits.append(edit)
    return {"schema_version": VERSION, "corpus_version": manifest["corpus_version"], "task_id": task["id"],
            "experiment": "baseline", "provenance": {"kind": "synthetic_fixture", "source": "local solution regression, no LLM"},
            "attempts": [{"index": 0, "events": [{"kind": "prompt", "text": task["prompt"]}], "edits": edits}]}


def self_test(manifest, checker):
    negative_cases = 0
    def rejects(callback):
        nonlocal negative_cases
        try:
            callback()
        except (Invalid, UnicodeError, json.JSONDecodeError):
            negative_cases += 1
        else:
            raise Invalid("negative test unexpectedly accepted")

    def manifest_rejected(value):
        path = checker.scratch / "invalid-manifest.json"
        path.write_text(json.dumps(value))
        rejects(lambda: load_manifest(path))

    for field, value in (("schema_version", 2), ("corpus_version", "future"), ("tasks", [])):
        bad = copy.deepcopy(manifest)
        bad[field] = value
        manifest_rejected(bad)
    bad = copy.deepcopy(manifest)
    bad["tasks"].append(copy.deepcopy(bad["tasks"][0]))
    manifest_rejected(bad)
    for text in ('{"schema_version":', '{"x": 1, "x": 2}', '{"x": NaN}', '{"x": 1e999}'):
        path = checker.scratch / "malformed.json"
        path.write_text(text)
        rejects(lambda: read_json(path))
    bad = copy.deepcopy(manifest)
    bad["tasks"][0]["initial"]["main.zi"]["sha256"] = "0" * 64
    manifest_rejected(bad)

    for task in manifest["tasks"]:
        before = checker.evaluate(task, files_for(task["initial"]), task["expected"]["initial"])
        require(before["passed"], task["id"] + " initial: " + json.dumps(before))
        record = solution_transcript(task, manifest)
        result = replay(record, manifest, FIXTURES, checker)
        require(result["passed"], task["id"] + " solution: " + json.dumps(result))
        require(all(x["status"] == "unmeasured" for x in result["usage"]["categories"].values()), "fabricated token measurement")
    record = read_json(FIXTURES / "replay.json")
    first = replay(record, manifest, FIXTURES, checker)
    require(first == replay(record, manifest, FIXTURES, checker) and first["passed"], "replay nondeterministic")
    require(first["metrics"]["retry_count"] == 0 and first["usage"]["total_tokens"] is None, "invalid metrics")
    require(set(first) == {"schema_version", "corpus_version", "task_id", "experiment", "provenance", "model", "compiler",
                           "experiment_metadata", "passed", "attempts", "usage", "metrics"}, "result schema changed")
    require(json.loads(json.dumps(first, allow_nan=False)) == first, "result is not strict JSON")
    for field, value in (("schema_version", 99), ("corpus_version", "0.0.0"), ("task_id", "missing"),
                         ("experiment", "compact"), ("extra", True)):
        bad = copy.deepcopy(record)
        bad[field] = value
        rejects(lambda: load_transcript(bad, manifest, FIXTURES))
    for path in ("../escape.zi", "/abs.zi", "a/../b.zi", "a\\b.zi", "file.c", "unintended.zi"):
        bad = copy.deepcopy(record)
        bad["attempts"][0]["edits"][0]["path"] = path
        rejects(lambda: load_transcript(bad, manifest, FIXTURES))
    bad = copy.deepcopy(record)
    bad["attempts"][0]["edits"][0]["replacement_file"] = "../private.zi"
    rejects(lambda: load_transcript(bad, manifest, FIXTURES))
    task = next(t for t in manifest["tasks"] if t["id"] == record["task_id"])
    initial = files_for(task["initial"])
    edits = record["attempts"][0]["edits"]
    stale = copy.deepcopy(edits)
    stale[0]["base_sha256"] = "0" * 64
    rejects(lambda: apply_edits(initial, stale, FIXTURES))
    rejects(lambda: apply_edits(initial, edits * 2, FIXTURES))
    boundary = copy.deepcopy(edits)
    boundary[0]["start_byte"] = boundary[0]["end_byte"] = initial["main.zi"].index("é".encode()) + 1
    rejects(lambda: apply_edits(initial, boundary, FIXTURES))
    require(initial == files_for(task["initial"]), "rejected edit mutated input")
    retry = copy.deepcopy(record)
    retry["attempts"][0]["edits"] = stale
    retry["attempts"].append({**copy.deepcopy(record["attempts"][0]), "index": 1})
    repaired = replay(retry, manifest, FIXTURES, checker)
    require(repaired["passed"] and repaired["metrics"]["retry_count"] == 1
            and repaired["metrics"]["first_correct_attempt"] == 1, "retry not replayed")
    wrong = copy.deepcopy(record)
    wrong["attempts"][0]["edits"][0].pop("replacement_file")
    wrong["attempts"][0]["edits"][0]["replacement"] = "40"
    wrong_result = replay(wrong, manifest, FIXTURES, checker)
    require(not wrong_result["passed"] and wrong_result["attempts"][0]["edit_accepted"], "incorrect but valid code accepted as correct")
    # Real compiler failure, then repair against the failed candidate's digest.
    repair_task = next(t for t in manifest["tasks"] if t["id"] == "diagnostic-repair")
    repair = solution_transcript(repair_task, manifest)
    bad_attempt = copy.deepcopy(repair["attempts"][0])
    bad_attempt["edits"][0]["replacement_file"] = repair_task["initial"]["main.zi"]["fixture"]
    repair["attempts"] = [bad_attempt, {**copy.deepcopy(repair["attempts"][0]), "index": 1}]
    repaired = replay(repair, manifest, FIXTURES, checker)
    require(repaired["passed"] and not repaired["attempts"][0]["correctness"]["passed"]
            and repaired["metrics"]["first_correct_attempt"] == 1, "compiler repair loop ignored")
    usage = {"category": "prompt", "count": 17, "unit": "tokens",
             "tokenizer": {"family": "openai", "name": "test-only", "version": "fixture-v1"},
             "provenance": {"kind": "synthetic_fixture", "source": "unit test only; not a provider measurement", "locator": "test-case"},
             "cache_policy": "not_reported"}
    supplied = copy.deepcopy(record)
    supplied["attempts"][0]["usage"] = [usage]
    load_transcript(supplied, manifest, FIXTURES)
    measured = usage_report(supplied, manifest)
    require(measured["observations"][0]["tokenizer"] == usage["tokenizer"]
            and measured["observations"][0]["count"] == 17
            and measured["categories"]["prompt"]["status"] == "unmeasured", "synthetic measurement leaked")
    for field, value in (("count", -1), ("count", True), ("unit", "words"), ("category", "cache_guess")):
        bad = copy.deepcopy(supplied)
        bad["attempts"][0]["usage"][0][field] = value
        rejects(lambda: load_transcript(bad, manifest, FIXTURES))
    bad = copy.deepcopy(supplied)
    del bad["attempts"][0]["usage"][0]["tokenizer"]["version"]
    rejects(lambda: load_transcript(bad, manifest, FIXTURES))
    null_tokenizer = copy.deepcopy(supplied)
    null_tokenizer["attempts"][0]["usage"][0]["tokenizer"] = {"family": None, "name": None, "version": None}
    load_transcript(null_tokenizer, manifest, FIXTURES)
    # A mock supplied record exercises preservation, never a reported benchmark.
    supplied["provenance"] = {"kind": "user_supplied", "source": "unit-test mock"}
    supplied["attempts"][0]["usage"][0]["provenance"] = {"kind": "user_supplied", "source": "unit-test mock"}
    load_transcript(supplied, manifest, FIXTURES)
    observed = usage_report(supplied, manifest)
    require(observed["categories"]["prompt"]["status"] == "supplied"
            and observed["categories"]["reasoning"]["status"] == "unmeasured"
            and observed["tokenizer_families"]["anthropic"]["status"] == "unmeasured"
            and observed["total_tokens"] is None, "missing categories were inferred")
    rejects(lambda: unique([("schema_version", 1), ("schema_version", 2)]))
    # Change an oracle independently of implementation to prove mismatches fail.
    wrong_goal = copy.deepcopy(task["expected"]["initial"])
    wrong_goal["run_stdout"] = "999\n"
    require(not checker.evaluate(task, initial, wrong_goal)["passed"], "runtime oracle ignored")
    broken = next(t for t in manifest["tasks"] if t["id"] == "law-repair")
    wrong_goal = copy.deepcopy(broken["expected"]["initial"])
    wrong_goal["checks"]["zx"]["laws"]["Sum"] = "proved"
    require(not checker.evaluate(broken, files_for(broken["initial"]), wrong_goal)["passed"], "law oracle ignored")
    return {"schema_version": VERSION, "corpus_version": manifest["corpus_version"], "passed": True,
            "task_count": len(manifest["tasks"]), "corpus_states_checked": 2 * len(manifest["tasks"]),
            "negative_cases": negative_cases,
            "compiler_tool_turns": checker.turns, "llm_calls": 0, "local_tokenization": "not_run"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ziran", type=Path, default=REPO / "build/bin/ziran")
    parser.add_argument("--manifest", type=Path, default=SURFACE / "tasks.json")
    parser.add_argument("--transcript", type=Path)
    parser.add_argument("--artifacts", type=Path, default=FIXTURES)
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    try:
        manifest = load_manifest(args.manifest)
        transcript = read_json(args.transcript) if args.transcript else None
        if transcript is not None:
            load_transcript(transcript, manifest, args.artifacts.resolve())
        if args.validate_only:
            result = {"schema_version": VERSION, "corpus_version": manifest["corpus_version"], "passed": True, "task_count": len(manifest["tasks"])}
        else:
            scratch_root = REPO / "build/scratch/llm-authoring"
            # Verify the ignore before writing any supplied transcript-derived source.
            ignored = subprocess.run(["git", "check-ignore", "-q", "build/scratch/llm-authoring/record.json"], cwd=REPO)
            require(ignored.returncode == 0, "scratch records must be ignored")
            scratch_root.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory(prefix="run-", dir=scratch_root) as name:
                checker = Checker(args.ziran.resolve(), Path(name))
                result = replay(transcript, manifest, args.artifacts.resolve(), checker) if transcript is not None else self_test(manifest, checker)
        print(json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True))
        return 0 if result["passed"] else 1
    except (Invalid, OSError, UnicodeError, json.JSONDecodeError, subprocess.TimeoutExpired) as error:
        print(json.dumps({"schema_version": VERSION, "passed": False, "error": str(error)}, sort_keys=True), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
