#!/usr/bin/env python3
"""Check the portable publication tree and its source/binary correspondence."""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit
from prepare_vendor import VENDOR

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()

def verify_git_candidates(folder, managed):
    """Check actual Git ignore behaviour, including links to private caches.

    Use a temporary index/repository; never stage files in the user's repository.
    """
    git = shutil.which('git')
    if not git:
        return {'status': 'not checked; install Git and rerun before publishing'}
    with tempfile.TemporaryDirectory(prefix='clioplay-publication-git-') as temp:
        subprocess.run([git, 'init', '--quiet', temp], check=True)
        raw = subprocess.check_output([
            git, '-c', 'core.excludesFile='+os.devnull, '--git-dir='+temp+'/.git',
            '--work-tree='+str(folder.resolve()), 'ls-files', '--others',
            '--exclude-standard', '-z'])
    files = set(filter(None, raw.decode('utf-8').split('\0')))
    expected = set(managed) | {'latest.json'}
    if files != expected:
        raise ValueError('Git publication differs from managed files: unexpected '+
                         repr(sorted(files-expected))+'; ignored/missing '+
                         repr(sorted(expected-files)))
    return {'status': 'verified; only managed files would be published',
            'files': len(files), 'bytes': sum((folder/name).stat().st_size for name in files)}

def verify_text_and_links(folder, paths):
    patterns = {
        'private key': rb'-----BEGIN (?:RSA |DSA |EC |OPENSSH |PGP )?PRIVATE KEY-----',
        'GitHub token': rb'\b(?:gh[pousr]_[A-Za-z0-9_]{30,}|github_pat_[A-Za-z0-9_]{30,})\b',
        'AWS key': rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b',
        'Google API key': rb'\bAIza[0-9A-Za-z_-]{35}\b',
    }
    for path in paths:
        raw = path.read_bytes()
        if b'\0' in raw[:8192]:
            continue
        for name, pattern in patterns.items():
            if re.search(pattern, raw):
                raise ValueError(name+' pattern in publication: '+str(path.relative_to(folder)))
    docs = ('README.md', 'CONTRIBUTING.md', 'SECURITY.md', 'src/README.md',
            'src/BUILDING.md', 'src/docs/github-launch.md', 'WINDOWS.md')
    for name in docs:
        path = folder/name
        text = path.read_text()
        links = re.findall(r'\[[^\]\n]*\]\(([^)\s]+)\)', text)
        links += re.findall(r'(?:href|src|srcset)="([^"]+)"', text)
        for link in links:
            url = urlsplit(link)
            if url.scheme or url.netloc or not url.path:
                continue
            target = (path.parent/unquote(url.path)).resolve()
            if not target.is_relative_to(folder.resolve()) or not target.exists():
                raise ValueError('Broken/local-only documentation link in '+name+': '+link)
    return {'credential_patterns': 'no high-confidence matches; not an exhaustive secret audit',
            'owned_documentation_links': 'verified local targets'}


def verify_tree(folder):
    folder = Path(folder)
    source = folder / 'src'
    manifest = folder / 'latest.json'
    if (folder/'SENDER').exists() or (source/'sender').exists():
        raise ValueError('Companion website must stay outside the ClioPlay publication')
    if (folder/'SD-CARD').exists() or (source/'sd-card').exists():
        raise ValueError('Obsolete update overlay in clean-install publication')
    managed = json.loads(manifest.read_text())['files'] if manifest.exists() else None
    if managed:
        for name in managed:
            p=PurePosixPath(name)
            if p.is_absolute() or '..' in p.parts or '\\' in name:
                raise ValueError('Unsafe publication path: '+name)
    paths = [folder / name for name in managed] if managed else [p for p in folder.rglob('*') if p.is_file()]
    vendor_hashes = {digest for _, digest in VENDOR.values()}
    forbidden = {'bond.dat', 'recent.dat', 'trip.dat', 'faves.dat', 'audlast.dat',
                 'ttsystem', 'PNDNavigator', 'vehicledaemon'}
    for path in paths:
        cursor=folder
        for part in path.relative_to(folder).parts:
            cursor=cursor/part
            if cursor.is_symlink():raise ValueError('Redirected publication path: '+str(cursor))
        if path.is_symlink() or path.name in forbidden or path.suffix in ('.cab', '.pbf', '.rgb565', '.log'):
            raise ValueError('Unexpected publication file: ' + str(path))
        if path.stat().st_size >= 100 * 1024 * 1024:
            raise ValueError('File exceeds normal GitHub file limit: ' + str(path))
        digest = sha(path)
        if digest in vendor_hashes:
            raise ValueError('Vendor firmware found in publication: ' + str(path))
        if managed and digest != managed[path.relative_to(folder).as_posix()]:
            raise ValueError('Publication checksum mismatch: ' + str(path))
    text_report = verify_text_and_links(folder, paths)
    for name in ('clio040', 'cliospl', 'clioa2dp'):
        report = json.loads((source / 'build-reports' / (name + '.build.json')).read_text())
        for directory in ('clio843',):
            if sha(source / directory / name) != report['sha256']:
                raise ValueError('Stale source-tree executable: ' + name)
        for relative, digest in report['inputs'].items():
            if sha(source / relative) != digest:
                raise ValueError('Missing/changed build source: ' + relative)
    bootstrap = json.loads((source / 'bootstrap-SHA256.json').read_text())
    for relative, digest in bootstrap.items():
        if sha(source / 'card-template' / relative) != digest:
            raise ValueError('Changed default bootstrap: ' + relative)
    legacy = json.loads((source / 'legacy-binaries/manifest.json').read_text())
    for relative, info in legacy['files'].items():
        if sha(source / relative) != info['sha256']:
            raise ValueError('Changed legacy binary: ' + relative)
    embedded = (source / 'card-template/ttn').read_text().split('write_aux_init(){', 1)[1].split('\n}', 1)[0]
    octets = ''.join(re.findall(r"'([^']*)'", embedded))
    if not re.fullmatch(r'(?:\\[0-7]{3})+', octets):
        raise ValueError('Unsupported embedded AUX format')
    decoded = bytes(int(value, 8) for value in re.findall(r'\\([0-7]{3})', octets))
    if decoded != (source / 'legacy-binaries/clioaux-init').read_bytes():
        raise ValueError('Preserved AUX binary differs from embedded copy')
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        if (folder / name).read_bytes() != (source / name).read_bytes():
            raise ValueError('Publication notice mismatch: ' + name)
    if not (source / 'third_party/bluez-2.15/bluez-utils_2.15.orig.tar.gz').is_file():
        raise ValueError('Missing available hidd upstream source')
    if not (source / 'clio843/src/recovered/focusd.ghidra.c').is_file():
        raise ValueError('Missing available focusd reference')
    if (source/'docs/screenshots').exists():
        raise ValueError('Obsolete screenshot gallery; publication now contains the full video only')
    demo = source/'docs/demo'
    demo_report = None
    if (demo/'manifest.json').is_file():
        film = json.loads((demo/'manifest.json').read_text())
        captures = film['capture_provenance']
        for name,digest in {**film.get('historical_capture_tools',{}), **film['sender_browser']['inputs']}.items():
            if Path(name).is_absolute() or '..' in Path(name).parts or not re.fullmatch('[0-9a-f]{64}',digest):
                raise ValueError('Invalid external capture provenance')
        scenes = captures['scenes']
        scene_by_id = {scene['id']:scene for scene in scenes}
        expected = {theme+'/'+scene['id'] for theme in captures['themes'] for scene in scenes}
        if (len(scene_by_id) != len(scenes) or set(film['covered_scenes']) != expected
                or set(captures['final_frames']) != {name+'.png' for name in expected}):
            raise ValueError('Showcase is missing a catalog action/theme')
        for relative, digest in {**captures['native_sources'], **film['generator_inputs']}.items():
            path = Path(relative)
            if path.is_absolute() or '..' in path.parts or sha(source/path) != digest:
                raise ValueError('Showcase inputs changed; run tools/render_demo.py: '+relative)
        for name, digest in captures['executables'].items():
            if name not in ('clio040','cliospl','clioa2dp') or sha(source/'clio843'/name) != digest:
                raise ValueError('Showcase executable version is stale')
        if (sha(demo/'clioplay-demo.mp4') != film['video_sha256']
                or sha(demo/'poster.png') != film['poster_sha256']):
            raise ValueError('Showcase asset checksum mismatch')
        preview = film.get('readme_preview', {})
        if (preview.get('file') != 'preview.gif'
                or sha(demo/'preview.gif') != preview.get('sha256')
                or preview.get('source_video_sha256') != film['video_sha256']
                or preview.get('generator') != 'tools/build_readme_preview.py'
                or sha(source/preview['generator']) != preview.get('generator_sha256')
                or preview.get('frames') != 175 or preview.get('seconds') != 17.5):
            raise ValueError('Missing/stale README preview; regenerate the demo preview')
        for relative, digest in film['source_frames'].items():
            if relative not in captures['frames'] or captures['frames'][relative]['png_sha256'] != digest:
                raise ValueError('Demo framebuffer provenance mismatch: '+relative)
        input_keys = {event['frame']:event['key'] for events in captures['inputs'].values() for event in events}
        required_inputs = {event['frame'] for name,events in captures['inputs'].items()
                           if scene_by_id[name.split('/',1)[1]]['group'] != 'Animations' for event in events}
        shown_inputs = set();shown_frames = set();shown_browser = set();total_frames = 0
        browser_ids = {sid for _,entries in film['selection'] for kind,sid,_,_ in entries if kind=='browser'}
        for shot in film['timeline']:
            if (shot['frame'] not in film['source_frames'] or shot['duration_frames'] <= 0
                    or abs(shot['start_seconds']-total_frames/film['fps']) > .00001):
                raise ValueError('Invalid showcase timeline')
            shown_frames.add(shot['frame'])
            if shot.get('browser_scene'):
                if shot['key'] or shot['browser_scene'] not in browser_ids:
                    raise ValueError('Unexpected browser input highlight')
                shown_browser.add((shot['theme'],shot['browser_scene']))
            if shot['stage'] == 'REMOTE INPUT':
                if input_keys.get(shot['frame']) != shot['key']:
                    raise ValueError('Demo key differs from recorded native event')
                shown_inputs.add(shot['frame'])
            elif shot['key'] and not (shot['stage']=='NATIVE ANIMATION' and shot['key']=='K_MAP'
                                      and scene_by_id[shot['scene']]['group']=='Animations'):
                raise ValueError('Unexpected key highlight without an input event')
            total_frames += shot['duration_frames']
        if (not required_inputs <= shown_inputs or not set(captures['final_frames']) <= shown_frames
                or shown_browser != {(theme,sid) for theme in captures['themes'] for sid in browser_ids}
                or abs(total_frames/film['fps']-film['duration_seconds']) > .00001):
            raise ValueError('Showcase omits an action result or individual remote step')
        with (demo/'clioplay-demo.mp4').open('rb') as stream:
            if stream.read(12)[4:8] != b'ftyp':raise ValueError('Invalid MP4 container')
        demo_report = {'seconds':film['duration_seconds'], 'dimensions':'1280x720',
                       'selected_native_scenes':len(scenes),'starting_themes':captures['themes'],
                       'individual_native_inputs':len(input_keys),
                       'actual_sender_browser_states':len(browser_ids),
                       'readme_preview':'verified extract of current demo; 17.5 seconds',
                       'remote_highlights':'verified against actual handler invocations',
                       'selected_actions_results_and_steps':'verified; no exhaustive matrix',
                       'asset_and_source_hashes':'native runtime and published media verified',
                       'companion_capture_sources':'recorded hashes only; private inputs not included',
                       'hardware':'not tested'}
    return {'source_files': sum(str(p.relative_to(folder)).startswith('src/') for p in paths),
            'text_and_links': text_report,
            'runtime_source_inputs': 'all present and match supplied build reports',
            'bootstrap': 'default scripts, focusd, hidd and extracted AUX verified',
            'vendor_and_personal_data': 'excluded from managed publication files',
            'framebuffer_demo': demo_report,
            'git_publication': verify_git_candidates(folder, managed) if managed else 'staged export; no Git index',
            'hardware': 'not tested'}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument('--folder', type=Path,
                        default=root.parent if root.name == 'src' else root.parent / 'CLIOPLAY-LATEST-VERSION')
    print(json.dumps(verify_tree(parser.parse_args().folder), indent=2))
