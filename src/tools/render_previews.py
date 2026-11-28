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
                            V225_GPS_PANEL_SAMPLES, V227_CAMERA_ALERTS_SAMPLES,
                            V300_MULTILEG_SAMPLES, V300_QR_SYNC_SAMPLES, V300_PHONE_LINK_SAMPLES,
                            ROOT, sha)



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
        'v225-gps-panel.gif': timeline('night', timed_scenes('preview-v225-gps-panel', V225_GPS_PANEL_SAMPLES, 8700)),
        'v227-camera-alerts.gif': timeline('night', timed_scenes('preview-v227-camera-alerts', V227_CAMERA_ALERTS_SAMPLES, 8600)),
        'v300-multileg-and-chevron.gif': timeline('night', timed_scenes('preview-v300-multileg', V300_MULTILEG_SAMPLES, 10600)),
        'v300-qr-sync-and-connect.gif': timeline('night', timed_scenes('preview-v300-qr-sync', V300_QR_SYNC_SAMPLES, 11100)),
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
        'phone-destination.gif': timeline('night', timed_scenes('preview-v300-phone-link', V300_PHONE_LINK_SAMPLES, 8800)),
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
            use_direct = any(s.get('direct_panel') for s in shots)
            if use_direct:
                picked = max((s for s in shots if s['ms'] <= at), key=lambda s: s['ms'], default=shots[0])
                source = supplied / Path(picked['file']).name
                with Image.open(source) as image:
                    panel = image.convert('RGB')
                    if panel.size != (400, 444):
                        panel = panel.resize((400, 444), Image.Resampling.LANCZOS)
            else:
                source=supplied/'ready.jpg' if at<1000 else supplied/Path(min(shots,key=lambda x:abs(x['ms']-max(0,at-1000)))['file']).name
                with Image.open(source) as image:panel=image.convert('RGB').crop((342,252,938,914)).resize((400,444),Image.Resampling.LANCZOS)
        else:panel=max((f for f in frames if f[0]<=at),key=lambda f:f[0])[1]
        image=Image.new('RGB',(936,510),(12,18,26));draw=ImageDraw.Draw(image)
        draw.text((16,12),'ClioPlay Link',font=font,fill='white');draw.text((440,12),'ClioPlay',font=font,fill='white')
        image.paste(panel,(16,40))
        with Image.open(captures/native) as head:image.paste(head.convert('RGB'),(440,110))
        caption=(
            'Send multi-stop trip without a receive screen' if at<900
            else 'Receiving 3 stops via CLP3' if at<2300
            else 'Trip ready · active leg + future legs' if at<4800
            else 'Next + Total ETAs & Places sync' if at<6600
            else 'Car settings & Driving Insights synced'
        )
        draw.text((440,402),caption,font=font,fill='white')
        draw.text((440,432),'ClioPlay Link + native UI; transport simulated',font=font,fill=(151,166,185))
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
                'Real Central London (Paddington to City of London) route and legal '
                'alternative geometry from the installed CPR2 routing graph. The active '
                'route uses the v2.2.6.2 fewer-turns preference (-3 turns for +1.0 s, '
                'zero U-turn spurs) and is shorter and faster than the diversion in clear '
                'traffic. Historical congestion observations are controlled examples. The '
                'production traffic search, shared-geometry rendering, BT CTRL, Explore '
                'diversion preview and direct map confirmation handler are used. This is '
                'not a claim about actual traffic conditions.'),
            'v223-view-diversion.gif': (
                'Real Central London (Paddington to City of London) route and legal '
                'alternative geometry from the installed CPR2 routing graph. The clip '
                'starts with the clear-road ETA on the v2.2.6.2 route chosen by the '
                'fewer-turns preference (-3 turns for +1.0 s, zero U-turn spurs). The '
                'diversion is longer and slower in clear traffic. Historical congestion '
                'observations are controlled examples. The production traffic search, '
                'shared-geometry rendering, BT CTRL, Explore diversion preview and direct '
                'map confirmation handler are used. This is not a claim about actual '
                'traffic conditions.'),
            'v223-secondary-manoeuvre.gif': (
                'Real Milton Keynes (Central Milton Keynes to Bletchley) route calculated '
                'by v2.2.6.2 production routing from the installed CPR2 routing graph '
                'with zero U-turn spurs. Vehicle movement along route vertices is '
                'simulated; production nav_maneuver_find() and nav_next_maneuver_find() '
                'drive both Turn cards across chained normal turns and roundabouts.'),
            'v223-nearby-pois.gif': (
                'Real Bristol city-centre POI tiles scanned offline by production '
                'nearby_scan() from the installed CPM5 map bundle (Z5 3x3 inner pass '
                'plus Z3 expanding rings), using production remote handlers to add a '
                'nearby Bristol fuel stop to the itinerary.'),
            'v224-ml-eta-and-habits.gif': (
                'Real Central London (Westminster to Paddington via Mayfair) spur-free '
                'route and legal parallel-street usual-route alternative (Park Street and '
                'Bryanston Street bypassing the Oxford Street dogleg) from the installed '
                'CPR2 routing graph with zero historical traffic congestion, driven by '
                'production unguided ml_pref_arcs search and calibrated '
                'ETA variance (active route stays normal route colour with no yellow bypass).'),
            'v224-ml-slowdowns.gif': (
                'Real Liverpool (Lime Street to Sefton Park) route from the installed '
                'CPR2 routing graph with zero U-turn spurs and production '
                'ml_observe_decel() / ml_hazard_level() / traffic_scan_ex() '
                'demonstrating warm repeat slowdown alerts, deduplication, cold-only '
                'silence at 18C, cold-weather slowdown alerts at 1C and winter-calibrated ETA.'),
            'v224-ml-parking-and-pois.gif': (
                'Real Leeds city-centre routing (to Work / Q-Park The Light) and offline '
                'CPM5 POI tiles, with controlled local ML parking, recurring-stop and '
                'fuel-affinity records driving production confirmation sheets, '
                'route-coloured POI highlighting and Nearby POI ranking.'),
            'v224-ml-adaptive-hud.gif': (
                'Real Edinburgh (Princes Street) map tiles demonstrating production '
                'ml_note_zoom_override() speed-band Auto-zoom personalisation and '
                'ml_learned_brightness() automatic Day-to-Night brightness adjustment '
                'on an Auto theme transition.'),
            'v225-gps-panel.gif': (
                'Real Liverpool Waterfront (Liver Street to Lime Street) driving map '
                'demonstrating the expandable Clock/GPS HUD pill: double-OK expansion '
                'from the driving map (first OK shows road name and ETA, second OK '
                'expands the clock pill with live seconds, recessed compass, heading, '
                'GST accuracy, signal bars/satellites, altitude and coordinates), '
                'non-overlapping BT CTRL status pill, compact 52 px strip above a '
                'centred confirmation popup, seconds-only menu fold, and smooth return '
                'to full driving GPS HUD.'),
            'v227-camera-alerts.gif': (
                'Real Central London route (A3211 Victoria Embankment to Blackfriars) '
                'calculated against the UK CPR2 graph, demonstrating v2.2.7 camera '
                'warnings: matching supersampled on-map upcoming-camera badges placed '
                'on the real carriageway, flush 43 px frosted-glass camera card with '
                'BT-CTRL-style capsule progress bar, synced red overspeed rim and red '
                'map badge, clean centred red-light alert, and active average-speed '
                'zone progress with twin-camera exit marker.'),
            'v300-multileg-and-chevron.gif': (
                'Real Manchester 3-stop itinerary (Northern Quarter -> Shell Deansgate -> '
                'Manchester Piccadilly -> MediaCityUK) calculated against the UK CPR2 '
                'routing graph by production route_start() and trip_plan_service_until(): '
                'Z8 driving approaching a real turn with perspective-scaled white '
                'on-carriageway turn chevron (draw_route_turn_chevron) and Option B '
                'next-stop HUD, Z5 overview with active Leg 0 in magenta (C_ROUTE) and '
                'future Legs 1-2 in lavender (C_ROUTE_FUTURE), and Menu -> Trip showing '
                'both Next and Total journey ETAs and per-stop arrival times.'),
            'v300-qr-sync-and-connect.gif': (
                'Real Manchester map and production qr_encode.h / qr_sync.h optical '
                'pipeline: Bluetooth -> Pair new phone with built-in Connect QR staying '
                'open upon pairing (Connected + Done button) until CLP3:H Link Ping '
                'auto-closes it, followed by CLP3:Q:S popping up the frosted-glass '
                'floating map QR pill, stepping stacked CS1 frames on CLP3:Q:A ACKs, '
                'flashing Synced on CLP3:Q:D, and auto-dismissing back to the map.'),
            'phone-destination.gif': (
                'Side-by-side composite pairing Retina captures of live ClioPlay Link v3.0 '
                'with production ClioPlay v3.0 framebuffer frames on the real Manchester '
                'CPR2 graph: 3-stop CLP3:T burst (Receiving trip capsule progress -> '
                'atomic multi-leg magenta+lavender route), Trip sheet with Next + Total '
                'ETAs, Places sync, Driving Insights, and live CLP3:P car settings update.'),
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
                'Manchester baseline (Deansgate to Salford Quays) plus Central London, '
                'Milton Keynes, Liverpool, Leeds, Bristol, Edinburgh and Newcastle '
                'routes with simulated car and phone inputs; all routes and diversions '
                'are calculated by v2.2.6.2 production routing against the UK CPR2 graph'
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
