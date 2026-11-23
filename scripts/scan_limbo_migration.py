import argparse
import collections
import json
import os
import re
import sys


INCLUDE_RE = re.compile(r'include\s+"([^"]+)"')
DRAW_REAL_RE = re.compile(
    r"\bload\s+Draw\b|\bDraw->(?!Context\b)[A-Za-z_]\w*|\bdraw->[A-Za-z_]\w*"
)
TK_REAL_RE = re.compile(r"\bload\s+Tk\b|\bTk->[A-Za-z_]\w*|\btk->[A-Za-z_]\w*")
FEATURE_PATTERNS = {
    "alt": re.compile(r"\balt\b"),
    "chan": re.compile(r"\bchan\b"),
    "spawn": re.compile(r"\bspawn\b"),
    "pick": re.compile(r"\bpick\b"),
    "adt": re.compile(r"\badt\b"),
    "ref": re.compile(r"\bref\b"),
    "list": re.compile(r"\blist\s+of\b"),
    "exception": re.compile(r"\b(?:exception|raise)\b"),
    "load": re.compile(r"\bload\b"),
    "cyclic": re.compile(r"\bcyclic\b"),
    "slice": re.compile(r"\[[^\]\n]*:[^\]\n]*\]"),
    "file2chan": re.compile(r"\bfile2chan\b"),
    "ns_ops": re.compile(r"\b(?:mount|unmount|bind|export|pctl|pipe|dial|announce|listen)\b"),
}

WAVE_BY_DIR = {
    "lib": "W1_libraries",
    "math": "W1_libraries",
    "cmd": "W2_cmd_svc",
    "svc": "W2_cmd_svc",
    "tiny": "W2_cmd_svc",
    "alphabet": "W3_frameworks",
    "grid": "W3_frameworks",
    "collab": "W3_frameworks",
    "wm": "W4_gui_wm_acme",
    "acme": "W4_gui_wm_acme",
    "charon": "W4_gui_wm_acme",
    "ebook": "W4_gui_wm_acme",
    "spree": "W5_spree_demos",
    "demo": "W5_spree_demos",
    "examples": "W5_spree_demos",
}

WAVE_ORDER = [
    "W1_libraries",
    "W2_cmd_svc",
    "W3_frameworks",
    "W4_gui_wm_acme",
    "W5_spree_demos",
]

DIFFICULTY_ORDER = [
    "D1_direct_ready",
    "D2_sys_stdlib",
    "D3_concurrent_9p",
    "D4_gui_draw_tk",
]


def strip_limbo_comments_and_strings(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == "#":
            while i < n and text[i] != "\n":
                i += 1
        elif ch == '"' or ch == "'":
            quote = ch
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    i += 2
                else:
                    i += 1
            if i < n:
                i += 1
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def classify_difficulty(loc, includes, features):
    inc_set = set(includes)
    has_gui = (
        features["real_draw"]
        or features["real_tk"]
        or "tk.m" in inc_set
        or "tkclient.m" in inc_set
        or "wmclient.m" in inc_set
        or "prefab.m" in inc_set
        or "freetype.m" in inc_set
        or "dialog.m" in inc_set
    )
    if has_gui:
        return "D4_gui_draw_tk"

    has_conc_or_9p = (
        features["spawn"]
        or features["chan"]
        or features["alt"]
        or features["file2chan"]
        or "styx.m" in inc_set
        or "styxservers.m" in inc_set
        or "9p.m" in inc_set
    )
    if has_conc_or_9p:
        return "D3_concurrent_9p"

    has_crypto_math = bool(
        inc_set.intersection({"keyring.m", "crypt.m", "ipints.m", "math.m", "security.m"})
    )
    if (
        loc <= 400
        and not features["pick"]
        and not features["exception"]
        and not features["ns_ops"]
        and not has_crypto_math
        and "dial.m" not in inc_set
    ):
        return "D1_direct_ready"

    return "D2_sys_stdlib"


def scan_repo(taijios_root):
    appl_root = os.path.join(taijios_root, "appl")
    module_root = os.path.join(taijios_root, "module")

    b_files = []
    zi_files = []
    for dirpath, dirnames, filenames in os.walk(appl_root):
        dirnames.sort()
        for name in sorted(filenames):
            full = os.path.join(dirpath, name)
            if name.endswith(".b"):
                b_files.append(full)
            elif name.endswith(".zi"):
                zi_files.append(full)

    m_files = []
    for dirpath, dirnames, filenames in os.walk(module_root):
        dirnames.sort()
        for name in sorted(filenames):
            if name.endswith(".m"):
                m_files.append(os.path.join(dirpath, name))

    extra_b = []
    for dirpath, dirnames, filenames in os.walk(taijios_root):
        dirnames.sort()
        if os.path.abspath(dirpath).startswith(os.path.abspath(appl_root)):
            continue
        for name in sorted(filenames):
            if name.endswith(".b"):
                extra_b.append(os.path.relpath(os.path.join(dirpath, name), taijios_root))

    dir_counts = collections.Counter()
    dir_loc = collections.Counter()
    wave_counts = collections.Counter()
    wave_loc = collections.Counter()
    diff_counts = collections.Counter()
    diff_loc = collections.Counter()
    matrix = collections.Counter()
    feature_counts = collections.Counter()
    include_counts = collections.Counter()
    total_loc = 0
    file_records = []

    for full in b_files:
        rel = os.path.relpath(full, appl_root)
        top_dir = rel.split(os.sep)[0]
        with open(full, "r", encoding="utf-8", errors="replace") as f:
            raw = f.read()
        loc = raw.count("\n") + (1 if raw and not raw.endswith("\n") else 0)
        total_loc += loc
        includes = INCLUDE_RE.findall(raw)
        for inc in set(includes):
            include_counts[inc] += 1
        stripped = strip_limbo_comments_and_strings(raw)
        features = {k: bool(pat.search(stripped)) for k, pat in FEATURE_PATTERNS.items()}
        features["real_draw"] = bool(DRAW_REAL_RE.search(stripped))
        features["real_tk"] = bool(TK_REAL_RE.search(stripped))
        for k, present in features.items():
            if present:
                feature_counts[k] += 1
        inc_set = set(includes)
        if "draw.m" in inc_set:
            feature_counts["inc_draw"] += 1
            if not features["real_draw"]:
                feature_counts["draw_ctxt_only"] += 1
        if "tk.m" in inc_set or "tkclient.m" in inc_set:
            feature_counts["inc_tk"] += 1
        if "styx.m" in inc_set or "styxservers.m" in inc_set or "9p.m" in inc_set:
            feature_counts["inc_styx"] += 1
        if "sys.m" in inc_set:
            feature_counts["inc_sys"] += 1

        wave = WAVE_BY_DIR.get(top_dir, "W5_spree_demos")
        diff = classify_difficulty(loc, includes, features)

        dir_counts[top_dir] += 1
        dir_loc[top_dir] += loc
        wave_counts[wave] += 1
        wave_loc[wave] += loc
        diff_counts[diff] += 1
        diff_loc[diff] += loc
        matrix[(wave, diff)] += 1
        file_records.append(
            {
                "path": rel,
                "top_dir": top_dir,
                "loc": loc,
                "wave": wave,
                "difficulty": diff,
                "includes": sorted(set(includes)),
            }
        )

    assert sum(wave_counts.values()) == len(b_files)
    assert sum(diff_counts.values()) == len(b_files)
    assert sum(matrix.values()) == len(b_files)

    return {
        "taijios_root": taijios_root,
        "appl_b_count": len(b_files),
        "appl_b_loc": total_loc,
        "module_m_count": len(m_files),
        "appl_zi_count": len(zi_files),
        "non_appl_b_count": len(extra_b),
        "non_appl_b_files": extra_b,
        "dir_counts": dict(dir_counts.most_common()),
        "dir_loc": dict(dir_loc),
        "wave_counts": {w: wave_counts[w] for w in WAVE_ORDER},
        "wave_loc": {w: wave_loc[w] for w in WAVE_ORDER},
        "difficulty_counts": {d: diff_counts[d] for d in DIFFICULTY_ORDER},
        "difficulty_loc": {d: diff_loc[d] for d in DIFFICULTY_ORDER},
        "wave_x_difficulty": {
            w: {d: matrix[(w, d)] for d in DIFFICULTY_ORDER} for w in WAVE_ORDER
        },
        "feature_counts": dict(feature_counts.most_common()),
        "top_includes": include_counts.most_common(30),
        "files": file_records,
    }


def print_report(data):
    print(f"appl/*.b files: {data['appl_b_count']} ({data['appl_b_loc']} lines)")
    print(f"module/*.m files: {data['module_m_count']}")
    print(f"appl/*.zi files: {data['appl_zi_count']}")
    print(f"non-appl .b files: {data['non_appl_b_count']}")
    print()
    print("Top directories under appl/:")
    for d, c in data["dir_counts"].items():
        print(f"  {d:12s} {c:4d} files  {data['dir_loc'][d]:7d} lines")
    print()
    print("By dependency wave (sum =", sum(data["wave_counts"].values()), "):")
    for w in WAVE_ORDER:
        print(f"  {w:18s} {data['wave_counts'][w]:4d} files  {data['wave_loc'][w]:7d} lines")
    print()
    print("By difficulty bucket (sum =", sum(data["difficulty_counts"].values()), "):")
    for d in DIFFICULTY_ORDER:
        print(f"  {d:18s} {data['difficulty_counts'][d]:4d} files  {data['difficulty_loc'][d]:7d} lines")
    print()
    print("Wave x Difficulty:")
    header = "  " + f"{'Wave':18s}" + "".join(f"{d:>18s}" for d in DIFFICULTY_ORDER) + f"{'Total':>8s}"
    print(header)
    for w in WAVE_ORDER:
        row = data["wave_x_difficulty"][w]
        tot = sum(row.values())
        print(
            "  "
            + f"{w:18s}"
            + "".join(f"{row[d]:18d}" for d in DIFFICULTY_ORDER)
            + f"{tot:8d}"
        )
    print()
    print("Feature prevalence across 946 appl/*.b files:")
    for feat, count in data["feature_counts"].items():
        pct = 100.0 * count / data["appl_b_count"]
        print(f"  {feat:14s} {count:4d} ({pct:5.1f}%)")
    print()
    print("Top 25 included .m interfaces:")
    for inc, count in data["top_includes"][:25]:
        print(f"  {inc:22s} {count:4d}")


def main():
    default_root = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "..", "..", "taijiosnet", "taijios")
    )
    parser = argparse.ArgumentParser(description="Scan TaijiOS Limbo sources for Ziran migration.")
    parser.add_argument("--taijios-root", default=default_root)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--expect-count", type=int, default=946)
    args = parser.parse_args()

    data = scan_repo(args.taijios_root)
    if args.expect_count and data["appl_b_count"] != args.expect_count:
        sys.stderr.write(
            f"expected {args.expect_count} appl/*.b files, found {data['appl_b_count']}\n"
        )
        return 1
    if args.json:
        out = dict(data)
        out.pop("files", None)
        print(json.dumps(out, indent=2))
    else:
        print_report(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
