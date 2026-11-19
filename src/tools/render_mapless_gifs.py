#!/usr/bin/env python3
"""Render real ClioPlay C framebuffer animations without map files.

These previews intentionally show the production 'Map data missing' pill; no
roads, navigation geometry, or external map assets are fabricated. The route,
phone and saved-place states are controlled fixtures from tests/native.c.

Requires a host C compiler and Pillow: python3 -m pip install Pillow
Run: python3 tools/render_mapless_gifs.py --output docs/demo
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image

from mobility_stories import DESTINATION_EVENTS, DIVERSION_EVENTS, FREQUENT_PLACE_EVENTS

ROOT = Path(__file__).resolve().parents[1]
INTERVAL_MS = 100

# Actual production key handlers and page transitions, not painted screenshots.
STORIES = {
    'trips-layout.gif': {
        'duration': 13200,
        'events': [
            (0, 'menu_open(P_TRIP,0,hud_now_ms());'),
            (1200, 'key_handle(K_RIGHT);assert(sheet_page==P_PLACES&&places_tab==0);'),
            (2400, 'key_handle(K_RIGHT);assert(sheet_page==P_PLACES&&places_tab==1);'),
            (3600, 'key_handle(K_ENTER);assert(sheet_page==P_PLACE);'),
            (4700, 'key_handle(K_BACK);assert(sheet_page==P_PLACES);'),
            (5400, 'key_handle(K_LEFT);assert(places_tab==0);'),
            (6200, 'key_handle(K_LEFT);assert(sheet_page==P_TRIP);'),
            (7000, 'key_handle(K_DOWN);'),
            (7800, 'key_handle(K_ENTER);assert(sheet_page==P_DEST);'),
            (8600, 'key_handle(K_BACK);assert(sheet_page==P_TRIP);'),
            (9400, 'key_handle(K_DOWN);'),
            (9800, 'key_handle(K_ENTER);assert(sheet_page==P_SOURCE);'),
            (10600, 'key_handle(K_DOWN);'),
            (11000, 'key_handle(K_ENTER);assert(sheet_page==P_PLACES&&places_picker);'),
            (11600, 'key_handle(K_ENTER);assert(sheet_page==P_REVIEW);'),
        ],
    },
    'trips-empty-layout.gif': {
        'duration': 6200,
        'events': [
            (0, 'clear_trip();menu_open(P_TRIP,0,hud_now_ms());'),
            (1800, 'key_handle(K_ENTER);assert(sheet_page==P_COORD);'),
            (4200, 'key_handle(K_BACK);assert(sheet_page==P_TRIP);'),
        ],
    },
    'popups-layout.gif': {
        'duration': 10800,
        'events': [
            (0, 'menu_open(P_TRIP,0,hud_now_ms());'),
            (900, 'confirm_kind=0;menu_change(P_CONFIRM,0,hud_now_ms());'),
            (2350, 'menu_close(hud_now_ms());modal=MOD_CANCEL_ROUTE;'),
            (4000, 'modal=MOD_SOURCE_PICK;dest_pick_action=DEST_PICK_ADD_STOP;'),
            (5400, 'modal=MOD_ROUTE_POINT;pending_point_stop=0;'),
            (6650, 'modal=MOD_NONE;keyboard_open_fave_name(5497370,-161331);menu_open(P_NAME,0,hud_now_ms());'),
            (7800, 'pk_focus=PK_LANGUAGE;key_handle(K_ENTER);assert(sheet_page==P_LANG&&kbd_picker);'),
            (9300, 'key_handle(K_BACK);assert(sheet_page==P_NAME);'),
        ],
    },
}
# ClioPlay 2.1.6 real-renderer demos (no map bundle needed).
STORIES.update({'v216-coordinates.gif': {'duration': 11200, 'events': [(0, 'clear_trip();menu_open(P_TRIP,0,hud_now_ms());'), (800, 'key_handle(K_ENTER);assert(sheet_page==P_COORD);'), (1400, 'coordinates.focus=4;key_handle(K_ENTER);'), (1650, 'coordinates.focus=3;key_handle(K_ENTER);'), (1900, 'coordinates.focus=COORD_DOT;key_handle(K_ENTER);'), (2150, 'coordinates.focus=8;key_handle(K_ENTER);'), (2400, 'coordinates.focus=6;key_handle(K_ENTER);'), (3000, 'key_handle(K_SR);assert(coordinates.step==COORD_LON);'), (3600, 'coordinates.focus=COORD_SIGN;key_handle(K_ENTER);'), (3800, 'coordinates.focus=0;key_handle(K_ENTER);'), (4000, 'coordinates.focus=COORD_DOT;key_handle(K_ENTER);'), (4200, 'coordinates.focus=5;key_handle(K_ENTER);'), (4600, 'key_handle(K_ESC);assert(streq(coordinates.value[1],"-1."));'), (5100, 'coordinates.focus=COORD_NEXT;key_handle(K_ENTER);assert(coordinates.error&&coordinates.step==COORD_LON);'), (6500, 'coordinates.focus=5;key_handle(K_ENTER);'), (7100, 'key_handle(K_SR);assert(coordinates.step==COORD_LAT);'), (7900, 'coordinates.focus=COORD_NEXT;key_handle(K_ENTER);assert(sheet_page==P_NAME);'), (9600, 'pk_focus=PK_SAVE;key_handle(K_ENTER);assert(sheet_page==P_REVIEW);')]}, 'v216-hud-settings.gif': {'duration': 10000, 'events': [(0, 'menu_open(P_SETTINGS,1,hud_now_ms());'), (700, 'key_handle(K_ENTER);assert(sheet_page==P_SET_HUD);'), (1700, 'key_handle(K_RIGHT);assert(pref.top_swap);'), (3600, 'key_handle(K_DOWN);assert(settings_row(sheet_page,sheet_focus)==11);'), (4200, 'key_handle(K_RIGHT);assert(pref.bottom_swap);'), (6100, 'key_handle(K_MAP);assert(sheet_page==P_CLOSED);'), (8200, 'menu_open_setting(11,hud_now_ms());')]}, 'v216-bluetooth-hidden.gif': {'duration': 9600, 'events': [(0, 'menu_open(P_HOME,0,hud_now_ms());'), (2200, 'hw_bluetooth_enabled=0;menu_change(P_HOME,0,hud_now_ms());assert(menu_count(P_HOME)==2);'), (4400, 'key_handle(K_DOWN);key_handle(K_ENTER);assert(sheet_page==P_SETTINGS);'), (6600, 'key_handle(K_MENU);assert(sheet_page==P_HOME);'), (7500, 'hw_bluetooth_enabled=1;menu_change(P_HOME,1,hud_now_ms());'), (8500, 'key_handle(K_ENTER);assert(sheet_page==P_PHONE);')]},  'v216-itinerary.gif': {'duration': 9900, 'events': [(0, 'for(int i=trip.count;i<TRIP_MAX_TARGETS;i++){char name[32];snprintf(name,sizeof(name),"Stop %d",i);assert(trip_add_stop(name,5498000+i*100,-164000-i*100));}menu_open(P_TRIP,0,hud_now_ms());')] + [(1000+i*500, 'key_handle(K_DOWN);assert(sheet_focus==%d);' % (i+1)) for i in range(11)] + [(6800, 'key_handle(K_UP);assert(sheet_focus==trip.count);key_handle(K_ENTER);assert(sheet_page==P_TRIP);'), (7800, 'key_handle(K_UP);assert(sheet_focus==trip.count-1);'), (8800, 'for(int i=0;i<9;i++)key_handle(K_UP);assert(sheet_focus==0);')]}})

STORIES.update({
    'v220-frequent-place.gif': {'duration': 5900, 'events': FREQUENT_PLACE_EVENTS},
    'v220-destination-prediction.gif': {'duration': 6500, 'events': DESTINATION_EVENTS},
    'v220-intelligent-diversions.gif': {'duration': 6200, 'events': DIVERSION_EVENTS},
})


def build_c(root: Path, work: Path):
    native = (root / 'tests/native.c').read_text()
    native = native.replace('int main(int argc,char**argv)', 'int unused_test_main(int argc,char**argv)')
    native = native.replace('#include "fixtures/route.h"', '#include "' + str(root / 'tests/fixtures/route.h') + '"')
    functions = []
    for index, (name, story) in enumerate(STORIES.items()):
        cases = '\n'.join(f'case {when // INTERVAL_MS}: {{{action} break;}}' for when, action in story['events']
                          if when % INTERVAL_MS == 0)
        # Align sub-frame events to the next sampling tick; no action is dropped.
        cases += '\n' + '\n'.join(f'case {(when + INTERVAL_MS - 1) // INTERVAL_MS}: {{{action} break;}}'
                                  for when, action in story['events'] if when % INTERVAL_MS)
        functions.append(f'''
static void story_{index}(void){{
  fixture();hud_capture_clock=1;hud_capture_ms=s_now_ms();
  mapok=0;pref.theme=THEME_NIGHT;apply_palette(1);screen=SCR_MAP;
  pref.top_swap=pref.bottom_swap=0;hw_bluetooth_enabled=1;
  menu_reset(hud_now_ms());popup_reset();
  u32 start=hud_capture_ms;
  for(int t=0;t<{story['duration']//INTERVAL_MS};t++){{
    hud_capture_ms=start+(u32)t*{INTERVAL_MS}u;
    switch(t){{{cases}default:break;}}
    if(t==0)settle(); /* First frame begins on a readable, completed pose. */
    draw();
    if(sheet_page&&sheet_page!=P_NAME){{
      assert(sheet_to_box.y>=40);
      assert(sheet_to_box.y+sheet_to_box.h<=HUD_BOTTOM_Y-7);
      assert(bottom_to==256);
    }}
    if(sheet_page==P_NAME)assert(bottom_to==0);
    if(modal!=MOD_NONE){{
      assert(popup.to.y>=40);
      assert(popup.to.y+popup.to.h<=HUD_BOTTOM_Y-7);
    }}
    char filename[100];snprintf(filename,sizeof(filename),"scene-{index}-%04d",t);
    snapshot(filename);
  }}
}}
''')
    code = '#define CLIO_BOOT_ART "' + str(root / 'clio843/src/boot_art.h') + '"\n'
    code += '#define CLIO_SOURCE "' + str(root / 'clio843/src/clio040.c') + '"\n'
    code += '#define CLIO_HOST_ROOT "' + str(work / 'host') + '/"\n'
    code += native
    code += '\n'.join(functions)
    code += '''
int main(int argc,char**argv){
 (void)argc;(void)argv;outdir=CLIO_OUTPUT_DIR;
 assert(load_font());
'''
    for i in range(len(STORIES)):
        code += f' story_{i}();\n'
    code += 'return 0;}\n'
    code = code.replace('CLIO_OUTPUT_DIR', json.dumps(str(work)))
    c = work / 'capture.c'
    c.write_text(code)
    exe = work / 'capture'
    subprocess.run(['cc', '-O1', '-Wno-unused-function', '-DCLIO_UNICODE=0', str(c), '-o', str(exe)], check=True)
    return exe


def decode(raw):
    # The production framebuffer uses native little-endian RGB565.
    import array
    pixels = array.array('H')
    pixels.frombytes(raw)
    if __import__('sys').byteorder != 'little':
        pixels.byteswap()
    rgb = bytearray(480 * 272 * 3)
    for i, pixel in enumerate(pixels):
        at = 3 * i
        r, g, b = (pixel >> 11) & 31, (pixel >> 5) & 63, pixel & 31
        rgb[at] = (r << 3) | (r >> 2)
        rgb[at+1] = (g << 2) | (g >> 4)
        rgb[at+2] = (b << 3) | (b >> 2)
    return Image.frombytes('RGB', (480, 272), bytes(rgb))


def encode(captures, path):
    height = 272
    shared = Image.new('RGB', (480, height * len(captures)))
    for index, frame in enumerate(captures):
        shared.paste(frame, (0, index * height))
    pal = shared.quantize(colors=256, dither=Image.Dither.NONE)
    palette = Image.new('P', (1, 1))
    palette.putpalette(pal.getpalette())
    frames = [image.quantize(palette=palette, dither=Image.Dither.NONE) for image in captures]
    path.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=INTERVAL_MS,
                   loop=0, optimize=True, disposal=2)
    with Image.open(path) as check:
        assert check.n_frames > 2 and check.size == (480, 272)
        print(f'{path}: {check.n_frames} frames, {path.stat().st_size/1024:.1f} KiB')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, default=ROOT/'docs/demo')
    p.add_argument('--only', nargs='*', choices=tuple(STORIES), help='Regenerate only selected animations')
    args = p.parse_args()
    if args.only:
        chosen=set(args.only)
        for name in tuple(STORIES):
            if name not in chosen: del STORIES[name]
    with tempfile.TemporaryDirectory(prefix='clioplay-mapless-') as temporary:
        temp = Path(temporary)
        target = temp/'host/clio843'
        target.mkdir(parents=True)
        shutil.copyfile(ROOT/'clio843/font.bin', target/'font.bin')
        subprocess.run([str(build_c(ROOT, temp))], check=True)
        for i, (name, story) in enumerate(STORIES.items()):
            frames = [decode((temp/f'scene-{i}-{tick:04d}.rgb565').read_bytes())
                      for tick in range(story['duration']//INTERVAL_MS)]
            encode(frames, args.output/name)


if __name__ == '__main__':
    main()
