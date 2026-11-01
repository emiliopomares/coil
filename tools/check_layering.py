#!/usr/bin/env python3
"""Coil layering checker (SPEC-0004): rejects #include directives that break the dependency rules.

Scans modules/ and backends/ under --root against the rules in --config (tools/layering.json).
Exit codes: 0 = no violations, 1 = violations found, 2 = usage or configuration error.
Output: one line per violation, sorted: <path>:<line>: error: <message> [<rule-id>]
"""

import argparse
import json
import re
import sys
from pathlib import Path

FOUNDATION = "foundation"

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')

SOURCE_SUFFIXES = (".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp", ".c", ".cc", ".cpp", ".cxx")

TEST_FOLDERS = ("tests", "testing")

TEST_FRAMEWORK_PREFIXES = ("gtest/", "gmock/")

# C++ standard library headers (C++23). Adding a new standard header is a one-line, reviewed change.
STD_HEADERS = frozenset(
    """
    algorithm any array atomic barrier bit bitset cassert cctype cerrno cfenv cfloat charconv chrono
    cinttypes climits clocale cmath codecvt compare complex concepts condition_variable coroutine
    csetjmp csignal cstdarg cstddef cstdint cstdio cstdlib cstring ctime cuchar cwchar cwctype deque
    exception execution expected filesystem flat_map flat_set format forward_list fstream functional
    future generator initializer_list iomanip ios iosfwd iostream istream iterator latch limits list
    locale map mdspan memory memory_resource mutex new numbers numeric optional ostream print queue
    random ranges ratio regex scoped_allocator semaphore set shared_mutex source_location span
    spanstream sstream stack stacktrace stdexcept stdfloat stop_token streambuf string string_view
    syncstream system_error thread tuple type_traits typeindex typeinfo unordered_map unordered_set
    utility valarray variant vector version
    """.split()
)


class ConfigError(Exception):
    """Invalid rules file; reported with exit code 2."""


class Rules:
    """Validated contents of the rules file."""

    def __init__(self, path):
        try:
            data = json.loads(Path(path).read_text(encoding="utf-8"))
        except FileNotFoundError:
            raise ConfigError(f"rules file not found: {path}") from None
        except json.JSONDecodeError as error:
            raise ConfigError(f"rules file is not valid JSON: {path}: {error}") from None
        if not isinstance(data, dict):
            raise ConfigError("rules file must contain a JSON object")
        self.modules = self._edges(data, "modules")
        self.backends = self._edges(data, "backends")
        for owner, deps in list(self.modules.items()) + list(self.backends.items()):
            for dep in deps:
                if dep not in self.modules:
                    raise ConfigError(f"'{owner}' depends on undefined module '{dep}' [config-undefined]")
        self._check_acyclic()

    @staticmethod
    def _edges(data, key):
        edges = data.get(key, {})
        if not isinstance(edges, dict) or not all(
            isinstance(deps, list) and all(isinstance(dep, str) for dep in deps)
            for deps in edges.values()
        ):
            raise ConfigError(f"'{key}' must map names to lists of module names")
        return edges

    def _check_acyclic(self):
        done = set()
        for start in sorted(self.modules):
            stack = [start]
            if self._visit(start, stack, set(), done):
                return

    def _visit(self, name, stack, active, done):
        if name in done:
            return False
        active.add(name)
        for dep in self.modules[name]:
            if dep in active:
                cycle = stack[stack.index(dep):] + [dep]
                raise ConfigError(f"dependency cycle {' -> '.join(cycle)} [config-cycle]")
            stack.append(dep)
            self._visit(dep, stack, active, done)
            stack.pop()
        active.discard(name)
        done.add(name)
        return False


class HeaderIndex:
    """Maps `coil/...` include paths to the headers that provide them."""

    # (path pattern relative to the owner folder, kind)
    MODULE_ROOTS = (("include", "public"), ("testing/include", "testing"), ("tests/contract", "contract"))

    def __init__(self, root):
        self.entries = {}
        for owner_kind, top, roots in (
            ("module", "modules", self.MODULE_ROOTS),
            ("backend", "backends", (("include", "public"),)),
        ):
            for owner in _subfolders(root / top):
                for relative, kind in roots:
                    include_root = owner / relative
                    if not include_root.is_dir():
                        continue
                    for header in include_root.rglob("*"):
                        if not header.is_file():
                            continue
                        path = header.relative_to(include_root).as_posix()
                        if kind == "public" and owner_kind == "module" and path.endswith(".in"):
                            path = path[: -len(".in")]  # configured header template (rule 5)
                        self.entries.setdefault(path, []).append((owner_kind, owner.name, kind))

    def resolve(self, path):
        return self.entries.get(path, [])


class Checker:
    def __init__(self, root, rules):
        self.root = root
        self.rules = rules
        self.index = HeaderIndex(root)
        self.violations = []

    def run(self):
        for owner_kind, top, known in (
            ("module", "modules", self.rules.modules),
            ("backend", "backends", self.rules.backends),
        ):
            for owner in _subfolders(self.root / top):
                if owner.name not in known:
                    self._report(owner, 0, f"{owner_kind} folder '{owner.name}' has no entry in the rules file", f"unknown-{owner_kind}")
                    continue
                for source in sorted(owner.rglob("*")):
                    if source.is_file() and _is_source(source):
                        self._check_file(owner_kind, owner, source)
        return sorted(self.violations)

    def _check_file(self, owner_kind, owner, source):
        is_test = source.relative_to(owner).parts[0] in TEST_FOLDERS
        text = source.read_text(encoding="utf-8", errors="replace")
        for line, delimiter, path in _includes(text):
            if delimiter == '"':
                self._check_quote(owner, source, line, path)
            elif path.startswith("coil/"):
                self._check_coil(owner_kind, owner.name, source, line, path, is_test)
            elif path.startswith(TEST_FRAMEWORK_PREFIXES):
                if not is_test:
                    self._report(source, line, f"<{path}> may only be included by test code", "test-framework")
            elif owner_kind == "module" and path not in STD_HEADERS:
                self._report(
                    source, line,
                    f"external header <{path}> is not allowed in modules/ (only C++ standard and coil/ headers)",
                    "external-header",
                )

    def _check_quote(self, owner, source, line, path):
        target = (source.parent / path).resolve()
        if not target.is_file() or owner.resolve() not in target.parents:
            self._report(source, line, f'"{path}" must resolve to a file inside {owner.parent.name}/{owner.name}', "quote-include")

    def _check_coil(self, owner_kind, name, source, line, path, is_test):
        candidates = self.index.resolve(path)
        if not candidates:
            self._report(source, line, f"<{path}> matches no header under the include roots", "unresolved-coil-header")
            return
        if len(candidates) > 1:
            self._report(source, line, f"<{path}> is ambiguous (provided by {len(candidates)} include roots)", "unresolved-coil-header")
            return
        header_owner_kind, header_owner, kind = candidates[0]
        if kind != "public" and not is_test:
            self._report(source, line, f"test-support header <{path}> may only be included by test code", "test-support")
        if owner_kind == "module":
            allowed = {name, FOUNDATION, *self.rules.modules[name]}
            if header_owner_kind != "module" or header_owner not in allowed:
                self._report(
                    source, line,
                    f"module '{name}' may not include <{path}> ('{header_owner}' is not a direct dependency)",
                    "module-edge",
                )
        else:
            allowed = {FOUNDATION, *self.rules.backends[name]}
            own_header = header_owner_kind == "backend" and header_owner == name
            if not own_header and (header_owner_kind != "module" or header_owner not in allowed):
                self._report(
                    source, line,
                    f"backend '{name}' may not include <{path}> ('{header_owner}' is not in its allowed modules)",
                    "backend-edge",
                )

    def _report(self, path, line, message, rule):
        relative = path.relative_to(self.root).as_posix()
        self.violations.append((relative, line, f"{relative}:{line}: error: {message} [{rule}]"))


def _subfolders(folder):
    if not folder.is_dir():
        return []
    return sorted(child for child in folder.iterdir() if child.is_dir() and not child.name.startswith("."))


def _is_source(path):
    name = path.name[: -len(".in")] if path.name.endswith(".in") else path.name
    return name.endswith(SOURCE_SUFFIXES)


def _includes(text):
    """Yields (line number, delimiter, path) for each #include outside block comments."""
    in_block_comment = False
    for number, line in enumerate(text.splitlines(), start=1):
        if in_block_comment:
            end = line.find("*/")
            if end < 0:
                continue
            line = line[end + 2:]
            in_block_comment = False
        match = INCLUDE_RE.match(line)
        if match:
            yield number, match.group(1), match.group(2)
        start = line.rfind("/*")
        if start >= 0 and line.find("*/", start) < 0:
            in_block_comment = True


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", help="repository root to scan (default: this script's repository)")
    parser.add_argument("--config", help="rules file (default: <root>/tools/layering.json)")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parent.parent
    if not root.is_dir():
        print(f"error: root is not a directory: {root}", file=sys.stderr)
        return 2
    config = Path(args.config) if args.config else root / "tools" / "layering.json"
    try:
        rules = Rules(config)
    except ConfigError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    violations = Checker(root, rules).run()
    for _, _, text in violations:
        print(text)
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
