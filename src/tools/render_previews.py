#!/usr/bin/env python3
"""Render the README's short GIFs from ClioPlay's native framebuffer."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from PIL import Image
from render_gallery import (BLUETOOTH_SAMPLES, MENU_RETURN_SAMPLES, PHONE_CALC_SAMPLES, ROUNDABOUT_ETA_SAMPLES,
                            SPEED_WARNING_SAMPLES, ROOT, sha)


def timeline(theme, scenes):
    return [(f'{theme}/{scene}.png', ms) for scene, ms in scenes]


def timed_scenes(prefix, samples, end):
    return [(f'{prefix}-{start:04d}', stop - start)
            for start, stop in zip(samples, [*samples[1:], end])]


def previews():
    road_frames = (0, 164, 228, 292, 356, 420, 1164, 1228, 1292, 1356, 1420,
                   4100, 4164, 4228, 4292, 4356, 4420, 5300)
    return {
        'driving.gif': timeline('night', [
            (f'hud-road-then-camera-{start:04d}', end - start)
            for start, end in zip(road_frames, road_frames[1:])
        ]),
        'menus.gif': timeline('night', [
            ('map-menu', 850), ('menu-open-0', 1050), ('favourites-tab', 1050),
            ('showcase-keyboard-char', 1050), ('phone', 900), ('settings', 950),
            ('showcase-theme-day', 850),
            *timed_scenes('preview-menu-return', MENU_RETURN_SAMPLES, 1300),
        ]),
        'bluetooth.gif': timeline('night', timed_scenes('hud-bluetooth', BLUETOOTH_SAMPLES, 4750)),
        'phone-destination.gif': timeline('night', [
            ('empty-receive', 1450),
            *((f'phone-preview-calculating-{ms:04d}', 80) for ms in PHONE_CALC_SAMPLES),
            ('phone-preview-trip', 1750),
        ]),
        'speed-warning.gif': timeline('night', timed_scenes('hud-speed-warning', SPEED_WARNING_SAMPLES, 3650)),
        'map-browsing.gif': timeline('night', [
            ('driving', 800),
            *((f'explore-enter-{i:02}', 100) for i in range(5)), ('explore-enter-05', 600),
            ('preview-explore-pan', 700), ('preview-explore-zoom', 750),
            *((f'preview-explore-return-{i:02}', 100) for i in range(6)),
            ('preview-explore-return-06', 750),
        ]),
        'roundabout-eta.gif': timeline('night', timed_scenes('preview-roundabout-eta', ROUNDABOUT_ETA_SAMPLES, 5600)),
        'add-stop.gif': timeline('night', [
            ('preview-add-stop', 1100), ('preview-add-source', 1200),
            ('preview-pick-favourite', 1250), ('preview-stop-added', 1700),
        ]),
    }


def encode_gif(captures, destination, sequence):
    images = []
    for relative, _ in sequence:
        with Image.open(captures / relative) as image:
            images.append(image.convert('RGB'))
    # Share a palette across the loop so stationary map pixels stay steady.
    w, h = images[0].size
    palette_input = Image.new('RGB', (w, h * len(images)))
    for i, image in enumerate(images):
        palette_input.paste(image, (0, i * h))
    palette = palette_input.quantize(colors=256)
    indexed = [image.quantize(palette=palette, dither=Image.Dither.NONE) for image in images]
    durations = [round(ms / 10) * 10 for _, ms in sequence]
    indexed[0].save(destination, save_all=True, append_images=indexed[1:],
                    duration=durations, loop=0, optimize=True)
    with Image.open(destination) as image:
        if image.size != (480, 272) or image.n_frames < 2 or image.info.get('loop') != 0:
            raise ValueError(f'Invalid preview: {destination.name}')
        duration = 0
        for index in range(image.n_frames):
            image.seek(index)
            image.load()
            duration += image.info['duration']
        if duration != sum(durations):
            raise ValueError(f'Preview duration changed during encoding: {destination.name}')
        return {'sha256': sha(destination), 'frames': image.n_frames, 'duration_ms': duration}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps', type=Path, default=ROOT / 'navigation-output',
                        help='existing UK map bundle covering Newcastle')
    args = parser.parse_args()
    output = ROOT / 'docs/demo'
    plans = previews()
    scenes = sorted({Path(relative).stem for sequence in plans.values() for relative, _ in sequence})
    with tempfile.TemporaryDirectory(prefix='clioplay-readme-previews-') as temp:
        work = Path(temp)
        captures = work / 'captures'
        selection = work / 'scenes.json'
        selection.write_text(json.dumps(scenes))
        subprocess.run([sys.executable, str(ROOT / 'tools/render_gallery.py'),
                        '--maps', str(args.maps.resolve()), '--output', str(captures),
                        '--scene-ids', str(selection), '--themes', 'night', '--frames-only'],
                       check=True)
        capture = json.loads((captures / 'manifest.json').read_text())
        reports = {}
        for name, sequence in plans.items():
            result = encode_gif(captures, work / name, sequence)
            reports[name] = {**result, 'source_frames': [
                {'frame': relative, 'rgb565_sha256': capture['frames'][relative]['rgb565_sha256']}
                for relative, _ in sequence
            ]}
        generator_paths = ('tools/render_previews.py', 'tools/render_gallery.py', 'tools/text_engine.py',
                           'tests/native.c', 'tests/splash.c', 'tests/fixtures/route.h', 'clio843/font.bin')
        manifest = {
            'schema': 'clioplay-readme-previews-1',
            'framebuffer': '480x272 RGB565 from production C draw() and key_handle()',
            'example': 'Newcastle upon Tyne, with simulated car and phone inputs',
            'native_sources': capture['native_sources'],
            'executables': capture['executables'],
            'generator_inputs': {path: sha(ROOT / path) for path in generator_paths},
            'previews': reports,
        }
        (work / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        for name in (*reports, 'manifest.json'):
            shutil.copyfile(work / name, output / name)
    for name, report in reports.items():
        print(f'{name}: {report["duration_ms"] / 1000:g}s, {report["frames"]} frames')


if __name__ == '__main__':
    main()
