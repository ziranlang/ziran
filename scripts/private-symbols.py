#!/usr/bin/env python3
"""List a split compiler module's private declarations for COFF localization."""
from pathlib import Path
import re
import sys

header = Path(sys.argv[1]).read_text()
block = header.split("#pragma GCC visibility push(hidden)", 1)[1].split(
    "#pragma GCC visibility pop", 1
)[0]
block = re.sub(r"/\*.*?\*/|//[^\n]*", "", block, flags=re.S)
keywords = {
    "char", "const", "double", "float", "int", "long", "short", "signed",
    "sizeof", "struct", "union", "unsigned", "void",
}
names = set(re.findall(r"\b([A-Za-z_]\w*)\s*\((?!\s*\*)", block)) - keywords
output = Path(sys.argv[2])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("".join(name + "\n" for name in sorted(names)))
