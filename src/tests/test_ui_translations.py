"""Check locale parity and the fixed English lookup keys used by the native UI."""
import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import build_translations

# Tokenize quoted C strings and parentheses; includes conditional lookup keys.
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|[()]')
STRINGS = re.compile(r'"(?:\\.|[^"\\])*"')

def literal_keys(text):
    for match in re.finditer(r'\bui_tr\s*\(', text):
        depth = 1
        for token in TOKENS.finditer(text, match.end()):
            value = token.group()
            if value == '(':
                depth += 1
            elif value == ')':
                depth -= 1
                if not depth:
                    break
            else:
                yield json.loads(value)


class TranslationCoverage(unittest.TestCase):
    def test_native_lookup_keys(self):
        base = build_translations.load()
        self.assertEqual(len(base['languages']), 18)
        keys = set(base['strings'])
        src = ROOT/'clio843/src'
        for path in sorted(src.iterdir()):
            if path.suffix not in ('.c', '.h') or path.name == 'ui_translations.h':
                continue
            with self.subTest(path=path.name):
                self.assertEqual(set(literal_keys(path.read_text())) - keys, set())
        # Indirect lookup tables and menu titles do not contain literal ui_tr calls.
        indirect = {
            'clio040.c': [r'setting_names\[\]\s*=\s*\{([^}]+)\}'],
            'menu_sheet.h': [r'settings_groups\[\]\s*=\s*\{(.*?)\n\};', r'menu_title\(int p\)\{([^}]+)\}',
                             r'const char\*actions\[\]\s*=\s*\{([^}]+)\}',
                             r'const char\*names\[\]\s*=\s*\{("Itinerary"[^}]+)\}',
                             r'options\[\d+\]\s*=([^;]+);'],
            'popups.h': [r'const char\*options\[\]\s*=\s*\{([^}]+)\}'],
        }
        for file, patterns in indirect.items():
            for pattern in patterns:
                fragments = re.findall(pattern, (src/file).read_text(), re.S)
                self.assertTrue(fragments, (file, pattern))
                for frag in fragments:
                    self.assertEqual({json.loads(x) for x in STRINGS.findall(frag)}-keys, set(), (file, frag))

    def test_generated_header(self):
        data = build_translations.load()
        header = (ROOT/'clio843/src/ui_translations.h').read_text()
        for key, translations in data['strings'].items():
            row = ' {'+','.join(json.dumps(x, ensure_ascii=False) for x in [key,*translations])+'},'
            self.assertIn(row, header, key)

if __name__ == '__main__':
    unittest.main()
