#!/usr/bin/env python3
"""Render the README's short GIFs from ClioPlay's native framebuffer."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont
from render_gallery import (BLUETOOTH_SAMPLES, MENU_ENTRY_SAMPLES, MENU_RETURN_SAMPLES, PHONE_CALC_SAMPLES, ROUNDABOUT_ETA_SAMPLES,
                            SPEED_WARNING_SAMPLES, EXPLORE_SAMPLES, TWEEN_STEPS, ZOOM_MODE_SAMPLES, ROUTE_READY_SAMPLES, POPUP_SAMPLES, ROOT, sha)


def timeline(theme, scenes):
    return [(f'{theme}/{scene}.png', ms) for scene, ms in scenes]


def timed_scenes(prefix, samples, end):
    return [(f'{prefix}-{start:04d}', stop - start)
            for start, stop in zip(samples, [*samples[1:], end])]


def previews():
    road_frames = sorted({0,5300}|{at+step for at in (100,1100,4100) for step in TWEEN_STEPS})
    status_samples = sorted({at+step for at in (0,800,1600,2600,6500,7000) for step in TWEEN_STEPS} | {4400,4800,5400,6100,6200,6280,6360,6440,8600})
    metadata_samples = sorted({at+step for at in (0,900,1700,2900,4300,8500) for step in TWEEN_STEPS} | {700,2600,3900,7200,9200})
    return {
        'popups.gif': timeline('night', timed_scenes('preview-popups', POPUP_SAMPLES, 7200)),
        'metadata-updates.gif': timeline('night', timed_scenes('preview-metadata', metadata_samples, 9600)),
        'status-notifications.gif': timeline('night', timed_scenes('preview-status', status_samples, 9200)),
        'driving.gif': timeline('night', [
            (f'hud-road-then-camera-{start:04d}', end - start)
            for start, end in zip(road_frames, road_frames[1:])
        ]),
        'menus.gif': timeline('night', [
            *timed_scenes('preview-menu-entry', MENU_ENTRY_SAMPLES, 1900), ('favourites-tab', 1050),
            ('showcase-keyboard-char', 850), ('phone', 700), ('settings', 700),
            ('schedule-edit-day', 800), ('schedule-edit-minute', 800), ('showcase-theme-day', 650),
            *timed_scenes('preview-menu-return', MENU_RETURN_SAMPLES, 1300),
        ]),
        'bluetooth.gif': timeline('night', timed_scenes('hud-bluetooth', BLUETOOTH_SAMPLES, 4750)),
        'phone-destination.gif': timeline('night', [
            ('empty-receive',1000),('empty-receive',500),
            *timed_scenes('phone-preview-calculating', list(PHONE_CALC_SAMPLES),1400),
            ('phone-preview-trip',2100),('phone-preview-trip',2200),
        ]),
        'speed-warning.gif': timeline('night', timed_scenes('hud-speed-warning', SPEED_WARNING_SAMPLES, 3650)),
        'map-browsing.gif': timeline('night', timed_scenes('preview-zoom-modes', ZOOM_MODE_SAMPLES, 6500)),
        'roundabout-eta.gif': timeline('night', timed_scenes('preview-roundabout-eta', ROUNDABOUT_ETA_SAMPLES, 5600)),
        'coordinate-route.gif': timeline('night', [
            ('coordinate-route-source',800), ('preview-coordinates-latitude',900),
            ('preview-coordinates-longitude',900), ('preview-coordinates-name',800),
            ('preview-coordinates-review',1100),
            *timed_scenes('coordinate-route-calculating', list(PHONE_CALC_SAMPLES), 1300),
            *timed_scenes('coordinate-route-ready', list(ROUTE_READY_SAMPLES), 1700),
            ('coordinate-route-recents',900),
        ]),
        'map-point-route.gif': timeline('night', [
            ('map-point-driving',600),
            *timed_scenes('map-point-explore', list(EXPLORE_SAMPLES), 1000),
            ('map-point-selected',1000), ('map-point-confirm',1400),
            *timed_scenes('map-point-calculating', list(PHONE_CALC_SAMPLES), 1300),
            *timed_scenes('map-point-ready', list(ROUTE_READY_SAMPLES), 1700),
        ]),
        'coordinates.gif': timeline('night', [(f'preview-coordinates-{stage}', ms) for stage, ms in [('entry',700),('latitude',1000),('longitude',1000),('name',1000),('review',1600),('saved',1500)]]),
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
    # GIF has a 10 ms clock: carry rounding error to retain total timing.
    durations, elapsed, encoded = [], 0, 0
    for _, ms in sequence:
        elapsed += ms
        target = round(elapsed / 10) * 10
        durations.append(target - encoded)
        encoded = target
    indexed[0].save(destination, save_all=True, append_images=indexed[1:],
                    duration=durations, loop=0, optimize=True)
    with Image.open(destination) as image:
        if image.size not in ((480,272),(936,510)) or image.n_frames < 2 or image.info.get('loop') != 0:
            raise ValueError(f'Invalid preview: {destination.name}')
        duration = 0
        for index in range(image.n_frames):
            image.seek(index)
            image.load()
            duration += image.info['duration']
        if duration != sum(durations):
            raise ValueError(f'Preview duration changed during encoding: {destination.name}')
        return {'sha256': sha(destination), 'frames': image.n_frames, 'duration_ms': duration}


def compose_phone(captures, output, sequence, supplied, previous):
    """Combine native frames with external browser captures; no website tooling."""
    output.mkdir()
    frames=[]
    if supplied:
        shots=json.loads((supplied/'frames.json').read_text())
        provenance={'kind':'local browser capture','transport':'simulated',
                    'website':'https://pxwel-dev.github.io/ClioPlay-Send/',
                    'screenshots':{Path(s['file']).name:sha(supplied/Path(s['file']).name) for s in shots},
                    'ready_sha256':sha(supplied/'ready.jpg')}
    else:
        if not previous.is_file():raise ValueError('Provide --phone-capture for the first website composite')
        with Image.open(previous) as image:
            if image.size!=(936,510):raise ValueError('Provide --phone-capture to replace the old native-only phone GIF')
            at=0
            for i in range(image.n_frames):
                image.seek(i);frames.append((at,image.convert('RGB').crop((16,40,416,484))))
                at+=image.info['duration']
        prior=json.loads((ROOT/'docs/demo/manifest.json').read_text())['previews']['phone-destination.gif'].get('browser_capture',{})
        provenance={**prior,'kind':'browser panel retained from published composite','previous_gif_sha256':sha(previous),
                    'website':'https://pxwel-dev.github.io/ClioPlay-Send/','transport':'simulated'}
    font=ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'),16)
    timeline_out=[];at=0
    for index,(native,ms) in enumerate(sequence):
        if supplied:
            source=supplied/'ready.jpg' if at<1000 else supplied/Path(min(shots,key=lambda x:abs(x['ms']-max(0,at-1000)))['file']).name
            with Image.open(source) as image:panel=image.convert('RGB').crop((342,252,938,914)).resize((400,444),Image.Resampling.LANCZOS)
        else:panel=max((f for f in frames if f[0]<=at),key=lambda f:f[0])[1]
        image=Image.new('RGB',(936,510),(12,18,26));draw=ImageDraw.Draw(image)
        draw.text((16,12),'ClioPlay Send',font=font,fill='white');draw.text((440,12),'ClioPlay',font=font,fill='white')
        image.paste(panel,(16,40))
        with Image.open(captures/native) as head:image.paste(head.convert('RGB'),(440,110))
        caption='Send without opening a receive screen' if at<1500 else 'Calculating route' if at<2900 else 'Destination received'
        draw.text((440,402),caption,font=font,fill='white')
        draw.text((440,432),'Browser + native UI; transport simulated',font=font,fill=(151,166,185))
        name=f'{index:03d}.png';image.save(output/name);timeline_out.append((name,ms));at+=ms
    return timeline_out,provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps', type=Path, default=ROOT / 'navigation-output',
                        help='existing UK map bundle covering Newcastle')
    parser.add_argument('--phone-capture',type=Path,help='private external browser screenshots; otherwise reuse the published browser panel')
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
            encoded_sequence=sequence;image_root=captures;browser_report=None
            if name=='phone-destination.gif':
                image_root=work/'phone-composite'
                encoded_sequence,browser_report=compose_phone(captures,image_root,sequence,args.phone_capture,output/name)
            result = encode_gif(image_root, work / name, encoded_sequence)
            if browser_report:result['browser_capture']=browser_report

            reports[name] = {**result, 'source_frames': [
                {'frame': relative, 'rgb565_sha256': capture['frames'][relative]['rgb565_sha256']}
                for relative, _ in sequence
            ]}
        for name in ('coordinate-route.gif','map-point-route.gif'):
            reports[name]['routing_provenance'] = (
                'Production remote handlers create the destination and production routing '
                'runs to R_READY on the local map bundle; target coordinates and nonempty '
                'route geometry are asserted. GPS is held stationary. The 1.3-second '
                'calculation display is a controlled preview interval, not a timing benchmark.')
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
