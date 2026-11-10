#!/usr/bin/env python3
"""Keep the builtin documentation in step with the shell.

The builtin table in src/builtins.c (name, synopsis, description) is what
`help` prints, and it is the source of truth for two generated documents:

  debian/hash-shell.1          the SHELL BUILTIN COMMANDS section
  book/src/reference/builtins.md   the book's Built-in Commands page

    python3 scripts/docs-parity.py --write    regenerate both (make docs)
    python3 scripts/docs-parity.py --check    fail when either differs from
                                              the table, or when the quick
                                              reference (book/src/reference/
                                              index.md) lacks a builtin
                                              (make check-docs; CI runs it)
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "builtins.c"
MAN = ROOT / "debian" / "hash-shell.1"
BOOK_PAGE = ROOT / "book" / "src" / "reference" / "builtins.md"
QUICK_REF = ROOT / "book" / "src" / "reference" / "index.md"
SITE = "https://hash-shell.org/"

MAN_BEGIN = '.\\" docs-parity: generated from the builtin table in src/builtins.c'
MAN_END = '.\\" docs-parity: end of the generated entries'
BOOK_NOTE = "<!-- docs-parity: generated from the builtin table in src/builtins.c -->"

STR = r'"(?:[^"\\]|\\.)*"'
STRS = rf"(?:{STR}\s*)+"
ENTRY = re.compile(
    rf"\[BUILTIN_FUNC_\w+\]\s*=\s*\(Builtin\)\{{\s*({STR})\s*,\s*&\w+\s*,\s*({STRS}),\s*({STRS})\}}",
    re.S,
)
ESCAPES = {"n": "\n", "t": "\t", '"': '"', "\\": "\\", "'": "'"}


def c_string(literals):
    """The text of one or more adjacent C string literals."""
    out = []
    for m in re.finditer(STR, literals):
        body = m.group(0)[1:-1]
        out.append(re.sub(r"\\(.)", lambda e: ESCAPES.get(e.group(1), "\\" + e.group(1)), body))
    return "".join(out)


def read_table():
    """[(name, [synopsis lines], [description lines])], sorted by name."""
    src = SRC.read_text()
    items = []
    for m in ENTRY.finditer(src):
        name = c_string(m.group(1))
        synopses = [s for line in c_string(m.group(2)).split("\n") for s in re.split(r" {3,}", line) if s]
        description = c_string(m.group(3)).rstrip("\n").split("\n")
        items.append((name, synopses, description))
    if not items:
        sys.exit("docs-parity: no builtin table found in src/builtins.c")
    return sorted(items, key=lambda item: item[0])


def read_pages():
    """{name: book page} for the topics with a page of their own, and the default."""
    src = SRC.read_text()
    block = re.search(r"help_pages\[\]\s*=\s*\{(.*?)\};", src, re.S)
    body = re.search(r"help_page_for\([^)]*\)\s*\{(.*?)\n\}", src, re.S)
    if not block or not body:
        sys.exit("docs-parity: help_pages or help_page_for not found in src/builtins.c")
    pages = {c_string(n): c_string(p) for n, p in re.findall(rf"\{{\s*({STR})\s*,\s*({STR})\s*\}}", block.group(1))}
    returns = re.findall(rf"return\s+({STR})\s*;", body.group(1))
    if not returns:
        sys.exit("docs-parity: help_page_for has no default page")
    return pages, c_string(returns[-1])


# --- the man page section ----------------------------------------------------

def groff(text):
    text = text.replace("\\", "\\e").replace("-", "\\-")
    if text[:1] in (".", "'"):
        text = "\\&" + text
    return text


def man_section(items, pages, default):
    out = [
        MAN_BEGIN,
        '.\\" by scripts/docs-parity.py; `help NAME` prints the same text. Edit the',
        '.\\" table, then run: make docs',
        "Every builtin takes \\-\\- as the end of its options. In the shell,",
        ".B help",
        "lists these entries and",
        ".B help NAME",
        "prints one; the book keeps the same text at",
        groff(SITE + "reference/builtins.html") + " .",
    ]
    for name, synopses, description in items:
        for i, synopsis in enumerate(synopses):
            rest = synopsis[len(name):] if synopsis.startswith(name) else " " + synopsis
            if i == 1:
                out.append(".PD 0")
            out.append(".TP")
            out.append("\\fB" + groff(name) + "\\fR" + groff(rest))
        if len(synopses) > 1:
            out.append(".PD")
        out.extend(groff(line) for line in description)
        out.append(".br")
        out.append("More: " + groff(SITE + pages.get(name, default)))
    out.append(MAN_END)
    return "\n".join(out) + "\n"


def man_replace(text, section):
    start = text.find(MAN_BEGIN)
    end = text.find(MAN_END)
    if start < 0 or end < 0:
        # first run: the section is the hand-written one between .SH SHELL BUILTIN COMMANDS and the next .SH
        head = text.find(".SH SHELL BUILTIN COMMANDS\n")
        if head < 0:
            sys.exit("docs-parity: no SHELL BUILTIN COMMANDS section in the man page")
        start = head + len(".SH SHELL BUILTIN COMMANDS\n")
        nxt = text.find("\n.SH ", start)
        return text[:start] + section + text[nxt + 1:]
    end += len(MAN_END) + 1
    return text[:start] + section + text[end:]


# --- the book page -----------------------------------------------------------

def md_id(name):
    return {".": "dot", ":": "colon", "[": "bracket"}.get(name, name)


def book_page(items, pages, default):
    out = [
        "# Built-in Commands",
        "",
        BOOK_NOTE,
        "<!-- by scripts/docs-parity.py; `help NAME` prints the same text. Edit the table, then run: make docs -->",
        "",
        f"The {len(items)} builtins, as `help` lists them in the shell; `help NAME` prints one entry.",
        "Every builtin takes `--` as the end of its options. The [quick reference](index.md#built-in-commands)",
        "has the one-line form.",
        "",
    ]
    for name, synopses, description in items:
        out.append(f"## {name} {{#{md_id(name)}}}")
        out.append("")
        out.append("```")
        out.extend(synopses)
        out.append("```")
        out.append("")
        out.append(" ".join(line.strip() for line in description if line.strip()))
        page = pages.get(name)
        if page:
            path, _, fragment = page.partition("#")
            link = "../" + path[: -len(".html")] + ".md" + ("#" + fragment if fragment else "")
            out.append("")
            out.append(f"More: [{path.rsplit('/', 1)[-1][:-len('.html')].replace('-', ' ')}]({link})")
        out.append("")
    return "\n".join(out)


# --- the quick reference -----------------------------------------------------

def quick_reference_gaps(items):
    text = QUICK_REF.read_text()
    start = text.find("## Built-in Commands")
    if start < 0:
        return ["book/src/reference/index.md has no Built-in Commands section"]
    end = text.find("\n## ", start + 1)
    table = text[start:end if end > 0 else len(text)]
    return [f"book/src/reference/index.md: no row for `{name}`" for name, _, _ in items if f"`{name}`" not in table]


# --- main --------------------------------------------------------------------

def main(argv):
    if argv[1:] not in (["--write"], ["--check"]):
        sys.exit(__doc__)
    items = read_table()
    pages, default = read_pages()
    man_new = man_replace(MAN.read_text(), man_section(items, pages, default))
    book_new = book_page(items, pages, default)
    if argv[1] == "--write":
        MAN.write_text(man_new)
        BOOK_PAGE.write_text(book_new)
        print(f"docs-parity: {len(items)} builtins written to {MAN.relative_to(ROOT)} and {BOOK_PAGE.relative_to(ROOT)}")
        gaps = quick_reference_gaps(items)
        for gap in gaps:
            print("docs-parity: " + gap)
        return 1 if gaps else 0
    problems = []
    if MAN.read_text() != man_new:
        problems.append(f"{MAN.relative_to(ROOT)}: the SHELL BUILTIN COMMANDS section differs from the table")
    if not BOOK_PAGE.exists() or BOOK_PAGE.read_text() != book_new:
        problems.append(f"{BOOK_PAGE.relative_to(ROOT)} differs from the table")
    problems.extend(quick_reference_gaps(items))
    if problems:
        for p in problems:
            print("docs-parity: " + p, file=sys.stderr)
        print("docs-parity: run `make docs`, then fill in the quick reference by hand", file=sys.stderr)
        return 1
    print(f"docs-parity: {len(items)} builtins, man page and book in step with src/builtins.c")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
