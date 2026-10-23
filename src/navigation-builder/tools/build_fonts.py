#!/usr/bin/env python3
"""Build an offline CPF1 native-script bitmap/shape-table pack from OFL Noto fonts.

The ARM device shapes with HarfBuzz/SheenBidi and reads pre-rasterized glyphs.
No font outline rasterizer, floating point rasterization or desktop fonts are
required in the vehicle. Downloads are cached, with checksums recorded.
"""
import argparse,hashlib,io,json,os,struct,time,urllib.request
from pathlib import Path
import freetype
from fontTools.ttLib import TTFont
SIZES=(11,13,15)
FONTS=['NotoSans','NotoSansArabic','NotoSansHebrew','NotoSansArmenian','NotoSansGeorgian','NotoSansThai','NotoSansLao','NotoSansKhmer','NotoSansMyanmar','NotoSansDevanagari','NotoSansBengali','NotoSansGurmukhi','NotoSansGujarati','NotoSansOriya','NotoSansTamil','NotoSansTelugu','NotoSansKannada','NotoSansMalayalam','NotoSansSinhala','NotoSerifTibetan','NotoSansEthiopic','NotoSansThaana','NotoSansCJKsc']
HEADER=struct.Struct('<4sHHIIII3H2x');FONT=struct.Struct('<32sIIII');GLYPH=struct.Struct('<IbbBBhH')
KEEP={'cmap','head','maxp','hhea','hmtx','GSUB','GPOS','GDEF'}
def get(cache,name):
 filename=name+'-Regular.'+('otf' if 'CJK' in name else 'ttf');target=cache/filename
 url=('https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/OTF/SimplifiedChinese/'+filename if 'CJK' in name else 'https://raw.githubusercontent.com/notofonts/noto-fonts/main/hinted/ttf/'+name+'/'+filename)
 if not target.exists():
  request=urllib.request.Request(url,headers={'User-Agent':'ClioPlay-Global/1.0'})
  with urllib.request.urlopen(request,timeout=60) as f:raw=f.read()
  tmp=target.with_suffix('.download');tmp.write_bytes(raw);os.replace(tmp,target)
 return target,url

def build(out,cache,only=None):
 out=Path(out);cache=Path(cache);cache.mkdir(parents=True,exist_ok=True);out.parent.mkdir(parents=True,exist_ok=True)
 names=only or FONTS;records=[];sources=[];notices=[]
 data=out.with_suffix('.data.tmp');ix=out.with_suffix('.idx.tmp');metas=out.with_suffix('.meta.tmp')
 with data.open('wb') as bitmap,ix.open('wb') as index,metas.open('wb') as metadata:
  for name in names:
   path,url=get(cache,name);font=TTFont(path);ng=len(font.getGlyphOrder());face=freetype.Face(str(path));print(f'Rasterizing {name}: {ng:,} glyphs at {SIZES}',flush=True)
   if ng>65535:raise ValueError('font has too many glyphs')
   notices.append({'font':name,'source':url,'copyright':sorted({n.toUnicode() for n in font['name'].names if n.nameID==0}),'license':sorted({n.toUnicode() for n in font['name'].names if n.nameID==13}),'license_url':sorted({n.toUnicode() for n in font['name'].names if n.nameID==14})})
   # Strip outlines/unused tables after baking; retain complete cmap and layout tables
   # so arbitrary new phone names still shape, including unseen ligatures.
   for table in list(font.keys()):
    if table!='GlyphOrder' and table not in KEEP:del font[table]
   font.recalcBBoxes=False;font.recalcTimestamp=False;stream=io.BytesIO();font.save(stream);blob=stream.getvalue();mo=metadata.tell();metadata.write(blob)
   start=index.tell()
   for size in SIZES:
    face.set_pixel_sizes(0,size)
    for gid in range(ng):
     face.load_glyph(gid,freetype.FT_LOAD_RENDER|freetype.FT_LOAD_TARGET_NORMAL)
     g=face.glyph;b=g.bitmap;w,h=b.width,b.rows
     if (w*h+1)//2>1024 or w>128 or h>128 or not -128<=g.bitmap_left<=127 or not -128<=g.bitmap_top<=127:raise ValueError('glyph outside bounded bitmap cache')
     off=bitmap.tell();advance=round(g.advance.x/64)
     index.write(GLYPH.pack(off,g.bitmap_left,g.bitmap_top,w,h,advance,0))
     # Store 4-bit coverage with row padding removed.
     pixels=b.buffer;raw=bytearray((w*h+1)//2)
     for y in range(h):
      for x in range(w):
       a=(pixels[y*b.pitch+x]+8)//17;n=y*w+x;raw[n//2]|=min(a,15)<<((n&1)*4)
     bitmap.write(raw)
   records.append((name,mo,len(blob),ng,start));sources.append({'name':name,'url':url,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
 metaoff=HEADER.size+len(records)*FONT.size;indexoff=metaoff+metas.stat().st_size;dataoff=indexoff+ix.stat().st_size
 if dataoff+data.stat().st_size>0x7fffffff:raise ValueError('font pack exceeds device limit')
 tmp=out.with_suffix('.new')
 import shutil
 with tmp.open('wb') as f:
  f.write(HEADER.pack(b'CPF1',1,FONT.size,len(records),metaoff,indexoff,dataoff,*SIZES))
  for name,mo,ml,ng,ioff in records:f.write(FONT.pack(name.encode(),metaoff+mo,ml,ng,indexoff+ioff))
  for path in (metas,ix,data):
   with path.open('rb') as inp:shutil.copyfileobj(inp,f,1024*1024)
 os.replace(tmp,out)
 for path in (metas,ix,data):path.unlink()
 info={'format':'CPF1','sizes':SIZES,'fonts':sources,'bytes':out.stat().st_size,'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'license':'SIL Open Font License 1.1; see data/OFL.txt'}
 out.with_name('font-notices.json').write_text(json.dumps(notices,indent=2,ensure_ascii=False)+'\n')
 out.with_suffix(out.suffix+'.json').write_text(json.dumps(info,indent=2)+'\n');print(json.dumps(info,indent=2),flush=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--cache',type=Path,required=True);p.add_argument('--font',action='append');a=p.parse_args();build(a.out,a.cache,a.font)
