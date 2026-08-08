#!/usr/bin/env python3
"""Repository policy checker (AGENTS.md section 34).

Enforces the source rules that clang-tidy cannot see: preprocessing
directives, header units, forbidden file extensions, lint suppressions,
forbidden library tokens, and coroutine tokens across all dialect code —
including GCC-only reflection files that the Clang lint graph excludes.

This checker is deliberately blunt (regex over text). It does not decide
subtle semantic questions; those remain concept/review rules per AGENTS.md.

Usage: python3 tools/check_policy.py [repo-root]
Exit status: 0 clean, 1 violations found.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

DIALECT_DIRS = ("src", "meta", "tests")

ALLOWED_EXTENSIONS = {".cpp", ".cppm"}

FORBIDDEN_EXTENSIONS = {
    ".h", ".hh", ".hpp", ".hxx", ".inc", ".inl", ".ipp", ".tpp",
}

PREPROCESSOR_DIRECTIVE = re.compile(
    r"(?m)^\s*#\s*(include|define|undef|if|ifdef|ifndef|elif|else|endif"
    r"|pragma|error|warning|line|embed)\b"
)

HEADER_UNIT = re.compile(r'(?m)^\s*(export\s+)?import\s*[<"]')

LINT_SUPPRESSION = re.compile(r"\bNOLINT(?:NEXTLINE|BEGIN|END)?\b")

FORBIDDEN_TOKENS = (
    "std::shared_ptr",
    "std::weak_ptr",
    "std::enable_shared_from_this",
    "std::make_shared",
    "std::function<",
    "std::any",
    "std::type_info",
    "std::type_index",
    "std::thread",
    "std::jthread",
    "std::async",
    "std::future",
    "std::promise",
    "std::packaged_task",
    "std::mutex",
    "std::recursive_mutex",
    "std::shared_mutex",
    "std::condition_variable",
    "std::atomic",
    "std::enable_if",
    "std::enable_if_t",
    "std::void_t",
)

COROUTINE_TOKEN = re.compile(r"\b(co_await|co_yield|co_return)\b")


def check_file(path: Path, root: Path) -> list[str]:
    rel = path.relative_to(root)
    problems: list[str] = []

    if path.suffix in FORBIDDEN_EXTENSIONS:
        problems.append(f"{rel}: forbidden header-like file extension '{path.suffix}'")
        return problems
    if path.suffix not in ALLOWED_EXTENSIONS:
        return problems

    text = path.read_text(encoding="utf-8")

    def report(pattern: re.Pattern[str], message: str) -> None:
        for match in pattern.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            problems.append(f"{rel}:{line}: {message}: {match.group(0).strip()}")

    report(PREPROCESSOR_DIRECTIVE, "preprocessing directive is forbidden in dialect code")
    report(HEADER_UNIT, "header units are forbidden; import named modules")
    report(LINT_SUPPRESSION, "lint suppression is forbidden in dialect code")
    report(COROUTINE_TOKEN, "direct coroutine syntax is forbidden; use std::execution senders")

    for token in FORBIDDEN_TOKENS:
        for match in re.finditer(re.escape(token) + r"\b" if token[-1].isalnum() else re.escape(token), text):
            line = text.count("\n", 0, match.start()) + 1
            problems.append(f"{rel}:{line}: forbidden token in dialect code: {token}")

    return problems


def main() -> int:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent

    problems: list[str] = []
    scanned = 0
    for dir_name in DIALECT_DIRS:
        zone = root / dir_name
        if not zone.is_dir():
            continue
        for path in sorted(zone.rglob("*")):
            if path.is_file():
                scanned += 1
                problems.extend(check_file(path, root))

    for problem in problems:
        print(problem)
    if problems:
        print(f"\npolicy check FAILED: {len(problems)} violation(s) in {scanned} file(s)")
        return 1
    print(f"policy check passed: {scanned} file(s) scanned")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
