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
from publication import PREVIEW_GIFS

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
            'src/BUILDING.md', 'src/docs/github-launch.md', 'src/docs/demo/README.md', 'WINDOWS.md')
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


def verify_previews(source, readme):
    demo = source / 'docs/demo'
    manifest = json.loads((demo / 'manifest.json').read_text())
    previews = manifest['previews']
    if manifest['schema'] != 'clioplay-readme-previews-1' or set(previews) != set(PREVIEW_GIFS):
        raise ValueError('Missing or unexpected README previews')
    native_paths = {p.relative_to(source).as_posix() for p in (source / 'clio843/src').rglob('*')
                    if p.is_file() and p.suffix in ('.c', '.h', '.cc') and 'recovered' not in p.parts}
    if set(manifest['native_sources']) != native_paths:
        raise ValueError('Preview native source list changed; run tools/render_previews.py')
    for group in ('native_sources', 'generator_inputs'):
        for relative, digest in manifest[group].items():
            path = Path(relative)
            if path.is_absolute() or '..' in path.parts or sha(source / path) != digest:
                raise ValueError('Preview input changed; run tools/render_previews.py: ' + relative)
    programs = manifest['executables']
    if set(programs) != {'clio040', 'cliospl', 'clioa2dp'} or any(
            sha(source / 'clio843' / name) != digest for name, digest in programs.items()):
        raise ValueError('Preview executables are stale; run tools/render_previews.py')
    for name, report in previews.items():
        if f'](src/docs/demo/{name})' not in readme:
            raise ValueError('README is missing a preview: ' + name)
        path = demo / name
        with path.open('rb') as stream:
            header = stream.read(10)
        if (sha(path) != report['sha256'] or header[:6] != b'GIF89a'
                or struct.unpack('<HH', header[6:]) != (480, 272)
                or report['frames'] < 2 or not 0 < report['duration_ms'] < 10000):
            raise ValueError('Invalid or changed preview: ' + name)
    return {name: {'duration_ms': report['duration_ms'], 'frames': report['frames']}
            for name, report in previews.items()}


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
    preview_report = verify_previews(source, (folder / 'README.md').read_text())
    return {'source_files': sum(str(p.relative_to(folder)).startswith('src/') for p in paths),
            'text_and_links': text_report,
            'runtime_source_inputs': 'all present and match supplied build reports',
            'bootstrap': 'default scripts, focusd, hidd and extracted AUX verified',
            'vendor_and_personal_data': 'excluded from managed publication files',
            'framebuffer_previews': preview_report,
            'git_publication': verify_git_candidates(folder, managed) if managed else 'staged export; no Git index',
            'hardware': 'not tested'}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument('--folder', type=Path,
                        default=root.parent if root.name == 'src' else root.parent / 'CLIOPLAY-LATEST-VERSION')
    print(json.dumps(verify_tree(parser.parse_args().folder), indent=2))
