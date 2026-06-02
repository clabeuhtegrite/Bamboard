#!/usr/bin/env python3
"""Validate the website i18n.

Checks that the JSON dictionary block in web/i18n.js parses, that every language
shares exactly the same key set, and that every data-i18n* key referenced in
web/index.html exists in the dictionary. Exits non-zero on any problem so CI can
gate it. Pairs with the `node --check` step that covers JS syntax.
"""
import io
import json
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent
js = io.open(root / "web" / "i18n.js", encoding="utf-8").read()
html = io.open(root / "web" / "index.html", encoding="utf-8").read()

m = re.search(r"/\*i18n:json\*/(.*?)/\*i18n:json\*/", js, re.S)
if not m:
    sys.exit("i18n:json markers not found in web/i18n.js")
try:
    d = json.loads(m.group(1))
except json.JSONDecodeError as e:
    sys.exit("i18n dictionary is not valid JSON: %s" % e)

langs = sorted(d)
# The dictionary holds the four non-English tables; English is the live DOM
# baseline (no table). This set must mirror firmware/src/ui/i18n.cpp's CODES so
# a deleted/added language can't slip through unnoticed.
EXPECTED = {"es", "fr", "pt", "de"}
if set(d) != EXPECTED:
    sys.exit("i18n language tables %s != expected %s (mirror firmware/src/ui/i18n.cpp)"
             % (sorted(d), sorted(EXPECTED)))
mlangs = re.search(r"var LANGS = \[([^\]]*)\]", js)
codes = re.findall(r'"([a-z]{2})"', mlangs.group(1)) if mlangs else []
if codes != ["en", "es", "fr", "pt", "de"]:
    sys.exit("web/i18n.js LANGS %s != ['en','es','fr','pt','de'] (firmware source of truth)" % codes)

bad = False
base = set(d["fr"])
for lang in langs:
    diff = set(d[lang]) ^ base
    if diff:
        print("::error::i18n keys for '%s' differ from 'fr': %s" % (lang, sorted(diff)))
        bad = True

used = set(re.findall(r'data-i18n="([^"]+)"', html))
used |= set(re.findall(r'data-i18n-html="([^"]+)"', html))
for spec in re.findall(r'data-i18n-attr="([^"]+)"', html):
    for pair in spec.split(";"):
        if ":" in pair:
            used.add(pair.split(":", 1)[1].strip())

missing = sorted(used - base)
if missing:
    print("::error::index.html references i18n keys missing from web/i18n.js: %s" % missing)
    bad = True

if bad:
    sys.exit(1)
print("i18n OK — %d languages, %d keys each, %d keys referenced in index.html"
      % (len(langs), len(base), len(used)))
