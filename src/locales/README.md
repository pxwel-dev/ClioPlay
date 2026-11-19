# Interface languages

Fixed notifications, turn instructions, camera labels and route states use sentence
case. System acronyms such as GPS, ETA, BT, AUX and FM, units and brand names retain their normal spelling. Compass headings use local direction abbreviations.

The language setting changes interface text only. Road/destination names are kept
as received, except for the reserved internal unnamed-place markers MAP POINT
and PHONE DESTINATION, whose display labels are translated. GPS country profiles independently select Auto units, driving side
and camera policy; changing the interface language cannot change those policies.
The naming keyboard has independent language layouts. The phone can still
send destinations in native scripts.

Supported: English, French, Spanish, German, Italian, Portuguese, Dutch, Polish,
Turkish, Greek, Ukrainian, Russian, Arabic, Simplified Chinese, Japanese, Korean,
Hindi and Hebrew. The existing font pack covers these interface strings; adding
these languages does not require rebuilding a map.

`ui.json` contains the ordered language codes/native names and original
English/French/Spanish dictionary. Each additional `<code>.json` contains a
complete mapping of the same English keys. Append language codes: indices 0–2
are existing card preferences and must remain English/French/Spanish.

Run `python3 tools/build_translations.py` from `src/` to regenerate
the native header. Normal `tools/build_ui.py` does this automatically and hashes
all locale JSON files. Missing/extra keys, empty strings, overlong UTF-8 strings
and missing numeric-prefix spaces fail validation. The host CI also checks literal
translation calls, indirect menu/setting keys and the generated native table.

Run `python3 tools/verify_locales.py` for native UBSan checks: all dictionary
entries/glyphs, saved preferences, remote cycling/wrap, bounded labels/statuses,
UTF-8 buffers and unchanged received names/units/policies. It renders fourteen states
in day/night for every language. `tools/render_locales.py` (Pillow required)
decodes those RGB565 captures and creates a browsable gallery. Captions are drawn
outside native frames; UI pixels are not edited.

Arabic/Hebrew use native shaping and bidirectional text. Signed clock offsets
are isolated as left-to-right numbers. Remote control directions and the spatial
layout remain consistent when switching languages.

## Favourite keyboard

The left soft key deletes; the right soft key inserts a space. The wide language
button opens the layout chooser. Use it to choose
one of the 18 layouts with Up/Down, then confirm. Left/Right jumps a page in that
chooser; Back cancels without losing the name. Choosing a keyboard never changes
the interface language, country, units or camera policy. The choice is saved.

Naming uses a map-based glass sheet. The title, toolbar and entry field sit above
an adaptive grid. Latin keyboards use QWERTY, French AZERTY or German QWERTZ,
followed by their native accented letters. Uppercase letters retain their
language's accents; uncased scripts keep their normal forms. Received/saved names
keep their original case. Latin/Greek/Cyrillic keys align on the base character,
so accents do not push the letter bodies down.

The toolbar contains language, script/number mode, previous/next page, page count
and **Save**. When naming coordinates, **Next** opens review instead of saving;
Back returns to longitude while retaining the name. In favourite naming, Back
leaves without saving. The arrows are dim on a single-page layout. Numbers and
punctuation use a separate mode. Each language uses the available grid space;
extra character pages have their own proportional navigation controls.

Horizontal movement wraps the current row, including RTL rows. Vertical movement
wraps through the grid and toolbar while preserving the preferred horizontal
position. The console knob cycles through all selectable keys. Layout changes
never alter interface language, units, camera policy or existing entered text.
Names are bounded to 40 Unicode code points and 160 UTF-8 bytes.

- Chinese starts with common Hanzi. Its mode button opens a syllable index
  represented by a sample character from each reading; selecting one opens that
  syllable's characters. Back returns to the index and then common characters.
  The mode button also reaches numbers/symbols. The bundled Unicode 17 Unihan
  Mandarin dictionary supplies the browser; phrase prediction is not included.
- Japanese uses separate hiragana and katakana modes, then numbers/symbols.
  Each script has base kana and voiced/small-kana pages. Kana-to-kanji conversion
  is not included.
- Korean composes modern Hangul from consonant/vowel keys, including compound
  vowels/finals and syllable-boundary splitting. Backspace decomposes the last
  syllable before removing it.
- Arabic/Hebrew and Hindi use the native shaping engine; Hindi pages include
  vowel signs and virama for conjuncts. Isolated combining marks use a dotted
  circle on the keycap only; entry inserts the actual character.

Layouts are in `keyboards.json`. Normal builds regenerate their header locally
and use the bundled Pinyin header, without downloads or additional SD-card data.
`tools/build_keyboard_data.py --download` reproduces that table from the pinned
Unicode archive; its manifest and Unicode-3.0 licence accompany the generated
source in `clio843/src/keyboard-data/`. No navigation rebuild is required.
