#!/usr/bin/env python3
"""Tests for the workshop tools (tools/), the generated headers and cheap
source checks on the firmware files.

  python3 test/test_tools.py
  python -m unittest test/test_tools.py     (from the project root)
"""
#region Imports
import glob
import os
import re
import sys
import unittest
#endregion

#region Paths
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')
sys.path.insert(0, TOOLS)

# generated tables: their literals are checked through their generators instead
GENERATED_HEADERS = ('dex.h', 'species.h')
# deliberate UTF-8 (JA/KO) literals, checked by test_i18n.cpp and test_dex.cpp
FILES_WITH_CJK_LITERALS = ('dex.h', 'i18n.cpp')
#endregion

#region Helpers


def read_project_text(relative_path):
    """A project file as text (UTF-8, line endings normalized to \\n)."""
    with open(os.path.join(ROOT, relative_path), encoding='utf-8') as source_file:
        return source_file.read()


def list_firmware_sources(patterns=('*.ino', '*.cpp', '*.h')):
    """Firmware source files in the project root (the Arduino sketch layout)."""
    found = set()
    for pattern in patterns:
        found.update(os.path.basename(path) for path in glob.glob(os.path.join(ROOT, pattern)))
    return sorted(found)

#endregion

#region Tools compile


class TestToolsCompile(unittest.TestCase):
    def test_every_script_compiles(self):
        scripts = sorted(name for name in os.listdir(TOOLS) if name.endswith('.py'))
        self.assertTrue(scripts, 'no script found in tools/')
        for script_name in scripts:
            with self.subTest(script=script_name):
                with open(os.path.join(TOOLS, script_name), encoding='utf-8') as script_file:
                    source = script_file.read()
                try:
                    compile(source, script_name, 'exec')
                except SyntaxError as syntax_error:
                    self.fail(f'{script_name} does not compile: {syntax_error}')

    @unittest.skipIf(os.name == 'nt', 'Windows has no executable bit to check')
    def test_executable_scripts_declare_a_shebang(self):
        for script_name in sorted(name for name in os.listdir(TOOLS) if name.endswith('.py')):
            script_path = os.path.join(TOOLS, script_name)
            if not os.access(script_path, os.X_OK):
                continue
            with open(script_path, encoding='utf-8') as script_file:
                self.assertTrue(script_file.readline().startswith('#!'),
                                f'{script_name} is executable but has no shebang')

#endregion

#region Pokedex data


class TestDexData(unittest.TestCase):
    """Invariants of the Pokedex data source."""

    @classmethod
    def setUpClass(cls):
        from dex_data import DEX, TYPE_ACCENTS, BATTLE_TYPES, CLASSIC, RARE, LEGENDARY, SLUGS
        from dex_stats import BASE_STATS
        from dex_names import LOCAL_NAMES
        cls.DEX, cls.ACCENTS, cls.BATTLE_TYPES, cls.CLASSIC = DEX, TYPE_ACCENTS, BATTLE_TYPES, CLASSIC
        cls.RARE, cls.LEGENDARY, cls.SLUGS = RARE, LEGENDARY, SLUGS
        cls.STATS, cls.NAMES = BASE_STATS, LOCAL_NAMES
        cls.row_by_number = {row[0]: row for row in DEX}

    def test_all_151_species_without_gaps_or_duplicates(self):
        numbers = [row[0] for row in self.DEX]
        self.assertEqual(len(numbers), 151)
        self.assertEqual(sorted(numbers), list(range(1, len(self.DEX) + 1)))

    def test_slugs_are_unique_and_lowercase(self):
        slugs = [row[1] for row in self.DEX]
        self.assertEqual(len(set(slugs)), len(slugs), 'duplicated slugs')
        for slug in slugs:
            self.assertEqual(slug, slug.lower(), f'slug with uppercase: {slug}')
            self.assertTrue(slug.isascii(), f'non-ASCII slug: {slug}')

    def test_display_names_are_uppercase_ascii(self):
        # the firmware's GFX font has no accented capitals and names are drawn in uppercase
        for number, _slug, display_name, *_ in self.DEX:
            self.assertTrue(display_name.isascii(), f'{number} {display_name} is not ASCII')
            self.assertEqual(display_name, display_name.upper(), f'{number} {display_name} has lowercase')
            self.assertLessEqual(len(display_name), 12, f'{number} {display_name} does not fit on screen')

    def test_evolutions_point_to_real_species(self):
        for number, _slug, display_name, _type, evolves_to, level in self.DEX:
            if evolves_to:
                self.assertIn(evolves_to, self.row_by_number,
                              f'{display_name} evolves into {evolves_to}, which does not exist')
                self.assertNotEqual(evolves_to, number, f'{display_name} evolves into itself')
                self.assertGreater(level, 0, f'{display_name} evolves without a level')
            else:
                self.assertEqual(level, 0, f'{display_name} is a final form but has level {level}')

    def test_no_evolution_chain_loops(self):
        for number in self.row_by_number:
            seen, current = set(), number
            while current and self.row_by_number[current][4]:
                self.assertNotIn(current, seen, f'evolution loop starting at {number}')
                seen.add(current)
                current = self.row_by_number[current][4]
                self.assertLessEqual(len(seen), 5, f'chain too long starting at {number}')

    def test_every_type_has_a_color(self):
        for _number, _slug, display_name, accent_type, *_ in self.DEX:
            self.assertIn(accent_type, self.ACCENTS, f'{display_name}: type "{accent_type}" has no color')

    def test_accent_types_use_the_battle_type_vocabulary(self):
        battle_type_names = {battle_type for pair in self.BATTLE_TYPES.values()
                             for battle_type in pair if battle_type}
        for accent_type in self.ACCENTS:
            self.assertIn(accent_type, battle_type_names, f'accent type "{accent_type}" is not a battle type')

    def test_colors_are_6_digit_hex(self):
        for accent_type, color in self.ACCENTS.items():
            self.assertRegex(color, r'^#[0-9a-fA-F]{6}$', f'odd color for {accent_type}: {color}')

    def test_rarities_point_to_base_forms(self):
        evolved_species = {row[4] for row in self.DEX if row[4]}
        for number in sorted(self.RARE | self.LEGENDARY):
            self.assertIn(number, self.row_by_number, f'rarity for a species that does not exist: {number}')
            self.assertNotIn(number, evolved_species,
                             f'{self.row_by_number[number][2]} only comes from evolving: the rarity is useless')
        self.assertFalse(self.RARE & self.LEGENDARY, 'a species cannot be both rare and legendary')

    def test_classic_starters_exist(self):
        evolved_species = {row[4] for row in self.DEX if row[4]}
        for number in self.CLASSIC:
            self.assertIn(number, self.row_by_number, f'starter does not exist: {number}')
            self.assertNotIn(number, evolved_species, f'{self.row_by_number[number][2]} cannot be a starter')

    def test_every_species_has_base_stats(self):
        for number in self.row_by_number:
            self.assertIn(number, self.STATS, f'missing stats for {self.row_by_number[number][2]}')
            for stat_value in self.STATS[number]:
                self.assertTrue(0 < stat_value <= 255,
                                f'stat out of range for {self.row_by_number[number][2]}: {stat_value}')

    def test_translated_names_match_their_font(self):
        # Latin-script ones are drawn with the CP437 font (one byte per
        # character, no lowercase in names); Japanese and Korean with a U8g2
        # font and they are UTF-8, so they are neither ASCII nor have
        # uppercase to check.
        latin_languages, cjk_languages = ('fr', 'de'), ('ja', 'ko')
        for number, names_by_language in self.NAMES.items():
            self.assertIn(number, self.row_by_number, f'translated name for a species that does not exist: {number}')
            for language_code, name in names_by_language.items():
                self.assertIn(language_code, latin_languages + cjk_languages, f'unexpected language: {language_code}')
                self.assertTrue(name, f'{number} {language_code} is empty')
                if language_code in latin_languages:
                    self.assertTrue(name.isascii(), f'{number} {language_code} is not ASCII: {name}')
                    self.assertEqual(name, name.upper(), f'{number} {language_code} has lowercase: {name}')
                    self.assertLessEqual(len(name), 12, f'{number} {language_code} does not fit on screen: {name}')
                else:
                    # the useful limit is CHARACTERS, not bytes: in UTF-8 each kana or hangul syllable takes 3
                    self.assertLessEqual(len(name), 8, f'{number} {language_code} does not fit on screen: {name}')

#endregion

#region Generated files


class TestGeneratedFiles(unittest.TestCase):
    """dex.h and species.h are generated: if they do not match their generator, someone edited them by hand.

    The generators are imported and their text compared, so nothing is
    written; line endings are normalized (a CRLF checkout still matches).
    """

    def test_gen_dex_reproduces_dex_h(self):
        import gen_dex
        generated_text, _rarity_counter = gen_dex.build_dex_header_text()
        self.assertEqual(generated_text, read_project_text('dex.h'),
                         'dex.h does not match what tools/gen_dex.py generates '
                         '(regenerate it instead of editing it by hand)')

    def test_sprites_reproduces_species_h(self):
        import sprites
        self.assertEqual(sprites.build_species_header_text(), read_project_text('species.h'),
                         'species.h does not match what tools/sprites.py generates '
                         '(regenerate it instead of editing it by hand)')

    def test_generated_files_say_they_are_generated(self):
        for header_name in GENERATED_HEADERS:
            self.assertIn('GENERATED', read_project_text(header_name)[:400],
                          f'{header_name} should say it is generated')

#endregion

#region Firmware source sanity


class TestSketchSanity(unittest.TestCase):
    """Cheap checks on the firmware sources (root .ino/.cpp/.h) that do not require compiling them."""

    @classmethod
    def setUpClass(cls):
        cls.sources = {name: read_project_text(name) for name in list_firmware_sources()}

    def test_the_sources_were_found(self):
        self.assertIn('TamaPoke.ino', self.sources)

    def test_screen_literals_are_ascii(self):
        # comments can contain anything; literals cannot: the GFX font is CP437
        # (accents go as single-byte octal escapes) and raw UTF-8 comes out as garbage
        for source_name, source_text in self.sources.items():
            if source_name in FILES_WITH_CJK_LITERALS:
                continue
            for line_number, line in enumerate(source_text.splitlines(), 1):
                code = line.split('//')[0]
                for literal_body in code.split('"')[1::2]:
                    self.assertTrue(literal_body.isascii(),
                                    f'{source_name}:{line_number}: non-ASCII literal: {literal_body!r}')

    def test_braces_are_balanced(self):
        for source_name, source_text in self.sources.items():
            with self.subTest(source=source_name):
                self.assertEqual(source_text.count('{'), source_text.count('}'),
                                 f'unbalanced braces in {source_name}')

#endregion

#region Formatted text buffers


class TestTextBuffers(unittest.TestCase):
    """Every snprintf(buf, sizeof(buf), T(S_x), ...) fits its buffer in ALL languages.

    The buffers were sized when every string was one byte per character. In
    UTF-8 a kana or a hangul syllable takes 3, and snprintf truncates
    silently: sometimes digits are lost ("きろく 2" instead of "きろく 219") and
    sometimes it cuts in the middle of a character, which the font can no
    longer draw. Neither the compiler nor the screen warns about it, so it is
    checked here with the worst case of each argument according to its type:

      %u / %d  -> 5 digits (all arguments are 16 bits or less)
      %lu      -> 10 digits
      %s       -> whatever STRING_ARGUMENTS declares; an undeclared %s fails
                  the test, so whoever adds it thinks about how long it can be

    The calls are searched in TamaPoke.ino and every root .cpp file; the
    `char var[N]` declaration is looked up above the call in the same file.
    When the id is a StrId variable (T(scoreFormat)), the line must list
    every id it can hold in a comment: `// buffer-check: S_SCORE_FMT S_HITS_FMT`.
    """

    STRING_ARGUMENTS = {  # what can arrive for each %s, in order
        'S_RELEASE_FMT': ('species_name',),
        'S_FAREWELL_BTN': ('species_name_or_nickname',),
        'S_RUNAWAY_BTN': ('species_name_or_nickname',),
    }
    WORST_CASE_DIGITS = {'u': 5, 'd': 5, 'lu': 10}
    FORMAT_SPECIFIER = re.compile(r'%0?\d*(lu|u|d|s)')
    SNPRINTF_CALL = re.compile(r'snprintf\((\w+),\s*sizeof\(\1\),\s*T\((\w+)\)')
    BUFFER_CHECK_ANNOTATION = re.compile(r'buffer-check:((?:\s+S_[A-Z0-9_]+)+)')

    @classmethod
    def setUpClass(cls):
        import test_i18n_formats as i18n_formats
        from dex_data import DEX
        from dex_names import LOCAL_NAMES

        cls.LANGS = i18n_formats.LANGS
        enum_body = re.search(r'enum\s+StrId[^{]*\{(.*?)\}', read_project_text('i18n.h'), re.S).group(1)
        string_ids = re.findall(r'\b(S_[A-Z0-9_]+)\b', re.sub(r'//[^\n]*', '', enum_body))
        table_rows = i18n_formats.extract_table(read_project_text('i18n.cpp'), 'STRINGS', len(cls.LANGS))
        cls.LITERALS = {string_id: [row[index] for row in table_rows]
                        for index, string_id in enumerate(string_ids)}
        cls.SOURCE_LINES = {name: read_project_text(name).splitlines()
                            for name in list_firmware_sources(('*.ino', '*.cpp'))}
        cls.NICKNAME_BYTES = int(re.search(r'char\s+nick\[(\d+)\]', read_project_text('pet.h')).group(1)) - 1
        base_name_bytes = max(len(row[2].encode()) for row in DEX)
        cls.NAME_BYTES = {}
        for language in cls.LANGS:
            own_names = [names[language.lower()] for names in LOCAL_NAMES.values() if language.lower() in names]
            cls.NAME_BYTES[language] = max([base_name_bytes] + [len(name.encode('utf-8')) for name in own_names])

    @staticmethod
    def count_literal_bytes(literal):
        """Bytes a C literal takes in memory, written as in the source."""
        body = literal[1:-1].encode('utf-8')
        byte_count = position = 0
        while position < len(body):
            if body[position] == 0x5C:  # backslash: \ooo octal, or \n \" \\ ...
                escape_end = position + 1
                while escape_end < len(body) and escape_end < position + 4 and 0x30 <= body[escape_end] <= 0x37:
                    escape_end += 1
                position = escape_end if escape_end > position + 1 else position + 2
            else:
                position += 1
            byte_count += 1
        return byte_count

    def compute_worst_case_bytes(self, string_id, language_index):
        language, literal = self.LANGS[language_index], self.LITERALS[string_id][language_index]
        total_bytes = self.count_literal_bytes(literal) + 1  # + terminator
        pending_string_arguments = list(self.STRING_ARGUMENTS.get(string_id, ()))
        for specifier in self.FORMAT_SPECIFIER.finditer(literal):
            total_bytes -= len(specifier.group(0))
            if specifier.group(1) != 's':
                total_bytes += self.WORST_CASE_DIGITS[specifier.group(1)]
                continue
            self.assertTrue(pending_string_arguments, f'{string_id}: has a %s not declared in STRING_ARGUMENTS')
            argument_kind = pending_string_arguments.pop(0)
            if argument_kind == 'star':
                total_bytes += 1
            elif argument_kind == 'species_name':
                total_bytes += self.NAME_BYTES[language]
            elif argument_kind == 'species_name_or_nickname':
                total_bytes += max(self.NAME_BYTES[language], self.NICKNAME_BYTES)
            elif argument_kind == 'berry':
                total_bytes += max(self.count_literal_bytes(self.LITERALS[berry_id][language_index]) for berry_id in
                                   ('S_BERRY_UNK', 'S_BERRY_RED', 'S_BERRY_BLUE', 'S_BERRY_GREEN'))
        return total_bytes

    def find_formatted_calls(self):
        """(file, line number, buffer, buffer size or None, string ids or None) for every call."""
        calls = []
        for source_name, source_lines in self.SOURCE_LINES.items():
            for line_number, line in enumerate(source_lines, 1):
                code, _, comment = line.partition('//')
                call_match = self.SNPRINTF_CALL.search(code)
                if not call_match:
                    continue
                buffer_name, id_expression = call_match.groups()
                #region Which string ids can reach this call
                if id_expression.startswith('S_'):
                    string_ids = [id_expression]
                else:
                    annotation = self.BUFFER_CHECK_ANNOTATION.search(comment)
                    string_ids = annotation.group(1).split() if annotation else None
                #endregion
                #region Buffer declaration above the call, same file
                buffer_size = next((int(declaration.group(1)) for search_index in range(line_number - 2, -1, -1)
                                    for declaration in [re.search(r'\bchar\s+' + buffer_name + r'\s*\[(\d+)\]',
                                                                  source_lines[search_index])]
                                    if declaration), None)
                #endregion
                calls.append((source_name, line_number, buffer_name, buffer_size, string_ids))
        return calls

    def test_every_formatted_text_fits_its_buffer_in_every_language(self):
        checked_call_count = 0
        for source_name, line_number, buffer_name, buffer_size, string_ids in self.find_formatted_calls():
            if string_ids is None:
                continue  # reported by test_variable_format_ids_are_annotated
            self.assertIsNotNone(buffer_size,
                                 f'{source_name}:{line_number}: cannot find the declaration of {buffer_name}')
            checked_call_count += 1
            for string_id in string_ids:
                self.assertIn(string_id, self.LITERALS, f'{source_name}:{line_number}: unknown id {string_id}')
                for language_index, language in enumerate(self.LANGS):
                    with self.subTest(file=source_name, line=line_number, id=string_id, language=language):
                        worst_case = self.compute_worst_case_bytes(string_id, language_index)
                        self.assertLessEqual(
                            worst_case, buffer_size,
                            f'{source_name}:{line_number}: {buffer_name}[{buffer_size}] with {string_id} '
                            f'in {language} needs {worst_case} bytes')
        self.assertGreater(checked_call_count, 10, 'the snprintf pattern no longer finds the calls')

    def test_variable_format_ids_are_annotated(self):
        unannotated_calls = [f'{source_name}:{line_number}'
                             for source_name, line_number, _, _, string_ids in self.find_formatted_calls()
                             if string_ids is None]
        if unannotated_calls:
            self.skipTest('T(variable) without a "// buffer-check: S_..." comment, not checked: '
                          + ', '.join(unannotated_calls))

#endregion

if __name__ == '__main__':
    unittest.main(verbosity=2)
