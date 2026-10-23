#!/usr/bin/env python3
"""Compile complete interface dictionaries; received/user names are never lookup keys."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def load():
 data=json.loads((ROOT/'locales/ui.json').read_text());codes=data['languages'];base=data['strings'];names=data['language_names']
 if codes[:3]!=['en','fr','es'] or len(set(codes))!=len(codes):raise ValueError('Keep existing language IDs stable and codes unique')
 if set(names)!=set(codes) or any(not names[c] for c in codes):raise ValueError('Every language needs its native name')
 rows={key:list(values) for key,values in base.items()}
 if any(len(values)!=2 for values in rows.values()):raise ValueError('Base dictionary must retain French/Spanish columns')
 for code in codes[3:]:
  translated=json.loads((ROOT/'locales'/f'{code}.json').read_text())
  if set(translated)!=set(base):raise ValueError(f'{code}: missing/extra translation keys: {set(base)^set(translated)}')
  for key in rows:rows[key].append(translated[key])
 for key,values in rows.items():
  for value in values:
   if not isinstance(value,str) or not value.strip():raise ValueError('Empty/non-text translation: '+key)
   if len(value.encode('utf-8'))>=193:raise ValueError('Translation exceeds native shaping buffer: '+key)
   if key.endswith(' ') and not value.endswith(' '):raise ValueError('Numeric prefix needs trailing space: '+key)
 rtl=data.get('rtl_languages',[])
 if not set(rtl)<=set(codes):raise ValueError('Unknown RTL language')
 return {'languages':codes,'language_names':names,'rtl_languages':rtl,'strings':rows}

def main():
 data=load();codes=data['languages'];literal=lambda s:json.dumps(s,ensure_ascii=False)
 h='/* Generated from locale JSON files by tools/build_translations.py. */\n'
 h+=f'enum {{UI_LANGUAGE_COUNT={len(codes)}}};\n'
 h+='static const char*const ui_language_codes[UI_LANGUAGE_COUNT]={'+','.join(map(literal,codes))+'};\n'
 h+='static const char*const ui_language_names[UI_LANGUAGE_COUNT]={'+','.join(literal(data['language_names'][c]) for c in codes)+'};\n'
 h+='static int ui_language_rtl(void){static const unsigned char flags[UI_LANGUAGE_COUNT]={'+','.join('1' if c in data['rtl_languages'] else '0' for c in codes)+'};return pref.language>=0&&pref.language<UI_LANGUAGE_COUNT?flags[pref.language]:0;}\n'
 h+='static const char*ui_language_name(void){return ui_language_names[pref.language>=0&&pref.language<UI_LANGUAGE_COUNT?pref.language:0];}\n'
 h+='static const char*ui_tr(const char*s){static const char*const rows[][UI_LANGUAGE_COUNT]={\n'
 for key,values in data['strings'].items():h+=' {'+','.join(map(literal,[key,*values]))+'},\n'
 h+=' };if(!s||pref.language<1||pref.language>=UI_LANGUAGE_COUNT)return s;for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);i++)if(streq(s,rows[i][0]))return rows[i][pref.language];return s;}\n'
 (ROOT/'clio843/src/ui_translations.h').write_text(h)
if __name__=='__main__':main()
