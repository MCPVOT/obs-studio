#!/usr/bin/env -S uv run --script
"""Match an immutable prepared runtime to a freshly built package closure.

False selects the stock installation path; malformed metadata is an error.
Only an unqualified package and its native amd64 spelling are equivalent.
"""
from __future__ import annotations

import re

PACKAGE = re.compile(r'[a-z0-9][a-z0-9+.-]*(?::[a-z0-9][a-z0-9-]*)?\Z')
VERSION = re.compile(r'[0-9][a-zA-Z0-9.+:~_-]*\Z')


def records(text: str, separator: str) -> dict[str, str]:
    result = {}
    for line in text.splitlines():
        name, found, version = line.partition(separator)
        if not found or not PACKAGE.fullmatch(name) or not VERSION.fullmatch(version):
            raise ValueError('Invalid exact package record')
        canonical = name if ':' in name else name + ':amd64'
        if canonical in result:
            raise ValueError('Duplicate or ambiguous package record')
        result[canonical] = version
    if not result:
        raise ValueError('Empty package records')
    return result


def satisfies(requirements: str, inventory: str) -> bool:
    required = records(requirements, '=')
    installed = records(inventory, '\t')
    return all(installed.get(name) == version for name, version in required.items())
