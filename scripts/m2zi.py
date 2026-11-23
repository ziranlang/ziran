import argparse
import os
import re
import sys


KEYWORDS = {
    "adt",
    "array",
    "big",
    "byte",
    "chan",
    "con",
    "cyclic",
    "exception",
    "fn",
    "for",
    "implement",
    "import",
    "include",
    "int",
    "iota",
    "list",
    "module",
    "nil",
    "of",
    "or",
    "pick",
    "real",
    "ref",
    "self",
    "string",
    "type",
}

MULTI_OPS = (
    "->",
    "=>",
    "<<",
    ">>",
    "<=",
    ">=",
    "==",
    "!=",
    "&&",
    "||",
    "::",
    ":=",
)

PRIMITIVE_MAP = {
    "int": "s32",
    "big": "s64",
    "byte": "u8",
    "real": "float64",
    "string": "string",
}

ZIRAN_RESERVED = {
    "struct",
    "enum",
    "union",
    "cast",
    "ifx",
    "then",
    "else",
    "while",
    "for",
    "return",
    "break",
    "continue",
    "defer",
    "using",
    "true",
    "false",
    "null",
    "void",
    "bool",
    "string",
    "type",
    "in",
}

RADIX_RE = re.compile(r"^([0-9]+)[rR]([0-9A-Za-z]+)$")


class ParseError(Exception):
    pass


class Token:
    __slots__ = ("kind", "value", "line", "col")

    def __init__(self, kind, value, line, col):
        self.kind = kind
        self.value = value
        self.line = line
        self.col = col

    def __repr__(self):
        return f"Token({self.kind!r}, {self.value!r}, {self.line}:{self.col})"


def tokenize(text, filename="<input>"):
    tokens = []
    i = 0
    n = len(text)
    line = 1
    col = 1
    hash_char = chr(35)

    while i < n:
        ch = text[i]
        if ch == "\n":
            i += 1
            line += 1
            col = 1
            continue
        if ch in " \t\r\f\v":
            i += 1
            col += 1
            continue
        if ch == hash_char:
            while i < n and text[i] != "\n":
                i += 1
            continue

        start_line = line
        start_col = col

        if ch == '"':
            j = i + 1
            while j < n and text[j] != '"':
                if text[j] == "\\" and j + 1 < n:
                    j += 2
                else:
                    if text[j] == "\n":
                        line += 1
                        col = 1
                    j += 1
            if j >= n:
                raise ParseError(f"{filename}:{start_line}:{start_col}: unterminated string")
            val = text[i : j + 1]
            tokens.append(Token("STRING", val, start_line, start_col))
            col += (j + 1) - i
            i = j + 1
            continue

        if ch == "'":
            j = i + 1
            while j < n and text[j] != "'":
                if text[j] == "\\" and j + 1 < n:
                    j += 2
                else:
                    j += 1
            if j >= n:
                raise ParseError(f"{filename}:{start_line}:{start_col}: unterminated char literal")
            val = text[i : j + 1]
            tokens.append(Token("CHAR", val, start_line, start_col))
            col += (j + 1) - i
            i = j + 1
            continue

        if ch.isdigit() or (ch == "." and i + 1 < n and text[i + 1].isdigit()):
            j = i
            while j < n and text[j].isdigit():
                j += 1
            if j < n and text[j] in "rR" and j > i:
                j += 1
                while j < n and (text[j].isdigit() or ("a" <= text[j].lower() <= "z")):
                    j += 1
            else:
                if j < n and text[j] == ".":
                    j += 1
                    while j < n and text[j].isdigit():
                        j += 1
                if j < n and text[j] in "eE":
                    k = j + 1
                    if k < n and text[k] in "+-":
                        k += 1
                    if k < n and text[k].isdigit():
                        j = k
                        while j < n and text[j].isdigit():
                            j += 1
            val = text[i:j]
            tokens.append(Token("NUMBER", val, start_line, start_col))
            col += j - i
            i = j
            continue

        if ch.isalpha() or ch == "_":
            j = i + 1
            while j < n and (text[j].isalnum() or text[j] == "_"):
                j += 1
            val = text[i:j]
            kind = "KW" if val in KEYWORDS else "IDENT"
            tokens.append(Token(kind, val, start_line, start_col))
            col += j - i
            i = j
            continue

        matched_op = None
        for op in MULTI_OPS:
            if text.startswith(op, i):
                matched_op = op
                break
        if matched_op is not None:
            tokens.append(Token("OP", matched_op, start_line, start_col))
            i += len(matched_op)
            col += len(matched_op)
            continue

        tokens.append(Token("OP", ch, start_line, start_col))
        i += 1
        col += 1

    tokens.append(Token("EOF", "", line, col))
    return tokens


class Parser:
    def __init__(self, tokens, filename="<input>"):
        self.tokens = tokens
        self.pos = 0
        self.filename = filename

    def peek(self, offset=0):
        idx = self.pos + offset
        if idx >= len(self.tokens):
            return self.tokens[-1]
        return self.tokens[idx]

    def at_eof(self):
        return self.peek().kind == "EOF"

    def advance(self):
        tok = self.peek()
        if self.pos < len(self.tokens) - 1:
            self.pos += 1
        return tok

    def error(self, msg, tok=None):
        if tok is None:
            tok = self.peek()
        raise ParseError(f"{self.filename}:{tok.line}:{tok.col}: {msg} (got {tok.kind} {tok.value!r})")

    def match(self, kind, value=None):
        tok = self.peek()
        if tok.kind == kind and (value is None or tok.value == value):
            self.advance()
            return tok
        return None

    def expect(self, kind, value=None):
        tok = self.match(kind, value)
        if tok is None:
            want = f"{kind} {value!r}" if value is not None else kind
            self.error(f"expected {want}")
        return tok

    def expect_name(self):
        tok = self.peek()
        if tok.kind in ("IDENT", "KW"):
            self.advance()
            return tok.value
        self.error("expected identifier")

    def parse_file(self):
        file_ast = {
            "filename": self.filename,
            "implement": None,
            "includes": [],
            "modules": [],
            "top_constants": [],
            "top_types": [],
            "top_adts": [],
            "top_functions": [],
            "top_vars": [],
        }
        while not self.at_eof():
            if self.match("OP", ";"):
                continue
            tok = self.peek()
            if tok.kind == "KW" and tok.value == "implement":
                self.advance()
                file_ast["implement"] = self.expect_name()
                self.expect("OP", ";")
                continue
            if tok.kind == "KW" and tok.value == "include":
                self.advance()
                inc = self.expect("STRING").value
                file_ast["includes"].append(inc.strip('"'))
                self.expect("OP", ";")
                continue

            if self.is_module_decl():
                file_ast["modules"].append(self.parse_module())
                continue

            decl = self.parse_member_decl()
            self.store_decl(file_ast, decl, prefix="top_")
        return file_ast

    def is_module_decl(self):
        if self.peek(0).kind not in ("IDENT", "KW"):
            return False
        if self.peek(1).kind != "OP" or self.peek(1).value != ":":
            return False
        return self.peek(2).kind == "KW" and self.peek(2).value == "module"

    def parse_module(self):
        name = self.expect_name()
        self.expect("OP", ":")
        self.expect("KW", "module")
        self.expect("OP", "{")
        mod = {
            "name": name,
            "path": None,
            "constants": [],
            "types": [],
            "adts": [],
            "functions": [],
            "vars": [],
            "exceptions": [],
        }
        while not self.at_eof() and not (self.peek().kind == "OP" and self.peek().value == "}"):
            if self.match("OP", ";"):
                continue
            decl = self.parse_member_decl()
            if decl["kind"] == "con":
                for cname, cexpr in decl["items"]:
                    if cname == "PATH" and mod["path"] is None:
                        mod["path"] = cexpr
                    mod["constants"].append({"name": cname, "expr": cexpr})
            elif decl["kind"] == "type":
                for tname in decl["names"]:
                    mod["types"].append({"name": tname, "target": decl["target"]})
            elif decl["kind"] == "adt":
                mod["adts"].append(decl)
            elif decl["kind"] == "fn":
                for fname in decl["names"]:
                    mod["functions"].append(
                        {
                            "name": fname,
                            "type_params": decl["type_params"],
                            "params": decl["params"],
                            "ret": decl["ret"],
                            "is_method": False,
                        }
                    )
            elif decl["kind"] == "exception":
                mod["exceptions"].append(decl)
            elif decl["kind"] == "var":
                for vname in decl["names"]:
                    mod["vars"].append({"name": vname, "type": decl["type"]})
        self.expect("OP", "}")
        self.match("OP", ";")
        return mod

    def store_decl(self, target, decl, prefix=""):
        if decl["kind"] == "con":
            for cname, cexpr in decl["items"]:
                target[prefix + "constants"].append({"name": cname, "expr": cexpr})
        elif decl["kind"] == "type":
            for tname in decl["names"]:
                target[prefix + "types"].append({"name": tname, "target": decl["target"]})
        elif decl["kind"] == "adt":
            target[prefix + "adts"].append(decl)
        elif decl["kind"] == "fn":
            for fname in decl["names"]:
                target[prefix + "functions"].append(
                    {
                        "name": fname,
                        "type_params": decl["type_params"],
                        "params": decl["params"],
                        "ret": decl["ret"],
                        "is_method": False,
                    }
                )
        elif decl["kind"] == "var":
            for vname in decl["names"]:
                target[prefix + "vars"].append({"name": vname, "type": decl["type"]})

    def parse_member_decl(self):
        is_cyclic = bool(self.match("KW", "cyclic"))
        names = [self.expect_name()]
        while self.match("OP", ","):
            names.append(self.expect_name())
        self.expect("OP", ":")

        tok = self.peek()
        if tok.kind == "KW" and tok.value == "con":
            self.advance()
            expr_tokens = self.consume_expr_until_semicolon()
            self.expect("OP", ";")
            items = expand_const_decl(names, expr_tokens)
            return {"kind": "con", "items": items}

        if tok.kind == "KW" and tok.value == "type":
            self.advance()
            target = self.parse_type()
            self.expect("OP", ";")
            return {"kind": "type", "names": names, "target": target}

        if tok.kind == "KW" and tok.value == "adt":
            if len(names) != 1:
                self.error("expected single name for adt")
            return self.parse_adt(names[0])

        if tok.kind == "KW" and tok.value == "fn":
            fn_sig = self.parse_fn_sig()
            self.skip_optional_for_clause()
            self.expect("OP", ";")
            fn_sig["kind"] = "fn"
            fn_sig["names"] = names
            return fn_sig

        if tok.kind == "KW" and tok.value == "exception":
            self.advance()
            payload = []
            if self.match("OP", "("):
                if not (self.peek().kind == "OP" and self.peek().value == ")"):
                    payload.append(self.parse_type())
                    while self.match("OP", ","):
                        payload.append(self.parse_type())
                self.expect("OP", ")")
            self.expect("OP", ";")
            return {"kind": "exception", "names": names, "payload": payload}

        vtype = self.parse_type()
        if self.match("OP", "="):
            self.consume_expr_until_semicolon()
        self.expect("OP", ";")
        return {"kind": "var", "names": names, "type": vtype, "cyclic": is_cyclic}

    def parse_adt(self, name):
        self.expect("KW", "adt")
        type_params = self.parse_optional_type_params()
        self.skip_optional_for_clause()
        self.expect("OP", "{")
        fields = []
        picks = []
        constants = []
        methods = []
        while not self.at_eof() and not (self.peek().kind == "OP" and self.peek().value == "}"):
            if self.match("OP", ";"):
                continue
            if self.peek().kind == "KW" and self.peek().value == "pick":
                picks.extend(self.parse_pick())
                self.match("OP", ";")
                continue
            decl = self.parse_member_decl()
            if decl["kind"] == "var":
                for fname in decl["names"]:
                    fields.append({"name": fname, "type": decl["type"]})
            elif decl["kind"] == "con":
                for cname, cexpr in decl["items"]:
                    constants.append({"name": cname, "expr": cexpr})
            elif decl["kind"] == "fn":
                for mname in decl["names"]:
                    methods.append(
                        {
                            "name": mname,
                            "type_params": decl["type_params"],
                            "params": decl["params"],
                            "ret": decl["ret"],
                            "is_method": True,
                        }
                    )
        self.expect("OP", "}")
        self.match("OP", ";")
        return {
            "kind": "adt",
            "name": name,
            "type_params": type_params,
            "fields": fields,
            "picks": picks,
            "constants": constants,
            "methods": methods,
        }

    def is_pick_arm_start(self):
        offset = 0
        if self.peek(offset).kind not in ("IDENT", "KW"):
            return False
        if self.peek(offset).value in ("cyclic", "pick"):
            return False
        offset += 1
        while self.peek(offset).kind == "KW" and self.peek(offset).value == "or":
            offset += 1
            if self.peek(offset).kind not in ("IDENT", "KW"):
                return False
            offset += 1
        return self.peek(offset).kind == "OP" and self.peek(offset).value == "=>"

    def parse_pick(self):
        self.expect("KW", "pick")
        self.expect("OP", "{")
        variants = []
        while not self.at_eof() and not (self.peek().kind == "OP" and self.peek().value == "}"):
            if self.match("OP", ";"):
                continue
            tag_names = [self.expect_name()]
            while self.match("KW", "or"):
                tag_names.append(self.expect_name())
            self.expect("OP", "=>")
            arm_fields = []
            while not self.at_eof():
                if self.peek().kind == "OP" and self.peek().value == "}":
                    break
                if self.match("OP", ";"):
                    continue
                if self.is_pick_arm_start():
                    break
                decl = self.parse_member_decl()
                if decl["kind"] == "var":
                    for fname in decl["names"]:
                        arm_fields.append({"name": fname, "type": decl["type"]})
            for tname in tag_names:
                variants.append({"name": tname, "fields": list(arm_fields)})
        self.expect("OP", "}")
        return variants

    def parse_optional_type_params(self):
        params = []
        if self.match("OP", "["):
            if not (self.peek().kind == "OP" and self.peek().value == "]"):
                params.append(self.expect_name())
                while self.match("OP", ","):
                    params.append(self.expect_name())
            self.expect("OP", "]")
        return params

    def skip_optional_for_clause(self):
        if not self.match("KW", "for"):
            return
        self.expect("OP", "{")
        depth = 1
        while not self.at_eof() and depth > 0:
            tok = self.advance()
            if tok.kind == "OP" and tok.value == "{":
                depth += 1
            elif tok.kind == "OP" and tok.value == "}":
                depth -= 1

    def parse_fn_sig(self):
        self.expect("KW", "fn")
        type_params = self.parse_optional_type_params()
        params = self.parse_param_list()
        ret = None
        if self.match("OP", ":"):
            ret = self.parse_type()
        return {"type_params": type_params, "params": params, "ret": ret}

    def parse_param_list(self):
        self.expect("OP", "(")
        params = []
        if self.match("OP", ")"):
            return params
        while True:
            if self.match("OP", "*"):
                params.append({"name": "args", "type": "..Any", "self": False, "variadic": True})
            else:
                params.extend(self.parse_param_group())
            if self.match("OP", ","):
                if self.peek().kind == "OP" and self.peek().value == ")":
                    break
                continue
            break
        self.expect("OP", ")")
        return params

    def looks_like_named_param_group(self):
        offset = 0
        tok = self.peek(offset)
        if tok.kind not in ("IDENT", "KW"):
            return False
        if tok.kind == "KW" and tok.value not in ("nil", "self") and tok.value in (
            "int",
            "big",
            "byte",
            "real",
            "string",
            "array",
            "list",
            "ref",
            "chan",
            "fn",
            "cyclic",
        ):
            nxt = self.peek(1)
            if not (nxt.kind == "OP" and nxt.value in (":", ",")):
                return False
        offset += 1
        while self.peek(offset).kind == "OP" and self.peek(offset).value == ",":
            offset += 1
            if self.peek(offset).kind not in ("IDENT", "KW"):
                return False
            offset += 1
        return self.peek(offset).kind == "OP" and self.peek(offset).value == ":"

    def parse_param_group(self):
        if self.looks_like_named_param_group():
            names = [self.expect_name()]
            while self.match("OP", ","):
                names.append(self.expect_name())
            self.expect("OP", ":")
            is_self = bool(self.match("KW", "self"))
            ptype = self.parse_type()
            out = []
            for idx, pname in enumerate(names):
                if pname == "nil":
                    eff = "self" if is_self else f"_p{idx}"
                else:
                    eff = pname
                out.append({"name": eff, "type": ptype, "self": is_self, "variadic": False})
            return out
        is_self = bool(self.match("KW", "self"))
        ptype = self.parse_type()
        return [{"name": "self" if is_self else None, "type": ptype, "self": is_self, "variadic": False}]

    def parse_type(self):
        self.match("KW", "cyclic")
        tok = self.peek()

        if tok.kind == "KW" and tok.value in PRIMITIVE_MAP:
            self.advance()
            return PRIMITIVE_MAP[tok.value]

        if tok.kind == "KW" and tok.value == "array":
            self.advance()
            self.expect("KW", "of")
            elem = self.parse_type()
            return f"[]{elem}"

        if tok.kind == "KW" and tok.value == "list":
            self.advance()
            self.expect("KW", "of")
            elem = self.parse_type()
            return f"Vec({elem})"

        if tok.kind == "KW" and tok.value == "chan":
            self.advance()
            self.expect("KW", "of")
            self.parse_type()
            return "u64"

        if tok.kind == "KW" and tok.value == "ref":
            self.advance()
            if self.peek().kind == "KW" and self.peek().value == "fn":
                sig = self.parse_fn_sig()
                return format_fn_type(sig)
            inner = self.parse_type()
            return f"*{inner}"

        if tok.kind == "KW" and tok.value == "fn":
            sig = self.parse_fn_sig()
            return format_fn_type(sig)

        if tok.kind == "OP" and tok.value == "(":
            self.advance()
            elems = []
            if not (self.peek().kind == "OP" and self.peek().value == ")"):
                elems.append(self.parse_type())
                while self.match("OP", ","):
                    elems.append(self.parse_type())
            self.expect("OP", ")")
            if len(elems) == 1:
                return elems[0]
            return "(" + ", ".join(elems) + ")"

        if tok.kind in ("IDENT", "KW"):
            base = self.advance().value
            parts = [base]
            while True:
                if self.match("OP", "->"):
                    parts.append(self.expect_name())
                elif self.match("OP", "."):
                    parts.append(self.expect_name())
                else:
                    break
            type_name = ".".join(parts)
            if self.match("OP", "["):
                targs = []
                if not (self.peek().kind == "OP" and self.peek().value == "]"):
                    targs.append(self.parse_type())
                    while self.match("OP", ","):
                        targs.append(self.parse_type())
                self.expect("OP", "]")
                return f"{type_name}({', '.join(targs)})"
            return type_name

        self.error("expected type")

    def consume_expr_until_semicolon(self):
        expr = []
        paren = 0
        bracket = 0
        brace = 0
        while not self.at_eof():
            tok = self.peek()
            if tok.kind == "OP" and tok.value == ";" and paren == 0 and bracket == 0 and brace == 0:
                break
            self.advance()
            if tok.kind == "OP":
                if tok.value == "(":
                    paren += 1
                elif tok.value == ")":
                    paren -= 1
                elif tok.value == "[":
                    bracket += 1
                elif tok.value == "]":
                    bracket -= 1
                elif tok.value == "{":
                    brace += 1
                elif tok.value == "}":
                    brace -= 1
            expr.append(tok)
        return expr


def convert_number_literal(raw):
    m = RADIX_RE.match(raw)
    if not m:
        return raw
    base = int(m.group(1))
    digits = m.group(2)
    if base == 16:
        return "0x" + digits
    if base == 8:
        return "0o" + digits
    if base == 2:
        return "0b" + digits
    return str(int(digits, base))


def format_expr_tokens(tokens, iota_value=None):
    out = []
    i = 0
    n = len(tokens)
    while i < n:
        tok = tokens[i]
        if tok.kind == "KW" and tok.value == "iota":
            out.append(str(iota_value if iota_value is not None else 0))
            i += 1
            continue
        if tok.kind == "KW" and tok.value == "nil":
            out.append("null")
            i += 1
            continue
        if tok.kind == "KW" and tok.value in PRIMITIVE_MAP:
            ztype = PRIMITIVE_MAP[tok.value]
            if i + 1 < n:
                out.append(f"cast({ztype})")
                i += 1
                continue
        if tok.kind == "NUMBER":
            out.append(convert_number_literal(tok.value))
            i += 1
            continue
        if tok.kind == "OP" and tok.value == "->":
            out.append(".")
            i += 1
            continue
        out.append(tok.value)
        i += 1
    return join_expr_pieces(out)


def join_expr_pieces(pieces):
    if not pieces:
        return "0"
    res = pieces[0]
    for prev, cur in zip(pieces, pieces[1:]):
        if prev in ("(", "[", "~", "!", ".", "cast(s32)", "cast(s64)", "cast(u8)", "cast(float64)"):
            res += cur
        elif cur in (")", "]", ",", "."):
            res += cur
        elif cur == "(" and (prev[-1].isalnum() or prev[-1] == "_"):
            res += cur
        elif prev in ("+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^", "==", "!=", "<", ">", "<=", ">="):
            res += " " + cur
        elif cur in ("+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^", "==", "!=", "<", ">", "<=", ">="):
            res += " " + cur
        else:
            res += " " + cur
    return res


def eval_simple_iota(tokens, idx):
    if len(tokens) == 1 and tokens[0].kind == "KW" and tokens[0].value == "iota":
        return str(idx)
    if (
        len(tokens) == 3
        and tokens[0].kind == "NUMBER"
        and tokens[1].kind == "OP"
        and tokens[1].value == "+"
        and tokens[2].kind == "KW"
        and tokens[2].value == "iota"
    ):
        base_str = convert_number_literal(tokens[0].value)
        try:
            base = int(base_str, 0)
            return str(base + idx)
        except ValueError:
            pass
    if (
        len(tokens) == 3
        and tokens[0].kind == "KW"
        and tokens[0].value == "iota"
        and tokens[1].kind == "OP"
        and tokens[1].value == "+"
        and tokens[2].kind == "NUMBER"
    ):
        base_str = convert_number_literal(tokens[2].value)
        try:
            base = int(base_str, 0)
            return str(base + idx)
        except ValueError:
            pass
    return format_expr_tokens(tokens, iota_value=idx)


def expand_const_decl(names, expr_tokens):
    has_iota = any(t.kind == "KW" and t.value == "iota" for t in expr_tokens)
    if has_iota:
        return [(name, eval_simple_iota(expr_tokens, idx)) for idx, name in enumerate(names)]
    rendered = format_expr_tokens(expr_tokens)
    return [(name, rendered) for name in names]


def safe_ident(name):
    if name in ZIRAN_RESERVED:
        return name + "_"
    return name


def format_params(params, type_params=None):
    parts = []
    if type_params:
        for tp in type_params:
            parts.append(f"${safe_ident(tp)}: Type")
    for idx, p in enumerate(params):
        pname = p["name"] if p["name"] else f"arg{idx}"
        parts.append(f"{safe_ident(pname)}: {p['type']}")
    return ", ".join(parts)


def format_fn_type(sig):
    params_str = format_params(sig["params"], sig["type_params"])
    ret_str = sig["ret"] if sig["ret"] else "void"
    return f"#type ({params_str}) -> {ret_str}"


def emit_fn_stub(name, fn_info, lines):
    params_str = format_params(fn_info["params"], fn_info["type_params"])
    ret_str = fn_info["ret"] if fn_info["ret"] else "void"
    lines.append(f"{safe_ident(name)} :: ({params_str}) -> {ret_str};")


def emit_adt(adt, lines):
    adt_name = safe_ident(adt["name"])
    tparams = adt["type_params"]
    tp_suffix = ""
    if tparams:
        tp_suffix = "(" + ", ".join(f"${safe_ident(tp)}: Type" for tp in tparams) + ")"

    if adt["picks"]:
        lines.append(f"{adt_name}Kind :: enum {{")
        for var in adt["picks"]:
            lines.append(f"    {safe_ident(var['name'])};")
        lines.append("}")
        lines.append("")

        for var in adt["picks"]:
            if var["fields"]:
                vstruct = f"{adt_name}_{safe_ident(var['name'])}"
                lines.append(f"{vstruct} :: struct{tp_suffix} {{")
                for fld in var["fields"]:
                    lines.append(f"    {safe_ident(fld['name'])}: {fld['type']};")
                lines.append("}")
                lines.append("")

    lines.append(f"{adt_name} :: struct{tp_suffix} {{")
    if adt["picks"]:
        lines.append(f"    kind: {adt_name}Kind;")
    for fld in adt["fields"]:
        lines.append(f"    {safe_ident(fld['name'])}: {fld['type']};")
    if adt["picks"]:
        tp_args = ""
        if tparams:
            tp_args = "(" + ", ".join(safe_ident(tp) for tp in tparams) + ")"
        for var in adt["picks"]:
            if var["fields"]:
                vfield = safe_ident(var["name"])
                vstruct = f"{adt_name}_{vfield}{tp_args}"
                lines.append(f"    {vfield}: {vstruct};")
    lines.append("}")

    for con in adt["constants"]:
        lines.append(f"{adt_name}_{safe_ident(con['name'])} :: {con['expr']};")

    for method in adt["methods"]:
        mname = f"{adt_name}_{safe_ident(method['name'])}"
        emit_fn_stub(mname, method, lines)

    lines.append("")


def emit_ziran(file_ast):
    lines = []
    source_name = os.path.basename(file_ast["filename"])
    lines.append(f"// Generated from {source_name} by scripts/m2zi.py")
    if file_ast["implement"]:
        lines.append(f"// implement {file_ast['implement']}")
    for inc in file_ast["includes"]:
        mod_base = os.path.splitext(inc)[0]
        lines.append(f'#import "{mod_base}";')
    if file_ast["includes"]:
        lines.append("")

    for con in file_ast["top_constants"]:
        lines.append(f"{safe_ident(con['name'])} :: {con['expr']};")
    if file_ast["top_constants"]:
        lines.append("")

    for tdecl in file_ast["top_types"]:
        lines.append(f"{safe_ident(tdecl['name'])} :: {tdecl['target']};")
    if file_ast["top_types"]:
        lines.append("")

    for adt in file_ast["top_adts"]:
        emit_adt(adt, lines)

    for fn in file_ast["top_functions"]:
        emit_fn_stub(fn["name"], fn, lines)
    if file_ast["top_functions"]:
        lines.append("")

    for mod in file_ast["modules"]:
        lines.append(f"// module {mod['name']}")
        if mod["path"] is not None:
            lines.append(f"// PATH {mod['path']}")
        if mod["constants"]:
            for con in mod["constants"]:
                lines.append(f"{safe_ident(con['name'])} :: {con['expr']};")
            lines.append("")
        if mod["types"]:
            for tdecl in mod["types"]:
                lines.append(f"{safe_ident(tdecl['name'])} :: {tdecl['target']};")
            lines.append("")
        for adt in mod["adts"]:
            emit_adt(adt, lines)
        if mod["vars"]:
            for vdecl in mod["vars"]:
                lines.append(f"{safe_ident(vdecl['name'])}: {vdecl['type']};")
            lines.append("")
        if mod["functions"]:
            for fn in mod["functions"]:
                emit_fn_stub(fn["name"], fn, lines)
            lines.append("")

    while lines and lines[-1] == "":
        lines.pop()
    return "\n".join(lines) + "\n"


def parse_file_path(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    tokens = tokenize(text, filename=path)
    parser = Parser(tokens, filename=path)
    return parser.parse_file()


def collect_stats(file_ast):
    modules = len(file_ast["modules"])
    constants = len(file_ast["top_constants"])
    adts = len(file_ast["top_adts"])
    picks = sum(len(a["picks"]) for a in file_ast["top_adts"])
    functions = len(file_ast["top_functions"]) + sum(len(a["methods"]) for a in file_ast["top_adts"])
    for mod in file_ast["modules"]:
        constants += len(mod["constants"])
        adts += len(mod["adts"])
        functions += len(mod["functions"])
        for adt in mod["adts"]:
            constants += len(adt["constants"])
            picks += len(adt["picks"])
            functions += len(adt["methods"])
    return {
        "modules": modules,
        "constants": constants,
        "adts": adts,
        "picks": picks,
        "functions": functions,
    }


def check_all(module_dir):
    m_files = []
    for dirpath, dirnames, filenames in os.walk(module_dir):
        dirnames.sort()
        for name in sorted(filenames):
            if name.endswith(".m"):
                m_files.append(os.path.join(dirpath, name))

    errors = []
    totals = {
        "files": len(m_files),
        "modules": 0,
        "constants": 0,
        "adts": 0,
        "picks": 0,
        "functions": 0,
    }

    for path in m_files:
        try:
            ast = parse_file_path(path)
            rendered = emit_ziran(ast)
            if not rendered:
                errors.append((path, "empty output"))
            st = collect_stats(ast)
            for k in ("modules", "constants", "adts", "picks", "functions"):
                totals[k] += st[k]
        except Exception as exc:
            errors.append((path, str(exc)))

    if errors:
        for path, err in errors[:30]:
            print(f"ERROR {path}: {err}", file=sys.stderr)
        print(f"Total parse errors: {len(errors)} / {len(m_files)}", file=sys.stderr)

    assert len(m_files) == 180, f"expected 180 .m files, found {len(m_files)}"
    assert len(errors) == 0, f"expected 0 parse errors, got {len(errors)}"

    print(f"Checked {totals['files']} .m files in {module_dir}: 0 parse errors")
    print(f"  modules:   {totals['modules']}")
    print(f"  constants: {totals['constants']}")
    print(f"  adts:      {totals['adts']}")
    print(f"  picks:     {totals['picks']}")
    print(f"  functions: {totals['functions']}")
    return 0


def main():
    default_mod_dir = "/mnt/storage/Projects/taijiosnet/taijios/module"
    ap = argparse.ArgumentParser(description="Convert Limbo .m module interfaces to Ziran .zi stubs.")
    ap.add_argument("input", nargs="?", help="Input Limbo .m file")
    ap.add_argument("-o", "--output", help="Output Ziran .zi file path")
    ap.add_argument("--check-all", action="store_true", help="Parse and convert all .m files in module-dir")
    ap.add_argument("--module-dir", default=default_mod_dir, help="Directory containing Limbo .m files")
    args = ap.parse_args()

    if args.check_all:
        return check_all(args.module_dir)

    if not args.input:
        ap.error("either an input .m file or --check-all is required")

    ast = parse_file_path(args.input)
    rendered = emit_ziran(ast)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(rendered)
    else:
        sys.stdout.write(rendered)
    return 0


if __name__ == "__main__":
    sys.exit(main())
