"""Minify GLSL for embedding: strip comments/whitespace, shorten floats, rename locals.

Uniforms, stage inputs/outputs, #version and struct members keep their names,
since the C side and the other pipeline stages refer to them.

usage: minify_glsl.py <in_dir> <out_dir>
"""
import os
import re
import sys
from collections import Counter

TYPES = set("""void bool int uint float double vec2 vec3 vec4 ivec2 ivec3 ivec4 uvec2 uvec3 uvec4
bvec2 bvec3 bvec4 mat2 mat3 mat4 mat2x2 mat2x3 mat2x4 mat3x2 mat3x3 mat3x4 mat4x2 mat4x3 mat4x4
sampler2D sampler3D samplerCube sampler2DShadow image2D""".split())
QUALIFIERS = {"uniform", "in", "out", "inout", "buffer", "shared", "varying", "attribute",
              "flat", "smooth", "noperspective", "writeonly", "readonly", "coherent", "layout"}
RESERVED = set("""do if in or as for int out abs cos sin tan exp log max min mix mod pow dot all any
not fma sign step tanh atan acos asin cosh sinh sqrt exp2 log2 ceil floor fract round trunc""".split())

TOKEN = re.compile(r"""
    (?P<ws>\s+)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<num>(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuU]?)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op><<=|>>=|\+\+|--|&&|\|\||\^\^|==|!=|<=|>=|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|>>|.)
""", re.S | re.X)


def tokenize(src):
    """Yield lines-aware tokens; preprocessor lines become ('pp', [tokens])."""
    out = []
    for line in re.sub(r"/\*.*?\*/", " ", src, flags=re.S).split("\n"):
        stripped = line.strip()
        if stripped.startswith("#"):
            out.append(("pp", stripped))
            continue
        for m in TOKEN.finditer(line):
            kind = m.lastgroup
            if kind in ("ws", "lc", "bc"):
                continue
            out.append((kind, m.group()))
    return out


def short_float(t):
    m = re.fullmatch(r"(\d*)\.(\d*)", t)
    if not m:
        return t
    i, f = m.group(1).lstrip("0"), m.group(2).rstrip("0")
    return f"{i}.{f}" if (i or f) else "0."


def collect_renames(toks, pp_lines):
    keep, declared, members = {"main"}, Counter(), set()
    depth = paren = 0
    stmt_start, stmt_qualified = True, False
    decl_paren = None  # paren depth of the declaration list currently open
    prev = None
    for i, (k, t) in enumerate(toks):
        if k == "pp":
            continue
        if t == "{":
            depth += 1
        elif t == "}":
            depth -= 1
        elif t == "(":
            paren += 1
        elif t == ")":
            paren -= 1
        if prev == ".":
            members.add(t)
        global_decl = depth == 0 and paren == 0
        if global_decl and stmt_start and k == "id" and t in QUALIFIERS:
            stmt_qualified = True
        name = None
        if k == "id" and t in TYPES and i + 1 < len(toks) and toks[i + 1][0] == "id":
            name = toks[i + 1][1]
            decl_paren = paren
            stmt_start = False
        elif t == "," and decl_paren == paren and i + 2 < len(toks) and toks[i + 1][0] == "id" \
                and toks[i + 2][1] in ("=", ";", ",", "["):
            name = toks[i + 1][1]
        if name:
            if stmt_qualified and global_decl:
                keep.add(name)
            else:
                declared[name] += 1
        if t in (";", "{", "}"):
            stmt_start, stmt_qualified, decl_paren = True, False, None
        prev = t
    for line in pp_lines:
        m = re.match(r"#\s*define\s+([A-Za-z_]\w*)", line)
        if m:
            declared[m.group(1)] += 0
    usage = Counter(t for k, t in toks if k == "id")
    for line in pp_lines:
        usage.update(re.findall(r"[A-Za-z_]\w*", line))
    rename = [n for n in declared if n not in keep and n not in members and not n.startswith("gl_")]
    rename.sort(key=lambda n: -usage[n])
    taken = set(usage) | RESERVED | TYPES | QUALIFIERS

    def names():
        letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
        for a in letters:
            yield a
        for a in letters:
            for b in letters + "0123456789_":
                yield a + b
    gen = names()
    mapping = {}
    for n in rename:
        for cand in gen:
            if cand not in taken:
                mapping[n] = cand
                taken.add(cand)
                break
    return mapping


def emit(toks, mapping):
    out, prev, prev_word = [], "", False
    for k, t in toks:
        if k == "pp":
            body = re.sub(r"[A-Za-z_]\w*", lambda m: mapping.get(m.group(), m.group()), t) \
                if not t.startswith("#version") else t
            body = re.sub(r"\s+", " ", body)
            body = re.sub(r"(\d*\.\d*)(?![\w.])", lambda m: short_float(m.group(1)), body) \
                if body.startswith("#define") else body
            if out and not out[-1].endswith("\n"):
                out.append("\n")
            out.append(body + "\n")
            prev, prev_word = "\n", False
            continue
        if k == "id":
            t = mapping.get(t, t)
        elif k == "num":
            t = short_float(t)
        word = k in ("id", "num")
        if (word and prev_word) or (k == "op" and prev and prev[-1] in "+-*/%<>=!&|^" and t[0] in "+-*/%<>=!&|^"):
            out.append(" ")
        out.append(t)
        prev, prev_word = t, word
    return "".join(out).strip() + "\n"


def minify(src):
    toks = tokenize(src)
    pp = [t for k, t in toks if k == "pp"]
    return emit(toks, collect_renames([x for x in toks if x[0] != "pp"], pp))


if __name__ == "__main__":
    src_dir, dst_dir = sys.argv[1], sys.argv[2]
    os.makedirs(dst_dir, exist_ok=True)
    for name in sorted(os.listdir(src_dir)):
        if os.path.splitext(name)[1] not in (".vert", ".frag", ".comp"):
            continue
        with open(os.path.join(src_dir, name), encoding="utf-8") as f:
            text = minify(f.read())
        dst = os.path.join(dst_dir, name)
        old = open(dst, encoding="utf-8").read() if os.path.exists(dst) else None
        if old != text:
            with open(dst, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
