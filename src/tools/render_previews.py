#!/usr/bin/env python3
"""Render the README's short GIFs from ClioPlay's native framebuffer."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops, ImageDraw, ImageFont
from render_gallery import (ANIMATION_SAMPLE_MS, BLUETOOTH_SAMPLES, MENU_ENTRY_SAMPLES, MENU_RETURN_SAMPLES, PHONE_CALC_SAMPLES, ROUNDABOUT_ETA_SAMPLES,
                            SPEED_WARNING_SAMPLES, EXPLORE_SAMPLES, TWEEN_STEPS, ZOOM_MODE_SAMPLES, ROUTE_READY_SAMPLES, POPUP_SAMPLES,
                            TRIPS_LAYOUT_SAMPLES, TRIPS_EMPTY_SAMPLES, POPUPS_LAYOUT_SAMPLES,
                            V216_COORDINATES_SAMPLES, V216_HUD_SETTINGS_SAMPLES,
                            V216_BLUETOOTH_HIDDEN_SAMPLES, V216_ITINERARY_SAMPLES,
                            V220_DESTINATION_SAMPLES, V220_DIVERSION_SAMPLES, V220_FREQUENT_PLACE_SAMPLES,
                            V223_NEARBY_SAMPLES, V223_SECONDARY_MANOEUVRE_SAMPLES, V223_VIEW_DIVERSION_SAMPLES,
                            V224_ML_ETA_HABITS_SAMPLES, V224_ML_SLOWDOWNS_SAMPLES,
                            V224_ML_PARKING_POIS_SAMPLES, V224_ML_ADAPTIVE_HUD_SAMPLES,
                            V225_GPS_PANEL_SAMPLES, ROOT, sha)



def timeline(theme, scenes):
    return [(f'{theme}/{scene}.png', ms) for scene, ms in scenes]


def timed_scenes(prefix, samples, end):
    return [(f'{prefix}-{start:04d}', stop - start)
            for start, stop in zip(samples, [*samples[1:], end])]


def previews():
    road_frames = sorted({0,5300}|{at+step for at in (100,1100,4100) for step in TWEEN_STEPS})
    status_samples = sorted({at+step for at in (0,800,1600,2600,6500,7000) for step in TWEEN_STEPS} | {4400,4800,5400,6100,6200,6280,6360,6440,8600})
    metadata_samples = sorted({at+step for at in (0,900,1700,2900,4300,8500) for step in TWEEN_STEPS} | {700,2600,3900,7200,9200})
    boot = [(f'boot-fade-{ms:04d}', ANIMATION_SAMPLE_MS) for ms in range(0, 500, ANIMATION_SAMPLE_MS)]
    boot += [('boot-fade-0500', 400), ('boot-fade-0900', 700)]
    boot[0] = (boot[0][0], 750)
    plans = {
        'v223-nearby-pois.gif': timeline('night', timed_scenes('preview-v223-nearby-pois', V223_NEARBY_SAMPLES, 11000)),
        'v223-secondary-manoeuvre.gif': timeline('night', timed_scenes('preview-v223-secondary-manoeuvre', V223_SECONDARY_MANOEUVRE_SAMPLES, 7800)),
        'v223-view-diversion.gif': timeline('night', timed_scenes('preview-v223-view-diversion', V223_VIEW_DIVERSION_SAMPLES, 13700)),
        'v224-ml-eta-and-habits.gif': timeline('night', timed_scenes('preview-v224-ml-eta-and-habits', V224_ML_ETA_HABITS_SAMPLES, 7600)),
        'v224-ml-slowdowns.gif': timeline('night', timed_scenes('preview-v224-ml-slowdowns', V224_ML_SLOWDOWNS_SAMPLES, 10800)),
        'v224-ml-parking-and-pois.gif': timeline('night', timed_scenes('preview-v224-ml-parking-and-pois', V224_ML_PARKING_POIS_SAMPLES, 10500)),
        'v224-ml-adaptive-hud.gif': timeline('day', timed_scenes('preview-v224-ml-adaptive-hud', V224_ML_ADAPTIVE_HUD_SAMPLES, 10100)),
        'v225-gps-panel.gif': timeline('night', timed_scenes('preview-v225-gps-panel', V225_GPS_PANEL_SAMPLES, 6200)),
        'v220-frequent-place.gif': timeline('night', timed_scenes('preview-v220-frequent-place', V220_FREQUENT_PLACE_SAMPLES, 5900)),
        'v220-destination-prediction.gif': timeline('night', timed_scenes('preview-v220-destination-prediction', V220_DESTINATION_SAMPLES, 6500)),
        'v220-intelligent-diversions.gif': timeline('night', timed_scenes('preview-v220-intelligent-diversions', V220_DIVERSION_SAMPLES, 6200)),
        'v216-coordinates.gif': timeline('night', timed_scenes('preview-v216-coordinates', V216_COORDINATES_SAMPLES, 11200)),
        'v216-hud-settings.gif': timeline('night', timed_scenes('preview-v216-hud-settings', V216_HUD_SETTINGS_SAMPLES, 10000)),
        'v216-bluetooth-hidden.gif': timeline('night', timed_scenes('preview-v216-bluetooth-hidden', V216_BLUETOOTH_HIDDEN_SAMPLES, 9600)),
        'v216-itinerary.gif': timeline('night', timed_scenes('preview-v216-itinerary', V216_ITINERARY_SAMPLES, 9900)),
        'trips-layout.gif': timeline('night', timed_scenes('preview-trips-layout', TRIPS_LAYOUT_SAMPLES, 13200)),
        'trips-empty-layout.gif': timeline('night', timed_scenes('preview-trips-empty', TRIPS_EMPTY_SAMPLES, 6200)),
        'popups-layout.gif': timeline('night', timed_scenes('preview-popups-layout', POPUPS_LAYOUT_SAMPLES, 10800)),
        'boot-day.gif': timeline('day', boot),
        'boot-night.gif': timeline('night', boot),
        'popups.gif': timeline('night', timed_scenes('preview-popups', POPUP_SAMPLES, 7200)),
        'metadata-updates.gif': timeline('night', timed_scenes('preview-metadata', metadata_samples, 9600)),
        'status-notifications.gif': timeline('night', timed_scenes('preview-status', status_samples, 9200)),
        'driving.gif': timeline('night', [
            (f'hud-road-then-camera-{start:04d}', end - start)
            for start, end in zip(road_frames, road_frames[1:])
        ]),
        'menus.gif': timeline('night', timed_scenes('preview-menu-tour', sorted(
            {at+step for at in (0,700,1500,2300,3100,3800,4600,5400,6200,7000,7800,8600,9400) for step in TWEEN_STEPS}|{9800}), 9900)),
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
            ('preview-pick-favourite', 1250), ('preview-stop-review', 1100), ('preview-stop-added', 1700),
        ]),
    }

    return {name: gif_clock(sequence) for name, sequence in plans.items()}


def gif_clock(sequence):
    """Drop samples that have no display time at GIF's 10 ms resolution."""
    result = []
    elapsed = encoded = 0
    for frame, ms in sequence:
        elapsed += ms
        target = round(elapsed / 10) * 10
        if target > encoded:
            result.append((frame, target - encoded))
            encoded = target
    return result


# RGB565-expanded body/facet colours from the Day/Night vehicle palette.
# Keep these small but important details out of whole-scene colour averaging.
POINTER_COLOURS = ((239,113,24), (247,138,49), (247,158,66), (247,174,90))


def quantize_preview(image, palette):
    indexed = image.quantize(palette=palette, dither=Image.Dither.NONE)
    # Pillow's nearest-colour cache can merge nearby RGB buckets even with
    # reserved palette entries. Restore exact native core pixels explicitly.
    for index, colour in enumerate(POINTER_COLOURS):
        delta = ImageChops.difference(image, Image.new('RGB', image.size, colour))
        red, green, blue = delta.split()
        mask = ImageChops.lighter(ImageChops.lighter(red, green), blue).point([255]+[0]*255)
        indexed.paste(index, mask=mask)
    return indexed


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
    learned = palette_input.quantize(colors=256-len(POINTER_COLOURS))
    palette = Image.new('P', (1, 1))
    reserved = [channel for colour in POINTER_COLOURS for channel in colour]
    palette.putpalette(reserved + learned.getpalette()[:(256-len(POINTER_COLOURS))*3])
    indexed = [quantize_preview(image, palette) for image in images]
    durations = [ms for _, ms in sequence]
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
        return {'sha256': sha(destination), 'frames': image.n_frames, 'duration_ms': duration,
                'reserved_pointer_colours': POINTER_COLOURS}


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


def select_plans(all_plans, requested):
    if not requested:
        return all_plans
    chosen = {}
    unknown = []
    for item in requested:
        name = item if item.endswith('.gif') else f'{item}.gif'
        if name in all_plans:
            chosen[name] = all_plans[name]
        else:
            unknown.append(item)
    if unknown:
        valid = ', '.join(sorted(all_plans))
        raise SystemExit(f"Unknown GIF(s): {', '.join(unknown)}\nAvailable: {valid}")
    return chosen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps', type=Path, default=ROOT / 'navigation-output',
                        help='existing UK map bundle covering Newcastle')
    parser.add_argument('--phone-capture',type=Path,help='private external browser screenshots; otherwise reuse the published browser panel')
    parser.add_argument('--only', '--gif', dest='only', nargs='+', metavar='GIF',
                        help='regenerate only the named GIF(s), with or without .gif suffix')
    parser.add_argument('--list', action='store_true',
                        help='list all available GIF preview names and exit')
    args = parser.parse_args()
    all_plans = previews()
    if args.list:
        for name in sorted(all_plans):
            print(name)
        return
    plans = select_plans(all_plans, args.only)
    output = ROOT / 'docs/demo'
    scenes = sorted({Path(relative).stem for sequence in plans.values() for relative, _ in sequence})
    themes = sorted({Path(relative).parts[0] for sequence in plans.values() for relative, _ in sequence})
    with tempfile.TemporaryDirectory(prefix='clioplay-readme-previews-') as temp:
        work = Path(temp)
        captures = work / 'captures'
        selection = work / 'scenes.json'
        selection.write_text(json.dumps(scenes))
        subprocess.run([sys.executable, str(ROOT / 'tools/render_gallery.py'),
                        '--maps', str(args.maps.resolve()), '--output', str(captures),
                        '--scene-ids', str(selection), '--themes', *themes, '--frames-only'],
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
        provenance = {
            'coordinate-route.gif': (
                'Production remote handlers create the destination and production routing '
                'runs to R_READY on the local map bundle; target coordinates and nonempty '
                'route geometry are asserted. GPS is held stationary. The 1.3-second '
                'calculation display is a controlled preview interval, not a timing benchmark.'),
            'map-point-route.gif': (
                'Production remote handlers create the destination and production routing '
                'runs to R_READY on the local map bundle; target coordinates and nonempty '
                'route geometry are asserted. GPS is held stationary. The 1.3-second '
                'calculation display is a controlled preview interval, not a timing benchmark.'),
            'v220-intelligent-diversions.gif': (
                'Real Newcastle route and legal alternative geometry from '
                'the installed CPR2 routing graph. Historical congestion '
                'observations are controlled examples. The production traffic '
                'search, shared-geometry rendering, BT CTRL, Explore diversion '
                'preview and direct map confirmation handler are used. This is '
                'not a claim about actual traffic conditions.'),
            'v223-view-diversion.gif': (
                'Real Newcastle route and legal alternative geometry from '
                'the installed CPR2 routing graph. Historical congestion '
                'observations are controlled examples. The production traffic '
                'search, shared-geometry rendering, BT CTRL, Explore diversion '
                'preview and direct map confirmation handler are used. This is '
                'not a claim about actual traffic conditions.'),
            'v223-secondary-manoeuvre.gif': (
                'Real Newcastle city-centre route calculated by production routing '
                'from the installed CPR2 routing graph. Vehicle movement along route '
                'vertices is simulated; production nav_maneuver_find() and '
                'nav_next_maneuver_find() drive both Turn cards.'),
            'v223-nearby-pois.gif': (
                'Real Newcastle POI tiles scanned offline by production nearby_scan() '
                'from the installed CPM5 map bundle (Z5 3x3 inner pass plus Z3 expanding '
                'rings), using production remote handlers to add a stop to the itinerary.'),
            'v224-ml-eta-and-habits.gif': (
                'Real Durham (Gilesgate to Neville\'s Cross) route and legal usual-route '
                'alternative from the installed CPR2 routing graph with zero historical '
                'traffic congestion, driven by production ml_pref_arcs search and calibrated '
                'ETA variance (active route stays normal route colour with no yellow bypass).'),
            'v224-ml-slowdowns.gif': (
                'Real York (Station to Heworth) route from the installed CPR2 routing graph '
                'with production ml_observe_decel() / ml_hazard_level() / traffic_scan_ex() '
                'demonstrating warm repeat slowdown alerts, deduplication, cold-only silence '
                'at 18C, cold-weather slowdown alerts at 1C and winter-calibrated ETA.'),
            'v224-ml-parking-and-pois.gif': (
                'Real Newcastle city-centre routing and offline CPM5 POI tiles, with '
                'controlled local ML parking, recurring-stop and fuel-affinity records '
                'driving production confirmation sheets, route-coloured POI highlighting '
                'and Nearby POI ranking.'),
            'v224-ml-adaptive-hud.gif': (
                'Real Newcastle map tiles demonstrating production ml_note_zoom_override() '
                'speed-band Auto-zoom personalisation and ml_learned_brightness() automatic '
                'Day-to-Night brightness adjustment on an Auto theme transition.'),
            'v225-gps-panel.gif': (
                'Real Newcastle driving map demonstrating production double-OK entry from '
                'the driving map (first OK shows road name and ETA, second OK within 4 s '
                'opens the live GPS panel), 1 Hz GPS telemetry updates (compass dial, '
                'heading, GST accuracy, satellites, altitude and coordinates), and BACK dismissal.'),
        }
        for name, note in provenance.items():
            if name in reports:
                reports[name]['routing_provenance'] = note
        generator_paths = ('tools/render_previews.py', 'tools/render_gallery.py', 'tools/text_engine.py', 'tools/mobility_stories.py',
                           'tests/native.c', 'tests/splash.c', 'tests/fixtures/route.h', 'clio843/font.bin')
        merged_previews = {}
        manifest_path = output / 'manifest.json'
        if args.only and manifest_path.is_file():
            merged_previews = json.loads(manifest_path.read_text()).get('previews', {})
        merged_previews.update(reports)
        manifest = {
            'schema': 'clioplay-readme-previews-1',
            'framebuffer': '480x272 RGB565 from production C draw() and key_handle()',
            'example': (
                'Newcastle upon Tyne, with simulated car and phone inputs; '
                'This demonstration uses controlled learned records and a real '
                'map-backed diversion calculated by production routing'
            ),
            'native_sources': capture['native_sources'],
            'executables': capture['executables'],
            'generator_inputs': {path: sha(ROOT / path) for path in generator_paths},
            'previews': merged_previews,
        }
        (work / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        for name in (*reports, 'manifest.json'):
            shutil.copyfile(work / name, output / name)
    for name, report in reports.items():
        print(f'{name}: {report["duration_ms"] / 1000:g}s, {report["frames"]} frames')


if __name__ == '__main__':
    main()
