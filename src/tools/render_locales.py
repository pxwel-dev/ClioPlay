#!/usr/bin/env python3
"""Decode actual RGB565 captures; add captions only outside native frames."""
from pathlib import Path
import html
from PIL import Image,ImageDraw,ImageFont
from verify import decode
import build_translations
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'docs/next-release/localized-framebuffer'
data=build_translations.load();codes=data['languages'];font=ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'),14)
pages=('Menu','Trip','Phone connection','Settings','Rename','Choose destination','Resume dialog','Missing map','Acquiring GPS','Outside map')
for path in out.glob('*.rgb565'):decode(path.read_bytes()).save(path.with_suffix('.png'))
for theme in ('day','night'):
 for group in range((len(codes)+2)//3):
  members=codes[group*3:group*3+3];board=Image.new('RGB',(len(members)*500,1540),(23,26,32));draw=ImageDraw.Draw(board)
  for col,code in enumerate(members):
   for row,label in enumerate(pages[:5]):
    x,y=10+col*500,10+row*305;draw.text((x,y),f'{code} · {theme} · {label}',font=font,fill='white');board.paste(Image.open(out/f'{code}-{theme}-{row}.png'),(x,y+23))
  board.save(out/f'overview-{theme}-{group+1}.png')
  if not group:board.save(out/f'overview-{theme}.png')
sections=[]
for code in codes:
 board=Image.new('RGB',(1000,len(pages)*305+10),(23,26,32));draw=ImageDraw.Draw(board)
 for col,theme in enumerate(('day','night')):
  for row,label in enumerate(pages):
   x,y=10+col*500,10+row*305;draw.text((x,y),f'{code} · {theme} · {label}',font=font,fill='white');board.paste(Image.open(out/f'{code}-{theme}-{row}.png'),(x,y+23))
 board.save(out/f'{code}-overview.png')
 rows=''.join('<h3>'+label+'</h3><div class="pair">'+''.join('<figure><figcaption>'+theme+'</figcaption><img loading="lazy" width="480" height="272" src="'+code+'-'+theme+'-'+str(i)+'.png"></figure>' for theme in ('day','night'))+'</div>' for i,label in enumerate(pages))
 sections.append('<section id="'+code+'"><h2>'+html.escape(data['language_names'][code])+'</h2>'+rows+'</section>')
nav=' '.join('<a href="#'+c+'">'+html.escape(data['language_names'][c])+'</a>' for c in codes)
(out/'index.html').write_text('<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ClioPlay interface languages</title><style>body{margin:24px auto;padding:0 12px;max-width:1000px;background:#171a20;color:white;font:16px system-ui}nav{line-height:2}a{color:#8ac5ff;margin-right:12px}.pair{display:flex;gap:20px;flex-wrap:wrap}figure{margin:0}img{max-width:100%;height:auto}h2{margin-top:48px}</style><h1>ClioPlay interface languages</h1><p>Actual 480 × 272 RGB565 output from the native renderer. Sample place names remain unchanged. Host previews do not establish head-unit performance.</p><nav>'+nav+'</nav>'+''.join(sections))
print(f'Decoded {len(codes)*len(pages)*2} native captures; generated per-language sheets, grouped overviews and a browsable gallery.')
