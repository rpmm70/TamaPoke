#!/usr/bin/env python3
"""Consistency checks for i18n.cpp's translation tables.

Several strings in STRINGS[][] are used as snprintf() format strings
(e.g. T(S_FAREWELL_BTN) in a snprintf() call). Since they're not
literals at the call site, the compiler can't validate them with
-Wformat: a translation with a wrong, missing or extra %-specifier is
undefined behavior that only shows up in the affected language. This
script parses the four [language][id] tables straight out of the .cpp
source (no build step needed) and checks:

  - every language row has the same number of entries as every other
    row, and that count matches the number of enumerators in StrId
    (STR_COUNT) resp. MED_COUNT for the medal tables. STRINGS is
    declared with both array dimensions fixed
    ([LANG_COUNT][STR_COUNT]), so a row with too FEW strings does not
    fail to compile -- the remaining slots are silently
    zero-initialized (null pointers), which crashes or reads garbage
    at runtime the first time that StrId is used in the affected
    language. Too many *is* a compile error, so this mainly guards
    the silent case, but checks the count outright either way.
  - for every StrId, the sequence of %-format specifiers is identical
    across all languages.

LANGS must list every row of the tables: extract_table() only pulls
the first len(LANGS) blocks, so it is read from the Lang enum in i18n.h
(LANG_ES ... up to LANG_COUNT) instead of being kept by hand. MED_COUNT is
read from medals.h (or pet.h, where it used to live).

  python3 tools/test_i18n_formats.py
"""
#region Imports
import os
import re
import sys
#endregion

#region Configuration
HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(HERE)
I18N_CPP = os.path.join(REPO_ROOT, 'i18n.cpp')
I18N_H = os.path.join(REPO_ROOT, 'i18n.h')
MED_COUNT_HEADERS = (os.path.join(REPO_ROOT, 'medals.h'), os.path.join(REPO_ROOT, 'pet.h'))

STRING_RE = re.compile(r'"(?:[^"\\]|\\.)*"')
SPEC_RE = re.compile(
    r'%[-+ #0]*[\d.]*(?:l|h|ll|hh)?[diouxXeEfFgGaAcspn%]')
#endregion

#region Header parsing


def read_language_codes(header_path=I18N_H):
    """['ES', 'EN', ...] from `enum Lang ... { LANG_ES = 0, ..., LANG_COUNT }`."""
    with open(header_path, encoding='utf-8') as header_file:
        source = header_file.read()
    match = re.search(r'enum\s+Lang\b[^{]*\{(.*?)\}', source, re.S)
    if not match:
        raise ValueError(f'enum Lang not found in {header_path}')
    enumerators = re.findall(r'\bLANG_([A-Z0-9_]+)', re.sub(r'//[^\n]*', '', match.group(1)))
    if 'COUNT' not in enumerators:
        raise ValueError('LANG_COUNT not found in enum Lang')
    return enumerators[:enumerators.index('COUNT')]


def read_med_count(header_paths=MED_COUNT_HEADERS):
    """Value of `#define MED_COUNT N` in the first header that defines it."""
    for header_path in header_paths:
        if not os.path.exists(header_path):
            continue
        with open(header_path, encoding='utf-8') as header_file:
            match = re.search(r'^\s*#define\s+MED_COUNT\s+(\d+)', header_file.read(), re.M)
        if match:
            return int(match.group(1))
    raise ValueError(f'MED_COUNT not defined in any of {header_paths}')


LANGS = read_language_codes()

#endregion

#region Table checks


def extract_balanced_blocks(text, count):
    """Return the `count` top-level {...} blocks found in `text`.

    Assumes no nested braces inside a block (true for STRINGS/MED_*
    here: each block is a flat, comma-separated list of string
    literals).
    """
    blocks = []
    depth = 0
    start = None
    for i, ch in enumerate(text):
        if ch == '{':
            if depth == 0:
                start = i
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                blocks.append(text[start + 1:i])
                if len(blocks) == count:
                    return blocks
    raise ValueError(
        f'expected {count} top-level blocks, found {len(blocks)}')


def extract_table(source, var_name, count):
    """Find the `var_name[...][...] = { ... };` table and return its
    `count` per-language string lists."""
    m = re.search(
        rf'\b{re.escape(var_name)}\s*\[[^;{{]*\]\s*=\s*\{{', source)
    if not m:
        raise ValueError(f'table {var_name} not found')
    # body starts at the '{' the regex matched; extract_balanced_blocks
    # wants to see that opening brace as depth 0 -> 1, so start there.
    body_start = m.end() - 1
    depth = 0
    for i in range(body_start, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                body = source[body_start:i + 1]
                break
    else:
        raise ValueError(f'unterminated table {var_name}')
    lang_blocks = extract_balanced_blocks(body[1:-1], count)
    return [STRING_RE.findall(block) for block in lang_blocks]


def check_table(source, var_name, expected_len, label_for):
    """Validate one [LANG_COUNT][N] table. Returns error strings."""
    errors = []
    rows = extract_table(source, var_name, len(LANGS))
    for lang, row in zip(LANGS, rows):
        if len(row) != expected_len:
            errors.append(
                f'{var_name}[{lang}] has {len(row)} entries, '
                f'expected {expected_len}')
    if errors:
        return errors  # zip() below assumes equal-length rows

    for idx, texts in enumerate(zip(*rows)):
        specs = {tuple(SPEC_RE.findall(t)) for t in texts}
        if len(specs) != 1:
            per_lang = {
                lang: SPEC_RE.findall(t) for lang, t in zip(LANGS, texts)
            }
            errors.append(
                f'{var_name}[*][{label_for(idx)}] specifiers differ '
                f'across languages: {per_lang}')
    return errors


def count_enumerators(header_path, enum_name, terminator):
    """Count comma-separated enumerators before `terminator`."""
    src = open(header_path, encoding='utf-8').read()
    m = re.search(
        rf'enum\s+{enum_name}\s*(?::\s*\w+\s*)?\{{(.*?)\}};', src, re.S)
    if not m:
        raise ValueError(f'enum {enum_name} not found in {header_path}')
    # strip line comments first: they sit between enumerators, not just
    # at line ends, so a plain comma-split would swallow them into names
    body = re.sub(r'//[^\n]*', '', m.group(1))
    names = [
        n.strip().split('=')[0].strip()
        for n in body.split(',') if n.strip()
    ]
    if terminator not in names:
        raise ValueError(f'{terminator} not found in enum {enum_name}')
    return names.index(terminator)


#endregion

#region Public API


def main():
    with open(I18N_CPP, encoding='utf-8') as source_file:
        source = source_file.read()

    str_count = count_enumerators(I18N_H, 'StrId', 'STR_COUNT')
    med_count = read_med_count()

    errors = []
    errors += check_table(
        source, 'STRINGS', str_count, lambda i: f'StrId {i}')
    errors += check_table(
        source, 'MED_NAME', med_count, lambda i: f'medal {i}')
    errors += check_table(
        source, 'MED_LBL', med_count, lambda i: f'medal {i}')
    errors += check_table(
        source, 'MED_DSC', med_count, lambda i: f'medal {i}')

    if errors:
        print(f'{len(errors)} i18n consistency error(s):')
        for e in errors:
            print(f'  - {e}')
        return 1

    print(
        f'OK: STRINGS ({str_count} ids) and the 3 medal tables '
        f'({med_count} medals) are consistent across all '
        f'{len(LANGS)} languages.')
    return 0


if __name__ == '__main__':
    sys.exit(main())

#endregion
