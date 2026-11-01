#!/usr/bin/env python3
"""Coil layering checker (SPEC-0004): rejects #include directives that break the dependency rules."""

import argparse
import sys


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", help="repository root to scan")
    parser.add_argument("--config", help="rules file (default: <root>/tools/layering.json)")
    parser.parse_args(argv)
    return 0  # SPEC-0004 Red stub: no checks yet.


if __name__ == "__main__":
    sys.exit(main())
