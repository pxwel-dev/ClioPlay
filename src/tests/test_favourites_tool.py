"""Unit tests for tools/favourites_tool.py (CLP2 favourites/recents CLI)."""
from __future__ import annotations

import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import favourites_tool as ft


class FavouritesToolTests(unittest.TestCase):
    def test_sanitize_name_unicode_and_bounds(self):
        self.assertEqual(ft.sanitize_name("  Café\tdu\nNord  "), "Café du Nord")
        self.assertEqual(ft.sanitize_name("   "), "MAP POINT")
        self.assertEqual(ft.sanitize_name(None), "MAP POINT")
        # Combining accent normalizes to NFC (1 char instead of 2).
        self.assertEqual(ft.sanitize_name("Cafe\u0301"), "Café")
        # 40-character cap with multi-byte scripts (Japanese/Polish/Arabic).
        long_jp = "東京駅" * 20
        clamped = ft.sanitize_name(long_jp)
        self.assertEqual(len(clamped), 40)
        self.assertLessEqual(len(clamped.encode("utf-8")), 160)
        # 4-byte emoji characters clamp safely at 40 chars / 160 bytes without invalid UTF-8.
        long_emoji = "\U0001f697" * 50
        clamped_emoji = ft.sanitize_name(long_emoji)
        self.assertEqual(len(clamped_emoji), 40)
        self.assertEqual(len(clamped_emoji.encode("utf-8")), 160)

    def test_clp2_roundtrip_and_formats(self):
        places = [
            ft.Place(5497830, -161780, "Newcastle Quayside"),
            ft.Place(4885661, 235222, "Café de Flore"),
            ft.Place(3568124, 13976712, "東京駅"),
            ft.Place(-3386882, 15120929, "Sydney Opera House"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            faves = root / "faves.dat"
            ft.write_clp2_atomic(faves, places)
            self.assertEqual(faves.stat().st_size, 8 + 4 * 172)
            self.assertEqual(ft.read_clp2(faves), places)

            for fmt in ("csv", "json", "gpx"):
                exported = root / f"out.{fmt}"
                self.assertEqual(ft.main(["export", str(faves), str(exported)]), 0)
                reimported = root / f"re_{fmt}.dat"
                self.assertEqual(ft.main(["import", str(exported), str(reimported)]), 0)
                self.assertEqual(ft.read_clp2(reimported), places)
                self.assertEqual(ft.main(["verify", str(reimported)]), 0)

    def test_merge_dedup_backup_and_cap(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            faves = root / "faves.dat"
            initial = [
                ft.Place(5150740, -12780, "London"),
                ft.Place(5348076, -224263, "Manchester"),
            ]
            ft.write_clp2_atomic(faves, initial)

            # Nearby duplicate within 8e-5 deg should be skipped; distinct place appended.
            src = root / "more.json"
            src.write_text(json.dumps({
                "places": [
                    {"name": "London Duplicate", "lat": 51.50745, "lon": -0.12775},
                    {"name": "Edinburgh", "lat": 55.95325, "lon": -3.18827},
                ]
            }), encoding="utf-8")
            self.assertEqual(ft.main(["import", "--merge", "--backup", str(src), str(faves)]), 0)
            self.assertTrue((root / "faves.dat.bak").exists())
            self.assertEqual(ft.read_clp2(root / "faves.dat.bak"), initial)
            merged = ft.read_clp2(faves)
            self.assertEqual([p.name for p in merged], ["London", "Manchester", "Edinburgh"])

            # Overflow > 25 places is rejected in --strict mode and capped otherwise.
            big = root / "big.csv"
            lines = ["name,lat,lon"] + [f"Place {i},{50 + i * 0.01:.5f},{-1.0:.5f}" for i in range(30)]
            big.write_text("\n".join(lines) + "\n", encoding="utf-8")
            self.assertEqual(ft.main(["import", "--strict", str(big), str(faves)]), 1)
            self.assertEqual(ft.main(["import", str(big), str(faves)]), 0)
            self.assertEqual(len(ft.read_clp2(faves)), ft.MAX_PLACES)

    def test_rejects_corrupt_or_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bad = root / "bad.dat"
            bad.write_bytes(b"CLP1" + struct.pack("<I", 0))
            self.assertEqual(ft.main(["verify", str(bad)]), 1)

            truncated = root / "trunc.dat"
            truncated.write_bytes(struct.pack("<II", ft.CLP2_MAGIC, 1) + b"\x00" * 100)
            self.assertEqual(ft.main(["verify", str(truncated)]), 1)

            bad_csv = root / "bad.csv"
            bad_csv.write_text("name,lat,lon\nInvalid,95.0,0.0\n", encoding="utf-8")
            self.assertEqual(ft.main(["import", str(bad_csv), str(root / "out.dat")]), 1)

    def test_exact_c_binary_interoperability(self):
        """Verify CLP2 files written by favourites_tool.py are read verbatim by clio040.c."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            faves = root / "faves_test.dat"
            places = [
                ft.Place(5497548, -164050, "Łódź Centralna"),
                ft.Place(3568124, 13976712, "東京駅"),
            ]
            ft.write_clp2_atomic(faves, places)

            probe_c = root / "probe.c"
            probe_c.write_text(
                '#define HOST_TEST 1\n'
                '#define main unused_device_entry\n'
                f'#include "{(ROOT / "clio843/src/clio040.c").as_posix()}"\n'
                '#undef main\n'
                '#include <assert.h>\n'
                'int main(void){\n'
                ' struct SavedPlace loaded[FAVE_MAX];\n'
                ' int n=read_places(FAVES_PATH,loaded,FAVE_MAX);\n'
                ' assert(sizeof(struct SavedPlace)==172);\n'
                ' assert(n==2);\n'
                ' assert(loaded[0].lat==5497548&&loaded[0].lon==-164050&&streq(loaded[0].name,"Łódź Centralna"));\n'
                ' assert(loaded[1].lat==3568124&&loaded[1].lon==13976712&&streq(loaded[1].name,"東京駅"));\n'
                ' add_fave_place("Added By C",4885661,235222);\n'
                ' return 0;\n'
                '}\n',
                encoding="utf-8",
            )
            exe = root / "probe"
            subprocess.run(
                ["cc", "-O1", f'-DCLIO_HOST_ROOT="{root.as_posix()}/"', str(probe_c), "-lm", "-o", str(exe)],
                check=True,
            )
            subprocess.run([str(exe)], check=True)
            # And favourites_tool.py reads back what C wrote!
            # (Note add_fave_place wrote nfaves=1 since probe didn't load into faves[] first.)
            reloaded = ft.read_clp2(faves)
            self.assertEqual(reloaded, [ft.Place(4885661, 235222, "Added By C")])


if __name__ == "__main__":
    unittest.main()
