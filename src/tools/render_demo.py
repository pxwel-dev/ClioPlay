#!/usr/bin/env python3
"""Make a concise dark-led showcase of everyday Global UI controls and Sender.

Pillow + FFmpeg (or imageio-ffmpeg) required. Capture frames, raw framebuffers and
host binaries stay temporary. Only the film, player, poster and coverage manifest
are published. Runtime rendering and remote actions use the actual native code.
"""
import argparse
from functools import lru_cache
import hashlib
import html
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
SIZE=(1280,720);FPS=30
KEYS={'K_ESC':'LEFT SOFT','K_SR':'RIGHT SOFT','K_BACK':'BACK','K_ENTER':'CENTRE / OK',
      'K_MAP':'MAP','K_MENU':'MENU','K_UP':'UP','K_DOWN':'DOWN','K_LEFT':'LEFT','K_RIGHT':'RIGHT'}
BG='#0b111c';INK='#eef4fc';MUTED='#91a3b9';ACCENT='#65dfcf'


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


@lru_cache(maxsize=32)
def font(size,bold=False):
    return ImageFont.truetype(str(ROOT/'fonts'/('VeraBd.ttf' if bold else 'Vera.ttf')),size)


def center(d,xy,text,size,fill=INK,bold=False):
    d.text(xy,text,font=font(size,bold),fill=fill,anchor='mm')


def curve(points):
    """Closed cubic contour in the remote photo's broad, tapered proportions."""
    out=[points[0]];start=points[0]
    for a,b,end in points[1:]:
        for i in range(1,21):
            t=i/20;v=1-t
            out.append((v**3*start[0]+3*v*v*t*a[0]+3*v*t*t*b[0]+t**3*end[0],
                        v**3*start[1]+3*v*v*t*a[1]+3*v*t*t*b[1]+t**3*end[1]))
        start=end
    return out


@lru_cache(maxsize=12)
def remote(active):
    # Original antialiased drawing from the user's supplied photo. No third-party
    # photograph is redistributed. Soft keys are BELOW the direction ring, with
    # BACK between them; MAP and MENU are the next row down.
    scale=4
    im=Image.new('RGB',(224*scale,544*scale),'#121d2c');d=ImageDraw.Draw(im)
    def box(b):return tuple(round(v*scale) for v in b)
    def oval(b,fill,outline=None,w=1):d.ellipse(box(b),fill=fill,outline=outline,width=w*scale)
    def text(xy,s,n,fill=INK,bold=False):d.text((xy[0]*scale,xy[1]*scale),s,font=ImageFont.truetype(str(ROOT/'fonts'/('VeraBd.ttf' if bold else 'Vera.ttf')),n*scale),fill=fill,anchor='mm')
    def poly(points,fill):d.polygon([(int(x*scale),int(y*scale)) for x,y in points],fill=fill)
    contour=curve([(35,80),((12,92),(16,173),(24,244)),((28,300),(35,362),(45,412)),((53,446),(171,446),(179,412)),((189,362),(196,300),(200,244)),((208,173),(212,92),(189,80)),((151,64),(73,64),(35,80))])
    poly(contour,'#626970')
    inner=[(112+(x-112)*.94,254+(y-254)*.975) for x,y in contour];poly(inner,'#e2e5e6')
    inner=[(112+(x-112)*.85,251+(y-251)*.90) for x,y in contour];poly(inner,'#161a20')
    oval((54,117,170,233),'#e0e3e5','#a8afb5')
    # Four discreet direction dots match the white ring in the reference.
    for key,xy,angle in [('K_UP',(112,132),270),('K_RIGHT',(155,175),0),('K_DOWN',(112,218),90),('K_LEFT',(69,175),180)]:
        if active==key:
            d.pieslice(box((54,117,170,233)),angle-38,angle+38,fill=ACCENT)
        x,y=xy;oval((x-2,y-2,x+2,y+2),'#17453f' if active==key else '#b3bac0')
    oval((86,149,138,201),ACCENT if active=='K_ENTER' else '#eceff0','#8a9197',1)
    def key(x,y,name):
        if active==name:oval((x-16,y-16,x+16,y+16),'#1a6a62');oval((x-12,y-12,x+12,y+12),ACCENT)
        else:oval((x-9,y-9,x+9,y+9),'#101419','#1a2027')
    key(61,265,'K_ESC');key(163,265,'K_SR');key(112,280,'K_BACK')
    # The bracket-shaped legends sit below the two soft keys.
    d.line([(53*scale,281*scale),(53*scale,289*scale),(75*scale,289*scale)],fill=ACCENT if active=='K_ESC' else INK,width=3*scale)
    d.line([(171*scale,281*scale),(171*scale,289*scale),(149*scale,289*scale)],fill=ACCENT if active=='K_SR' else INK,width=3*scale)
    text((112,302),'BACK',12,ACCENT if active=='K_BACK' else INK,True)
    key(70,321,'K_MAP');key(154,321,'K_MENU')
    text((70,347),'MAP',12,ACCENT if active=='K_MAP' else INK,True)
    text((70,362),'2D / 3D',8,ACCENT if active=='K_MAP' else INK)
    text((154,350),'MENU',12,ACCENT if active=='K_MENU' else INK,True)
    return im.resize((224,544),Image.Resampling.LANCZOS)


def compose(shot,native,phone=None):
    im=Image.new('RGB',SIZE,BG);d=ImageDraw.Draw(im)
    d.text((36,22),'CLIOPLAY  /  GLOBAL',font=font(20,True),fill=ACCENT)
    title=shot['chapter']
    d.text((36,58),title,font=font(24,True),fill=INK)
    d.rounded_rectangle((30,102,1002,658),radius=8,fill='#253448')
    if phone is None:
        im.paste(native.resize((960,544),Image.Resampling.NEAREST),(36,108))
    else:
        im.paste(phone.resize((384,544),Image.Resampling.LANCZOS),(36,108))
        d.text((456,183),'ClioPlay',font=font(17,True),fill=INK)
        im.paste(native,(456,225))
    im.paste(remote(shot['key']),(1020,108))
    d.rounded_rectangle((1020,108,1243,651),radius=20,outline='#29394b',width=2)
    caption=shot['caption'];size=18
    while d.textlength(caption,font=font(size))>1208 and size>12:size-=1
    d.text((36,677),caption,font=font(size),fill=INK)
    return im


def showcase_plan():
    """Editorial scope: useful controls once, dark-led; no exhaustive value matrix."""
    return [
      ('Driving and Explore', [
        ('state','splash',3,'ClioPlay Global'),
        ('state','driving',7,'Guidance, road limits and media in one compact view'),
        ('animation','enter',3,'MAP opens Explore'),
        ('action','pan-left',4,'Use the direction pad to browse the map'),
        ('action','zoom-in-explore',4,'Right soft key zooms in'),
        ('action','zoom-out-explore',4,'Left soft key zooms out'),
        ('state','map-stop-0-choice',4,'Choose a map point to add as a stop'),
        ('action','map-stop-0-result',4,'Press the centre button to confirm'),
        ('animation','leave',3,'MAP returns to the driving view'),
      ]),
      ('Destinations and favourites', [
        ('action','map-menu',4,'MENU opens Trip, Bluetooth and Settings'),
        ('action','menu-open-0',5,'Open Trip to manage destinations and saved places'),
        ('state','recent-active-0-choice',4,'Use a recent destination as a stop or a new route'),
        ('action','recent-active-0-result',4,'Add the selected recent destination as a stop'),
        ('action','favourites-tab',4,'Left or right switches Recents and Favourites'),
        ('state','favourite-active-2-choice',4,'Saved places have route, rename and remove actions'),
        ('action','favourite-active-2-result',4,'Rename a favourite with the remote keyboard'),
        ('action','showcase-keyboard-char',4,'Press the centre button to enter a character'),
        ('action','showcase-keyboard-delete',4,'Left soft key deletes the last character'),
        ('action','showcase-keyboard-space',4,'Right soft key inserts a space'),
        ('action','keyboard-language-open',4,'The wide language button opens keyboard layouts'),
        ('action','keyboard-language-french',4,'Choose a keyboard language without changing the interface'),
        ('action','keyboard-accent',4,'Accented letters stay in capitals'),
        ('action','keyboard-save',5,'Choose Save to keep the edited name'),
        ('action','favourite-active-3-result',4,'Remove a saved favourite when it is no longer needed'),
        ('action','destinations-map',3,'MAP brings you straight back to navigation'),
      ]),
      ('Manage your trip', [
        ('action','itinerary-focus',5,'Right soft key moves focus to the current itinerary'),
        ('action','itinerary-down',4,'Select a stop or the final destination'),
        ('state','stop-2-choice',4,'Stop actions let you remove or reorder a stop'),
        ('action','stop-2-result',4,'Move a stop down in the itinerary'),
        ('action','remove-stop-0-result',4,'Confirm removal of a stop'),
        ('action','add-source-1-result',4,'Choose a new stop from Favourites'),
        ('action','add-pick-favourites',5,'Add the selected favourite before your destination'),
        ('action','destination-0-result',4,'Change the final destination without discarding the trip'),
      ]),
      ('Send from your phone', [
        ('action','change-source-2-result',5,'Choose Send from phone on the head unit'),
        ('browser','sender-empty',3,'Open ClioPlay Send on your phone'),
        ('browser','sender-paste',5,'Paste the full Google Maps business link'),
        ('browser','sender-check',5,'Check the detected name and pin coordinates'),
        ('browser','sender-route',4,'Choose Route as new to replace the current route'),
        ('browser','sender-route-sending',4,'Tap Send to ClioPlay and keep the phone connected'),
        ('browser','sender-route-finished',4,'When the transfer finishes, check ClioPlay'),
        ('browser','sender-stop',4,'Choose Add as stop to keep the current destination'),
        ('browser','sender-stop-sending',4,'Send the extra stop from the phone'),
        ('browser','sender-stop-finished',5,'The received stop appears in the itinerary'),
        ('browser','sender-native-name',4,'Names can be edited on the phone in their native script'),
        ('browser','sender-manual',4,'Coordinates can also be entered manually'),
        ('browser','sender-duration-4',4,'Transfer options adjust the send window if needed'),
        ('state','incoming-native-name',5,'ClioPlay preserves incoming non-Latin destination names'),
      ]),
      ('Bluetooth connection', [
        ('action','menu-open-1',5,'Open Bluetooth to manage connected and saved phones'),
        ('action','saved-phone-dialog',4,'Select a saved phone to reconnect or forget it'),
        ('action','saved-phone-0-result',4,'Reconnect a saved phone'),
        ('action','pair-phone',5,'Use Pair a new phone when connecting another device'),
        ('state','phone-pairing',4,'ClioPlay shows the current pairing status'),
        ('action','connected-0-result',4,'Disconnect the current phone from this page'),
        ('action','saved-phone-1-result',4,'Forget a saved phone you no longer use'),
      ]),
      ('Audio controls', [
        ('action','transport-enter',5,'BACK activates the phone audio controls'),
        ('action','transport-next',4,'RIGHT requests the next track'),
        ('action','transport-previous',4,'LEFT requests the previous track'),
        ('action','transport-pause',4,'Press the centre button to pause'),
        ('action','transport-play',4,'Press again to resume playback'),
        ('action','transport-leave',4,'BACK leaves the audio controls'),
        ('state','radio-fm',5,'The top bar follows the vehicle source when you switch to radio'),
        ('state','aux',4,'AUX appears when that source is selected'),
      ]),
      ('Display and navigation settings', [
        ('action','menu-open-2',5,'Open Settings to personalise ClioPlay'),
        ('action','brightness-60',4,'Left and right adjust screen brightness'),
        ('action','showcase-theme-day',4,'Day theme is available for bright conditions'),
        ('action','showcase-theme-auto',4,'Auto follows local day and night'),
        ('action','showcase-theme-night',4,'Return to the Night theme'),
        ('action','units-metric',4,'Choose Metric units'),
        ('state','limit-metric',4,'Speed, road limits and distances change together'),
        ('action','units-imperial',4,'Imperial units are available as an override'),
        ('action','showcase-units-auto',4,'Auto chooses units for the current country'),
        ('action','autozoom-1',4,'Enable automatic map zoom'),
        ('action','autozoom-0',4,'Disable it to keep manual zoom control'),
      ]),
      ('Alerts, clock and language', [
        ('action','showcase-camera-off',4,'Camera alerts can be turned off'),
        ('action','camera-setting-1',4,'Visual keeps camera warnings on screen'),
        ('action','showcase-camera-audio',5,'Phone audio adds a cue through the Bluetooth stream'),
        ('action','clock-1',4,'Automatic clock uses the local time zone'),
        ('action','clock-0',4,'The centre button switches to manual time'),
        ('action','clock-plus-quarter',4,'Left and right adjust the manual offset'),
        ('action','language-fr',5,'Choose your interface language in Settings'),
        ('action','showcase-language-en',4,'Return to English'),
        ('action','debug-1',4,'Enable Debug log when troubleshooting'),
        ('action','debug-0',4,'Turn logging off for normal use'),
      ]),
      ('On the road', [
        ('state','limit-estimated',5,'A question mark identifies an estimated road limit'),
        ('state','gps-lost',4,'GPS status remains visible when reception is lost'),
        ('state','route-rerouting',4,'ClioPlay shows when it is recalculating the route'),
        ('state','camera-1',5,'Speed-camera alerts show the camera limit and distance'),
        ('state','camera-2',5,'Red-light alerts show the type and distance'),
        ('state','camera-average',5,'Average-speed sections show your measured average'),
        ('state','arrived-stop-5',4,'Arrival at a stop offers a short confirmation countdown'),
        ('action','arrived-stop-continue',4,'Confirm to continue to the next stop'),
        ('state','arrived-destination-5',4,'The final destination has its own arrival confirmation'),
        ('action','arrived-destination-continue',4,'Confirm arrival to finish the trip'),
        ('action','resume-trip-0-result',5,'Resume a saved trip after restarting ClioPlay'),
        ('action','cancel-route-0-result',4,'Cancel the route from the Trip workspace'),
        ('state','driving',6,'ClioPlay Global'),
      ]),
    ]


def selected_scene_ids():
    ids=set()
    for _,entries in showcase_plan():
        for kind,sid,_,_ in entries:
            if kind=='animation':
                ids.update(f'explore-{sid}-{i:02}' for i in range(6 if sid=='enter' else 7))
            elif kind!='browser':ids.add(sid)
    # Browser flow uses these receiver states beside the real Sender page.
    ids.update(('sender-route-wait','sender-route-result','sender-stop-wait','sender-stop-result','driving'))
    return sorted(ids)


def storyboard(captures,sender):
    shots=[];chapters=[];covered=[]
    scenes={s['id']:s for s in captures['scenes']}
    browser={s['id']:s for s in sender['frames']}
    theme='night'
    def add(sid,frame,seconds,stage,caption,key=None,**extra):
        shots.append(dict(theme=theme,scene=sid,chapter=chapter,frame=frame,
                          seconds=seconds,stage=stage,key=key,caption=caption,**extra))
        name=theme+'/'+sid
        if name not in covered:covered.append(name)
    for chapter,entries in showcase_plan():
        chapters.append(dict(title=chapter,shot=len(shots)))
        for kind,sid,hold,caption in entries:
            if kind=='browser':
                b=browser[sid];head=b['head']
                add(head,theme+'/'+head+'.png',hold,'PHONE / BROWSER',caption,phone_frame=b['frame'],browser_scene=sid)
            elif kind=='animation':
                count=6 if sid=='enter' else 7
                for i in range(count):
                    name=f'explore-{sid}-{i:02}'
                    add(name,theme+'/'+name+'.png',.1 if i<count-1 else hold,'NATIVE ANIMATION',caption,'K_MAP' if i<3 else None)
            else:
                stem=theme+'/'+sid;events=captures['inputs'].get(stem,[]) if kind=='action' else []
                if events:
                    assert len(events)<=2, 'Showcase must not repeat long input sequences'
                    for i,event in enumerate(events):
                        add(sid,event['frame'],.55,'REMOTE INPUT',caption,event['key'])
                        if i<len(events)-1:add(sid,event['frame'],.15,'INPUT RELEASED',caption)
                    add(sid,stem+'.png',hold-.55,'ACTION RESULT',caption)
                else:add(sid,stem+'.png',hold,'EXAMPLE STATE',caption)
    assert set(covered)=={'night/'+sid for sid in selected_scene_ids()}
    return shots,chapters,covered


def find_ffmpeg(explicit):
    if explicit:return explicit
    if shutil.which('ffmpeg'):return shutil.which('ffmpeg')
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except (ImportError,RuntimeError) as error:raise SystemExit('Install FFmpeg or imageio-ffmpeg; or use --ffmpeg /path/to/ffmpeg') from error


def encode(capture_dir,sender_dir,out,ffmpeg):
    captures=json.loads((capture_dir/'manifest.json').read_text())
    sender=json.loads((sender_dir/'manifest.json').read_text())
    shots,chapters,covered=storyboard(captures,sender);used={};timeline=[];cursor=0
    with tempfile.TemporaryDirectory(prefix='clioplay-full-video-') as td:
        work=Path(td);entries=[]
        for i,shot in enumerate(shots):
            relative=shot['frame'];path=capture_dir/relative
            if relative not in used:
                digest=sha(path)
                if digest!=captures['frames'][relative]['png_sha256']:raise ValueError('Changed capture: '+relative)
                used[relative]=digest
            phone=None
            if shot.get('phone_frame'):
                pp=sender_dir/shot['phone_frame'];expected=next(s['png_sha256'] for s in sender['frames'] if s['frame']==shot['phone_frame'])
                if sha(pp)!=expected:raise ValueError('Changed Sender browser capture')
                phone=Image.open(pp).convert('RGB')
            with Image.open(path) as native:
                assert native.size==(480,272)
                name=f'{i:05}.png';compose(shot,native.convert('RGB'),phone).save(work/name)
            frames=round(shot['seconds']*FPS);duration=frames/FPS
            entries.extend([f"file '{name}'",f'option framerate {FPS}',f'duration {duration:.9f}'])
            timeline.append(dict(shot,start_seconds=round(cursor,6),duration_frames=frames))
            cursor+=duration
            if i%200==0:print(f'Composed {i}/{len(shots)} video segments',flush=True)
        entries.extend([entries[-3],f'option framerate {FPS}'])
        (work/'frames.txt').write_text('\n'.join(entries)+'\n')
        metadata=[';FFMETADATA1','title=ClioPlay Global — Everyday controls']
        for i,chapter in enumerate(chapters):
            chapter['start_seconds']=timeline[chapter.pop('shot')]['start_seconds']
            end=timeline[chapters[i+1]['shot']]['start_seconds'] if i+1<len(chapters) else cursor
            title=chapter['title'].replace('=','\\=')
            metadata.extend(['[CHAPTER]','TIMEBASE=1/1000',f'START={round(chapter["start_seconds"]*1000)}',f'END={round(end*1000)}','title='+title])
        (work/'chapters.txt').write_text('\n'.join(metadata)+'\n')
        print(f'Encoding {cursor/60:.1f} minutes; {len(shots)} timed segments',flush=True)
        movie=work/'clioplay-demo.mp4'
        command=[ffmpeg,'-hide_banner','-loglevel','error','-y','-f','concat','-safe','0','-i',str(work/'frames.txt'),
                 '-f','ffmetadata','-i',str(work/'chapters.txt'),'-map','0:v','-map_metadata','1','-map_chapters','1',
                 '-vf',f'fps={FPS}','-t',f'{cursor:.9f}','-c:v','libx264','-preset','veryfast','-crf','19',
                 '-pix_fmt','yuv420p','-movflags','+faststart','-an','-progress',str(work/'progress.txt'),str(movie)]
        process=subprocess.Popen(command)
        import time
        while process.poll() is None:
            time.sleep(10)
            if (work/'progress.txt').exists():
                times=[s for s in (work/'progress.txt').read_text().splitlines() if s.startswith('out_time=')]
                if times:print('Encoded '+times[-1].split('=',1)[1],flush=True)
        if process.returncode:raise RuntimeError('Video encoder failed')
        if movie.stat().st_size>=100*1024*1024:raise ValueError('Video exceeds publication file limit')
        subprocess.run([ffmpeg,'-v','error','-xerror','-i',str(movie),'-map','0:v','-f','null','-'],check=True)
        info=subprocess.run([ffmpeg,'-hide_banner','-i',str(movie)],capture_output=True,text=True).stderr
        if 'h264' not in info or '1280x720' not in info or '30 fps' not in info:raise ValueError('Unexpected stream: '+info)
        out.mkdir(parents=True,exist_ok=True);shutil.copyfile(movie,out/'clioplay-demo.mp4')
        poster=next(s for s in shots if s['scene']=='explore-enter-00' and s['key']=='K_MAP')
        compose(poster,Image.open(capture_dir/poster['frame']).convert('RGB')).save(out/'poster.png')
    inputs={name:sha(ROOT/name) for name in ('tools/render_demo.py','tools/render_gallery.py','tests/native.c','tests/splash.c','tests/fixtures/route.h','fonts/Vera.ttf','fonts/VeraBd.ttf')}
    report=dict(schema='clioplay-curated-framebuffer-demo-3',video='clioplay-demo.mp4',video_sha256=sha(out/'clioplay-demo.mp4'),
                poster_sha256=sha(out/'poster.png'),generator_inputs=inputs,capture_provenance=captures,sender_browser=sender,
                source_frames=used,timeline=timeline,chapters=chapters,covered_scenes=covered,selection=showcase_plan(),
                width=1280,height=720,fps=FPS,duration_seconds=round(cursor,6),codec='H.264 / yuv420p',audio=False,
                remote_reference='Original drawing based on the user-supplied remote photo; white direction ring, soft keys below it, central BACK, lower MAP/MENU. Photograph not redistributed.',
                limits='Selected everyday actions and settings, night starting theme with brief Day/Auto examples. No exhaustive character/value/scroll/boundary matrix. Independent host examples; phone/vehicle/GPS fixtures and simulated radio hop. Explore at native 100ms cadence; other inputs paced for readability.')
    from build_readme_preview import build_preview
    report['readme_preview']=build_preview(out,report,ffmpeg)
    (out/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    write_docs(out,report)
    print(json.dumps({'seconds':round(cursor,3),'minutes':round(cursor/60,2),'scenes_per_theme':len(captures['scenes']),
                      'theme_scenes':len(covered),'remote_presses_recorded':sum(map(len,captures['inputs'].values())),
                      'segments':len(shots),'bytes':(out/'clioplay-demo.mp4').stat().st_size,'entire_video_decoded':True},indent=2),flush=True)


def stamp(seconds):
    n=int(seconds);return f'{n//3600}:{n//60%60:02}:{n%60:02}' if n>=3600 else f'{n//60}:{n%60:02}'


def write_docs(out,report):
    body=['# ClioPlay Global showcase','',
          f'[Watch the video](index.html) or [download the MP4](clioplay-demo.mp4) — {stamp(report["duration_seconds"])}, 1280×720, 30 fps, silent.','',
          'A dark-led tour of everyday remote controls, destinations/favourites, itinerary actions, Bluetooth/audio, useful settings, camera alerts and arrivals. The Day and Auto theme options are shown briefly. Characters, setting values, languages, scrolling and boundary cases are not exhaustively repeated.','',
          'The remote drawing follows the supplied photograph and highlights the key used. ClioPlay Send is shown as the actual website: paste a business link, review the destination, send a new route or an extra stop, edit native names, and use manual coordinates or transfer options.','',
          'The [looping README preview](preview.gif) is a short extract from this movie. GitHub renders it as an animated image, respecting viewer motion settings; the full film opens separately.','',
          'These are independent examples from the production framebuffer/handler, a real UK map and a real baseline route. Sender captures use actual browser MediaSession metadata and silent-WAV playback. The native daemon parser validates the packets before they enter the UI through its file-input handler. The Bluetooth radio hop is simulated. Other phone, vehicle, GPS and camera statuses use controlled fixtures; hardware timing, audio and wireless operation are not validated.','',
          'The film keeps only short action descriptions and key highlights. Technical provenance stays in the manifest. Explore uses the native 100 ms animation steps. The framebuffer is scaled 2× in the main view and shown at native size beside the Sender; H.264 encoding is lossy. Raw captures and composition frames stay temporary, outside the project.','',
          '## Chapters','', '| Time | Chapter |','|---|---|']
    for chapter in report['chapters']:body.append(f'| {stamp(chapter["start_seconds"])} | {chapter["title"]} |')
    body+=['','## Regenerate','', 'From `src/`:','', '```sh','python3 tools/render_demo.py --maps navigation-output --sender-captures /path/to/private-browser-captures','```','',
           'Requires Pillow, a host C/C++ compiler, FFmpeg (`ffmpeg` on PATH, `imageio-ffmpeg`, or `--ffmpeg /path/to/ffmpeg`). NumPy optionally speeds up RGB565 decoding. Companion browser captures must be supplied using `--sender-captures PATH`; their source and capture tooling live in the separate private project. Map files are read-only. The plan in `tools/render_demo.py` selects the showcase; `tools/render_gallery.py` captures only its chosen scenes in the Night starting theme.']
    (out/'README.md').write_text('\n'.join(body)+'\n')
    rows=[];indexed=set()
    for shot in report['timeline']:
        identity=(shot['chapter'],shot['caption'])
        if identity in indexed:continue
        indexed.add(identity);label=shot['chapter']+' / '+shot['caption']
        rows.append(f'<button data-time="{shot["start_seconds"]}" type="button">{stamp(shot["start_seconds"])} · {html.escape(label)}</button>')
    page='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ClioPlay Global showcase</title><style>body{margin:24px auto;padding:0 20px;max-width:1280px;background:#0b111c;color:#eef4fc;font:16px system-ui}video{width:100%;border-radius:12px}a{color:#65dfcf}p{color:#91a3b9;line-height:1.5}input{box-sizing:border-box;width:100%;padding:12px;background:#162333;color:white;border:1px solid #536478;border-radius:8px;font:inherit}#actions{max-height:460px;overflow:auto;margin:12px 0}button{display:block;width:100%;text-align:left;background:#142030;border:0;border-bottom:1px solid #2e3d50;padding:10px;color:#eef4fc;font:inherit;cursor:pointer}button:hover{background:#274254}button[hidden]{display:none}</style><h1>ClioPlay Global</h1><video id="film" controls playsinline preload="metadata" poster="poster.png" src="clioplay-demo.mp4"></video><p>A dark-led tour of the controls, Trip, Bluetooth, settings and ClioPlay Send. Host-rendered examples; Bluetooth radio transport simulated.</p><p><a href="clioplay-demo.mp4" download>Download MP4</a> · <a href="README.md">Chapters and notes</a></p><input id="filter" type="search" placeholder="Jump to a feature" aria-label="Find a feature"><div id="actions">ROWS</div><script>const v=document.querySelector('#film'),buttons=[...document.querySelectorAll('button')];for(const b of buttons)b.addEventListener('click',()=>{v.currentTime=Number(b.dataset.time);v.play().catch(()=>{});v.scrollIntoView({behavior:'smooth',block:'center'})});document.querySelector('#filter').addEventListener('input',e=>{const q=e.target.value.toLowerCase();for(const b of buttons)b.hidden=!b.textContent.toLowerCase().includes(q)});</script></html>'''
    (out/'index.html').write_text(page.replace('ROWS',''.join(rows)))


def decode_sender_packets(sender_dir):
    # Use the production daemon's packet/CRC/UTF-8 routines, not a second parser.
    def function(text,name):
        start=text.index('static ',text.index(name+'(')-32)
        brace=text.index('{',text.index(name+'(',start));depth=1;end=brace+1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        return text[start:end]+'\n'

    text=(ROOT/'clio843/src/clioa2dp.c').read_text()
    code='#include <assert.h>\n#include <string.h>\n#include <stdio.h>\ntypedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef int s32;\n'
    code+='#include "'+str(ROOT/'clio843/src/utf8.h')+'"\n'
    for name in ('slen','hexval0','parse_dec_span','parse_hex16_span','crc16_ccitt_text','nav_packet_parse','meta_text'):code+=function(text,name)
    code+='int main(int argc,char**argv){assert(argc==3);char action;int lat,lon;u16 seq;assert(nav_packet_parse(argv[1],&action,&lat,&lon,&seq));char name[161];meta_text(name,sizeof(name),(const u8*)argv[2],strlen(argv[2]));assert(!strcmp(name,argv[2]));printf("%c %d %d %u\\n",action,lat,lon,seq);return 0;}'
    packets=json.loads((sender_dir/'manifest.json').read_text())['packets'];receipts=[]
    with tempfile.TemporaryDirectory(prefix='clioplay-sender-receiver-') as td:
        td=Path(td);(td/'decode.c').write_text(code)
        subprocess.run(['cc','-O1','-fsanitize=undefined','-fno-sanitize-recover=all',str(td/'decode.c'),'-o',str(td/'decode')],check=True)
        for packet in packets:
            fields=subprocess.check_output([str(td/'decode'),packet['title'],packet['artist']],text=True).split()
            action,lat,lon,seq=fields
            assert action==('N' if packet['id']=='route' else 'S')
            receipts.append(dict(id=packet['id'],action=action,lat=int(lat),lon=int(lon),seq=int(seq),name=packet['artist']))
    return receipts


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ffmpeg');p.add_argument('--maps',type=Path,default=ROOT/'navigation-output')
    p.add_argument('--captures',type=Path,help='reuse an existing temporary native capture run')
    p.add_argument('--sender-captures',type=Path,required=True,help='external browser captures from the private companion project')
    p.add_argument('--output',type=Path,default=ROOT/'docs/demo')
    a=p.parse_args();ffmpeg=find_ffmpeg(a.ffmpeg)
    with tempfile.TemporaryDirectory(prefix='clioplay-all-actions-') as td:
        temp=Path(td);sender=a.sender_captures.resolve()
        receipts=decode_sender_packets(sender);(temp/'receipts.json').write_text(json.dumps(receipts,ensure_ascii=False))
        (temp/'selection.json').write_text(json.dumps(selected_scene_ids()))
        captures=a.captures.resolve() if a.captures else temp/'native'
        if not a.captures:
            subprocess.run([sys.executable,str(ROOT/'tools/render_gallery.py'),'--maps',str(a.maps.resolve()),'--output',str(captures),
                            '--record-inputs','--frames-only','--sender-receipts',str(temp/'receipts.json'),
                            '--scene-ids',str(temp/'selection.json'),'--themes','night'],check=True)
        encode(captures,sender,a.output.resolve(),ffmpeg)


if __name__=='__main__':main()
