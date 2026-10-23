# Interface languages

The language setting changes interface text only. Road/destination names are kept
as received. GPS country profiles independently select Auto units, driving side
and camera policy; changing the interface language cannot change those policies.
The favourite keyboard has independent language layouts. The phone can still
send destinations in native scripts.

Supported: English, French, Spanish, German, Italian, Portuguese, Dutch, Polish,
Turkish, Greek, Ukrainian, Russian, Arabic, Simplified Chinese, Japanese, Korean,
Hindi and Hebrew. The existing font pack covers these interface strings; adding
these languages does not require rebuilding a map.

`ui.json` contains the ordered language codes/native names and original
English/French/Spanish dictionary. Each additional `<code>.json` contains a
complete mapping of the same English keys. Append language codes: indices 0–2
are existing card preferences and must remain English/French/Spanish.

Run `python3 tools/build_translations.py` from the edition's root to regenerate
the native header. Normal `tools/build_ui.py` does this automatically and hashes
all locale JSON files. Missing/extra keys, empty strings, overlong UTF-8 strings
and missing numeric-prefix spaces fail validation.

Run `python3 tools/verify_locales.py` for native UBSan checks: all dictionary
entries/glyphs, saved preferences, remote cycling/wrap, bounded labels/statuses,
UTF-8 buffers and unchanged received names/units/policies. It renders ten states
in day/night for every language. `tools/render_locales.py` (Pillow required)
decodes those RGB565 captures and creates a browsable gallery. Captions are drawn
outside native frames; UI pixels are not edited.

Arabic/Hebrew use native shaping and bidirectional text. Signed clock offsets
are isolated as left-to-right numbers. Remote control directions and the spatial
layout remain consistent when switching languages.

## Favourite keyboard

The left soft key deletes; the right soft key inserts a space. The old on-screen
Space/Delete buttons are replaced by one wide language button. Press it to choose
one of the 18 layouts with Up/Down, then confirm. Left/Right jumps a page in that
chooser; Back cancels without losing the name. Choosing a keyboard never changes
the interface language, country, units or camera policy. The choice is saved.

Keyboard buttons and Latin, Greek and Cyrillic input use capitals, including
the language's accents.
Uncased scripts retain their normal forms. Existing received/saved names keep
their original case. For larger alphabets, the < / > keys below the letters page
through characters; the language button shows the current page. One wide Save button sits
beside it; the remote Back key leaves the editor without saving. Names are bounded to 40 Unicode code points and 160 UTF-8 bytes.

- Chinese uses per-character Pinyin candidates from Unicode 17 Unihan readings.
  Type a syllable (V represents Ü), move Up from the first letter row to the
  candidates and confirm a character. Candidate arrows show more choices.
  This is single-character composition, without phrase prediction. The right
  soft key commits the first candidate and inserts a space; pending syllables
  must be confirmed before saving. The table covers BMP Han with Mandarin
  readings; other Chinese characters can still arrive from the phone.
- Japanese provides hiragana/katakana and voiced/small kana through character
  pages; kana-to-kanji conversion is not included.
- Korean composes modern Hangul from consonant/vowel keys, including compound
  vowels/finals and syllable-boundary splitting. Backspace decomposes the last
  syllable before removing it.
- Arabic/Hebrew and Hindi use the existing native shaping engine; Hindi pages
  include vowel signs and virama for conjuncts.

Layouts are in `keyboards.json`. Normal builds regenerate their header locally
and use the bundled Pinyin header, without downloads or additional SD-card data.
`tools/build_keyboard_data.py --download` reproduces that table from the pinned
Unicode archive; its manifest and Unicode-3.0 licence accompany the generated
source in `clio843/src/keyboard-data/`. No navigation rebuild is required.
