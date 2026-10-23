#!/usr/bin/env python3
"""Extract a small looping README preview from the verified full demo.

No UI is repainted. Clips, hashes and generation parameters accompany the movie
in its existing manifest. Normal demo generation calls build_preview too.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CLIPS = ('driving', 'explore-enter-00', 'itinerary-down',
         'keyboard-language-french', 'sender-route-sending',
         'transport-next', 'camera-1')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_preview(folder, report, ffmpeg):
    movie = folder/'clioplay-demo.mp4'
    if sha(movie) != report['video_sha256']:
        raise ValueError('Demo movie differs from its manifest')
    clips = []
    for name in CLIPS:
        key = 'browser_scene' if name.startswith('sender-') else 'scene'
        shot = next(shot for shot in report['timeline'] if shot.get(key) == name)
        clips.append({'scene': name, 'start_seconds': shot['start_seconds'],
                      'duration_seconds': 2.5})
    with tempfile.TemporaryDirectory(prefix='clioplay-readme-preview-') as temp:
        gif = Path(temp)/'preview.gif'
        command = [ffmpeg, '-v', 'error', '-y']
        for clip in clips:
            command += ['-ss', str(clip['start_seconds']), '-t', '2.5', '-i', str(movie)]
        filters = [f'[{i}:v]setpts=PTS-STARTPTS[v{i}]' for i in range(len(clips))]
        filters.append(''.join(f'[v{i}]' for i in range(len(clips)))+
                       f'concat=n={len(clips)}:v=1:a=0,fps=10,'
                       'scale=960:540:flags=lanczos,split[a][b]')
        filters += ['[a]palettegen=stats_mode=diff:max_colors=192[p]',
                    '[b][p]paletteuse=dither=bayer:bayer_scale=4[gif]']
        command += ['-filter_complex', ';'.join(filters), '-map', '[gif]',
                    '-an', '-loop', '0', str(gif)]
        subprocess.run(command, check=True)
        from PIL import Image
        with Image.open(gif) as image:
            if image.size != (960, 540) or image.info.get('loop') != 0:
                raise ValueError('Unexpected README preview format')
            frames, milliseconds = image.n_frames, 0
            for i in range(frames):
                image.seek(i)
                image.load()
                milliseconds += image.info['duration']
        if frames != 175 or milliseconds != 17500 or gif.stat().st_size > 5*1024*1024:
            raise ValueError('README preview duration/size is outside its bounds')
        (folder/'preview.gif').write_bytes(gif.read_bytes())
    return {'file': 'preview.gif', 'sha256': sha(folder/'preview.gif'),
            'source_video_sha256': report['video_sha256'],
            'generator': 'tools/build_readme_preview.py',
            'generator_sha256': sha(Path(__file__)), 'width': 960, 'height': 540,
            'fps': 10, 'frames': frames, 'seconds': milliseconds/1000,
            'bytes': (folder/'preview.gif').stat().st_size, 'clips': clips}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--demo', type=Path, default=ROOT/'docs/demo')
    parser.add_argument('--ffmpeg')
    args = parser.parse_args()
    from render_demo import find_ffmpeg
    folder = args.demo.resolve()
    report = json.loads((folder/'manifest.json').read_text())
    report['readme_preview'] = build_preview(folder, report, find_ffmpeg(args.ffmpeg))
    (folder/'manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(report['readme_preview'], indent=2))


if __name__ == '__main__':
    main()
