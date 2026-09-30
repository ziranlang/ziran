#!/bin/sh
set -eu

usage()
{
    cat <<'USAGE'
usage: zi-fmt [--check] file.zi [...]

Formats Ziran source with stable indentation and simple spacing cleanup.
USAGE
}

check=0
if [ "${1:-}" = "--check" ]; then
    check=1
    shift
fi

[ $# -gt 0 ] || { usage >&2; exit 2; }

status=0
tmp=
trap 'if [ -n "$tmp" ]; then rm -f "$tmp"; fi' 0
for file in "$@"; do
    [ -f "$file" ] || { printf 'zi-fmt: not found: %s\n' "$file" >&2; status=1; continue; }
    tmp=$(mktemp "${TMPDIR:-/tmp}/zi-fmt.XXXXXXXX")
    cp -p "$file" "$tmp"
    awk '
    function trim(s) {
        sub(/^[ \t\r\n]+/, "", s)
        sub(/[ \t\r\n]+$/, "", s)
        return s
    }
    function count_char(s, ch,    i,n,c,q,esc) {
        n = 0; q = ""; esc = 0
        for(i = 1; i <= length(s); i++) {
            c = substr(s, i, 1)
            if(q != "") {
                if(esc) esc = 0
                else if(c == "\\") esc = 1
                else if(c == q) q = ""
            } else if(c == "\"" || c == "'\''") {
                q = c
            } else if(c == ch) {
                n++
            }
        }
        return n
    }
    function fmt_spacing(s,    out,i,c,n,q,esc,nextc,prevc) {
        out = ""; q = ""; esc = 0; n = length(s)
        for(i = 1; i <= n; i++) {
            c = substr(s, i, 1)
            nextc = i < n ? substr(s, i + 1, 1) : ""
            prevc = i > 1 ? substr(s, i - 1, 1) : ""
            if(q != "") {
                out = out c
                if(esc) esc = 0
                else if(c == "\\") esc = 1
                else if(c == q) q = ""
            } else if(c == "\"" || c == "'\''") {
                q = c; out = out c
            } else if(c == ":" && nextc == ":") {
                sub(/[ \t]+$/, "", out)
                out = out " :: "
                i++
                while(i < n && substr(s, i + 1, 1) ~ /[ \t]/) i++
            } else if(c == "=" && nextc == ">") {
                sub(/[ \t]+$/, "", out)
                out = out " => "
                i++
                while(i < n && substr(s, i + 1, 1) ~ /[ \t]/) i++
            } else if(c == "=" && prevc !~ /[!<>=:+\-*\/%&|^]/ && nextc != "=") {
                sub(/[ \t]+$/, "", out)
                out = out " = "
                while(i < n && substr(s, i + 1, 1) ~ /[ \t]/) i++
            } else {
                out = out c
            }
        }
        return trim(out)
    }
    function string_marker(s,    i,c,q,esc,rest,marker) {
        q = ""; esc = 0
        for(i = 1; i <= length(s); i++) {
            c = substr(s, i, 1)
            if(block_comment) {
                if(c == "/" && substr(s, i + 1, 1) == "*") {
                    block_comment++; i++
                } else if(c == "*" && substr(s, i + 1, 1) == "/") {
                    block_comment--; i++
                }
                continue
            }
            if(q != "") {
                if(esc) esc = 0
                else if(c == "\\") esc = 1
                else if(c == q) q = ""
                continue
            }
            if(c == "\"" || c == "'\''") { q = c; continue }
            if(c == "/" && substr(s, i + 1, 1) == "/") break
            if(c == "/" && substr(s, i + 1, 1) == "*") {
                block_comment++; i++; continue
            }
            if(c == "#" && substr(s, i, 7) == "#string" &&
               (i == 1 || substr(s, i - 1, 1) !~ /[[:alnum:]_]/)) {
                rest = substr(s, i + 7)
                if(rest !~ /^[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t\r]*$/)
                    continue
                sub(/^[ \t]+/, "", rest)
                marker = rest
                sub(/[ \t\r]+$/, "", marker)
                return marker
            }
        }
        return ""
    }
    function track_structure(line,    opens,closes,delta) {
        paren_depth += count_char(line, "(") - count_char(line, ")")
        if(paren_depth < 0) paren_depth = 0
        opens = count_char(line, "{")
        closes = count_char(line, "}")
        delta = opens - closes
        if(substr(line, 1, 1) == "}") delta++
        indent += delta
        if(indent < 0) indent = 0
    }
    {
        raw = $0
        if(raw_delimiter != "") {
            print raw
            n = length(raw_delimiter)
            nextc = substr(raw, n + 1, 1)
            if(substr(raw, 1, n) == raw_delimiter &&
               nextc !~ /[[:alnum:]_]/) {
                suffix = substr(raw, n + 1)
                raw_delimiter = string_marker(suffix)
                if(substr(suffix, 1, 1) == "}" && indent > 0) indent--
                track_structure(suffix)
            }
            next
        }
        marker = string_marker(raw)
        line = fmt_spacing(trim(raw))
        if(line == "") {
            print ""
            next
        }
        leading_close = substr(line, 1, 1) == "}"
        if(leading_close && indent > 0)
            indent--
        continuation = paren_depth > 0 ? 1 : 0
        for(i = 0; i < indent + continuation; i++)
            printf "    "
        print line
        track_structure(line)
        if(marker != "") raw_delimiter = marker
    }' "$file" > "$tmp"
    if [ "$check" -eq 1 ]; then
        if ! cmp -s "$file" "$tmp"; then
            printf 'zi-fmt: would reformat %s\n' "$file" >&2
            status=1
        fi
        rm -f "$tmp"
    else
        mv "$tmp" "$file"
    fi
    tmp=
done

exit "$status"
