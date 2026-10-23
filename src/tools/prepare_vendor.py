#!/usr/bin/env python3
"""Acquire the official CAB and extract the two required compatibility files locally."""
import hashlib
import os
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from pathlib import Path

CAB_NAME = 'navcore_8843.3130258.Carminat_TomTom.cab'
CAB_URL = 'https://download.tomtom.com/sweet/navcore/' + CAB_NAME
CAB_SHA = 'bef363f8c770ba4102bd3b80195ff5ee0946617843b62fbe616c5f4061e2d104'
VENDOR = {
    'ttsystem': ('~tsystem', '4a6e2616a1250928a1bc684df2fe56dd3135f47d152ed51b110db073ca8955c0'),
    'vehicledaemon': ('vehicledaemon', '2aa828da8acd45be0c874c8b3b3dd5d7fb70e33e3e154fe81a5622d9a1bab5ca'),
}

def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

def fetch_cab(cache):
    """Reuse a verified CAB, or atomically cache a verified official download."""
    cache = Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    cab = cache/CAB_NAME
    if cab.exists():
        if sha(cab) == CAB_SHA:
            print('Using the verified cached Navcore 8.843 CAB.', flush=True)
            return cab
        print('Cached Navcore checksum mismatch; downloading a fresh copy.', flush=True)
    temporary = None
    try:
        print('Downloading Navcore 8.843 directly from TomTom...', flush=True)
        with tempfile.NamedTemporaryFile(prefix=CAB_NAME+'.', suffix='.download', dir=cache, delete=False) as out:
            temporary = Path(out.name)
            with urllib.request.urlopen(CAB_URL, timeout=60) as source:
                shutil.copyfileobj(source, out, 1024*1024)
        if sha(temporary) != CAB_SHA:
            raise ValueError('Downloaded Navcore CAB checksum mismatch')
        os.replace(temporary, cab)
        print('Navcore download verified.', flush=True)
        return cab
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def obtain_cab(cache=None):
    cache = Path(cache) if cache is not None else Path(__file__).resolve().parents[1]/'navigation-builder/.cache'
    try:
        return fetch_cab(cache)
    except (OSError, ValueError) as error:
        raise ValueError(f'Automatic Navcore download failed: {error}. Check your internet connection and run the installer again.') from error


def prompt_cab():
    """Compatibility hook for installers already waiting for navigation to finish."""
    return obtain_cab()

def extract(cab, output):
    """Verify everything in temporary storage before creating the output."""
    cab, output = Path(cab).expanduser().resolve(), Path(output).resolve()
    if sha(cab) != CAB_SHA:
        raise ValueError('Unsupported CAB or checksum mismatch: ' + str(cab))
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError('Use an empty output folder; existing files will not be replaced')
    extractor = shutil.which('cabextract') or shutil.which('7zz') or shutil.which('7z')
    if not extractor:
        raise ValueError('Install cabextract or 7-Zip, then run the import again')
    with tempfile.TemporaryDirectory(prefix='clioplay-navcore-') as temporary:
        temporary = Path(temporary)
        unpack, stage = temporary / 'cab', temporary / 'files'
        unpack.mkdir(); stage.mkdir()
        if Path(extractor).name == 'cabextract':
            command = [extractor, '-q', '-d', str(unpack), str(cab)]
        else:
            command = [extractor, 'x', '-y', '-o' + str(unpack), str(cab)]
        subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
        with zipfile.ZipFile(unpack / 'install/DS_1/IA0000700006swlimage0.zip') as archive:
            for destination, (member, digest) in VENDOR.items():
                target = stage / destination
                with archive.open(member) as source, target.open('wb') as sink:
                    shutil.copyfileobj(source, sink)
                if sha(target) != digest:
                    raise ValueError('Compatibility file checksum mismatch: ' + destination)
                target.chmod(0o755)
        output.mkdir(parents=True, exist_ok=True)
        for name in VENDOR:
            shutil.copy2(stage / name, output / name)
    return {name: digest for name, (_, digest) in VENDOR.items()}

