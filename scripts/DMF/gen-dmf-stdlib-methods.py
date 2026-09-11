#!/usr/bin/env python3
# gen-dmf-stdlib-methods.py
#
# Generates an XML listing of the "standard library" conversion methods
# exposed by common/dmf_stdlib.c (see include/dmf_stdlib.h), by parsing
# the single-source-of-truth DMF_STDLIB_METHODS(X) X-Macro list in that
# header. Intended uses:
#  - documentation (method names, arguments, return types);
#  - a basis for a future XML Schema addition so DMF XML files could
#    validate references to "conversion=" methods by name;
#  - a reference list that LUA glue code (where enabled) could load or
#    be checked against, so scripts do not call into unregistered names.
#
# This script has no dependencies beyond the Python 3 standard library.
#
# Copyright (C) 2026 Jim Klimov <jimklimov+nut@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

import argparse
import re
import sys
import xml.etree.ElementTree as ET
import xml.dom.minidom as MINIDOM


MACRO_NAME = "DMF_STDLIB_METHODS"

# Matches one X(...) entry once continuation backslashes and newlines
# have been collapsed into a single line of text.
ENTRY_RE = re.compile(
    r'X\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*'
    r'"((?:[^"\\]|\\.)*)"\s*,\s*'
    r'"((?:[^"\\]|\\.)*)"\s*,\s*'
    r'"((?:[^"\\]|\\.)*)"\s*,\s*'
    r'"((?:[^"\\]|\\.)*)"\s*\)'
)


def extract_macro_block(header_text, macro_name):
    """Return the raw text of the '#define macro_name(X) \\\n ... ' block,
    with backslash-newline continuations joined into a single string."""
    lines = header_text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if line.strip().startswith("#define %s(" % macro_name):
            start = i
            break
    if start is None:
        raise ValueError("Could not find '#define %s(...)' in header" % macro_name)

    block_lines = []
    i = start
    while i < len(lines):
        line = lines[i]
        block_lines.append(line)
        if not line.rstrip().endswith("\\"):
            break
        i += 1

    # Strip line-continuation backslashes and join
    joined = " ".join(l.rstrip().rstrip("\\").strip() for l in block_lines)
    return joined


def parse_methods(header_path):
    with open(header_path, "r", encoding="utf-8") as f:
        text = f.read()

    block = extract_macro_block(text, MACRO_NAME)

    methods = []
    for m in ENTRY_RE.finditer(block):
        c_name, dmf_name, description, args, retval = m.groups()
        methods.append({
            "c_name": c_name,
            "dmf_name": dmf_name,
            "description": description,
            "args": args,
            "retval": retval,
        })
    return methods


def build_xml(methods):
    root = ET.Element("dmf-stdlib-methods")
    for method in methods:
        method_el = ET.SubElement(root, "method",
                                   {"name": method["dmf_name"],
                                    "c_name": method["c_name"]})
        ET.SubElement(method_el, "description").text = method["description"]
        ET.SubElement(method_el, "args").text = method["args"]
        ET.SubElement(method_el, "retval").text = method["retval"]
    return root


def main():
    parser = argparse.ArgumentParser(
        description="Generate an XML listing of dmf_stdlib methods "
                    "from include/dmf_stdlib.h")
    parser.add_argument(
        "--header", default="include/dmf_stdlib.h",
        help="Path to dmf_stdlib.h (default: %(default)s)")
    parser.add_argument(
        "--output", default="-",
        help="Output file path, or '-' for stdout (default: %(default)s)")
    args = parser.parse_args()

    try:
        methods = parse_methods(args.header)
    except (OSError, ValueError) as e:
        sys.stderr.write("ERROR: %s\n" % e)
        return 1

    if not methods:
        sys.stderr.write("WARNING: no methods parsed from %s\n" % args.header)

    root = build_xml(methods)
    rough = ET.tostring(root, encoding="unicode")
    pretty = MINIDOM.parseString(rough).toprettyxml(indent="  ")

    if args.output == "-":
        sys.stdout.write(pretty)
    else:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(pretty)

    return 0


if __name__ == "__main__":
    sys.exit(main())
