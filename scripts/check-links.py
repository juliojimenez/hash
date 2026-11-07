#!/usr/bin/env python3
"""Check the links of a built mdBook.

    python3 scripts/check-links.py book/output

Every href and src in the HTML that points inside the book must name a file
that exists, and a #fragment must be an id on the target page. External URLs
are not fetched. A link to a chapter's README.md is the classic miss: mdBook
renders that file as index.html but rewrites the link to README.html (#401).
Exit status 1 when anything is broken; the broken links are listed.
"""
import html.parser
import os
import sys
import urllib.parse


class Page(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.links = []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "a" and a.get("href"):
            self.links.append(a["href"])
        if tag in ("img", "script", "iframe") and a.get("src"):
            self.links.append(a["src"])
        if tag == "link" and a.get("href"):
            self.links.append(a["href"])
        for key in ("id", "name"):
            if a.get(key):
                self.ids.add(a[key])


def parse(path):
    page = Page()
    with open(path, encoding="utf-8", errors="replace") as f:
        page.feed(f.read())
    return page


def main():
    if len(sys.argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    root = os.path.abspath(sys.argv[1])
    pages = {}
    for dirpath, _, names in os.walk(root):
        for name in names:
            if name.endswith(".html"):
                path = os.path.join(dirpath, name)
                pages[path] = parse(path)
    if not pages:
        print(f"check-links: no HTML under {root}", file=sys.stderr)
        return 2

    broken = []
    checked = 0
    for path, page in pages.items():
        for href in page.links:
            if href.startswith(("http:", "https:", "mailto:", "javascript:", "data:", "//")):
                continue
            target, _, fragment = href.partition("#")
            target = urllib.parse.unquote(target.split("?", 1)[0])
            if target == "":
                dest = path
            elif target.startswith("/"):
                dest = os.path.normpath(os.path.join(root, target.lstrip("/")))
            else:
                dest = os.path.normpath(os.path.join(os.path.dirname(path), target))
            if os.path.isdir(dest):
                dest = os.path.join(dest, "index.html")
            checked += 1
            if not os.path.exists(dest):
                broken.append((path, href, "no such file"))
                continue
            if fragment and dest.endswith(".html"):
                if dest not in pages:
                    pages[dest] = parse(dest)
                if fragment not in pages[dest].ids:
                    broken.append((path, href, "no such anchor"))

    for path, href, why in sorted(broken):
        print(f"{os.path.relpath(path, root)}: {href}: {why}")
    print(f"check-links: {len(pages)} pages, {checked} links, {len(broken)} broken")
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
