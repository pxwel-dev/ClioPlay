#!/usr/bin/env python3
"""Capture the current UI's remote actions and distinct states for the full demo.

Uses an existing Global map/atlas read-only. No device/phone/personal state is read.
Raw RGB565 captures and host binaries stay temporary; only PNGs and the gallery
catalog can be consumed by render_demo.py in a temporary directory. Intermediate
frames record each actual handler invocation, including loops/repeated presses.
"""
import argparse
import hashlib
import html
import json
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image, ImageDraw, ImageFont
from verify import decode
from text_engine import build as build_text

ROOT = Path(__file__).resolve().parents[1]
SCENES = []


def add(group, name, title, setup='', action='', check='', note=''):
    SCENES.append(dict(group=group, id=name, title=title, setup=setup,
                       action=action, check=check, note=note,
                       kind='remote' if action else 'state', remote_events=re.findall(r'key_handle\((K_\w+)\)',action)))


def key(group, name, title, setup, action, check='', note=''):
    add(group, name, title, setup, action, check, note)


def catalog(sender_receipts=None):
    SCENES.clear()
    add('Overview', 'driving', 'Driving with a route')
    for name, title, setup in [('explore','Explore map','explore();'), ('menu','Main menu','screen=SCR_MENU;'), ('trip-recents','Recent destinations','screen=SCR_TRIP;'), ('trip-favourites','Favourite destinations','screen=SCR_TRIP;trip_tab=1;'), ('itinerary','Current itinerary','screen=SCR_TRIP;trip_pane=1;'), ('phone','Phone connection','screen=SCR_BT;'), ('settings','Settings','screen=SCR_SET;'), ('keyboard','Name a favourite','name_keyboard();')]:
        add('Overview',name,title,setup)
    for i,title in enumerate(('Destinations','Bluetooth','Settings')):
        key('Menu',f'menu-focus-{i}',f'Select {title}', 'key_handle(K_MENU);', 'key_handle(K_DOWN);'*i, f'assert(menu_focus=={i});')
        key('Menu',f'menu-open-{i}',f'Open {title}', 'key_handle(K_MENU);'+'key_handle(K_DOWN);'*i, 'key_handle(K_ENTER);', f'assert(screen=={["SCR_TRIP","SCR_BT","SCR_SET"][i]});')
    key('Menu','menu-back','Back from menu to driving','screen=SCR_MENU;','key_handle(K_BACK);','assert(screen==SCR_MAP);')
    key('Menu','menu-up','Move menu selection up','screen=SCR_MENU;menu_focus=2;','key_handle(K_UP);','assert(menu_focus==1);')
    for page,name in [('SCR_TRIP','destinations'),('SCR_BT','phone'),('SCR_SET','settings'),('SCR_KEY','keyboard')]:
        key('Menu',f'{name}-map',f'MAP returns from {name}',f'screen={page};','key_handle(K_MAP);','assert(screen==SCR_MAP);')
        key('Menu',f'{name}-page-back',f'BACK leaves {name}',f'screen={page};','key_handle(K_BACK);')
    for name,code in [('up','K_UP'),('down','K_DOWN'),('left','K_LEFT'),('right','K_RIGHT')]:
        key('Map',f'pan-{name}',f'Pan Explore {name}','explore();',f'key_handle({code});')
    for free in (False,True):
        setup='explore();' if free else ''
        for name,code in [('in','K_SR'),('out','K_ESC')]:
            key('Map',f'zoom-{name}-'+('explore' if free else 'driving'),f'Zoom {name} in '+('Explore' if free else 'driving'),setup,f'key_handle({code});')
    key('Map','autozoom-override','Override automatic zoom','pref.autozoom=1;zoom_override=4;','key_handle(K_SR);','assert(zoom_override==5);')
    key('Map','autozoom-resume','Resume automatic zoom','pref.autozoom=1;zoom_override=4;','key_handle(K_ENTER);','assert(zoom_override==-1);')
    key('Map','map-menu','Open menu from driving','','key_handle(K_MENU);','assert(screen==SCR_MENU);')
    for mode in ('enter','leave'):
        count=6 if mode=='enter' else 7
        for frame in range(count):
            setup='' if mode=='enter' else 'explore();'
            action='key_handle(K_MAP);'+'freecam_orientation_anim_step();'*frame
            key('Animations',f'explore-{mode}-{frame:02}',f'Explore {mode} · frame {frame}',setup,action,note='Consecutive production orientation/tilt steps; 100 ms device cadence, not a measured host frame rate.')
    for name,code,expect in [('previous','K_LEFT','PREV'),('next','K_RIGHT','NEXT'),('pause','K_ENTER','PAUSE')]:
        key('Audio',f'transport-{name}',f'Phone audio · {name}','key_handle(K_BACK);',f'key_handle({code});',f'command_is(" {expect}");', 'The real handler queues a command in isolated host I/O; phone acknowledgement/audio output is simulated.')
    key('Audio','transport-play','Phone audio · play','btui.playing=0;key_handle(K_BACK);','key_handle(K_ENTER);','command_is(" PLAY");')
    key('Audio','transport-enter','Enter phone transport controls','','key_handle(K_BACK);','assert(bt_control_mode);')
    key('Audio','transport-leave','Leave phone transport controls','key_handle(K_BACK);','key_handle(K_BACK);','assert(!bt_control_mode);')
    for direction in ('K_UP','K_DOWN'):
        key('Audio','transport-ignore-'+direction.lower(),'Phone controls leave '+direction+' unused','key_handle(K_BACK);',f'key_handle({direction});','assert(bt_control_mode);')
    for name,title,setup in [('radio-fm','FM radio','set_band("FM");copystr(af.station,sizeof(af.station),"BBC Radio 1");copystr(af.freq,sizeof(af.freq),"98.8");'),('radio-am','AM radio','set_band("MW");copystr(af.freq,sizeof(af.freq),"909");'),('aux','AUX source','set_band("AUX");'),('audio-paused','Phone playback paused','btui.playing=0;'),('audio-long','Long track and artist','copystr(btui.title,sizeof(btui.title),"A long example track title to show the scrolling behaviour");copystr(btui.artist,sizeof(btui.artist),"Example artist with a long display name");')]:
        add('Audio',name,title,setup,note='Simulated vehicle/phone status; the radio/AUX source is changed by the vehicle, not an on-screen action.')
    for tab,name in [(0,'recents'),(1,'favourites')]:
        key('Destinations',f'{name}-tab',f'Switch to {name}',f'screen=SCR_TRIP;trip_tab={1-tab};','key_handle(K_RIGHT);',f'assert(trip_tab=={tab});')
        key('Destinations',f'{name}-last',f'Scroll to last {name}',f'screen=SCR_TRIP;trip_tab={tab};nrecents=nfaves=25;','for(int i=0;i<30;i++)key_handle(K_DOWN);','assert(trip_focus==24);')
        key('Destinations',f'{name}-up',f'Scroll {name} up',f'screen=SCR_TRIP;trip_tab={tab};trip_focus=2;','key_handle(K_UP);','assert(trip_focus==1);')
        add('Destinations',f'{name}-empty',f'Empty {name}',f'screen=SCR_TRIP;trip_tab={tab};nrecents=nfaves=0;')
    key('Destinations','itinerary-focus','Move focus to itinerary','screen=SCR_TRIP;','key_handle(K_SR);','assert(trip_pane==1);')
    key('Destinations','saved-focus','Move focus to saved places','screen=SCR_TRIP;trip_pane=1;','key_handle(K_ESC);','assert(trip_pane==0);')
    key('Destinations','itinerary-down','Select next itinerary item','screen=SCR_TRIP;trip_pane=1;','key_handle(K_DOWN);','assert(trip_right_focus==1);')
    key('Destinations','itinerary-up','Select previous itinerary item','screen=SCR_TRIP;trip_pane=1;trip_right_focus=1;','key_handle(K_UP);','assert(trip_right_focus==0);')
    add('Destinations','itinerary-full','Five-target itinerary','screen=SCR_TRIP;trip_pane=1;four_stops();')
    add('Destinations','itinerary-empty','No current route','screen=SCR_TRIP;trip_pane=1;trip.active=0;trip.count=0;')
    key('Destinations','empty-receive','Receive first destination from phone','screen=SCR_TRIP;trip_pane=1;trip.active=0;trip.count=0;','key_handle(K_ENTER);','assert(modal==MOD_WAIT_PHONE);')
    key('Destinations','full-add-disabled','Full itinerary: add-stop limit','screen=SCR_TRIP;trip_pane=1;four_stops();trip_right_focus=5;','key_handle(K_ENTER);','assert(modal==MOD_NONE&&trip.count==5);')
    # Every row in each saved-place and target action menu, plus its real outcome.
    for active in (False,True):
        for fav in (False,True):
            name='favourite' if fav else 'recent';actions=(['Add as stop'] if active else [])+['Route as new']+(['Rename favourite','Remove favourite'] if fav else ['Add to favourites'])
            setup=f'screen=SCR_TRIP;trip.active={int(active)};trip_tab={int(fav)};key_handle(K_ENTER);'
            for choice,title in enumerate(actions):
                state=setup+f'modal_choice={choice};'
                stem=f'{name}-'+('active' if active else 'idle')+f'-{choice}'
                add('Saved-place actions',stem+'-choice',f'{name.title()} · {title} selected',state)
                key('Saved-place actions',stem+'-result',f'{name.title()} · {title}',state,'key_handle(K_ENTER);')
    target_setup='screen=SCR_TRIP;three_stops();trip_pane=1;trip_right_focus=1;key_handle(K_ENTER);'
    for choice,title in enumerate(('Remove stop','Move stop up','Move stop down')):
        state=target_setup+f'modal_choice={choice};'
        add('Itinerary actions',f'stop-{choice}-choice',title+' selected',state)
        key('Itinerary actions',f'stop-{choice}-result',title,state,'key_handle(K_ENTER);')
    for choice,title in enumerate(('Change destination','Add destination to favourites')):
        state='screen=SCR_TRIP;trip_pane=1;trip_right_focus=1;key_handle(K_ENTER);'+f'modal_choice={choice};'
        add('Itinerary actions',f'destination-{choice}-choice',title+' selected',state)
        key('Itinerary actions',f'destination-{choice}-result',title,state,'key_handle(K_ENTER);')
    key('Itinerary actions','destination-unfavourite','Remove destination from favourites','screen=SCR_TRIP;faves[0].lat=trip.target[1].lat;faves[0].lon=trip.target[1].lon;trip_pane=1;trip_right_focus=1;key_handle(K_ENTER);modal_choice=1;','key_handle(K_ENTER);','assert(nfaves==3);')
    for change in (False,True):
        state='screen=SCR_TRIP;modal=MOD_SOURCE_PICK;dest_pick_action='+('DEST_PICK_CHANGE_DEST;' if change else 'DEST_PICK_ADD_STOP;')
        for choice,title in enumerate(('Recents','Favourites','Send from phone','Cancel')):
            stem=('change' if change else 'add')+f'-source-{choice}'
            setup=state+f'modal_choice={choice};'
            add('Choose destination',stem+'-choice',title+' · '+('change destination' if change else 'add stop'),setup)
            key('Choose destination',stem+'-result','Choose '+title,setup,'key_handle(K_ENTER);')
        for tab,name in ((0,'recents'),(1,'favourites')):
            key('Choose destination',('change' if change else 'add')+'-pick-'+name,('Replace destination' if change else 'Add stop')+' from '+name,f'screen=SCR_TRIP;trip_tab={tab};trip_pick_mode='+('DEST_PICK_CHANGE_DEST;' if change else 'DEST_PICK_ADD_STOP;'),'key_handle(K_ENTER);','assert(trip_pick_mode==DEST_PICK_NONE);')
    key('Saved-place actions','unnamed-recent-favourite','Name an unnamed map point as a favourite','screen=SCR_TRIP;copystr(recents[0].name,sizeof(recents[0].name),"MAP POINT");key_handle(K_ENTER);modal_choice=2;','key_handle(K_ENTER);','assert(screen==SCR_KEY);')
    key('Itinerary actions','unnamed-destination-favourite','Name an unnamed destination as a favourite','screen=SCR_TRIP;copystr(trip.target[1].name,sizeof(trip.target[1].name),"MAP POINT");trip_pane=1;trip_right_focus=1;key_handle(K_ENTER);modal_choice=1;','key_handle(K_ENTER);','assert(screen==SCR_KEY);')
    # Yes/no actions expose both selection states and confirmed outcomes.
    confirmations=[('map-route','Route to map point','explore();trip.active=0;key_handle(K_ENTER);'),('map-stop','Add map point as stop','explore();key_handle(K_ENTER);'),('cancel-route','Cancel current route','screen=SCR_TRIP;modal=MOD_CANCEL_ROUTE;'),('remove-stop','Remove itinerary stop','screen=SCR_TRIP;modal=MOD_REMOVE_STOP;modal_index=0;'),('resume-trip','Resume saved trip','screen=SCR_MAP;trip.paused=1;modal=MOD_RESUME_TRIP;')]
    for name,title,state in confirmations:
        for choice,answer in ((0,'Yes'),(1,'No')):
            setup=state+f'modal_choice={choice};'
            add('Confirmations',f'{name}-{choice}-choice',f'{title} · {answer} selected',setup)
            key('Confirmations',f'{name}-{choice}-result',f'{title} · {answer}',setup,'key_handle(K_ENTER);','assert(modal==MOD_NONE);')
    key('Confirmations','dismiss-dialog','Dismiss a destination dialog','screen=SCR_TRIP;key_handle(K_ENTER);','key_handle(K_BACK);','assert(modal==MOD_NONE);')
    key('Confirmations','resume-protected','Saved-trip prompt remains until answered','modal=MOD_RESUME_TRIP;','key_handle(K_BACK);key_handle(K_MAP);','assert(modal==MOD_RESUME_TRIP);')
    for stop in (True,False):
        name='stop' if stop else 'destination';state='screen=SCR_MAP;trip.paused=1;modal='+('MOD_ARRIVED_STOP;' if stop else 'MOD_ARRIVED_DEST;')
        for seconds in (5,1):
            add('Arrivals',f'arrived-{name}-{seconds}',f'Arrived at {name} · {seconds}-second countdown',state+f'arrival_modal_deadline=s_now_ms()+{seconds*1000};')
        key('Arrivals',f'arrived-{name}-continue',f'Confirm arrival at {name}',state,'key_handle(K_ENTER);','assert(modal==MOD_NONE);')
        add('Arrivals',f'arrived-{name}-timeout',f'Automatic arrival completion · {name}',state+'arrival_modal_deadline=s_now_ms()-1;arrival_modal_service();')
    for name,mode in [('new','DEST_PICK_NONE'),('stop','DEST_PICK_ADD_STOP'),('change','DEST_PICK_CHANGE_DEST')]:
        state=f'screen=SCR_TRIP;modal=MOD_WAIT_PHONE;phone_wait_action={mode};'
        add('Phone destinations',f'phone-wait-{name}','Waiting for phone · '+name,state)
        key('Phone destinations',f'phone-wait-{name}-back','Cancel waiting · '+name,state,'key_handle(K_BACK);','assert(modal==MOD_NONE);')
        add('Phone destinations',f'phone-received-{name}','Received destination · '+name,state+'incoming("Central Station",\'N\');',note='A validated incoming destination is injected at the daemon/UI boundary, then production phone_destination_apply() runs.')
    add('Phone destinations','incoming-native-name','Received native destination name','screen=SCR_TRIP;modal=MOD_WAIT_PHONE;phone_wait_action=DEST_PICK_CHANGE_DEST;incoming("東京駅 · محطة",\'N\');')
    add('Phone destinations','incoming-full','Phone stop rejected: itinerary full','screen=SCR_TRIP;four_stops();incoming("Extra stop",\'S\');')
    add('Phone destinations','incoming-no-route','Phone stop rejected: no active route','screen=SCR_TRIP;trip.active=0;trip.count=0;incoming("Extra stop",\'S\');')
    for choice,title,command in [(0,'Disconnect','DISCONNECT'),(1,'Forget connected phone','UNPAIR')]:
        state=f'screen=SCR_BT;bt_page_connected_choice={choice};'
        add('Phone connection',f'connected-{choice}-choice',title+' selected',state)
        key('Phone connection',f'connected-{choice}-result',title,state,'key_handle(K_ENTER);',f'command_is(" {command}");', 'The command file is checked; actual radio/phone response requires hardware.')
    for choice,title,command in [(0,'Reconnect saved phone','CONNECT'),(1,'Forget saved phone','UNPAIR')]:
        state='screen=SCR_BT;bt_page_focus=1;key_handle(K_ENTER);'+f'bt_page_dialog_choice={choice};'
        add('Phone connection',f'saved-phone-{choice}-choice',title+' selected',state)
        key('Phone connection',f'saved-phone-{choice}-result',title,state,'key_handle(K_ENTER);',f'command_is(" {command}");')
    key('Phone connection','saved-phone-dismiss','Dismiss saved-phone actions','screen=SCR_BT;bt_page_focus=1;key_handle(K_ENTER);','key_handle(K_BACK);','assert(bt_page_dialog<0);')
    key('Phone connection','pair-phone','Pair a new phone','screen=SCR_BT;bt_page_focus=bt_page_total()-1;','key_handle(K_ENTER);','command_is(" PAIR");')
    key('Phone connection','phone-last','Scroll saved phones to pairing row','screen=SCR_BT;','for(int i=0;i<20;i++)key_handle(K_DOWN);','assert(bt_page_focus==bt_page_total()-1);')
    for name,status in [('ready','BT READY'),('pairing','BT PAIR'),('waiting','BT WAIT'),('unavailable','BT DAEMON ERROR')]:
        add('Phone connection','phone-'+name,'Phone status · '+name,f'screen=SCR_BT;nbtdev=0;btui.connected=0;copystr(btui.status,sizeof(btui.status),"{status}");',note='Simulated daemon status, rendered by bt_status_text().')
    for name,title,state,action in [('letter','Type a letter','kbd_focus=0;','key_handle(K_ENTER);'),('digit','Type a digit','kbd_focus=26;','key_handle(K_ENTER);'),('space','Insert a space','copystr(kbd_text,sizeof(kbd_text),"HOME");','key_handle(K_SR);'),('delete','Delete a character','copystr(kbd_text,sizeof(kbd_text),"HOME");','key_handle(K_ESC);'),('backspace','Remote backspace','copystr(kbd_text,sizeof(kbd_text),"HOME");','key_handle(K_ESC);'),('save','Save a named favourite','copystr(kbd_text,sizeof(kbd_text),"RIVERSIDE");kbd_focus=38;','key_handle(K_ENTER);'),('back','Leave naming with the remote Back key','','key_handle(K_BACK);')]:
        key('Favourite keyboard','keyboard-'+name,title,'name_keyboard();'+state,action)
    for name,code in [('left','K_LEFT'),('right','K_RIGHT'),('up','K_UP'),('down','K_DOWN')]:
        key('Favourite keyboard','keyboard-focus-'+name,'Move keyboard focus '+name,'name_keyboard();kbd_focus=9;',f'key_handle({code});')
    key('Favourite keyboard','keyboard-rename','Rename a favourite','screen=SCR_KEY;keyboard_open_fave_rename(0);copystr(kbd_text,sizeof(kbd_text),"RENAMED");kbd_focus=38;','key_handle(K_ENTER);','assert(!strcmp(faves[0].name,"RENAMED"));')
    key('Favourite keyboard','keyboard-name-limit','Favourite naming limit','name_keyboard();copystr(kbd_text,sizeof(kbd_text),"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMN");kbd_focus=0;','key_handle(K_ENTER);')
    key('Favourite keyboard','keyboard-utf8-delete','Delete a whole native character','screen=SCR_KEY;keyboard_open_fave_rename(0);copystr(kbd_text,sizeof(kbd_text),"東京");','key_handle(K_ESC);','assert(!strcmp(kbd_text,"東"));')
    key('Favourite keyboard','keyboard-save-empty','Empty name stays in keyboard','name_keyboard();kbd_focus=38;','key_handle(K_ENTER);','assert(screen==SCR_KEY);')
    # Settings: row navigation, every discrete option, and meaningful numeric boundaries.
    for i,title in enumerate(('Brightness','Theme','Units','Auto zoom','Camera alerts','Clock','Debug log','Language')):
        key('Settings',f'settings-focus-{i}','Select '+title,'screen=SCR_SET;','key_handle(K_DOWN);'*i,f'assert(set_focus=={i});')
    key('Settings','settings-up','Move settings selection up','screen=SCR_SET;set_focus=7;','key_handle(K_UP);','assert(set_focus==6);')
    for value in (10,50,100):
        key('Settings',f'brightness-{value}',f'Brightness {value}%','screen=SCR_SET;pref.bright=50;', 'key_handle(K_LEFT);'*((50-value)//10) if value<50 else 'key_handle(K_RIGHT);'*((value-50)//10),f'assert(pref.bright=={value});')
    for value,title in enumerate(('Day','Night','Auto')):
        key('Settings',f'theme-{title.lower()}','Theme · '+title,'screen=SCR_SET;set_focus=1;pref.theme=THEME_DAY;','key_handle(K_RIGHT);'*value,f'assert(pref.theme=={value});', 'Changing theme changes the framebuffer palette; paired columns identify the starting theme, and can converge on the same explicit setting.')
    for value,title in enumerate(('Auto','Metric','Imperial')):
        key('Settings',f'units-{title.lower()}','Units · '+title,'screen=SCR_SET;set_focus=2;pref.units=0;','key_handle(K_RIGHT);'*value,f'assert(pref.units=={value});')
    for on in (False,True):
        key('Settings','autozoom-'+str(int(on)),'Auto zoom · '+('On' if on else 'Off'),f'screen=SCR_SET;set_focus=3;pref.autozoom={int(not on)};','key_handle(K_RIGHT);',f'assert(pref.autozoom=={int(on)});')
        key('Settings','debug-'+str(int(on)),'Debug log · '+('On' if on else 'Off'),f'screen=SCR_SET;set_focus=6;pref.debug={int(not on)};','key_handle(K_RIGHT);',f'assert(pref.debug=={int(on)});')
    for value,title in enumerate(('Off','Visual','Phone audio')):
        key('Settings',f'camera-setting-{value}','Camera alerts · '+title,'screen=SCR_SET;set_focus=4;pref.cameras=0;','key_handle(K_RIGHT);'*value,f'assert(pref.cameras=={value});')
    for on in (False,True):
        key('Settings','clock-'+str(int(on)),'Clock · '+('Automatic' if on else 'Manual'),f'screen=SCR_SET;set_focus=5;pref.clock_auto={int(not on)};','key_handle(K_ENTER);',f'assert(pref.clock_auto=={int(on)});')
    for name,setup,action in [('plus-quarter','pref.hoff=pref.moff=0;','key_handle(K_RIGHT);'),('minus-quarter','pref.hoff=pref.moff=0;','key_handle(K_LEFT);'),('lower-wrap','pref.hoff=-12;pref.moff=0;','key_handle(K_LEFT);'),('upper-wrap','pref.hoff=14;pref.moff=0;','key_handle(K_RIGHT);')]:
        key('Settings','clock-'+name,'Manual clock · '+name,'screen=SCR_SET;set_focus=5;'+setup,action)
    languages=json.loads((ROOT/'locales/ui.json').read_text())['languages']
    for i,code in enumerate(languages):
        key('Languages','language-'+code,'Language · '+code,'screen=SCR_SET;set_focus=7;pref.language=0;','key_handle(K_RIGHT);'*i,f'assert(pref.language=={i});')
    key('Languages','language-enter','Cycle language with ENTER','screen=SCR_SET;set_focus=7;','key_handle(K_ENTER);','assert(pref.language==1);')
    key('Languages','language-backward','Cycle language backwards','screen=SCR_SET;set_focus=7;','key_handle(K_LEFT);',f'assert(pref.language=={len(languages)-1});')
    states=[('gps-lost','Acquiring GPS','gps.fix=0;'),('route-calculating','Calculating route','route_status=R_CALC;'),('route-rerouting','Rerouting','route_status=R_CALC;route_rerouting=1;'),('route-error','No route available','route_status=R_UNREACHABLE;'),('no-route','Driving without a route','route_status=R_IDLE;route_n=0;trip.active=0;'),('limit-estimated','Estimated speed limit','roadctx.speed_flags=3;'),('limit-unknown','Unknown speed limit','roadctx.valid=0;roadctx.speed=0;'),('limit-variable','Variable speed limit','roadctx.speed=0;roadctx.speed_flags=8;'),('limit-unrestricted','Unrestricted speed context','roadctx.speed=0;roadctx.speed_flags=4;'),('limit-three-digit','Three-digit metric speed and limit','pref.units=1;gps.mph10=810;roadctx.speed=130;roadctx.speed_flags=0;'),('limit-metric','Metric speed and road limit','pref.units=1;roadctx.speed=50;roadctx.speed_flags=0;'),('limit-imperial','Imperial speed and road limit','pref.units=2;roadctx.speed=30;roadctx.speed_flags=2;'),('route-long-name','Long street name','copystr(roadctx.street,sizeof(roadctx.street),"A long example street name for fitting");'),('map-missing','Missing navigation data','mapok=0;'),('map-outside','Position outside installed coverage','gps.lat=track_lat=3500000;gps.lon=track_lon=-2000000;')]
    for name,title,setup in states:add('Navigation states',name,title,setup,note='Simulated GPS/road/route state rendered by production draw().')
    for kind,title in [(1,'Fixed speed camera'),(2,'Red-light camera'),(3,'Average-speed entry')]:
        add('Camera states',f'camera-{kind}',title,f'show_camera({kind},0);',note='Illustrative camera state at sample GPS coordinates; not a claim that a camera exists there.')
    add('Camera states','camera-average','Average-speed section in progress','show_camera(3,1);')
    add('Camera states','camera-overspeed','Camera limit exceeded','show_camera(1,0);gps.mph10=450;')
    add('Camera states','camera-hidden-gps','Camera alert suppressed after GPS loss','show_camera(1,0);gps.fix=0;')
    # Complete the actionable choices beyond the original overview catalog.
    for index, char in enumerate('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'):
        key('Every keyboard character','type-'+char,'Type '+char,
            f'name_keyboard();kbd_focus={index};','key_handle(K_ENTER);',
            f'assert(!strcmp(kbd_text,"{char}"));')
    for value in (20,30,40,60,70,80,90):
        key('Settings',f'brightness-{value}',f'Brightness {value}%',
            'screen=SCR_SET;pref.bright=50;',
            'key_handle(K_LEFT);'*((50-value)//10) if value<50 else 'key_handle(K_RIGHT);'*((value-50)//10),f'assert(pref.bright=={value});')
    for name,setup,action,check in [
        ('brightness-min','screen=SCR_SET;pref.bright=10;','K_LEFT','assert(pref.bright==10);'),
        ('brightness-max','screen=SCR_SET;pref.bright=100;','K_RIGHT','assert(pref.bright==100);'),
        ('theme-wrap-left','screen=SCR_SET;set_focus=1;pref.theme=0;','K_LEFT','assert(pref.theme==2);'),
        ('theme-wrap-right','screen=SCR_SET;set_focus=1;pref.theme=2;','K_RIGHT','assert(pref.theme==0);'),
        ('units-wrap-left','screen=SCR_SET;set_focus=2;pref.units=0;','K_LEFT','assert(pref.units==2);'),
        ('units-wrap-right','screen=SCR_SET;set_focus=2;pref.units=2;','K_RIGHT','assert(pref.units==0);'),
        ('cameras-wrap-left','screen=SCR_SET;set_focus=4;pref.cameras=0;','K_LEFT','assert(pref.cameras==2);'),
        ('cameras-wrap-right','screen=SCR_SET;set_focus=4;pref.cameras=2;','K_RIGHT','assert(pref.cameras==0);'),
        ('language-wrap-right','screen=SCR_SET;set_focus=7;pref.language=UI_LANGUAGE_COUNT-1;','K_RIGHT','assert(pref.language==0);'),
        ('language-wrap-enter','screen=SCR_SET;set_focus=7;pref.language=UI_LANGUAGE_COUNT-1;','K_ENTER','assert(pref.language==0);'),
        ('connected-choice-right','screen=SCR_BT;','K_RIGHT','assert(bt_page_connected_choice==1);'),
        ('connected-choice-left','screen=SCR_BT;bt_page_connected_choice=1;','K_LEFT','assert(bt_page_connected_choice==0);'),
        ('saved-phone-dialog','screen=SCR_BT;bt_page_focus=1;','K_ENTER','assert(bt_page_dialog>=0);'),
        ('saved-phone-right','screen=SCR_BT;bt_page_focus=1;key_handle(K_ENTER);','K_RIGHT','assert(bt_page_dialog_choice==1);'),
        ('saved-phone-left','screen=SCR_BT;bt_page_focus=1;key_handle(K_ENTER);bt_page_dialog_choice=1;','K_LEFT','assert(bt_page_dialog_choice==0);'),
        ('saved-phone-soft-back','screen=SCR_BT;bt_page_focus=1;key_handle(K_ENTER);','K_ESC','assert(bt_page_dialog<0);'),
        ('phone-up','screen=SCR_BT;bt_page_focus=2;','K_UP','assert(bt_page_focus==1);'),
        ('map-recenter','cam_lat+=100;cam_lon+=100;','K_ENTER',''),
        ('map-menu-from-explore','explore();','K_MENU','assert(screen==SCR_MENU);'),
        ('map-no-fix','gps.fix=track_valid=0;','K_MAP','assert(!freecam);'),
        ('trip-cancel-open','screen=SCR_TRIP;trip_pane=1;trip_right_focus=3;','K_ENTER','assert(modal==MOD_CANCEL_ROUTE);'),
        ('trip-add-open','screen=SCR_TRIP;trip_pane=1;trip_right_focus=2;','K_ENTER','assert(modal==MOD_SOURCE_PICK);'),
        ('trip-pick-back','screen=SCR_TRIP;trip_pick_mode=DEST_PICK_ADD_STOP;','K_BACK','assert(trip_pane==1&&trip_pick_mode==DEST_PICK_NONE);'),
        ('trip-pick-soft-back','screen=SCR_TRIP;trip_pick_mode=DEST_PICK_ADD_STOP;','K_ESC','assert(trip_pane==1&&trip_pick_mode==DEST_PICK_NONE);'),
        ('trip-pick-right-soft','screen=SCR_TRIP;trip_pick_mode=DEST_PICK_ADD_STOP;','K_SR','assert(trip_pane==1&&trip_pick_mode==DEST_PICK_NONE);')]:
        key('Additional controls',name,name.replace('-',' ').capitalize(),setup,f'key_handle({action});',check)
    for direction,code in [('up','K_UP'),('down','K_DOWN'),('left','K_LEFT'),('right','K_RIGHT')]:
        key('Dialog controls','dialog-'+direction,'Move action choice '+direction,
            'screen=SCR_TRIP;modal=MOD_SOURCE_PICK;modal_choice=1;',f'key_handle({code});')
    for direction,code in [('left','K_LEFT'),('right','K_RIGHT')]:
        key('Dialog controls','confirmation-'+direction,'Toggle confirmation '+direction,
            'screen=SCR_TRIP;modal=MOD_CANCEL_ROUTE;modal_choice=1;',f'key_handle({code});','assert(modal_choice==0);')
    for name,setup in [('source','modal=MOD_SOURCE_PICK;'),('waiting','modal=MOD_WAIT_PHONE;'),('saved-place','modal=MOD_RECENT;')]:
        key('Dialog controls','dialog-map-'+name,'MAP leaves '+name+' dialog','screen=SCR_TRIP;'+setup,'key_handle(K_MAP);','assert(screen==SCR_MAP&&modal==MOD_NONE);')
    for name,setup in [('stop','MOD_ARRIVED_STOP'),('destination','MOD_ARRIVED_DEST')]:
        key('Dialog controls','arrival-protected-'+name,'Arrival prompt awaits confirmation · '+name,
            f'modal={setup};','key_handle(K_BACK);key_handle(K_MAP);',f'assert(modal=={setup});')
    for name,setup in [('map-min','manual_zoom=mh.minlvl;'),('map-max','manual_zoom=mh.maxlvl;'),('explore-min','explore();freecam_zoom=mh.minlvl;'),('explore-max','explore();freecam_zoom=MAX_EXPLORE_ZOOM;')]:
        key('Map boundaries','zoom-boundary-'+name,'Zoom boundary · '+name,setup,
            'key_handle(K_ESC);' if name.endswith('min') else 'key_handle(K_SR);')
    for name,setup in [('disconnected','btui.connected=0;'),('no-track',''),('unavailable','btui.connected=0;copystr(btui.status,sizeof(btui.status),"BT DAEMON ERROR");screen=SCR_BT;')]:
        add('Bluetooth states','bt-header-'+name,'Bluetooth · '+name,
            'btui.title[0]=btui.artist[0]=btui.album[0]=0;'+setup)
    add('Camera states','camera-red-light-overspeed','Red-light alert ignores speed metadata','show_camera(2,0);gps.mph10=900;')
    add('Camera states','camera-red-light-metric','Red-light alert · metric distance','show_camera(2,0);pref.units=1;')
    add('Camera states','camera-limit-unknown','Speed camera without a known limit','show_camera(1,0);camera_current.speed=0;')
    add('Camera states','camera-limit-variable','Speed camera with a variable limit','show_camera(1,0);camera_current.flags=8;')
    # Short, everyday examples for the publication showcase.
    for name,title,setup,action in [
        ('showcase-theme-day','Choose the Day theme','screen=SCR_SET;set_focus=1;pref.theme=THEME_NIGHT;refresh_theme();','K_LEFT'),
        ('showcase-theme-auto','Automatic day/night theme','screen=SCR_SET;set_focus=1;pref.theme=THEME_NIGHT;refresh_theme();','K_RIGHT'),
        ('showcase-theme-night','Return to the Night theme','screen=SCR_SET;set_focus=1;pref.theme=THEME_AUTO;refresh_theme();','K_LEFT'),
        ('showcase-units-auto','Use automatic country units','screen=SCR_SET;set_focus=2;pref.units=2;','K_RIGHT'),
        ('showcase-language-en','Return to English','screen=SCR_SET;set_focus=7;pref.language=1;','K_LEFT'),
        ('showcase-camera-off','Turn camera alerts off','screen=SCR_SET;set_focus=4;pref.cameras=1;','K_LEFT'),
        ('showcase-camera-audio','Enable camera cues through phone audio','screen=SCR_SET;set_focus=4;pref.cameras=1;','K_RIGHT'),
        ('showcase-keyboard-char','Enter a character','screen=SCR_KEY;keyboard_open_fave_rename(0);kbd_focus=4;','K_ENTER'),
        ('showcase-keyboard-delete','Delete a character','screen=SCR_KEY;keyboard_open_fave_rename(0);appendstr(kbd_text,sizeof(kbd_text),"E");','K_ESC')]:
        key('Showcase controls',name,title,setup,f'key_handle({action});')
    key('Showcase controls','showcase-keyboard-space','Insert a space with the right soft key',
        'screen=SCR_KEY;keyboard_open_fave_rename(0);kbd_focus=4;', 'key_handle(K_SR);key_handle(K_ENTER);')
    key('Showcase controls','keyboard-language-open','Open keyboard languages',
        'screen=SCR_KEY;keyboard_open_fave_rename(0);kbd_focus=36;', 'key_handle(K_ENTER);', 'assert(kbd_picker);')
    key('Showcase controls','keyboard-language-french','Select the French keyboard',
        'screen=SCR_KEY;keyboard_open_fave_rename(0);kbd_focus=36;kbd_picker=1;kbd_pick=0;',
        'key_handle(K_DOWN);key_handle(K_ENTER);', 'assert(kbd_language==1&&!kbd_picker&&pref.language==0);')
    key('Showcase controls','keyboard-accent','Type a capital accented letter',
        'screen=SCR_KEY;pref_keyboard_language=1;keyboard_open_fave_rename(0);kbd_focus=30;',
        'key_handle(K_ENTER);', 'assert(strstr(kbd_text,"É"));')
    for receipt in sender_receipts or []:
        name=receipt['id'];mode='DEST_PICK_ADD_STOP' if receipt['action']=='S' else 'DEST_PICK_NONE'
        setup=f'screen=SCR_TRIP;modal=MOD_WAIT_PHONE;phone_wait_action={mode};'
        add('ClioPlay Send integration',f'sender-{name}-wait','Wait for '+name+' from ClioPlay Send',setup)
        payload=json.dumps(receipt['name'],ensure_ascii=False)
        add('ClioPlay Send integration',f'sender-{name}-result','Receive '+name+' from ClioPlay Send',
            setup+f'incoming_sender({payload},{receipt["seq"]},\'{receipt["action"]}\',{receipt["lat"]},{receipt["lon"]});',
            check='assert(modal==MOD_NONE);',note='Actual browser MediaSession packet decoded by the production daemon parser and received through production poll_phone_destination()/phone_destination_apply(); radio transport simulated.')
    return SCENES


HELPERS = r'''
static int gallery_night;
static int saved_n;static u32 saved_m,saved_cost;
static s32 saved_lat[ROUTE_PATH_MAX],saved_lon[ROUTE_PATH_MAX];
static u32 saved_node[ROUTE_PATH_MAX],saved_arc[ROUTE_PATH_MAX],saved_phys[ROUTE_PATH_MAX],saved_seg_m[ROUTE_PATH_MAX],saved_seg_cost[ROUTE_PATH_MAX];
static void save_route(void){saved_n=route_n;saved_m=route_result_m;saved_cost=route_result_cost;
 memcpy(saved_lat,route_lat,sizeof(saved_lat));memcpy(saved_lon,route_lon,sizeof(saved_lon));memcpy(saved_node,route_node,sizeof(saved_node));memcpy(saved_arc,route_seg_arc,sizeof(saved_arc));memcpy(saved_phys,route_seg_phys,sizeof(saved_phys));memcpy(saved_seg_m,route_seg_m,sizeof(saved_seg_m));memcpy(saved_seg_cost,route_seg_cost,sizeof(saved_seg_cost));}
static void restore_route(void){route_n=saved_n;route_head=0;route_result_m=saved_m;route_result_cost=saved_cost;route_status=R_READY;
 memcpy(route_lat,saved_lat,sizeof(saved_lat));memcpy(route_lon,saved_lon,sizeof(saved_lon));memcpy(route_node,saved_node,sizeof(saved_node));memcpy(route_seg_arc,saved_arc,sizeof(saved_arc));memcpy(route_seg_phys,saved_phys,sizeof(saved_phys));memcpy(route_seg_m,saved_seg_m,sizeof(saved_seg_m));memcpy(route_seg_cost,saved_seg_cost,sizeof(saved_seg_cost));}
static void explore(void){enter_freecam();for(int i=0;i<6;i++)freecam_orientation_anim_step();}
static void name_keyboard(void){screen=SCR_KEY;keyboard_open_fave_name(5497370,-161331);}
static void three_stops(void){trip.count=4;trip.active_idx=0;for(int i=0;i<3;i++){trip.target[i].lat=5497300+i*100;trip.target[i].lon=-161300-i*100;snprintf(trip.target[i].name,sizeof(trip.target[i].name),"Stop %d",i+1);}trip.target[3]=trip.target[0];copystr(trip.target[3].name,sizeof(trip.target[3].name),"Home");}
static void four_stops(void){three_stops();trip.count=5;trip.target[4]=trip.target[3];copystr(trip.target[3].name,sizeof(trip.target[3].name),"Stop 4");}
static void incoming(const char*name,char action){phone_dest.valid=1;phone_dest.action=action;phone_dest.lat=5497370;phone_dest.lon=-161331;copystr(phone_dest.name,sizeof(phone_dest.name),name);phone_destination_apply();}
static void incoming_sender(const char*name,int seq,char action,int lat,int lon){char packet[512];int n=snprintf(packet,sizeof(packet),"SEQ=%d\nACTION=%c\nLAT=%d\nLON=%d\nNAME=%s\n",seq,action,lat,lon,name);long fd=s_open(BT_NAV_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);assert(fd>=0);assert(s_write((int)fd,packet,n)==n);s_close((int)fd);poll_phone_destination();assert(phone_dest.valid);phone_destination_apply();}
static void show_camera(int kind,int avg){camera_visible=1;camera_average=avg;camera_current.kind=kind;camera_current.speed=30;camera_current.flags=2;camera_distance=avg?850:180;camera_average_speed=42;}
static void gallery_reset(int night){
 route_cancel();fixture();restore_route();atlas_active=0;atlas_future_m=atlas_future_cost=0;
 pref=(struct Prefs){50,1,0,0,0,night?THEME_NIGHT:THEME_DAY,0,1,0,0};apply_palette(night);
 mapok=1;screen=SCR_MAP;trip_pane=trip_tab=trip_focus=trip_right_focus=0;menu_focus=set_focus=0;
 bt_page_focus=bt_page_connected_choice=bt_page_dialog_choice=0;
 trip_pick_mode=dest_pick_action=phone_wait_action=DEST_PICK_NONE;modal_choice=0;modal_index=-1;pending_point_stop=0;
 memzero(&phone_dest,sizeof(phone_dest));phone_seq_seen_valid=0;arrival_modal_deadline=0;notice_deadline=0;
 nrecents=8;nfaves=4;copystr(faves[0].name,sizeof(faves[0].name),"Home");copystr(faves[1].name,sizeof(faves[1].name),"Work");for(int i=0;i<4;i++){faves[i].lat=5500000+i*300;faves[i].lon=-165000-i*300;}
 kbd_focus=0;kbd_action=KBD_NAME_FAVE;kbd_fave_idx=-1;kbd_text[0]=0;
 btui.album[0]=af.station[0]=af.freq[0]=0;copystr(btui.status,sizeof(btui.status),"BT READY");
 manual_zoom=freecam_zoom=5;zoom_override=-1;auto_zoom_level=-1;route_rerouting=0;
 ui_anim_tick=0;lastfix_valid=1;gps.hour=12;gps.min=35;gps.sec=0;gps.day=21;gps.mon=10;gps.year=2026;gps_fix_ms=s_now_ms();
 track_lat=gps.lat;track_lon=gps.lon;display_course=track_course=gps.course=70;
 roadctx.speed=30;roadctx.speed_flags=2;roadctx.reverse_speed=30;roadctx.reverse_flags=2;
 camera_clear();camera_next_ms=s_now_ms()+60000;clock_next_ms=0;region_next_ms=0;region_resolve(gps.lat,gps.lon);
}
'''


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()


def overview(out, scenes, theme):
    chosen=['splash','driving','explore','menu','trip-recents','itinerary','phone','settings','keyboard','camera-1','language-ar','language-zh-Hans']
    board=Image.new('RGB',(1000,6*308+12),(20,24,32));draw=ImageDraw.Draw(board)
    font=ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'),14)
    titles={s['id']:s['title'] for s in scenes};titles['splash']='Boot splash'
    for i,name in enumerate(chosen):
        x,y=10+(i%2)*500,10+(i//2)*308
        draw.text((x,y),titles[name],font=font,fill='white')
        with Image.open(out/theme/(name+'.png')) as im:board.paste(im,(x,y+23))
    board.save(out/('overview-'+theme+'.png'),optimize=True)


def write_gallery(out, manifest):
    groups=list(dict.fromkeys(s['group'] for s in manifest['scenes']))
    intro='Actual 480 × 272 RGB565 captures from the current C renderer and remote handler, with the native Unicode engine. UK map geometry and the baseline Newcastle route are real; GPS, saved places, vehicle, phone and camera states are controlled examples. These are host captures, not physical-head-unit photographs.'
    rows=[];md=['# ClioPlay framebuffer action gallery','',intro,'','## Coverage','',f"{len(manifest['scenes'])} named states/actions, each captured from day and night starting themes. Every menu, dialog type and settings row is covered; action results use the production remote handler. All 18 interface language selections are included.",'','Continuous values use representative examples and boundaries: map positions/zoom, brightness, manual clock offsets and text input. External events are injected at the UI boundary. Wireless pairing, radio/audio output, ignition/power, physical remote timing, real GPS and on-device performance require hardware validation. Camera examples do not identify real camera locations.','', 'Theme-selection actions can produce the same ending palette in both columns. Transport actions show queued commands; the phone response is separate. Explore sequences capture the actual animation steps. Other app openings are captured at their resulting layout; no extra animation is invented.','', 'Open [the searchable local HTML gallery](index.html) in a browser, or use the sections below. Explore animation previews: [day enter](explore-enter-day.gif), [day leave](explore-leave-day.gif), [night enter](explore-enter-night.gif), [night leave](explore-leave-night.gif). PNG frames are the pixel-accurate originals; GIF previews use a reduced palette.','', '![Day overview](overview-day.png)','', '![Night overview](overview-night.png)','']
    for group in groups:
        md += ['## '+group,'','| Action / state | Day start | Night start |','|---|---|---|']
        rows.append('<h2>'+html.escape(group)+'</h2>')
        for s in (s for s in manifest['scenes'] if s['group']==group):
            name=s['id'];title=s['title'];detail=s.get('note','')
            md.append('| '+title+' | [PNG](day/'+name+'.png) | [PNG](night/'+name+'.png) |')
            rows.append('<article data-group="'+html.escape(group)+'"><h3>'+html.escape(title)+'</h3><small>'+html.escape(s['kind']+(' · '+detail if detail else ''))+'</small><div class="pair">'+''.join('<figure data-theme="'+theme+'"><figcaption>'+theme.title()+' start</figcaption><a href="'+theme+'/'+name+'.png"><img loading="lazy" width="480" height="272" src="'+theme+'/'+name+'.png" alt="'+html.escape(title)+'"></a></figure>' for theme in ('day','night'))+'</div></article>')
        md.append('')
    (out/'README.md').write_text('\n'.join(md)+'\n')
    page='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ClioPlay framebuffer actions</title><style>body{margin:24px auto;padding:0 16px;max-width:1020px;background:#141820;color:#e7edf7;font:15px system-ui}p{line-height:1.5}a{color:#8bc5ff}header{position:sticky;top:0;background:#141820f5;padding:12px 0;z-index:1}input,select{font:inherit;padding:8px;margin:4px;max-width:90%;background:#243040;color:white;border:1px solid #59687c;border-radius:6px}.pair{display:flex;gap:20px;flex-wrap:wrap}figure{margin:10px 0}img{display:block;max-width:100%;height:auto}article{padding:12px 0 24px;border-bottom:1px solid #354052}small,figcaption{color:#b4c3d6}h2{margin-top:48px}h3{margin:10px 0}[hidden]{display:none!important}</style><h1>ClioPlay · framebuffer action gallery</h1><p>INTRO</p><p><a href="README.md">Coverage and hardware limitations</a> · <a href="manifest.json">Capture manifest</a></p><header><input id="search" type="search" placeholder="Find an action"><select id="group"><option value="">All sections</option>OPTIONS</select><select id="theme"><option value="">Both themes</option><option value="day">Day start</option><option value="night">Night start</option></select><span id="count"></span></header>ROWS<script>const cards=[...document.querySelectorAll('article')];function filter(){let q=document.querySelector('#search').value.toLowerCase(),g=document.querySelector('#group').value,t=document.querySelector('#theme').value,n=0;for(let c of cards){c.hidden=!!((g&&c.dataset.group!==g)||!c.textContent.toLowerCase().includes(q));if(!c.hidden)n++;for(let f of c.querySelectorAll('figure'))f.hidden=!!(t&&f.dataset.theme!==t)}for(let h of document.querySelectorAll('h2'))h.hidden=!cards.some(c=>!c.hidden&&c.dataset.group===h.textContent);document.querySelector('#count').textContent=n+' states / actions'}for(let e of document.querySelectorAll('input,select'))e.addEventListener('input',filter);filter();</script></html>'''
    (out/'index.html').write_text(page.replace('INTRO',html.escape(intro)).replace('OPTIONS',''.join('<option>'+html.escape(g)+'</option>' for g in groups)).replace('ROWS',''.join(rows)))


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--maps',type=Path,default=ROOT/'navigation-output')
    ap.add_argument('--output',type=Path,required=True,help='temporary capture directory (not the publication tree)')
    ap.add_argument('--sender-receipts',type=Path)
    ap.add_argument('--scene-ids',type=Path,help='JSON list of selected scene IDs')
    ap.add_argument('--themes',nargs='+',choices=('day','night'),default=['day','night'])
    ap.add_argument('--record-inputs',action='store_true',help='capture before/after every actual remote invocation for the full video')
    ap.add_argument('--frames-only',action='store_true',help='skip gallery pages and overview/GIF assets')
    args=ap.parse_args();maps=args.maps.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    scenes=catalog(json.loads(args.sender_receipts.read_text()) if args.sender_receipts else None);assert len({s['id'] for s in scenes})==len(scenes)
    if args.scene_ids:
        selected=set(json.loads(args.scene_ids.read_text()))-{'splash'}
        scenes=[s for s in scenes if s['id'] in selected]
        assert {s['id'] for s in scenes}==selected, 'Unknown selected scene'
    for theme in args.themes:(out/theme).mkdir(exist_ok=True)
    native_sources={str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'clio843/src').rglob('*') if p.is_file() and p.suffix in ('.c','.h','.cc') and 'recovered' not in p.parts}
    binaries={name:sha(ROOT/'clio843'/name) for name in ('clio040','cliospl','clioa2dp')}
    map_manifest=json.loads((maps/'build.json').read_text())
    before={str(p.relative_to(maps)):(p.stat().st_size,p.stat().st_mtime_ns) for p in maps.rglob('*') if p.is_file()}
    with tempfile.TemporaryDirectory(prefix='clioplay-gallery-') as temp:
        temp=Path(temp);host=temp/'host';(host/'clio843').mkdir(parents=True)
        (host/'clio843/maps').symlink_to(maps,target_is_directory=True);shutil.copyfile(ROOT/'clio843/font.bin',host/'clio843/font.bin')
        for theme in args.themes:
            (temp/theme).mkdir()
            (temp/theme/'steps').mkdir()
            if args.record_inputs:(out/theme/'steps').mkdir(exist_ok=True)
        harness=(ROOT/'tests/native.c').read_text().replace('int main(int argc,char**argv)','int unused_test_main(int argc,char**argv)').replace('#include "fixtures/route.h"','#include "'+str(ROOT/'tests/fixtures/route.h')+'"')
        cases='\n'.join(f'case {i}:{{{s["setup"]}gallery_record_start();{s["action"]}gallery_record=0;{s["check"]}break;}}' for i,s in enumerate(scenes))
        names=','.join(json.dumps(s['id']) for s in scenes)
        wrappers=r'''
#undef key_handle
static int gallery_record, gallery_step;
static const char*gallery_scene;
static void gallery_step_frame(const char*suffix){char name[240];snprintf(name,sizeof(name),"%s/steps/%s-%s",gallery_night?"night":"day",gallery_scene,suffix);draw();snapshot(name);}
static void gallery_record_start(void){gallery_step=0;if(GALLERY_INPUTS){gallery_step_frame("before");gallery_record=1;}}
static void key_handle(int code){gallery_production_key_handle(code);if(gallery_record){char suffix[24];snprintf(suffix,sizeof(suffix),"%03d",gallery_step);gallery_step_frame(suffix);printf("INPUT|%s|%s|%03d|%d\n",gallery_night?"night":"day",gallery_scene,gallery_step++,code);}}
'''.replace('GALLERY_INPUTS',str(int(args.record_inputs)))
        program='#define key_handle gallery_production_key_handle\n'+harness+HELPERS+wrappers+'''
int main(int argc,char**argv){assert(argc==2);outdir=argv[1];setbuf(stdout,0);fixture();assert(load_font());assert(cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf"));assert(load_regions());assert(load_timezones());assert(load_map());assert(load_route_data());assert(route_start(5497370,-161331));u32 deadline=s_now_ms()+120000;while(route_status==R_CALC&&time_before_u32(s_now_ms(),deadline))route_service_until(s_now_ms()+12);assert(route_status==R_READY&&route_n>1);save_route();printf("Baseline real route: %d points, %u m, %u ds\\n",saved_n,saved_m,saved_cost);static const char*names[]={NAMES};
for(int night=0;night<2;night++){THEME_FILTER gallery_night=night;for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++){gallery_reset(night);gallery_scene=names[i];switch(i){CASES}draw();char name[180];snprintf(name,sizeof(name),"%s/%s",night?"night":"day",names[i]);snapshot(name);printf("%s %s\\n",night?"night":"day",names[i]);}}return 0;}
'''.replace('NAMES',names).replace('CASES',cases).replace('THEME_FILTER',('if(night==0)continue;' if args.themes==['night'] else 'if(night==1)continue;' if args.themes==['day'] else ''))
        c=temp/'gallery.c';c.write_text(program)
        flags=['-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-Wno-unused-function','-DCLIO_UNICODE=1','-DCLIO_SOURCE="'+str(ROOT/'clio843/src/clio040.c')+'"','-DCLIO_HOST_ROOT="'+str(host)+'/"']
        obj=temp/'gallery.o';exe=temp/'gallery'
        subprocess.run(['cc',*flags,'-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['c++','-fsanitize=undefined',str(obj),str(build_text('host-pool')),'-o',str(exe)],check=True)
        result=subprocess.run([str(exe),str(temp)],text=True,capture_output=True,timeout=900)
        if result.returncode:raise RuntimeError(result.stdout[-2000:]+'\n'+result.stderr)
        splash=temp/'splash';subprocess.run(['cc',*[f for f in flags if f!='-DCLIO_UNICODE=1'],str(ROOT/'tests/splash.c'),'-o',str(splash)],check=True)
        subprocess.run([str(splash),str(temp)],check=True)
        frames={}
        for theme in args.themes:
            shutil.copyfile(temp/('splash-'+theme+'.rgb565'),temp/theme/'splash.rgb565')
            for capture in (temp/theme).rglob('*.rgb565'):
                raw=capture.read_bytes();png=(out/theme/capture.relative_to(temp/theme)).with_suffix('.png')
                try:
                    import numpy as np
                    v=np.frombuffer(raw,dtype='<u2').reshape(272,480)
                    rgb=np.empty((272,480,3),dtype=np.uint8)
                    for channel,shift,mask,left,right in ((0,11,31,3,2),(1,5,63,2,4),(2,0,31,3,2)):
                        c=(v>>shift)&mask;rgb[:,:,channel]=(c<<left)|(c>>right)
                    decoded=Image.fromarray(rgb)
                except ImportError:decoded=decode(raw)
                decoded.save(png,optimize=True)
                frames[str(png.relative_to(out))]={'rgb565_sha256':hashlib.sha256(raw).hexdigest(),'png_sha256':sha(png),'width':480,'height':272}
        capture_log=result.stdout.splitlines();print(capture_log[0]);print('Captured',len(frames),'frames.')
    scenes=[dict(group='Overview',id='splash',title='Boot splash',kind='state',note='Production cliospl framebuffer; grouped image and wordmark.')]+scenes
    manifest={'schema':'clioplay-framebuffer-gallery-1','renderer':'Production C draw(), key_handle(), cliospl; native Unicode engine; host UBSan','framebuffer':'480x272 RGB565 decoded to PNG without repainting the UI','map_source':map_manifest['source'],'baseline_route':capture_log[0],'native_sources':native_sources,'executables':binaries,'scenes':[{k:v for k,v in s.items() if k not in ('setup','action','check')} for s in scenes],'frames':frames,'hardware':'Not tested; external events are simulated. Remote commands and state invariants asserted in the harness.'}
    manifest['themes']=args.themes
    final_frames={theme+'/'+scene['id']+'.png' for theme in args.themes for scene in scenes}
    assert final_frames<=set(frames)
    code_names={1:'K_ESC',13:'K_BACK',14:'K_SR',28:'K_ENTER',103:'K_UP',105:'K_LEFT',106:'K_RIGHT',108:'K_DOWN',67:'K_MAP',68:'K_MENU'}
    inputs={}
    for line in capture_log:
        if not line.startswith('INPUT|'):continue
        _,theme,name,step,code=line.split('|');inputs.setdefault(theme+'/'+name,[]).append({'key':code_names[int(code)],'frame':f'{theme}/steps/{name}-{step}.png'})
    manifest['inputs']=inputs
    manifest['final_frames']=sorted(final_frames)
    if not args.record_inputs:assert len(frames)==len(final_frames)
    assert binaries=={name:sha(ROOT/'clio843'/name) for name in binaries}
    assert native_sources=={name:sha(ROOT/name) for name in native_sources}
    assert before=={str(p.relative_to(maps)):(p.stat().st_size,p.stat().st_mtime_ns) for p in maps.rglob('*') if p.is_file()}
    (out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    if not args.frames_only:
        for theme in ('day','night'):
            overview(out,scenes,theme)
            for mode,count in (('enter',6),('leave',7)):
                images=[Image.open(out/theme/f'explore-{mode}-{i:02}.png').copy() for i in range(count)]
                images[0].save(out/f'explore-{mode}-{theme}.gif',save_all=True,append_images=images[1:],duration=[100]*(count-1)+[600],loop=0)
        write_gallery(out,manifest)
    print('Captures:',out/'manifest.json')


if __name__=='__main__':main()
