#!/usr/bin/env python3
"""Capture the current UI's remote actions and distinct states.

Uses an existing Global map/atlas read-only. No device/phone/personal state is read.
Raw RGB565 captures and host binaries stay temporary; only PNGs and the gallery
catalog can be consumed by render_previews.py in a temporary directory. Intermediate
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

from mobility_stories import DESTINATION_EVENTS, MAP_DIVERSION_EVENTS, FREQUENT_PLACE_EVENTS

ROOT = Path(__file__).resolve().parents[1]
SCENES = []
ANIMATION_SAMPLE_MS = 50  # Match the runtime 20 Hz HUD and animation clock.
TWEEN_STEPS = tuple(range(0, 351, ANIMATION_SAMPLE_MS))
EXPLORE_SAMPLES = tuple(range(0, 568, ANIMATION_SAMPLE_MS))  # Include the first completed pose.
BLUETOOTH_EVENTS = (
    (0, 'set_volume(16);'),
    (650, 'key_handle(K_BACK);assert(bt_control_mode);'),
    (1800, ''),  # First volume notice expires.
    (2100, 'set_volume(18);btui.pos=95;'),
    (2770, 'key_handle(K_ENTER);command_is(" PAUSE");'),
    (3500, 'key_handle(K_BACK);assert(!bt_control_mode);'),
    (3900, ''),  # Second volume notice expires.
)
BLUETOOTH_SAMPLES = sorted({at + step for at, _ in BLUETOOTH_EVENTS for step in TWEEN_STEPS})
ROUTE_READY_SAMPLES = sorted(set(TWEEN_STEPS) | {500+step for step in TWEEN_STEPS} | {1200,1650})
PHONE_CALC_SAMPLES = tuple(range(0, 1300, 200))  # Whole UI targets 5 Hz while calculating.
MENU_RETURN_SAMPLES = (0, *(400 + step for step in TWEEN_STEPS))
MENU_ENTRY_SAMPLES = sorted({0, *(400+step for step in TWEEN_STEPS), *(950+step for step in TWEEN_STEPS), *(1400+step for step in TWEEN_STEPS)})
SPEED_WARNING_SAMPLES = (0, *range(400, 3401, ANIMATION_SAMPLE_MS))
ROUNDABOUT_ETA_SAMPLES = (0, *(600 + step for step in TWEEN_STEPS),
                        *(4800 + step for step in TWEEN_STEPS))
ZOOM_MODE_EVENTS = (
    (0, 'key_handle(K_SR);assert(zoom_override>=0);'),
    (1100, 'key_handle(K_MAP);assert(!freecam&&zoom_override==-1);'),
    (2200, 'key_handle(K_MAP);assert(freecam);'),
    (2700, ''),
    (4000, ''),
    (4500, 'key_handle(K_UP);key_handle(K_SR);assert(!hud_navigation.kind);'),
    (5200, 'key_handle(K_MAP);assert(!freecam);'),
)
ZOOM_MODE_SAMPLES = sorted({at+step for at in (0,1100,2200,4000,5200) for step in TWEEN_STEPS} |
                           {at+step for at in (2200,5200) for step in EXPLORE_SAMPLES} |
                           {4500,4700,4900,6000,6350})
POPUP_EVENTS = (
    (0, 'key_handle(K_ENTER);assert(modal==MOD_ROUTE_POINT);'),
    (600, 'key_handle(K_RIGHT);'),
    (1000, 'key_handle(K_BACK);'),
    (1500, 'menu_open(P_TRIP,trip.count,hud_now_ms());key_handle(K_ENTER);assert(sheet_page==P_SOURCE);'),
    (2100, 'key_handle(K_DOWN);key_handle(K_DOWN);'),
    (2500, 'key_handle(K_BACK);'),
    (3200, 'key_handle(K_MENU);'),
    (3700, 'key_handle(K_DOWN);key_handle(K_ENTER);key_handle(K_DOWN);key_handle(K_ENTER);assert(sheet_page==P_PHONE_ACTIONS);'),
    (4300, 'key_handle(K_DOWN);'),
    (4700, 'key_handle(K_BACK);'),
    (5200, 'keyboard_open_fave_rename(0);menu_open(P_NAME,0,hud_now_ms());pk_reset();pk_prepare();pk_focus=PK_LANGUAGE;key_handle(K_ENTER);assert(kbd_picker);'),
    (5800, 'key_handle(K_DOWN);key_handle(K_DOWN);'),
    (6300, 'key_handle(K_BACK);'),
)
POPUP_SAMPLES = sorted({at+step for at, _ in POPUP_EVENTS for step in TWEEN_STEPS} | {6900})

# The compact-sheet demos have corresponding map-free captures from the same
# production C renderer. With real map assets these scenes retain the map.
TRIPS_LAYOUT_EVENTS = (
    (0, 'menu_open(P_TRIP,0,hud_now_ms());'),
    (1200, 'key_handle(K_RIGHT);assert(sheet_page==P_PLACES&&places_tab==0);'),
    (2400, 'key_handle(K_RIGHT);assert(sheet_page==P_PLACES&&places_tab==1);'),
    (3600, 'key_handle(K_ENTER);assert(sheet_page==P_PLACE);'),
    (4700, 'key_handle(K_BACK);assert(sheet_page==P_PLACES);'),
    (5400, 'key_handle(K_LEFT);assert(places_tab==0);'),
    (6200, 'key_handle(K_LEFT);assert(sheet_page==P_TRIP);'),
    (7000, 'key_handle(K_DOWN);'),
    (7800, 'key_handle(K_ENTER);assert(sheet_page==P_DEST);'),
    (8600, 'key_handle(K_BACK);assert(sheet_page==P_TRIP);'),
    (9400, 'key_handle(K_DOWN);'),
    (9800, 'key_handle(K_ENTER);assert(sheet_page==P_SOURCE);'),
    (10600, 'key_handle(K_DOWN);'),
    (11000, 'key_handle(K_ENTER);assert(sheet_page==P_PLACES&&places_picker);'),
    (11600, 'key_handle(K_ENTER);assert(sheet_page==P_REVIEW);'),
)
TRIPS_LAYOUT_SAMPLES = sorted({at+step for at,_ in TRIPS_LAYOUT_EVENTS for step in TWEEN_STEPS} | {12800})
TRIPS_EMPTY_EVENTS = (
    (0, 'clear_trip();menu_open(P_TRIP,0,hud_now_ms());'),
    (1800, 'key_handle(K_ENTER);assert(sheet_page==P_COORD);'),
    (4200, 'key_handle(K_BACK);assert(sheet_page==P_TRIP);'),
)
TRIPS_EMPTY_SAMPLES = sorted({at+step for at,_ in TRIPS_EMPTY_EVENTS for step in TWEEN_STEPS}|{5900})
V220_DESTINATION_SAMPLES = sorted({at+step for at,_ in DESTINATION_EVENTS for step in TWEEN_STEPS}|{6000})
V220_FREQUENT_PLACE_SAMPLES = sorted({at+step for at,_ in FREQUENT_PLACE_EVENTS for step in TWEEN_STEPS}|{5600})
V220_DIVERSION_SAMPLES = sorted({
    at+step
    for at, _ in MAP_DIVERSION_EVENTS
    for step in TWEEN_STEPS
} | {5900})
POPUPS_LAYOUT_EVENTS = (
    (0, 'menu_open(P_TRIP,0,hud_now_ms());'),
    (900, 'confirm_kind=0;menu_change(P_CONFIRM,0,hud_now_ms());'),
    (2350, 'menu_close(hud_now_ms());modal=MOD_CANCEL_ROUTE;'),
    (4000, 'modal=MOD_SOURCE_PICK;dest_pick_action=DEST_PICK_ADD_STOP;'),
    (5400, 'modal=MOD_ROUTE_POINT;pending_point_stop=0;'),
    (6650, 'modal=MOD_NONE;keyboard_open_fave_name(5497370,-161331);menu_open(P_NAME,0,hud_now_ms());'),
    (7800, 'pk_focus=PK_LANGUAGE;key_handle(K_ENTER);assert(sheet_page==P_LANG&&kbd_picker);'),
    (9300, 'key_handle(K_BACK);assert(sheet_page==P_NAME);'),
)
POPUPS_LAYOUT_SAMPLES = sorted({at+step for at,_ in POPUPS_LAYOUT_EVENTS for step in TWEEN_STEPS}|{10500})


# ClioPlay 2.1.6 compact-screen and HUD customization animations.
V216_COORDINATES_EVENTS = ((0, 'clear_trip();menu_open(P_TRIP,0,hud_now_ms());'), (800, 'key_handle(K_ENTER);assert(sheet_page==P_COORD);'), (1400, 'coordinates.focus=4;key_handle(K_ENTER);'), (1650, 'coordinates.focus=3;key_handle(K_ENTER);'), (1900, 'coordinates.focus=COORD_DOT;key_handle(K_ENTER);'), (2150, 'coordinates.focus=8;key_handle(K_ENTER);'), (2400, 'coordinates.focus=6;key_handle(K_ENTER);'), (3000, 'key_handle(K_SR);assert(coordinates.step==COORD_LON);'), (3600, 'coordinates.focus=COORD_SIGN;key_handle(K_ENTER);'), (3800, 'coordinates.focus=0;key_handle(K_ENTER);'), (4000, 'coordinates.focus=COORD_DOT;key_handle(K_ENTER);'), (4200, 'coordinates.focus=5;key_handle(K_ENTER);'), (4600, 'key_handle(K_ESC);assert(streq(coordinates.value[1],"-1."));'), (5100, 'coordinates.focus=COORD_NEXT;key_handle(K_ENTER);assert(coordinates.error&&coordinates.step==COORD_LON);'), (6500, 'coordinates.focus=5;key_handle(K_ENTER);'), (7100, 'key_handle(K_SR);assert(coordinates.step==COORD_LAT);'), (7900, 'coordinates.focus=COORD_NEXT;key_handle(K_ENTER);assert(sheet_page==P_NAME);'), (9600, 'pk_focus=PK_SAVE;key_handle(K_ENTER);assert(sheet_page==P_REVIEW);'))
V216_COORDINATES_SAMPLES = sorted({at+step for at,_ in V216_COORDINATES_EVENTS for step in TWEEN_STEPS} | {11100})
V216_HUD_SETTINGS_EVENTS = ((0, 'menu_open(P_SETTINGS,1,hud_now_ms());'), (700, 'key_handle(K_ENTER);assert(sheet_page==P_SET_HUD);'), (1700, 'key_handle(K_RIGHT);assert(pref.top_swap);'), (3600, 'key_handle(K_DOWN);assert(settings_row(sheet_page,sheet_focus)==11);'), (4200, 'key_handle(K_RIGHT);assert(pref.bottom_swap);'), (6100, 'key_handle(K_MAP);assert(sheet_page==P_CLOSED);'), (8200, 'menu_open_setting(11,hud_now_ms());'))
V216_HUD_SETTINGS_SAMPLES = sorted({at+step for at,_ in V216_HUD_SETTINGS_EVENTS for step in TWEEN_STEPS} | {9900})
V216_BLUETOOTH_HIDDEN_EVENTS = ((0, 'menu_open(P_HOME,0,hud_now_ms());'), (2200, 'hw_bluetooth_enabled=0;menu_change(P_HOME,0,hud_now_ms());assert(menu_count(P_HOME)==2);'), (4400, 'key_handle(K_DOWN);key_handle(K_ENTER);assert(sheet_page==P_SETTINGS);'), (6600, 'key_handle(K_MENU);assert(sheet_page==P_HOME);'), (7500, 'hw_bluetooth_enabled=1;menu_change(P_HOME,1,hud_now_ms());'), (8500, 'key_handle(K_ENTER);assert(sheet_page==P_PHONE);'))
V216_BLUETOOTH_HIDDEN_SAMPLES = sorted({at+step for at,_ in V216_BLUETOOTH_HIDDEN_EVENTS for step in TWEEN_STEPS} | {9500})
# Keep Add stop enabled so the footer can show both wrap directions.
ITINERARY_SETUP = 'for(int i=trip.count;i<TRIP_MAX_TARGETS-1;i++){char name[32];snprintf(name,sizeof(name),"Stop %d",i);assert(trip_add_stop(name,5498000+i*100,-164000-i*100));}menu_open(P_TRIP,0,hud_now_ms());'
V216_ITINERARY_EVENTS = ((0, ITINERARY_SETUP),) + tuple(
    (1000+i*500, 'key_handle(K_DOWN);assert(sheet_focus==%d);' % (i+1)) for i in range(9)) + (
    (5750, 'key_handle(K_RIGHT);assert(sheet_focus==trip.count+1);'),
    (6400, 'key_handle(K_RIGHT);assert(sheet_focus==trip.count);'),
    (7100, 'key_handle(K_LEFT);assert(sheet_focus==trip.count+1);'),
    (7800, 'key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==1);'),
    (8800, 'key_handle(K_BACK);assert(sheet_page==P_TRIP&&trip.active);'))
V216_ITINERARY_SAMPLES = sorted({at+step for at,_ in V216_ITINERARY_EVENTS for step in TWEEN_STEPS} | {9800})

# ClioPlay 2.2.3 real-map feature previews (Newcastle upon Tyne).
V223_NEARBY_EVENTS = (
    (700, 'key_handle(K_MENU);assert(sheet_page==P_HOME);'),
    (1300, 'key_handle(K_ENTER);assert(sheet_page==P_TRIP);'),
    (2100, 'key_handle(K_LEFT);assert(sheet_page==P_NEARBY&&sheet_focus==0);'),
    (3000, 'key_handle(K_DOWN);assert(sheet_focus==1);'),
    (3500, 'key_handle(K_DOWN);assert(sheet_focus==2);'),
    (4200, 'key_handle(K_ENTER);assert(sheet_page==P_NEARBY_LIST&&nearby_category==NEARBY_CAT_HOSPITAL&&nearby_count>0);'),
    (5600, 'key_handle(K_BACK);assert(sheet_page==P_NEARBY&&sheet_focus==2);'),
    (6200, 'key_handle(K_UP);key_handle(K_UP);assert(sheet_focus==0);'),
    (6800, 'key_handle(K_ENTER);assert(sheet_page==P_NEARBY_LIST&&nearby_category==NEARBY_CAT_FUEL&&nearby_count>0);'),
    (8100, 'key_handle(K_ENTER);assert(sheet_page==P_PLACE&&place_from_nearby);'),
    (8900, 'key_handle(K_DOWN);assert(sheet_focus==1);'),
    (9500, 'key_handle(K_ENTER);assert(sheet_page==P_TRIP&&trip.count==3&&streq(trip.target[1].name,nearby_pois[0].name));'),
)
V223_NEARBY_SAMPLES = sorted({0} | {at+step for at,_ in V223_NEARBY_EVENTS for step in TWEEN_STEPS} | {10900})

V223_SECONDARY_MANOEUVRE_EVENTS = (
    (1100, 'preview_chained_step(10);assert(nav_man_type==MAN_SLIGHT_RIGHT&&nav_next_type==MAN_ROUNDABOUT&&nav_next_exit==3);'),
    (2100, 'preview_chained_step(14);'),
    (2900, 'preview_chained_step(18);'),
    (3700, 'preview_chained_step(22);'),
    (4500, 'preview_chained_step(25);assert(nav_man_type==MAN_ROUNDABOUT&&nav_man_exit==3&&nav_next_type==MAN_NONE);'),
    (5300, 'preview_chained_step(33);'),
    (6100, 'preview_chained_step(45);assert(nav_man_type==MAN_ROUNDABOUT&&nav_next_type==MAN_RIGHT);'),
    (6900, 'preview_chained_step(48);assert(nav_man_type==MAN_ROUNDABOUT&&nav_next_type==MAN_RIGHT);'),
)
V223_SECONDARY_MANOEUVRE_SAMPLES = sorted({0} | {at+step for at,_ in V223_SECONDARY_MANOEUVRE_EVENTS for step in TWEEN_STEPS} | {7700})

V223_VIEW_DIVERSION_EVENTS = (
    (1200, 'hud_show_driving_info();assert(hud_navigation.kind==HUD_JOURNEY&&traffic_warn&&!traffic_offer_valid());'),
    (2600, 'preview_map_diversion_reveal();assert(traffic_offer_valid()&&ml_offer_kind==0);'),
    (4400, 'draw();assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3&&confirm_diversion_kind==0&&sheet_focus==0);'),
    (5600, 'key_handle(K_DOWN);assert(sheet_page==P_CONFIRM&&sheet_focus==2);'),
    (6400, 'key_handle(K_ENTER);assert(sheet_page==P_CLOSED&&freecam&&diversion_preview_active);'),
    (7700, 'key_handle(K_SR);assert(freecam&&diversion_preview_active);'),
    (8500, 'key_handle(K_SR);assert(freecam&&diversion_preview_active);'),
    (9600, 'key_handle(K_MAP);assert(!freecam&&!diversion_preview_active&&sheet_page==P_CONFIRM&&confirm_kind==3&&sheet_focus==0);'),
    (11000,
     'int expected_n=route_n+traffic_offer_n-1-'
     '(traffic_join_idx-traffic_begin_idx);'
     'key_handle(K_ENTER);'
     'assert(mapok&&route_status==R_READY&&route_n==expected_n'
     '&&!traffic_offer_valid()&&streq(notice_text,"Diversion selected"));'),
    (12300, 'hud_show_driving_info();assert(hud_navigation.kind==HUD_JOURNEY&&!traffic_warn);'),
)
V223_VIEW_DIVERSION_SAMPLES = sorted({0} | {at+step for at,_ in V223_VIEW_DIVERSION_EVENTS for step in TWEEN_STEPS} | {13600})

V224_ML_ETA_HABITS_EVENTS = (
    (1100, 'hud_show_driving_info();assert(hud_navigation.kind==HUD_JOURNEY);'),
    (3400, 'draw();assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3&&confirm_diversion_kind==1&&ml_offer_kind==1&&sheet_focus==0);'),
    (5500,
     'int expected_n=route_n+traffic_offer_n-1-'
     '(traffic_join_idx-traffic_begin_idx);'
     'key_handle(K_ENTER);'
     'assert(mapok&&route_status==R_READY&&route_n==expected_n'
     '&&!traffic_offer_valid()&&streq(notice_text,"Usual route selected"));'),
)
V224_ML_ETA_HABITS_SAMPLES = sorted({0} | {at+step for at,_ in V224_ML_ETA_HABITS_EVENTS for step in TWEEN_STEPS} | {7500})

V224_ML_SLOWDOWNS_EVENTS = (
    (900,  'preview_ml_slowdowns_step(5);assert(hud_navigation.kind==HUD_SLOWDOWN&&!ml_slowdown_cold);'),
    (2400, 'preview_ml_slowdowns_step(11);assert(hud_navigation.kind==HUD_SLOWDOWN&&!ml_slowdown_cold);'),
    (5000, 'preview_ml_slowdowns_step(30);'),
    (5800, 'preview_ml_slowdowns_step(57);assert(hud_navigation.kind==HUD_NONE);'),
    (6600, 'car.temp=1;preview_ml_slowdowns_step(60);assert(hud_navigation.kind==HUD_SLOWDOWN&&ml_slowdown_cold);'),
    (8200, 'preview_ml_slowdowns_step(65);assert(hud_navigation.kind==HUD_SLOWDOWN&&ml_slowdown_cold);'),
    (9300, 'hud_show_driving_info();assert(hud_navigation.kind==HUD_JOURNEY);'),
)
V224_ML_SLOWDOWNS_SAMPLES = sorted({0} | {at+step for at,_ in V224_ML_SLOWDOWNS_EVENTS for step in TWEEN_STEPS} | {10700})

V224_ML_ADAPTIVE_HUD_EVENTS = (
    (1200, 'key_handle(K_SR);assert(active_zoom()==6);for(int i=0;i<3;i++){memzero(ml_zoom_nudged,sizeof(ml_zoom_nudged));ml_note_zoom_override(260,1);}assert(ml_autozoom_offset(260)==1);'),
    (2600, 'key_handle(K_MAP);assert(zoom_override<0&&active_zoom()==6);'),
    (4000, 'gps.mph10=460;render_background_dirty=1;assert(active_zoom()==4);'),
    (5300, 'menu_open_setting(0,hud_now_ms());assert(sheet_page==P_SET_DISPLAY&&theme_resolved==0&&pref.bright==90);'),
    (7000, 'gps.hour=20;gps.min=5;refresh_theme();render_background_dirty=1;assert(theme_resolved==1&&pref.bright==30);'),
    (8800, 'key_handle(K_MAP);assert(screen==SCR_MAP&&theme_resolved==1&&pref.bright==30);'),
)
V224_ML_ADAPTIVE_HUD_SAMPLES = sorted({0} | {at+step for at,_ in V224_ML_ADAPTIVE_HUD_EVENTS for step in TWEEN_STEPS} | {10000})

V224_ML_PARKING_POIS_EVENTS = (
    (1200, 'draw();assert(hud_parking_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==6&&sheet_focus==0);'),
    (2800, 'key_handle(K_ENTER);preview_ml_finish_route();assert(screen==SCR_MAP&&!ml_park_valid&&ml_stop_valid&&route_status==R_READY);'),
    (4400, 'draw();assert(hud_stop_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==7&&sheet_focus==0);'),
    (6000, 'key_handle(K_ENTER);preview_ml_finish_route();assert(screen==SCR_MAP&&trip.count==2&&streq(trip.target[0].name,"Coffee Stop"));'),
    (7100, 'key_handle(K_MENU);assert(sheet_page==P_HOME);'),
    (7600, 'key_handle(K_ENTER);assert(sheet_page==P_TRIP);'),
    (8100, 'key_handle(K_LEFT);assert(sheet_page==P_NEARBY&&sheet_focus==0);'),
    (8700, 'key_handle(K_ENTER);assert(sheet_page==P_NEARBY_LIST&&nearby_category==NEARBY_CAT_FUEL&&nearby_count>0&&nearby_pois[0].preferred);'),
)
V224_ML_PARKING_POIS_SAMPLES = sorted({0} | {at+step for at,_ in V224_ML_PARKING_POIS_EVENTS for step in TWEEN_STEPS} | {10400})



def add(group, name, title, setup='', action='', check='', note=''):
    SCENES.append(dict(group=group, id=name, title=title, setup=setup,
                       action=action, check=check, note=note,
                       kind='remote' if action else 'state', remote_events=re.findall(r'key_handle\((K_\w+)\)',action)))


def key(group, name, title, setup, action, check='', note=''):
    add(group, name, title, setup, action, check, note)


def motion(group, name, title, setup, events, samples):
    for elapsed in samples:
        source, previous = setup, 0
        for at, action in events:
            if at > elapsed:
                break
            source += f'hud_capture_ms+={at-previous}u;{action}draw();'
            previous = at
        add(group, f'{name}-{elapsed:04d}', f'{title} · {elapsed} ms',
            source + f'hud_capture_ms+={elapsed-previous}u;gallery_motion=1;')


def catalog(sender_receipts=None):
    SCENES.clear()
    add('Overview', 'driving', 'Driving with a route')
    for elapsed in (*range(0, 501, ANIMATION_SAMPLE_MS), 900):
        add('Overview', f'boot-fade-{elapsed:04d}', f'Boot to UI · {elapsed} ms',
            f'boot_preview({elapsed});gallery_motion=1;')
    for name, setup, steps in (
        ('recents','screen=SCR_TRIP;trip_pane=0;trip_tab=0;nrecents=25;',24),
        ('favourites','screen=SCR_TRIP;trip_pane=0;trip_tab=1;nfaves=25;',24),
        ('phones','screen=SCR_BT;bt_page_focus=0;',20),
        ('settings','menu_open_setting(0,hud_now_ms());',9),
        ('languages','screen=SCR_KEY;keyboard_open_fave_rename(0);kbd_picker=1;kbd_pick=0;',17),
    ):
        add('Scrollbars',f'scrollbar-{name}-top',name+' · first page',setup)
        key('Scrollbars',f'scrollbar-{name}-bottom',name+' · last page',setup,'key_handle(K_DOWN);'*steps)

    motion('Popup animations', 'preview-popups', 'Glass confirmations, animated choices, resizing and dismissal',
           'preview_enter_explore();settle();', POPUP_EVENTS, POPUP_SAMPLES)
    status_events = (
        (0, 'gps.fix=0;'),
        (800, 'set_volume(16);'),
        (1600, 'show_phone_connection();'),
        (2600, 'gps.fix=1;'),
        (6500, 'trip.active=0;route_status=R_IDLE;route_n=0;'),
        (7000, 'set_volume(18);'),
    )
    motion('Status notifications', 'preview-status', 'Shared GPS, phone, volume and route status',
           'settle();hud_capture_ms+=4500u;draw();hud_capture_ms+=320u;draw();',
           status_events, sorted({at+step for at, _ in status_events for step in TWEEN_STEPS}|
                                 {4400, 4800, 5400, 6100, 6200, 6280, 6360, 6440, 8600}))

    metadata_events = (
        (0, 'preview_track("After Hours","ClioPlay demo");'),
        (900, 'key_handle(K_BACK);assert(bt_control_mode);'),
        (1700, 'preview_track("City Lights","Another artist");'),
        (2900, 'key_handle(K_BACK);set_band("FM");copystr(af.station,sizeof(af.station),"RADIO 1");preview_frequency("98.8");'),
        (4300, 'preview_frequency("99.2");'),
        (8500, ''),
    )
    motion('Audio metadata', 'preview-metadata', 'Song changes and tuning with an unchanged station name',
           'preview_track("Previous track","ClioPlay demo");hud_connection.kind=hud_source.kind=HUD_NONE;settle();hud_capture_ms+=4500u;draw();hud_capture_ms+=320u;draw();',
           metadata_events, sorted({at+step for at, _ in metadata_events for step in TWEEN_STEPS}|{700,2600,3900,7200,9200}))

    add('Status notifications','menu-status','Action confirmation in the menu header',
        'screen=SCR_TRIP;notice_success(ui_tr("Favourite saved"));')
    add('Status notifications','menu-status-volume','Action confirmation and volume above Settings',
        'screen=SCR_SET;set_volume(18);notice_fail(ui_tr("Stop not added"));')
    add('Status notifications','menu-status-calculating','Moving route progress and status above Trip',
        'screen=SCR_TRIP;route_status=R_CALC;notice_success(ui_tr("Stop added"));')
    add('Playback progress','hud-playback','Track progress above Bluetooth controls',
        'key_handle(K_BACK);btui.pos=65;btui.dur=180;draw();hud_capture_ms+=400u;')
    add('Playback progress','hud-playback-unknown','Controls with no reported track duration',
        'key_handle(K_BACK);btui.pos=-1;btui.dur=0;')
    add('Playback progress','hud-playback-calc','Playback progress and routing progress remain distinct',
        'key_handle(K_BACK);btui.pos=65;btui.dur=180;route_status=R_CALC;set_volume(18);show_phone_connection();draw();hud_capture_ms+=400u;')
    motion('Map', 'preview-zoom-modes', 'Manual zoom, MAP restores Auto, and Explore notices occur once',
           'settle();pref.autozoom=1;zoom_override=-1;auto_zoom_level=5;hud_capture_ms+=4500u;draw();hud_capture_ms+=350u;draw();',
           ZOOM_MODE_EVENTS, ZOOM_MODE_SAMPLES)
    add('Audio sources','source-aux-unpaired','AUX without a phone',
        'set_band("AUX");btui.connected=btui.playing=0;btui.title[0]=btui.artist[0]=btui.album[0]=0;bt_source_service();')
    add('Audio sources','source-aux-phone','AUX remains wired AUX with a connected phone',
        'set_band("AUX");bt_source_service();')
    add('Audio sources','source-bt-default','Default BT with no readable radio or phone',
        'set_band("");btui.connected=btui.playing=0;btui.title[0]=btui.artist[0]=btui.album[0]=0;hud_source.kind=HUD_NONE;bt_source_service();')
    add('Audio sources','source-bt-independent','BT controls without readable radio source or volume',
        'set_band("");car.volume[0]=0;hud_source.kind=HUD_NONE;bt_source_service();key_handle(K_BACK);')
    add('Prototype HUD','hud-volume','Volume adjustment','set_volume(16);')
    add('Prototype HUD','hud-volume-ended','Volume notice cleared','settle();hud_capture_ms+=4300;')
    add('Prototype HUD','hud-route-volume','Volume during route calculation','route_status=R_CALC;set_volume(16);')
    add('Prototype HUD','hud-source','Source change','set_band("FM");copystr(af.station,sizeof(af.station),"BBC Radio 1");copystr(af.freq,sizeof(af.freq),"98.8");rebuild_source();')
    add('Prototype HUD','hud-eta','Route overview','key_handle(K_ENTER);')
    add('Prototype HUD','hud-media-volume','Volume during Bluetooth control','key_handle(K_BACK);set_volume(16);')
    add('Prototype HUD','hud-media-calc','Route calculation, Bluetooth control and volume','key_handle(K_BACK);route_status=R_CALC;set_volume(16);')
    add('Prototype HUD','hud-road-centred','Centred road notice without guidance','trip.active=0;route_status=R_IDLE;')
    add('Prototype HUD','hud-road-expired','Road notice expires','settle();hud_capture_ms+=4400;')
    add('Prototype HUD','hud-estimated-limit','Estimated limit has no speed pulse','roadctx.speed_flags|=1;gps.mph10=390;')
    add('Prototype HUD','hud-clock-only','Clock before temperature is available','car_read_temperature("temp=999");')
    add('Prototype HUD','hud-clock-temperature','Clock and validated temperature','car_read_temperature("temp=-4");')
    add('Prototype HUD','hud-phone-connected','Named phone connection on the Bluetooth page',
        'screen=SCR_BT;show_phone_connection();')
    add('Prototype HUD','hud-phone-route-volume','Phone connection alongside route progress, volume and Bluetooth controls',
        'key_handle(K_BACK);route_status=R_CALC;set_volume(18);show_phone_connection();copystr(btui.title,sizeof(btui.title),"Night Drive");')
    add('Prototype HUD','hud-setting-changed','Brightness updates in its row','menu_open_setting(0,hud_now_ms());key_handle(K_RIGHT);')
    add('Prototype HUD','hud-menu-audio-connection','Audio and connection notices stay above settings',
        'screen=SCR_SET;set_band("FM");copystr(af.station,sizeof(af.station),"BBC Radio 1");rebuild_source();set_volume(18);show_phone_connection();')
    for elapsed in (0,80,160,240,320):
        add('Prototype motion',f'hud-temperature-{elapsed:04d}',f'Temperature arrives · {elapsed} ms',
            f'car_read_temperature("temp=--");settle();car_read_temperature("temp=-4");draw();hud_capture_ms+={elapsed}u;gallery_motion=1;')
    for elapsed in (0,80,160,240,320,1750,1800,1880,1960,2040,2120):
        # Establish expiration at 1800 ms, then advance its existing exit tween.
        step = 'hud_capture_ms+=1800u;draw();' if elapsed >= 1800 else ''
        delta = elapsed-1800 if elapsed >= 1800 else elapsed
        add('Prototype motion',f'hud-volume-{elapsed:04d}',f'Volume notice · {elapsed} ms',
            f'settle();set_volume(16);draw();{step}hud_capture_ms+={delta}u;gallery_motion=1;')
    for elapsed in (0,80,160,240,320,440,600,760,920,1080,1260,1440,1620,1800,1980,2160):
        add('Prototype motion',f'hud-speed-{elapsed:04d}',f'Confirmed limit · {elapsed} ms',
            f'settle();gps.mph10=390;draw();hud_capture_ms+={elapsed}u;gallery_motion=1;')
    road_cleared = 'settle();hud_capture_ms+=4000u;draw();hud_capture_ms+=320u;draw();'
    new_road = 'copystr(roadctx.street,sizeof(roadctx.street),"Sandgate");'
    camera_on = 'show_camera(1,0);'
    road_sequences = (
        ('road-only', 'New road only', ((100, new_road), (4100, ''))),
        ('camera-only', 'Camera only', ((100, camera_on), (1700, 'camera_clear();'))),
        ('camera-then-road', 'Camera, then new road', ((100, camera_on), (1100, new_road), (5100, ''))),
        ('road-then-camera', 'New road, then camera', ((100, new_road), (1100, camera_on), (4100, ''))),
    )
    for name, title, events in road_sequences:
        samples = [0] + [at + delta for at, _ in events for delta in TWEEN_STEPS]
        motion('HUD motion', f'hud-{name}', title, road_cleared, events, samples)
    motion('HUD motion', 'hud-bluetooth', 'Bluetooth controls and volume', road_cleared,
           BLUETOOTH_EVENTS, BLUETOOTH_SAMPLES)
    motion('HUD motion', 'hud-speed-warning', 'Speeding warning', road_cleared,
           ((400, 'gps.mph10=390;assert(hud_is_overspeed());'),
            (2640, 'gps.mph10=240;assert(!hud_is_overspeed());')), SPEED_WARNING_SAMPLES)
    for name, title, setup in [('explore','Explore map','preview_enter_explore();'), ('menu','Main menu','screen=SCR_MENU;'), ('trip-recents','Recent destinations','screen=SCR_TRIP;'), ('trip-favourites','Favourite destinations','screen=SCR_TRIP;trip_tab=1;'), ('itinerary','Current itinerary','screen=SCR_TRIP;trip_pane=1;'), ('phone','Phone connection','screen=SCR_BT;'), ('settings','Settings','screen=SCR_SET;'), ('keyboard','Name a favourite','name_keyboard();')]:
        add('Overview',name,title,setup)
    for i,title in enumerate(('Destinations','Bluetooth','Settings')):
        key('Menu',f'menu-focus-{i}',f'Select {title}', 'key_handle(K_MENU);', 'key_handle(K_DOWN);'*i, f'assert(menu_focus=={i});')
        key('Menu',f'menu-open-{i}',f'Open {title}', 'key_handle(K_MENU);'+'key_handle(K_DOWN);'*i, 'key_handle(K_ENTER);', f'assert(screen=={["SCR_TRIP","SCR_BT","SCR_SET"][i]});')
    key('Menu','menu-back','Back from menu to driving','screen=SCR_MENU;','key_handle(K_BACK);','assert(screen==SCR_MAP);')
    motion('Menu', 'preview-menu-entry', 'Active status enters the menu once and persists through subpages',
           'settle();set_volume(16);draw();hud_capture_ms+=400u;draw();',
           ((400, 'key_handle(K_MENU);assert(screen==SCR_MENU);'),
            (950, 'key_handle(K_ENTER);assert(screen==SCR_TRIP);'), (1400, '')),
           MENU_ENTRY_SAMPLES)
    motion('Menu', 'preview-menu-return', 'Return to driving with a persistent clock',
           'screen=SCR_MENU;pref.theme=THEME_DAY;refresh_theme();settle();',
           ((400, 'key_handle(K_BACK);assert(screen==SCR_MAP);'),), MENU_RETURN_SAMPLES)
    key('Menu','menu-up','Move menu selection up','screen=SCR_MENU;menu_focus=2;','key_handle(K_UP);','assert(menu_focus==1);')
    for page,name in [('SCR_TRIP','destinations'),('SCR_BT','phone'),('SCR_SET','settings'),('SCR_KEY','keyboard')]:
        key('Menu',f'{name}-map',f'MAP returns from {name}',f'screen={page};','key_handle(K_MAP);','assert(screen==SCR_MAP);')
        key('Menu',f'{name}-page-back',f'BACK leaves {name}',f'screen={page};','key_handle(K_BACK);')
    for name,code in [('up','K_UP'),('down','K_DOWN'),('left','K_LEFT'),('right','K_RIGHT')]:
        key('Map',f'pan-{name}',f'Pan Explore {name}','preview_enter_explore();',f'key_handle({code});')
    for free in (False,True):
        setup='preview_enter_explore();' if free else ''
        for name,code in [('in','K_SR'),('out','K_ESC')]:
            key('Map',f'zoom-{name}-'+('explore' if free else 'driving'),f'Zoom {name} in '+('Explore' if free else 'driving'),setup,f'key_handle({code});')
    key('Map','autozoom-override','Override automatic zoom','pref.autozoom=1;zoom_override=4;','key_handle(K_SR);','assert(zoom_override==5);')
    key('Map','autozoom-resume','Resume automatic zoom','pref.autozoom=1;zoom_override=4;','key_handle(K_MAP);','assert(zoom_override==-1&&!freecam);')
    key('Map','map-menu','Open menu from driving','','key_handle(K_MENU);','assert(screen==SCR_MENU);')
    for mode in ('enter','leave'):
        for frame,elapsed in enumerate(EXPLORE_SAMPLES):
            setup='' if mode=='enter' else 'preview_enter_explore();'
            action=f'key_handle(K_MAP);explore_sample({elapsed});'
            key('Animations',f'explore-{mode}-{frame:02}',f'Explore {mode} · frame {frame}',setup,action,note='Elapsed-time camera samples at approximately 15 Hz; map/model cadence is separate. Host captures do not measure hardware throughput.')
    browse = 'key_handle(K_MAP);' + 'explore_sample(100);' * 6
    pan = browse + 'key_handle(K_UP);'
    zoom = pan + 'key_handle(K_SR);'
    key('Map', 'preview-explore-pan', 'Pan the map', browse + 's32 before=cam_lat;',
        'key_handle(K_UP);', 'assert(freecam&&cam_lat!=before);')
    key('Map', 'preview-explore-zoom', 'Zoom the map', pan + 'int before=active_zoom();',
        'key_handle(K_SR);', 'assert(freecam&&active_zoom()==before+1);')
    for frame,elapsed in enumerate(EXPLORE_SAMPLES):
        key('Map', f'preview-explore-return-{frame:02}', 'Return to driving', zoom,
            f'key_handle(K_MAP);explore_sample({elapsed});',
            'assert(!freecam);')
    for name,code,expect in [('previous','K_LEFT','PREV'),('next','K_RIGHT','NEXT'),('pause','K_ENTER','PAUSE')]:
        key('Audio',f'transport-{name}',f'Phone audio · {name}','key_handle(K_BACK);',f'key_handle({code});',f'command_is(" {expect}");', 'The real handler queues a command in isolated host I/O; phone acknowledgement/audio output is simulated.')
    key('Audio','transport-play','Phone audio · play','btui.playing=0;key_handle(K_BACK);','key_handle(K_ENTER);','command_is(" PLAY");')
    add('Audio','audio-paused-controls','Paused phone audio with controls visible','btui.playing=0;key_handle(K_BACK);')
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
    add('Destinations','itinerary-full','Ten-target itinerary','full_itinerary();menu_open(P_TRIP,0,hud_now_ms());')
    add('Destinations','itinerary-empty','No current route','screen=SCR_TRIP;trip_pane=1;trip.active=0;trip.count=0;')
    key('Destinations','empty-receive','Receive first destination from phone','screen=SCR_TRIP;trip_pane=1;trip.active=0;trip.count=0;','', 'assert(modal==MOD_NONE);')
    for stage, title in [('entry','Enter latitude'),('latitude','Latitude ready'),('longitude','Longitude ready'),('name','Optional destination name'),('review','Review coordinates and name'),('saved','Named destination saved to Recents')]:
        add('Coordinates','preview-coordinates-'+stage,title,
            f'coordinate_preview("{stage}");', note='Decimal-degree entry uses production remote keys; names and coordinates are controlled samples.')
    add('Offline routing','coordinate-route-source','Choose Coordinates from an empty itinerary',
        'clear_trip();gps.mph10=0;screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;',
        check='assert(!trip.active&&trip_right_focus==0);')
    coordinate_start='coordinate_route_start();'
    motion('Offline routing','coordinate-route-calculating','Coordinate destination calculation',
           coordinate_start,((0,''),),PHONE_CALC_SAMPLES)
    motion('Offline routing','coordinate-route-ready','Calculated coordinate route and journey estimate',
           coordinate_start+'preview_finish_route();',((0,''),),ROUTE_READY_SAMPLES)
    add('Offline routing','coordinate-route-recents','Coordinate route remains named in Recents',
        coordinate_start+'preview_finish_route();key_handle(K_MENU);key_handle(K_ENTER);trip_pane=0;trip_tab=trip_focus=0;',
        check='assert(route_status==R_READY&&streq(recents[0].name,"QUAYSIDE"));')
    map_start='map_point_preview("calculating");'
    add('Offline routing','map-point-driving','Start with no route', 'map_point_preview("driving");')
    for elapsed in EXPLORE_SAMPLES:
        add('Offline routing',f'map-point-explore-{elapsed:04d}','Enter map browsing',
            f'map_point_preview("driving");key_handle(K_MAP);explore_sample({elapsed});gallery_motion=1;',
            check='assert(freecam);')
    add('Offline routing','map-point-selected','Pan the crosshair to a destination', 'map_point_preview("selected");')
    add('Offline routing','map-point-confirm','OK opens the route-to-point confirmation', 'map_point_preview("confirm");',
        check='assert(modal==MOD_ROUTE_POINT&&!pending_point_stop);')
    motion('Offline routing','map-point-calculating','Calculate a route to the selected point',
           map_start,((0,''),),PHONE_CALC_SAMPLES)
    motion('Offline routing','map-point-ready','Calculated map-point route',
           map_start+'preview_finish_route();',((0,''),),ROUTE_READY_SAMPLES)
    add('Coordinates','coordinates-invalid-latitude','Invalid latitude remains editable',
        'coordinate_preview("entry");coordinate_digits("90.00001");coordinates.focus=COORD_NEXT;key_handle(K_ENTER);',check='assert(coordinates.error&&coordinates.step==COORD_LAT&&!trip.active);')
    add('Coordinates','coordinates-review-long-name','Long Unicode destination in review',
        'coordinate_preview("review");copystr(coordinates.name,sizeof(coordinates.name),"東京駅 丸の内中央口 · Newcastle Central Station");')
    add('Coordinates','coordinates-latitude-german','Translated numeric entry with valid clock and status',
        'coordinate_preview("latitude");pref.language=3;set_volume(16);')
    add('Coordinates','coordinates-latitude-arabic','Arabic numeric entry with inline validation',
        'coordinate_preview("latitude");pref.language=12;coordinates.error=1;')
    add('Coordinates','coordinates-review-hebrew','Hebrew review with left-to-right coordinates',
        'coordinate_preview("review");pref.language=17;')
    key('Destinations','full-add-disabled','Full itinerary: add-stop limit','full_itinerary();menu_open(P_TRIP,TRIP_MAX_TARGETS,hud_now_ms());','key_handle(K_ENTER);','assert(modal==MOD_NONE&&trip.count==TRIP_MAX_TARGETS);')
    add_stop = 'key_handle(K_MENU);key_handle(K_ENTER);key_handle(K_SR);key_handle(K_DOWN);key_handle(K_DOWN);'
    add_source = add_stop + 'key_handle(K_ENTER);key_handle(K_DOWN);'
    pick_favourite = add_source + 'key_handle(K_ENTER);key_handle(K_DOWN);'
    add('Destinations', 'preview-add-stop', 'Select Add stop', add_stop,
        check='assert(screen==SCR_TRIP&&trip_pane==1&&trip_right_focus==2);')
    add('Destinations', 'preview-add-source', 'Choose Favourites as the source', add_source,
        check='assert(modal==MOD_SOURCE_PICK&&modal_choice==1);')
    add('Destinations', 'preview-pick-favourite', 'Select Work from favourites', pick_favourite,
        check='assert(trip_pick_mode==DEST_PICK_ADD_STOP&&trip_tab==1&&trip_focus==1);')
    key('Destinations', 'preview-stop-added', 'Work added to the itinerary', pick_favourite,
        'key_handle(K_ENTER);',
        'assert(trip.count==3&&streq(trip.target[1].name,"Work")&&streq(trip.target[2].name,"Home"));')
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
        for choice,title in enumerate(('Recents','Favourites','Coordinates','Cancel')):
            stem=('change' if change else 'add')+f'-source-{choice}'
            setup=state+f'modal_choice={choice};'
            add('Choose destination',stem+'-choice',title+' · '+('change destination' if change else 'add stop'),setup)
            key('Choose destination',stem+'-result','Choose '+title,setup,'key_handle(K_ENTER);')
        for tab,name in ((0,'recents'),(1,'favourites')):
            key('Choose destination',('change' if change else 'add')+'-pick-'+name,('Replace destination' if change else 'Add stop')+' from '+name,f'screen=SCR_TRIP;trip_tab={tab};trip_pick_mode='+('DEST_PICK_CHANGE_DEST;' if change else 'DEST_PICK_ADD_STOP;'),'key_handle(K_ENTER);','assert(trip_pick_mode==DEST_PICK_NONE);')
    key('Saved-place actions','unnamed-recent-favourite','Name an unnamed map point as a favourite','screen=SCR_TRIP;copystr(recents[0].name,sizeof(recents[0].name),"MAP POINT");key_handle(K_ENTER);modal_choice=2;','key_handle(K_ENTER);','assert(screen==SCR_KEY);')
    key('Itinerary actions','unnamed-destination-favourite','Name an unnamed destination as a favourite','screen=SCR_TRIP;copystr(trip.target[1].name,sizeof(trip.target[1].name),"MAP POINT");trip_pane=1;trip_right_focus=1;key_handle(K_ENTER);modal_choice=1;','key_handle(K_ENTER);','assert(screen==SCR_KEY);')
    # Yes/no actions expose both selection states and confirmed outcomes.
    confirmations=[('map-route','Route to map point','preview_enter_explore();trip.active=0;key_handle(K_ENTER);'),('map-stop','Add map point as stop','preview_enter_explore();key_handle(K_ENTER);'),('cancel-route','Cancel current route','screen=SCR_TRIP;modal=MOD_CANCEL_ROUTE;'),('remove-stop','Remove itinerary stop','screen=SCR_TRIP;modal=MOD_REMOVE_STOP;modal_index=0;'),('resume-trip','Resume saved trip','screen=SCR_MAP;trip.paused=1;modal=MOD_RESUME_TRIP;')]
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
    add('Phone destinations','incoming-native-name','Received native destination name','screen=SCR_TRIP;incoming("東京駅 · محطة",\'N\');')
    phone_received = ('screen=SCR_TRIP;trip_pane=1;trip.active=0;trip.count=0;modal=MOD_NONE;settle();'
                      'incoming_sender("Central Station",1,\'N\',5497370,-161331);'
                      'assert(modal==MOD_NONE&&trip.active&&streq(trip.target[0].name,"Central Station"));'
                      'trip_service();trip_service();')
    add('Phone destinations','phone-preview-received','Calculate the route to the received destination',
        phone_received, check='assert(route_status==R_CALC);')
    motion('Phone destinations', 'phone-preview-calculating', 'Route calculation',
           phone_received + 'assert(route_status==R_CALC);', ((0, ''),), PHONE_CALC_SAMPLES)
    add('Phone destinations','phone-preview-trip','The received destination in Trip',
        phone_received + 'u32 deadline=s_now_ms()+120000;'
        'while(route_status==R_CALC&&time_before_u32(s_now_ms(),deadline))route_service_until(s_now_ms()+12);'
        'screen=SCR_TRIP;trip_pane=1;', check='assert(route_status==R_READY);')
    add('Phone destinations','incoming-full','Phone stop rejected: itinerary full','screen=SCR_TRIP;full_itinerary();incoming("Extra stop",\'S\');')
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
    # Keep each capture's remote interaction on the categorised settings tree.
    groups = ((0,1,8,9),(10,11),(2,3,4),(12,13,14,15),(7,5,6))
    titles = ('Brightness','Theme','Units','Auto zoom','Camera alerts','Clock','Debug log','Language','Day starts','Night starts','Top pill order','Bottom pill order','Learn driving history','Destination suggestions','Intelligent diversions','Clear driving history')
    for group,rows in enumerate(groups):
        for pos,i in enumerate(rows):
            key('Settings',f'settings-focus-{i}','Select '+titles[i],'menu_open(P_SETTINGS,0,hud_now_ms());','key_handle(K_DOWN);'*group+'key_handle(K_ENTER);'+'key_handle(K_DOWN);'*pos,f'assert(settings_row(sheet_page,sheet_focus)=={i});')
    key('Settings','settings-up','Move settings selection up','menu_open_setting(6,hud_now_ms());','key_handle(K_UP);','assert(settings_row(sheet_page,sheet_focus)==5);')
    key('Settings','schedule-edit-day','Adjust the Day start hour',
        'menu_open_setting(8,hud_now_ms());theme_time_part=0;','key_handle(K_RIGHT);',
        'assert(pref.day_start==8*60);')
    key('Settings','schedule-edit-minute','Adjust the Night start minute',
        'menu_open_setting(9,hud_now_ms());theme_time_part=0;','key_handle(K_ENTER);key_handle(K_LEFT);',
        'assert(pref.night_start==17*60+59);')
    for value in (10,50,100):
        key('Settings',f'brightness-{value}',f'Brightness {value}%','menu_open_setting(0,hud_now_ms());pref.bright=50;', 'key_handle(K_LEFT);'*((50-value)//10) if value<50 else 'key_handle(K_RIGHT);'*((value-50)//10),f'assert(pref.bright=={value});')
    for value,title in enumerate(('Day','Night','Auto')):
        key('Settings',f'theme-{title.lower()}','Theme · '+title,'menu_open_setting(1,hud_now_ms());pref.theme=THEME_DAY;','key_handle(K_RIGHT);'*value,f'assert(pref.theme=={value});', 'Changing theme changes the framebuffer palette; paired columns identify the starting theme, and can converge on the same explicit setting.')
    for value,title in enumerate(('Auto','Metric','Imperial')):
        key('Settings',f'units-{title.lower()}','Units · '+title,'menu_open_setting(2,hud_now_ms());pref.units=0;','key_handle(K_RIGHT);'*value,f'assert(pref.units=={value});')
    for on in (False,True):
        key('Settings','autozoom-'+str(int(on)),'Auto zoom · '+('On' if on else 'Off'),f'menu_open_setting(3,hud_now_ms());pref.autozoom={int(not on)};','key_handle(K_RIGHT);',f'assert(pref.autozoom=={int(on)});')
        key('Settings','debug-'+str(int(on)),'Debug log · '+('On' if on else 'Off'),f'menu_open_setting(6,hud_now_ms());pref.debug={int(not on)};','key_handle(K_RIGHT);',f'assert(pref.debug=={int(on)});')
    for value,title in enumerate(('Off','Visual','Phone audio')):
        key('Settings',f'camera-setting-{value}','Camera alerts · '+title,'menu_open_setting(4,hud_now_ms());pref.cameras=0;','key_handle(K_RIGHT);'*value,f'assert(pref.cameras=={value});')
    for on in (False,True):
        key('Settings','clock-'+str(int(on)),'Clock · '+('Automatic' if on else 'Manual'),f'menu_open_setting(5,hud_now_ms());pref.clock_auto={int(not on)};','key_handle(K_ENTER);',f'assert(pref.clock_auto=={int(on)});')
    for name,setup,action in [('plus-quarter','pref.hoff=pref.moff=0;','key_handle(K_RIGHT);'),('minus-quarter','pref.hoff=pref.moff=0;','key_handle(K_LEFT);'),('lower-wrap','pref.hoff=-12;pref.moff=0;','key_handle(K_LEFT);'),('upper-wrap','pref.hoff=14;pref.moff=0;','key_handle(K_RIGHT);')]:
        key('Settings','clock-'+name,'Manual clock · '+name,'menu_open_setting(5,hud_now_ms());'+setup,action)
    languages=json.loads((ROOT/'locales/ui.json').read_text())['languages']
    for i,code in enumerate(languages):
        key('Languages','language-'+code,'Language · '+code,'menu_open_setting(7,hud_now_ms());pref.language=0;','key_handle(K_RIGHT);'*i,f'assert(pref.language=={i});')
    key('Languages','language-enter','Choose a language using the picker','menu_open_setting(7,hud_now_ms());pref.language=0;','key_handle(K_ENTER);key_handle(K_DOWN);key_handle(K_ENTER);','assert(pref.language==1&&sheet_page==P_SET_SYSTEM);')
    key('Languages','language-backward','Cycle language backwards','menu_open_setting(7,hud_now_ms());','key_handle(K_LEFT);',f'assert(pref.language=={len(languages)-1});')
    roundabout = 'region_flags|=4;nav_man_type=MAN_ROUNDABOUT;nav_man_exit=3;nav_man_idx=route_head+8;nav_man_m=180;'
    add('Navigation states','roundabout-guidance','Roundabout circulation and exit number',roundabout,
        note='Controlled third-exit guidance state; production clockwise HUD glyph.')
    motion('HUD motion', 'preview-roundabout-eta', 'Roundabout and journey overview', roundabout + road_cleared,
           ((600, 'key_handle(K_ENTER);assert(hud_navigation.kind==HUD_JOURNEY);'), (4800, '')),
           ROUNDABOUT_ETA_SAMPLES)
    states=[('gps-lost','Acquiring GPS','gps.fix=0;'),('route-calculating','Calculating route','route_status=R_CALC;'),('route-rerouting','Rerouting','route_status=R_CALC;route_rerouting=1;'),('route-error','No route available','route_status=R_UNREACHABLE;'),('no-route','Driving without a route','route_status=R_IDLE;route_n=0;trip.active=0;'),('limit-estimated','Estimated speed limit','roadctx.speed_flags=3;'),('limit-unknown','Unknown speed limit','roadctx.valid=0;roadctx.speed=0;'),('limit-variable','Variable speed limit','roadctx.speed=0;roadctx.speed_flags=8;'),('limit-unrestricted','Unrestricted speed context','roadctx.speed=0;roadctx.speed_flags=4;'),('limit-three-digit','Three-digit metric speed and limit','pref.units=1;gps.mph10=810;roadctx.speed=130;roadctx.speed_flags=0;'),('limit-metric','Metric speed and road limit','pref.units=1;roadctx.speed=50;roadctx.speed_flags=0;'),('limit-imperial','Imperial speed and road limit','pref.units=2;roadctx.speed=30;roadctx.speed_flags=2;'),('route-long-name','Long street name','copystr(roadctx.street,sizeof(roadctx.street),"A long example street name for fitting");'),('map-missing','Missing navigation data','mapok=0;'),('map-outside','Position outside installed coverage','gps.lat=track_lat=3500000;gps.lon=track_lon=-2000000;')]
    for name,title,setup in states:add('Navigation states',name,title,setup,note='Simulated GPS/road/route state rendered by production draw().')
    for kind,title in [(1,'Fixed speed camera'),(2,'Red-light camera'),(3,'Average-speed entry')]:
        add('Camera states',f'camera-{kind}',title,road_cleared+f'show_camera({kind},0);',note='Illustrative camera state at sample GPS coordinates; not a claim that a camera exists there.')
    add('Camera states','camera-average','Average-speed section in progress',road_cleared+'show_camera(3,1);')
    add('Camera states','camera-overspeed','Camera limit exceeded','show_camera(1,0);gps.mph10=450;')
    add('Camera states','camera-hidden-gps','Camera alert suppressed after GPS loss','show_camera(1,0);gps.fix=0;')
    # Complete the actionable choices beyond the original overview catalog.
    for index, char in enumerate('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'):
        key('Every keyboard character','type-'+char,'Type '+char,
            f'name_keyboard();kbd_focus={index};','key_handle(K_ENTER);',
            f'assert(!strcmp(kbd_text,"{char}"));')
    for value in (20,30,40,60,70,80,90):
        key('Settings',f'brightness-{value}',f'Brightness {value}%',
            'menu_open_setting(0,hud_now_ms());pref.bright=50;',
            'key_handle(K_LEFT);'*((50-value)//10) if value<50 else 'key_handle(K_RIGHT);'*((value-50)//10),f'assert(pref.bright=={value});')
    for name,setup,action,check in [
        ('brightness-min','menu_open_setting(0,hud_now_ms());pref.bright=10;','K_LEFT','assert(pref.bright==10);'),
        ('brightness-max','menu_open_setting(0,hud_now_ms());pref.bright=100;','K_RIGHT','assert(pref.bright==100);'),
        ('theme-wrap-left','menu_open_setting(1,hud_now_ms());pref.theme=0;','K_LEFT','assert(pref.theme==2);'),
        ('theme-wrap-right','menu_open_setting(1,hud_now_ms());pref.theme=2;','K_RIGHT','assert(pref.theme==0);'),
        ('units-wrap-left','menu_open_setting(2,hud_now_ms());pref.units=0;','K_LEFT','assert(pref.units==2);'),
        ('units-wrap-right','menu_open_setting(2,hud_now_ms());pref.units=2;','K_RIGHT','assert(pref.units==0);'),
        ('cameras-wrap-left','menu_open_setting(4,hud_now_ms());pref.cameras=0;','K_LEFT','assert(pref.cameras==2);'),
        ('cameras-wrap-right','menu_open_setting(4,hud_now_ms());pref.cameras=2;','K_RIGHT','assert(pref.cameras==0);'),
        ('language-wrap-right','menu_open_setting(7,hud_now_ms());pref.language=UI_LANGUAGE_COUNT-1;','K_RIGHT','assert(pref.language==0);'),
        ('language-wrap-enter','menu_open_setting(7,hud_now_ms());pref.language=UI_LANGUAGE_COUNT-1;','K_ENTER','assert(sheet_page==P_LANG&&sheet_focus==UI_LANGUAGE_COUNT-1);'),
        ('connected-choice-right','screen=SCR_BT;','K_RIGHT','assert(bt_page_connected_choice==1);'),
        ('connected-choice-left','screen=SCR_BT;bt_page_connected_choice=1;','K_LEFT','assert(bt_page_connected_choice==0);'),
        ('saved-phone-dialog','screen=SCR_BT;bt_page_focus=1;','K_ENTER','assert(bt_page_dialog>=0);'),
        ('saved-phone-right','screen=SCR_BT;bt_page_focus=1;key_handle(K_ENTER);','K_RIGHT','assert(bt_page_dialog_choice==1);'),
        ('saved-phone-left','screen=SCR_BT;bt_page_focus=1;key_handle(K_ENTER);bt_page_dialog_choice=1;','K_LEFT','assert(bt_page_dialog_choice==0);'),
        ('saved-phone-soft-back','screen=SCR_BT;bt_page_focus=1;key_handle(K_ENTER);','K_ESC','assert(bt_page_dialog<0);'),
        ('phone-up','screen=SCR_BT;bt_page_focus=2;','K_UP','assert(bt_page_focus==1);'),
        ('map-recenter','cam_lat+=100;cam_lon+=100;','K_ENTER',''),
        ('map-menu-from-explore','preview_enter_explore();','K_MENU','assert(screen==SCR_MENU);'),
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
    for name,setup in [('source','modal=MOD_SOURCE_PICK;'),('saved-place','modal=MOD_RECENT;')]:
        key('Dialog controls','dialog-map-'+name,'MAP leaves '+name+' dialog','screen=SCR_TRIP;'+setup,'key_handle(K_MAP);','assert(screen==SCR_MAP&&modal==MOD_NONE);')
    for name,setup in [('stop','MOD_ARRIVED_STOP'),('destination','MOD_ARRIVED_DEST')]:
        key('Dialog controls','arrival-protected-'+name,'Arrival prompt awaits confirmation · '+name,
            f'modal={setup};','key_handle(K_BACK);key_handle(K_MAP);',f'assert(modal=={setup});')
    for name,setup in [('map-min','manual_zoom=mh.minlvl;'),('map-max','manual_zoom=mh.maxlvl;'),('explore-min','preview_enter_explore();freecam_zoom=mh.minlvl;'),('explore-max','preview_enter_explore();freecam_zoom=MAX_EXPLORE_ZOOM;')]:
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
        ('showcase-theme-day','Choose the Day theme','menu_open_setting(1,hud_now_ms());pref.theme=THEME_NIGHT;refresh_theme();','K_LEFT'),
        ('showcase-theme-auto','Automatic day/night theme','menu_open_setting(1,hud_now_ms());pref.theme=THEME_NIGHT;refresh_theme();','K_RIGHT'),
        ('showcase-theme-night','Return to the Night theme','menu_open_setting(1,hud_now_ms());pref.theme=THEME_AUTO;refresh_theme();','K_LEFT'),
        ('showcase-units-auto','Use automatic country units','menu_open_setting(2,hud_now_ms());pref.units=2;','K_RIGHT'),
        ('showcase-language-en','Return to English','menu_open_setting(7,hud_now_ms());pref.language=1;','K_LEFT'),
        ('showcase-camera-off','Turn camera alerts off','menu_open_setting(4,hud_now_ms());pref.cameras=1;','K_LEFT'),
        ('showcase-camera-audio','Enable camera cues through phone audio','menu_open_setting(4,hud_now_ms());pref.cameras=1;','K_RIGHT'),
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
        setup=f'screen=SCR_TRIP;modal=MOD_NONE;'
        add('ClioPlay Send integration',f'sender-{name}-wait','Wait for '+name+' from ClioPlay Send',setup)
        payload=json.dumps(receipt['name'],ensure_ascii=False)
        add('ClioPlay Send integration',f'sender-{name}-result','Receive '+name+' from ClioPlay Send',
            setup+f'incoming_sender({payload},{receipt["seq"]},\'{receipt["action"]}\',{receipt["lat"]},{receipt["lon"]});',
            check='assert(modal==MOD_NONE);',note='Actual browser MediaSession packet decoded by the production daemon parser and received through production poll_phone_destination()/phone_destination_apply(); radio transport simulated.')
    modern_menus()
    return SCENES



def modern_menus():
    # Retain native map/routing/audio coverage; replace legacy full-screen
    # menu cases with current sheets and real production-controller actions.
    retired = {'Menu','Settings','Every keyboard character','Saved-place actions',
               'Additional controls','Destinations','Choose destination','Languages',
               'Phone connection','Favourite keyboard','Showcase controls',
               'Itinerary actions','Scrollbars','Dialog controls'}
    SCENES[:] = [s for s in SCENES if s['group'] not in retired]
    for scene in SCENES:
        if scene['id']=='coordinate-route-source':
            scene.update(setup='clear_trip();gps.mph10=0;menu_open(P_TRIP,0,hud_now_ms());',check='assert(!trip.active&&sheet_page==P_TRIP);')
        if scene['id']=='coordinate-route-recents':
            scene['setup']='coordinate_route_start();preview_finish_route();key_handle(K_MENU);key_handle(K_ENTER);key_handle(K_RIGHT);'
        if scene['id']=='phone-preview-trip':scene['setup']+='menu_open(P_TRIP,trip.active_idx,hud_now_ms());'
    for page,name,title in [('P_HOME','menu','Menu'),('P_TRIP','itinerary','Active itinerary'),('P_PHONE','phone','Saved phones'),('P_SETTINGS','settings','Settings')]:
        SCENES[:] = [s for s in SCENES if s['id']!=name]
        add('Menu sheets',name,title,f'menu_open({page},0,hud_now_ms());')
    for group,title in enumerate(('Display','HUD layout','Navigation','Driving history','System')):
        key('Menu sheets',f'settings-category-{group}',title,f'menu_open(P_SETTINGS,{group},hud_now_ms());','key_handle(K_ENTER);',f'assert(sheet_page==P_SET_DISPLAY+{group});')
    for row,title in enumerate(('Brightness','Theme','Units','Auto zoom','Camera alerts','Clock','Debug log','Language','Day starts','Night starts','Top pill order','Bottom pill order','Learn driving history','Destination suggestions','Intelligent diversions','Clear driving history')):
        add('Menu sheets',f'settings-focus-{row}',title,f'menu_open_setting({row},hud_now_ms());')
    for tab,name in [(0,'recents'),(1,'favourites')]:
        add('Menu sheets',name+'-tab',name.title(),f'menu_open(P_TRIP,0,hud_now_ms());'+('key_handle(K_RIGHT);'*(tab+1)))
        for bottom in (False,True):
            add('Scrollbars','scrollbar-'+name+('-bottom' if bottom else '-top'),name+' list extent',
                f'nrecents=nfaves=25;places_tab={tab};menu_open(P_PLACES,{24 if bottom else 0},hud_now_ms());')
    add('Menu sheets','showcase-keyboard-char','Adaptive naming keyboard','keyboard_open_fave_rename(0);menu_open(P_NAME,0,hud_now_ms());pk_reset();pk_prepare();pk_focus=4;', 'key_handle(K_ENTER);')
    for name,row,part in [('schedule-edit-day',8,0),('schedule-edit-minute',9,1)]:
        add('Menu sheets',name,'Custom theme schedule',f'menu_open_setting({row},hud_now_ms());theme_time_part={part};')
    add('Menu sheets','showcase-theme-day','Choose Day theme','menu_open_setting(1,hud_now_ms());pref.theme=THEME_NIGHT;refresh_theme();','key_handle(K_LEFT);')
    for lang in range(18):
        add('Menu sheets','keyboard-'+str(lang),'Native naming layout',f'pref_keyboard_language={lang};keyboard_open_fave_rename(0);menu_open(P_NAME,0,hud_now_ms());pk_reset();pk_prepare();')
    events=((0,'key_handle(K_MENU);'),(700,'key_handle(K_ENTER);'),(1500,'key_handle(K_RIGHT);'),
            (2300,'key_handle(K_RIGHT);'),(3100,'key_handle(K_BACK);'),
            (3800,'key_handle(K_DOWN);key_handle(K_ENTER);'),(4600,'key_handle(K_DOWN);key_handle(K_ENTER);'),
            (5400,'key_handle(K_BACK);'),(6200,'key_handle(K_MENU);'),
            (7000,'key_handle(K_DOWN);key_handle(K_DOWN);key_handle(K_ENTER);'),
            (7800,'key_handle(K_DOWN);'),(8600,'key_handle(K_ENTER);assert(sheet_page==P_SET_HUD);'),(9400,'key_handle(K_MAP);'))
    samples=sorted({at+step for at,_ in events for step in TWEEN_STEPS}|{9800})
    motion('Menu sheets','preview-menu-tour','On-map Menu, three Trip tabs, Bluetooth actions and Settings',
           'settle();set_volume(16);draw();hud_capture_ms+=400u;draw();',events,samples)
    motion('Compact Trips', 'preview-trips-layout', 'Itinerary, Recents, Favourites, destination and review',
           'settle();', TRIPS_LAYOUT_EVENTS, TRIPS_LAYOUT_SAMPLES)
    motion('Compact Trips', 'preview-trips-empty', 'Empty trip and coordinate input',
           'settle();', TRIPS_EMPTY_EVENTS, TRIPS_EMPTY_SAMPLES)
    motion('Compact Trips', 'preview-popups-layout', 'Confirmations and expanded typing keyboard with compact picker',
           'settle();', POPUPS_LAYOUT_EVENTS, POPUPS_LAYOUT_SAMPLES)
    motion('Version 2.1.7', 'preview-v216-coordinates', 'Soft keys cycle axis/delete, Next validates, then name and review',
           'settle();pref.top_swap=pref.bottom_swap=0;hw_bluetooth_enabled=1;', V216_COORDINATES_EVENTS, V216_COORDINATES_SAMPLES)
    motion('Version 2.1.6', 'preview-v216-hud-settings', 'Top and bottom HUD card side preferences',
           'settle();pref.top_swap=pref.bottom_swap=0;hw_bluetooth_enabled=1;', V216_HUD_SETTINGS_EVENTS, V216_HUD_SETTINGS_SAMPLES)
    motion('Version 2.1.6', 'preview-v216-bluetooth-hidden', 'Simulated Bluetooth hardware flag hides UI entries',
           'settle();pref.top_swap=pref.bottom_swap=0;hw_bluetooth_enabled=1;', V216_BLUETOOTH_HIDDEN_EVENTS, V216_BLUETOOTH_HIDDEN_SAMPLES)
    motion('Version 2.1.8', 'preview-v216-itinerary', 'Scroll the itinerary and wrap between footer actions',
           'settle();pref.top_swap=pref.bottom_swap=0;hw_bluetooth_enabled=1;', V216_ITINERARY_EVENTS, V216_ITINERARY_SAMPLES)
    motion('Intelligent navigation', 'preview-v220-frequent-place', 'Stationary startup: offer, editable Favourite name, save',
           'settle();', FREQUENT_PLACE_EVENTS, V220_FREQUENT_PLACE_SAMPLES)
    motion('Intelligent navigation', 'preview-v220-destination-prediction', 'Learned destination: BT CTRL, map invitation and confirmation',
           'settle();', DESTINATION_EVENTS, V220_DESTINATION_SAMPLES)
    motion(
        'Intelligent navigation',
        'preview-v220-intelligent-diversions',
        'Historical traffic warning and real map-backed diversion',
        'settle();',
        MAP_DIVERSION_EVENTS,
        V220_DIVERSION_SAMPLES
    )
    motion(
        'Version 2.2.3',
        'preview-v223-nearby-pois',
        'Offline Nearby POIs (Hospital and Fuel) and adding a stop to the itinerary',
        'preview_nearby_seed();settle();',
        V223_NEARBY_EVENTS,
        V223_NEARBY_SAMPLES
    )
    motion(
        'Version 2.2.3',
        'preview-v223-secondary-manoeuvre',
        'Secondary Then manoeuvre cue above Turn card promoting on roundabout exit',
        'preview_chained_seed();settle();',
        V223_SECONDARY_MANOEUVRE_EVENTS,
        V223_SECONDARY_MANOEUVRE_SAMPLES
    )
    motion(
        'Version 2.2.3',
        'preview-v223-view-diversion',
        'Traffic warning, delayed ETA, Traffic diversion available, Explore preview and acceptance',
        'preview_map_diversion_warn_seed();settle();',
        V223_VIEW_DIVERSION_EVENTS,
        V223_VIEW_DIVERSION_SAMPLES
    )
    motion(
        'Version 2.2.4 (Unreleased)',
        'preview-v224-ml-eta-and-habits',
        'Clear-road usual-route proposal and calibrated ETA reliability range on Durham map',
        'preview_ml_eta_habits_seed();settle();',
        V224_ML_ETA_HABITS_EVENTS,
        V224_ML_ETA_HABITS_SAMPLES
    )
    motion(
        'Version 2.2.4 (Unreleased)',
        'preview-v224-ml-slowdowns',
        'Warm repeat hard-braking alert, deduplication, cold-only silence and frosty cold-weather slowdown cue in York',
        'preview_ml_slowdowns_seed();settle();',
        V224_ML_SLOWDOWNS_EVENTS,
        V224_ML_SLOWDOWNS_SAMPLES
    )
    motion(
        'Version 2.2.4 (Unreleased)',
        'preview-v224-ml-parking-and-pois',
        'Usual parking confirmation, recurring intermediate stop and preferred Nearby POI ranking',
        'preview_ml_parking_pois_seed();settle();',
        V224_ML_PARKING_POIS_EVENTS,
        V224_ML_PARKING_POIS_SAMPLES
    )
    motion(
        'Version 2.2.4 (Unreleased)',
        'preview-v224-ml-adaptive-hud',
        'Adaptive Auto-zoom per speed band and automatic Day/Night learned brightness switch',
        'preview_ml_adaptive_hud_seed();settle();',
        V224_ML_ADAPTIVE_HUD_EVENTS,
        V224_ML_ADAPTIVE_HUD_SAMPLES
    )
    add('Menu sheets','empty-receive','Empty itinerary receives phone destinations automatically','trip.active=trip.count=0;menu_open(P_TRIP,0,hud_now_ms());')
    add_stop='key_handle(K_MENU);key_handle(K_ENTER);key_handle(K_DOWN);key_handle(K_DOWN);'
    source=add_stop+'key_handle(K_ENTER);key_handle(K_DOWN);'
    favourite=source+'key_handle(K_ENTER);key_handle(K_DOWN);'
    add('Menu sheets','preview-add-stop','Choose Add stop',add_stop,check='assert(sheet_page==P_TRIP&&sheet_focus==trip.count);')
    add('Menu sheets','preview-add-source','Choose Favourites',source,check='assert(sheet_page==P_SOURCE&&sheet_focus==1);')
    add('Menu sheets','preview-pick-favourite','Choose Work',favourite,check='assert(sheet_page==P_PLACES&&places_picker&&places_tab==1&&sheet_focus==1);')
    add('Menu sheets','preview-stop-review','Review Work as a stop',favourite+'key_handle(K_ENTER);',check='assert(sheet_page==P_REVIEW&&trip.count==2);')
    add('Menu sheets','preview-stop-added','Work added to itinerary',favourite+'key_handle(K_ENTER);key_handle(K_ENTER);',check='assert(trip.count==3&&streq(trip.target[1].name,"Work")&&streq(trip.target[2].name,"Home"));')


HELPERS = r'''
static int gallery_night,gallery_motion;
static int saved_n;static u32 saved_m,saved_cost;
static s32 saved_lat[ROUTE_PATH_MAX],saved_lon[ROUTE_PATH_MAX];
static u32 saved_node[ROUTE_PATH_MAX],saved_arc[ROUTE_PATH_MAX],saved_phys[ROUTE_PATH_MAX],saved_seg_m[ROUTE_PATH_MAX],saved_seg_cost[ROUTE_PATH_MAX];
static void save_route(void){saved_n=route_n;saved_m=route_result_m;saved_cost=route_result_cost;
 memcpy(saved_lat,route_lat,sizeof(saved_lat));memcpy(saved_lon,route_lon,sizeof(saved_lon));memcpy(saved_node,route_node,sizeof(saved_node));memcpy(saved_arc,route_seg_arc,sizeof(saved_arc));memcpy(saved_phys,route_seg_phys,sizeof(saved_phys));memcpy(saved_seg_m,route_seg_m,sizeof(saved_seg_m));memcpy(saved_seg_cost,route_seg_cost,sizeof(saved_seg_cost));}
static void restore_route(void){route_n=saved_n;route_head=0;route_result_m=saved_m;route_result_cost=saved_cost;route_status=R_READY;
 memcpy(route_lat,saved_lat,sizeof(saved_lat));memcpy(route_lon,saved_lon,sizeof(saved_lon));memcpy(route_node,saved_node,sizeof(saved_node));memcpy(route_seg_arc,saved_arc,sizeof(saved_arc));memcpy(route_seg_phys,saved_phys,sizeof(saved_phys));memcpy(route_seg_m,saved_seg_m,sizeof(saved_seg_m));memcpy(route_seg_cost,saved_seg_cost,sizeof(saved_seg_cost));}

/* Real map-backed diversion preview.
 *
 * Keep the calculated Newcastle route and use controlled historical
 * observations to create congestion on part of that route.
 * The production traffic search must discover a legal alternative.
 *
 * Cache the resulting candidate because every animation frame starts
 * from a freshly restored baseline route.
 */

static int demo_div_n, demo_div_a, demo_div_b,demo_profile_a,demo_profile_b;
static u32 demo_div_saving;
static struct TrafficPoint demo_div_path[TRAFFIC_PATH_MAX];

static void preview_map_traffic_profile(int a, int b, u32 now)
{
    const u32 epoch = mob_epoch(2026, 10, 21, 12, 35, 0);

    mob_model_reset();
    mob.serial = 20;
    mob_current_epoch = epoch;
    mob_clock_ms = now;

    /* Twenty controlled observations of congestion on each directed
     * road in the selected section. Leave static graph costs intact.
     */
    for (int i = a + 1; i <= b;) {
        u32 arc = route_seg_arc[i];
        u32 full = 0;
        u32 key = traffic_key_for_arc(arc, &full);

        if (key && full && full <= 36000u) {
            struct MobRoad *r =
                mob_road_find(key, mob_bucket(epoch), 1);

            if (r) {
                r->count = 20;
                r->stamp = mob.serial;
                r->mean_ds = full * 5u;
            }
        }

        while (i <= b && route_seg_arc[i] == arc)
            i++;
    }

    u32 elapsed = 0;
    u32 delay = 0;

    for (int i = route_head + 1; i <= b;) {
        u32 arc = route_seg_arc[i];
        u32 base = 0;

        while (i <= b && route_seg_arc[i] == arc) {
            base = sat_add32(base, route_seg_cost[i]);
            i++;
        }

        u32 adjusted = traffic_adjust(
            arc, epoch + elapsed / 10u, base, 0, 0
        );

        if (adjusted > base)
            delay = sat_add32(delay, adjusted - base);

        elapsed = sat_add32(elapsed, adjusted);
    }

    traffic_eta_ds = delay;
    traffic_eta_head = route_head;
    traffic_eta_revision = traffic_route_revision;
    traffic_slow_end = b;
    traffic_warn = delay >= 600u;
}

static int preview_find_map_diversion(u32 now)
{
    const u32 distances[] = {150u, 250u, 350u, 400u, 700u, 1000u, 1300u};
    u32 approach = 0;
    u32 last_attempt = 0;

    /* Try several nearby divergence and rejoin points on the real
     * route. Use actual turn-state nodes, not arbitrary vertices.
     */
    for (int a = route_head + 1; a < route_n - 2; a++) {
        approach = sat_add32(approach, route_seg_m[a]);

        if (approach > 1000u)
            break;

        if (route_node[a] == ROUTE_NOARC)
            continue;

        if (last_attempt && approach < last_attempt + 60u)
            continue;

        last_attempt = approach;

        struct CprNode node;

        if (!route_read_node(route_node[a], &node))
            continue;

        route_dest_comp = node.comp;

        for (unsigned d = 0;
             d < sizeof(distances) / sizeof(distances[0]); d++) {

            u32 distance = 0;
            u32 raw = 0;
            int b = -1;

            for (int i = a + 1; i < route_n; i++) {
                distance = sat_add32(distance, route_seg_m[i]);
                raw = sat_add32(raw, route_seg_cost[i]);

                if (distance >= distances[d] &&
                    route_node[i] != ROUTE_NOARC) {
                    b = i;
                    break;
                }
            }

            if (b <= a + 1 || !raw ||
                raw > TRAFFIC_LOOKAHEAD_DS)
                continue;

            /* Try blocking either the initial or an interior arc. */
            u32 blocks[] = {
                route_seg_arc[a + 1],
                route_seg_arc[a + (b - a) / 2]
            };

            for (unsigned k = 0; k < 2; k++) {
                if (blocks[k] == ROUTE_NOARC ||
                    blocks[k] == route_seg_arc[b])
                    continue;

                if (k && blocks[k] == blocks[0])
                    continue;

                traffic_invalidate();
                preview_map_traffic_profile(a, b, now);

                if (!traffic_warn)
                    continue;

                if (!traffic_search_start(
                        a, b, blocks[k], raw, 0, now))
                    continue;

                /* Fixed preview time makes capture deterministic.
                 * The production search retains its state limit.
                 */
                for (int step = 0;
                     traffic_searching && step < 1024; step++)
                    traffic_search_step(now);

                traffic_search_cancel();

                if (!traffic_offer_valid())
                    continue;

                demo_profile_a=a;demo_profile_b=b;
                demo_div_a = traffic_begin_idx;
                demo_div_b = traffic_join_idx;
                demo_div_n = traffic_offer_n;
                demo_div_saving = traffic_saving;

                memcpy(
                    demo_div_path,
                    traffic_path,
                    sizeof(struct TrafficPoint) * demo_div_n
                );

                return 1;
            }
        }
    }

    return 0;
}

static void preview_map_diversion_seed(void)
{
    assert(mapok && routeok);
    assert(route_status == R_READY && route_n > 2);

    pref.learn_history = 1;
    pref.diversions = 1;
    gps.fix = 1;

    u32 now = hud_now_ms();

    if (!demo_div_n) {
        if (!preview_find_map_diversion(now)) {
            fprintf(stderr,
                "No legal map-backed preview diversion found "
                "on the Newcastle baseline route.\n");
            assert(demo_div_n > 1);
        }
    }

    traffic_invalidate();

    /* Restore the learned context for this animation frame. */
    preview_map_traffic_profile(demo_profile_a, demo_profile_b, now);

    traffic_begin_idx = demo_div_a;
    traffic_join_idx = demo_div_b;
    traffic_offer_n = demo_div_n;
    traffic_saving = demo_div_saving;
    traffic_region = atlas_route_region;
    traffic_offer_revision = traffic_route_revision;

    memcpy(
        traffic_path,
        demo_div_path,
        sizeof(struct TrafficPoint) * demo_div_n
    );

    assert(traffic_prepare_offer());

    /* Position the simulated vehicle immediately before the
     * divergence, using coordinates from the actual route.
     */
    int p = demo_div_a - 1;

    if (p > route_head)
        route_trim_prefix(p);

    gps.lat = track_lat = route_lat[route_head];
    gps.lon = track_lon = route_lon[route_head];
    track_valid = 1;

    int heading = nav_heading10(
        route_lat[route_head],
        route_lon[route_head],
        route_lat[demo_div_a],
        route_lon[demo_div_a]
    );

    if (heading >= 0 && heading < 360)
        gps.course = track_course = display_course = heading;

    display_course_valid = 1;

    /* Match the normal gallery driving zoom, without auto-zoom changes. */
    pref.autozoom = 0;
    manual_zoom = 5;
    zoom_override = -1;

    trip.active = 1;
    trip.paused = 0;
    trip.count = 1;
    trip.active_idx = 0;

    trip.target[0].lat = route_target_lat = 5497370;
    trip.target[0].lon = route_target_lon = -161331;

    copystr(
        trip.target[0].name,
        sizeof(trip.target[0].name),
        "Work"
    );

    struct CprNode node;
    assert(route_read_node(route_node[demo_div_a], &node));
    route_dest_comp = node.comp;

    traffic_eta_head = route_head;
    nav_maneuver_find();
    render_background_dirty = 1;

    assert(mapok && traffic_warn && traffic_offer_valid());
}
static void preview_map_diversion_warn_seed(void)
{
    preview_map_diversion_seed();
    traffic_offer_n = 0;
    traffic_saving = 0;
    ml_offer_kind = 0;
    hud_navigation.kind = HUD_NONE;
    render_background_dirty = 1;
    assert(mapok && traffic_warn && !traffic_offer_valid());
}
static void preview_map_diversion_reveal(void)
{
    traffic_begin_idx = demo_div_a;
    traffic_join_idx = demo_div_b;
    traffic_offer_n = demo_div_n;
    traffic_saving = demo_div_saving;
    traffic_region = atlas_route_region;
    traffic_offer_revision = traffic_route_revision;
    ml_offer_kind = 0;
    memcpy(traffic_path, demo_div_path, sizeof(struct TrafficPoint) * demo_div_n);
    assert(traffic_prepare_offer());
    render_background_dirty = 1;
    assert(traffic_offer_valid() && ml_offer_kind == 0);
}
static int demo_chain_n;
static u32 demo_chain_m, demo_chain_cost;
static s32 demo_chain_lat[ROUTE_PATH_MAX], demo_chain_lon[ROUTE_PATH_MAX];
static u32 demo_chain_node[ROUTE_PATH_MAX], demo_chain_arc[ROUTE_PATH_MAX], demo_chain_phys[ROUTE_PATH_MAX];
static u32 demo_chain_seg_m[ROUTE_PATH_MAX], demo_chain_seg_cost[ROUTE_PATH_MAX];
static void preview_nearby_seed(void)
{
    assert(mapok && routeok);
    gps.lat = track_lat = 5497830;
    gps.lon = track_lon = -161780;
    track_valid = 1;
    gps.fix = 1;
    gps.course = track_course = display_course = 35;
    display_course_valid = 1;
    pref.autozoom = 0;
    manual_zoom = 5;
    zoom_override = -1;
    hud_road_live = 0;
    hud_last_road[0] = 0;
    roadctx.street[0] = 0;
    render_background_dirty = 1;
}
static void preview_chained_step(int idx)
{
    assert(idx >= route_head && idx + 1 < route_n);
    if (idx > route_head)
        route_trim_prefix(idx);
    gps.lat = track_lat = route_lat[idx];
    gps.lon = track_lon = route_lon[idx];
    track_valid = 1;
    int ahead = idx + 2 < route_n ? idx + 2 : idx + 1;
    int heading = nav_heading10(route_lat[idx], route_lon[idx], route_lat[ahead], route_lon[ahead]);
    if (heading >= 0 && heading < 360)
        gps.course = track_course = display_course = heading;
    display_course_valid = 1;
    nav_maneuver_find();
    render_background_dirty = 1;
}
static void preview_chained_seed(void)
{
    assert(mapok && routeok);
    if (!demo_chain_n) {
        gps.lat = track_lat = 5497500;
        gps.lon = track_lon = -160000;
        track_valid = 1;
        gps.fix = 1;
        assert(route_start(5498400, -161900));
        u32 deadline = s_now_ms() + 120000u;
        while (route_status == R_CALC && time_before_u32(s_now_ms(), deadline))
            route_service_until(s_now_ms() + 12u);
        assert(route_status == R_READY && route_n > 50);
        demo_chain_n = route_n;
        demo_chain_m = route_result_m;
        demo_chain_cost = route_result_cost;
        memcpy(demo_chain_lat, route_lat, sizeof(demo_chain_lat));
        memcpy(demo_chain_lon, route_lon, sizeof(demo_chain_lon));
        memcpy(demo_chain_node, route_node, sizeof(demo_chain_node));
        memcpy(demo_chain_arc, route_seg_arc, sizeof(demo_chain_arc));
        memcpy(demo_chain_phys, route_seg_phys, sizeof(demo_chain_phys));
        memcpy(demo_chain_seg_m, route_seg_m, sizeof(demo_chain_seg_m));
        memcpy(demo_chain_seg_cost, route_seg_cost, sizeof(demo_chain_seg_cost));
    }
    route_n = demo_chain_n;
    route_head = 0;
    route_result_m = demo_chain_m;
    route_result_cost = demo_chain_cost;
    route_status = R_READY;
    memcpy(route_lat, demo_chain_lat, sizeof(route_lat));
    memcpy(route_lon, demo_chain_lon, sizeof(route_lon));
    memcpy(route_node, demo_chain_node, sizeof(route_node));
    memcpy(route_seg_arc, demo_chain_arc, sizeof(route_seg_arc));
    memcpy(route_seg_phys, demo_chain_phys, sizeof(route_seg_phys));
    memcpy(route_seg_m, demo_chain_seg_m, sizeof(route_seg_m));
    memcpy(route_seg_cost, demo_chain_seg_cost, sizeof(route_seg_cost));
    trip.active = 1;
    trip.paused = 0;
    trip.count = 1;
    trip.active_idx = 0;
    trip.target[0].lat = route_target_lat = 5498400;
    trip.target[0].lon = route_target_lon = -161900;
    copystr(trip.target[0].name, sizeof(trip.target[0].name), "Royal Victoria Infirmary");
    pref.autozoom = 0;
    manual_zoom = 5;
    zoom_override = -1;
    gps.fix = 1;
    gps.mph10 = 260;
    region_flags |= 4;
    hud_road_live = 0;
    hud_last_road[0] = 0;
    roadctx.street[0] = 0;
    preview_chained_step(4);
    assert(nav_man_type == MAN_SLIGHT_RIGHT && nav_next_type == MAN_SHARP_RIGHT);
}
static void preview_ml_finish_route(void)
{
    trip_service();
    trip_service();
    u32 deadline = s_now_ms() + 120000u;
    while (route_status == R_CALC && time_before_u32(s_now_ms(), deadline))
        route_service_until(s_now_ms() + 12u);
    assert(route_status == R_READY && route_n > 1);
    hud_observed_route = R_READY;
    nav_maneuver_find();
    render_background_dirty = 1;
}
static int demo_usual_n, demo_usual_head, demo_usual_a, demo_usual_b, demo_usual_offer_n;
static u32 demo_usual_m, demo_usual_cost, demo_usual_comp;
static s32 demo_usual_lat[ROUTE_PATH_MAX], demo_usual_lon[ROUTE_PATH_MAX];
static u32 demo_usual_node[ROUTE_PATH_MAX], demo_usual_arc[ROUTE_PATH_MAX], demo_usual_phys[ROUTE_PATH_MAX];
static u32 demo_usual_seg_m[ROUTE_PATH_MAX], demo_usual_seg_cost[ROUTE_PATH_MAX];
static struct TrafficPoint demo_usual_path[TRAFFIC_PATH_MAX];
static int preview_find_durham_usual_route(u32 now)
{
    const u32 epoch = mob_epoch(2026, 10, 21, 14, 15, 0);
    gps.fix = 1;
    gps.lat = track_lat = 5477800;
    gps.lon = track_lon = -155600;
    track_valid = 1;
    assert(route_start(5477100, -159100));
    u32 deadline = s_now_ms() + 120000u;
    while (route_status == R_CALC && time_before_u32(s_now_ms(), deadline))
        route_service_until(s_now_ms() + 12u);
    assert(route_status == R_READY && route_n > 180);
    int cand_a = 141, cand_b = 180;
    if (cand_b >= route_n - 1) cand_b = route_n - 2;
    u32 raw = 0; u32 blocks[12]; int bn = 0;
    for (int i = cand_a + 1; i <= cand_b; i++) {
        raw = sat_add32(raw, route_seg_cost[i]);
        u32 arc = route_seg_arc[i];
        if (arc == ROUTE_NOARC) continue;
        int dup = 0; for (int k = 0; k < bn; k++) if (blocks[k] == arc) dup = 1;
        if (!dup && bn < 12) blocks[bn++] = arc;
    }
    for (int k = 0; k < bn; k++) {
        u32 avoided_key = traffic_key_for_arc(blocks[k], 0);
        if (!avoided_key) continue;
        /* Discover the legal alternative geometry first, then clear ALL traffic data
         * and run production traffic_scan_ex() purely from learned ml_pref_arcs. */
        mob_model_reset(); mob.n = 20; mob.serial = 20; mob_current_epoch = epoch; mob_clock_ms = now;
        for (int i = cand_a + 1; i <= cand_b; ) {
            u32 arc = route_seg_arc[i], full = 0; u32 key = traffic_key_for_arc(arc, &full);
            if (key && full && full <= 36000u) {
                struct MobRoad *r = mob_road_find(key, mob_bucket(epoch), 1);
                if (r) { u32 slow = full * 5u; if (slow < full + 3600u) slow = full + 3600u; r->mean_ds = slow > 36000u ? 36000u : slow; r->count = 20; r->stamp = mob.serial; }
            }
            while (i <= cand_b && route_seg_arc[i] == arc) i++;
        }
        traffic_invalidate(); traffic_search_kind = 0;
        if (!traffic_search_start(cand_a, cand_b, blocks[k], raw, 0, now)) continue;
        for (int s = 0; traffic_searching && s < 1024; s++) traffic_search_step(now);
        traffic_search_cancel();
        if (!traffic_offer_valid() || traffic_offer_n < 6) continue;
        u32 alt_keys[64]; int alt_kn = 0;
        for (int i = 1; i < traffic_offer_n; i++) {
            u32 ak = traffic_key_for_arc(traffic_path[i].arc, 0);
            if (!ak) continue;
            int dup = 0; for (int j = 0; j < alt_kn; j++) if (alt_keys[j] == ak) dup = 1;
            if (!dup && alt_kn < 64) alt_keys[alt_kn++] = ak;
        }
        /* Clear all historical traffic so this is a pure, clear-road usual-route suggestion. */
        mob_model_reset(); ml_reset();
        mob.n = 20; mob.serial = 20; mob_current_epoch = epoch; mob_clock_ms = now;
        for (int rep = 0; rep < 4; rep++) {
            mob_recording = mob_started = 1; ml_session_reset();
            ml_stage_pref_arc(avoided_key, -10);
            for (int j = 0; j < alt_kn; j++) ml_stage_pref_arc(alt_keys[j], 10);
            ml_commit_pref_arcs();
        }
        mob_recording = mob_started = 0;
        traffic_invalidate();
        int prev_head = route_head;
        route_head = cand_a - 3;
        gps.lat = track_lat = route_lat[route_head];
        gps.lon = track_lon = route_lon[route_head];
        traffic_scan_ex(now, 1);
        for (int s = 0; traffic_searching && s < 1024; s++) traffic_search_step(now);
        traffic_search_cancel();
        if (traffic_offer_valid() && ml_offer_kind == 1 && !traffic_warn && traffic_saving < 600u) {
            demo_usual_n = route_n;
            demo_usual_head = route_head;
            demo_usual_m = route_result_m;
            demo_usual_cost = route_result_cost;
            demo_usual_a = traffic_begin_idx;
            demo_usual_b = traffic_join_idx;
            demo_usual_offer_n = traffic_offer_n;
            memcpy(demo_usual_lat, route_lat, sizeof(demo_usual_lat));
            memcpy(demo_usual_lon, route_lon, sizeof(demo_usual_lon));
            memcpy(demo_usual_node, route_node, sizeof(demo_usual_node));
            memcpy(demo_usual_arc, route_seg_arc, sizeof(demo_usual_arc));
            memcpy(demo_usual_phys, route_seg_phys, sizeof(demo_usual_phys));
            memcpy(demo_usual_seg_m, route_seg_m, sizeof(demo_usual_seg_m));
            memcpy(demo_usual_seg_cost, route_seg_cost, sizeof(demo_usual_seg_cost));
            memcpy(demo_usual_path, traffic_path, sizeof(struct TrafficPoint) * demo_usual_offer_n);
            struct CprNode node;
            assert(route_read_node(route_node[demo_usual_a], &node));
            demo_usual_comp = node.comp;
            return 1;
        }
        route_head = prev_head;
    }
    return 0;
}
static void preview_ml_eta_habits_seed(void)
{
    assert(mapok && routeok);
    pref.learn_history = 1;
    pref.diversions = 1;
    gps.fix = 1;
    u32 now = hud_now_ms();
    if (!demo_usual_offer_n) {
        assert(preview_find_durham_usual_route(now));
    }
    mob_model_reset();
    ml_reset();
    mob.n = 20;
    mob.serial = 20;
    mob_current_epoch = mob_epoch(2026, 10, 21, 14, 15, 0);
    mob_clock_ms = now;
    traffic_invalidate();
    route_n = demo_usual_n;
    route_head = 0;
    route_result_m = demo_usual_m;
    route_result_cost = demo_usual_cost;
    route_status = R_READY;
    route_dest_comp = demo_usual_comp;
    memcpy(route_lat, demo_usual_lat, sizeof(route_lat));
    memcpy(route_lon, demo_usual_lon, sizeof(route_lon));
    memcpy(route_node, demo_usual_node, sizeof(route_node));
    memcpy(route_seg_arc, demo_usual_arc, sizeof(route_seg_arc));
    memcpy(route_seg_phys, demo_usual_phys, sizeof(route_seg_phys));
    memcpy(route_seg_m, demo_usual_seg_m, sizeof(route_seg_m));
    memcpy(route_seg_cost, demo_usual_seg_cost, sizeof(route_seg_cost));
    route_trim_prefix(demo_usual_head);
    traffic_begin_idx = demo_usual_a;
    traffic_join_idx = demo_usual_b;
    traffic_offer_n = demo_usual_offer_n;
    traffic_saving = 0;
    ml_offer_kind = 1;
    traffic_region = atlas_route_region;
    traffic_offer_revision = traffic_route_revision;
    memcpy(traffic_path, demo_usual_path, sizeof(struct TrafficPoint) * demo_usual_offer_n);
    assert(traffic_prepare_offer());
    gps.lat = track_lat = route_lat[route_head];
    gps.lon = track_lon = route_lon[route_head];
    track_valid = 1;
    int heading = nav_heading10(route_lat[route_head], route_lon[route_head], route_lat[demo_usual_a], route_lon[demo_usual_a]);
    if (heading >= 0 && heading < 360)
        gps.course = track_course = display_course = heading;
    display_course_valid = 1;
    pref.autozoom = 0;
    manual_zoom = 5;
    zoom_override = -1;
    trip.active = 1;
    trip.paused = 0;
    trip.count = 1;
    trip.active_idx = 0;
    trip.target[0].lat = route_target_lat = 5477100;
    trip.target[0].lon = route_target_lon = -159100;
    copystr(trip.target[0].name, sizeof(trip.target[0].name), "Neville's Cross");
    traffic_eta_ds = 0;
    traffic_warn = 0;
    traffic_cal_delta_ds = 1680;
    ml_route_var_ds = 1800u;
    traffic_eta_head = route_head;
    traffic_eta_revision = traffic_route_revision;
    hud_observed_route = R_READY;
    hud_navigation.kind = HUD_NONE;
    hud_road_live = 0;
    hud_last_road[0] = 0;
    roadctx.street[0] = 0;
    nav_maneuver_find();
    render_background_dirty = 1;
    assert(mapok && !traffic_warn && traffic_offer_valid() && ml_offer_kind == 1 && traffic_saving < 600u);
}
static void preview_ml_parking_pois_seed(void)
{
    preview_nearby_seed();
    pref.learn_history = 1;
    pref.destination_predictions = 1;
    pref.diversions = 1;
    /* Learn affinity for a real nearby Newcastle fuel station so it ranks #1 with a cyan star. */
    nearby_scan(NEARBY_CAT_FUEL);
    assert(nearby_count >= 2);
    int pick = (nearby_pois[1].dist_m <= nearby_pois[0].dist_m + 600u) ? 1 : 0;
    ml_record_poi_use(6, nearby_pois[pick].lat, nearby_pois[pick].lon, nearby_pois[pick].name);
    ml_record_poi_use(6, nearby_pois[pick].lat, nearby_pois[pick].lon, nearby_pois[pick].name);
    /* Active route in Newcastle with learned usual parking & recurring Coffee Stop. */
    trip.active = 1;
    trip.paused = 0;
    trip.count = 1;
    trip.active_idx = 0;
    trip.target[0].lat = route_target_lat = 5498300;
    trip.target[0].lon = route_target_lon = -161700;
    copystr(trip.target[0].name, sizeof(trip.target[0].name), "Work");
    trip_schedule_route();
    preview_ml_finish_route();
    ml_park_for_target = (struct MobPoint){5498300, -161700};
    ml_park_point = (struct MobPoint){5498249, -161837};
    add_recent("Claremont Road Car Park", ml_park_point.lat, ml_park_point.lon);
    ml_park_valid = 1;
    ml_park_serial = 1;
    memzero(&ml_stop_habit, sizeof(ml_stop_habit));
    ml_stop_habit.origin = (struct MobPoint){gps.lat, gps.lon};
    ml_stop_habit.dest = (struct MobPoint){5498300, -161700};
    ml_stop_habit.stop = (struct MobPoint){5498050, -161650};
    ml_stop_habit.count = 4;
    copystr(ml_stop_habit.name, sizeof(ml_stop_habit.name), "Coffee Stop");
    ml_stop_valid = 1;
    ml_stop_serial = 1;
    hud_navigation.kind = HUD_NONE;
    render_background_dirty = 1;
}
static int demo_slow_n;
static u32 demo_slow_m, demo_slow_cost, demo_slow_key_warm, demo_slow_key_cold;
static s32 demo_slow_lat[ROUTE_PATH_MAX], demo_slow_lon[ROUTE_PATH_MAX];
static u32 demo_slow_node[ROUTE_PATH_MAX], demo_slow_arc[ROUTE_PATH_MAX], demo_slow_phys[ROUTE_PATH_MAX];
static u32 demo_slow_seg_m[ROUTE_PATH_MAX], demo_slow_seg_cost[ROUTE_PATH_MAX];
static void preview_ml_slowdowns_step(int idx)
{
    assert(idx >= route_head && idx + 2 < route_n);
    if (idx > route_head)
        route_trim_prefix(idx);
    gps.lat = track_lat = route_lat[idx];
    gps.lon = track_lon = route_lon[idx];
    track_valid = 1;
    int ahead = idx + 2 < route_n ? idx + 2 : idx + 1;
    int heading = nav_heading10(route_lat[idx], route_lon[idx], route_lat[ahead], route_lon[ahead]);
    if (heading >= 0 && heading < 360)
        gps.course = track_course = display_course = heading;
    display_course_valid = 1;
    nav_maneuver_find();
    render_background_dirty = 1;
    traffic_scan_ex(hud_now_ms(), 0);
}
static void preview_ml_slowdowns_seed(void)
{
    assert(mapok && routeok);
    if (!demo_slow_n) {
        gps.lat = track_lat = 5395780;
        gps.lon = track_lon = -109310;
        track_valid = 1;
        gps.fix = 1;
        assert(route_start(5396900, -106400));
        u32 deadline = s_now_ms() + 120000u;
        while (route_status == R_CALC && time_before_u32(s_now_ms(), deadline))
            route_service_until(s_now_ms() + 12u);
        assert(route_status == R_READY && route_n > 90);
        demo_slow_n = route_n;
        demo_slow_m = route_result_m;
        demo_slow_cost = route_result_cost;
        memcpy(demo_slow_lat, route_lat, sizeof(demo_slow_lat));
        memcpy(demo_slow_lon, route_lon, sizeof(demo_slow_lon));
        memcpy(demo_slow_node, route_node, sizeof(demo_slow_node));
        memcpy(demo_slow_arc, route_seg_arc, sizeof(demo_slow_arc));
        memcpy(demo_slow_phys, route_seg_phys, sizeof(demo_slow_phys));
        memcpy(demo_slow_seg_m, route_seg_m, sizeof(demo_slow_seg_m));
        memcpy(demo_slow_seg_cost, route_seg_cost, sizeof(demo_slow_seg_cost));
        demo_slow_key_warm = traffic_key_for_arc(route_seg_arc[16], 0);
        demo_slow_key_cold = traffic_key_for_arc(route_seg_arc[71], 0);
        assert(demo_slow_key_warm && demo_slow_key_cold && demo_slow_key_warm != demo_slow_key_cold);
    }
    mob_model_reset();
    ml_reset();
    mob.n = 20;
    mob.serial = 20;
    mob_current_epoch = mob_epoch(2026, 10, 21, 13, 35, 0);
    mob_clock_ms = hud_now_ms();
    traffic_invalidate();
    pref.learn_history = 1;
    pref.diversions = 0; /* Slowdown alerts work independently of traffic diversions. */
    /* Train warm repeat braking spot (3 sessions) & cold-only braking spot (2 sessions <= 3C). */
    car.tempok = 1;
    car.temp = 18;
    for (int i = 0; i < 3; i++) {
        mob_recording = mob_started = 1;
        ml_session_reset();
        ml_prev_speed = -1;
        ml_prev_speed_ms = 0;
        ml_observe_decel(1000u, 280, demo_slow_key_warm, demo_slow_lat[16], demo_slow_lon[16]);
        ml_observe_decel(2000u, 140, demo_slow_key_warm, demo_slow_lat[16], demo_slow_lon[16]);
        ml_commit_hazards();
    }
    car.temp = 1;
    for (int i = 0; i < 2; i++) {
        mob_recording = mob_started = 1;
        ml_session_reset();
        ml_prev_speed = -1;
        ml_prev_speed_ms = 0;
        ml_observe_decel(1000u, 260, demo_slow_key_cold, demo_slow_lat[71], demo_slow_lon[71]);
        ml_observe_decel(2000u, 120, demo_slow_key_cold, demo_slow_lat[71], demo_slow_lon[71]);
        ml_commit_hazards();
    }
    /* Train modest cold-weather road-class ETA calibration (+25%) for winter ETA readout. */
    for (int cls = 0; cls < MOB_ML_CLASSES; cls++) {
        mob_ml.road_cls[cls].count = 10u;
        mob_ml.road_cls[cls].ratio_q8 = 256u;
        mob_ml.road_cls[cls].cold_count = 6u;
        mob_ml.road_cls[cls].cold_ratio_q8 = 320u;
    }
    mob_recording = mob_started = 0;
    ml_session_reset();
    car.temp = 18;
    route_n = demo_slow_n;
    route_head = 0;
    route_result_m = demo_slow_m;
    route_result_cost = demo_slow_cost;
    route_status = R_READY;
    memcpy(route_lat, demo_slow_lat, sizeof(route_lat));
    memcpy(route_lon, demo_slow_lon, sizeof(route_lon));
    memcpy(route_node, demo_slow_node, sizeof(route_node));
    memcpy(route_seg_arc, demo_slow_arc, sizeof(route_seg_arc));
    memcpy(route_seg_phys, demo_slow_phys, sizeof(route_seg_phys));
    memcpy(route_seg_m, demo_slow_seg_m, sizeof(route_seg_m));
    memcpy(route_seg_cost, demo_slow_seg_cost, sizeof(route_seg_cost));
    trip.active = 1;
    trip.paused = 0;
    trip.count = 1;
    trip.active_idx = 0;
    trip.target[0].lat = route_target_lat = 5396900;
    trip.target[0].lon = route_target_lon = -106400;
    copystr(trip.target[0].name, sizeof(trip.target[0].name), "Heworth");
    pref.autozoom = 0;
    manual_zoom = 5;
    zoom_override = -1;
    gps.fix = 1;
    gps.mph10 = 280;
    region_flags |= 4;
    gps.lat = track_lat = route_lat[0];
    gps.lon = track_lon = route_lon[0];
    track_valid = 1;
    int heading = nav_heading10(route_lat[0], route_lon[0], route_lat[2], route_lon[2]);
    if (heading >= 0 && heading < 360)
        gps.course = track_course = display_course = heading;
    display_course_valid = 1;
    nav_maneuver_find();
    hud_observed_route = R_READY;
    hud_navigation.kind = HUD_NONE;
    hud_road_live = 0;
    hud_last_road[0] = 0;
    roadctx.street[0] = 0;
    render_background_dirty = 1;
    assert(ml_hazard_level(demo_slow_key_warm, 0) == 1);
    assert(ml_hazard_level(demo_slow_key_cold, 0) == 0 && ml_hazard_level(demo_slow_key_cold, 1) == 2);
}
static void preview_ml_adaptive_hud_seed(void)
{
    preview_nearby_seed();
    ml_reset();
    pref.learn_history = 1;
    pref.autozoom = 1;
    pref.theme = THEME_AUTO;
    pref.day_start = 420;
    pref.night_start = 1200;
    pref.hoff = 0;
    pref.moff = 0;
    gps.hour = 13;
    gps.min = 35;
    gps.mph10 = 260;
    auto_zoom_level = 5;
    zoom_override = -1;
    ml_note_brightness(0, 90);
    ml_note_brightness(0, 90);
    ml_note_brightness(1, 30);
    ml_note_brightness(1, 30);
    pref.bright = 90;
    apply_palette(0);
    trip.active = 0;
    trip.count = 0;
    route_status = R_IDLE;
    hud_observed_route = R_IDLE;
    hud_navigation.kind = HUD_NONE;
    render_background_dirty = 1;
    assert(theme_resolved == 0 && pref.bright == 90 && active_zoom() == 5);
}
static void preview_enter_explore(void){enter_freecam();explore_sample(500);}
static void name_keyboard(void){screen=SCR_KEY;keyboard_open_fave_name(5497370,-161331);}
static void coordinate_digits(const char*s){while(*s){char c=*s++;coordinates.focus=c=='-'?COORD_SIGN:c=='.'?COORD_DOT:c=='0'?COORD_ZERO:c-'1';key_handle(K_ENTER);}}
static void coordinate_preview(const char*stage){
 clear_trip();menu_open(P_TRIP,0,hud_now_ms());key_handle(K_ENTER);
 assert(screen==SCR_COORDINATES&&coordinates_active&&sheet_page==P_COORD);
 if(streq(stage,"entry"))return;
 coordinate_digits("54.97548");
 coordinates.focus=0;if(streq(stage,"latitude"))return;
 key_handle(K_SR);assert(coordinates.step==COORD_LON);
 coordinate_digits("-1.64050");
 coordinates.focus=0;if(streq(stage,"longitude"))return;
 /* Soft-right switches axes; the keypad Next validates both coordinates. */
 coordinates.focus=COORD_NEXT;key_handle(K_ENTER);assert(screen==SCR_KEY&&sheet_page==P_NAME);
 const char*name="QUAYSIDE";
 while(*name)keyboard_type((unsigned char)*name++);
 pk_focus=PK_SAVE;if(streq(stage,"name"))return;
 key_handle(K_ENTER);
 assert(sheet_page==P_REVIEW&&coordinates.step==COORD_REVIEW&&!trip.active);
 if(streq(stage,"review"))return;
 key_handle(K_ENTER);menu_open(P_TRIP,0,hud_now_ms());key_handle(K_RIGHT);
 assert(trip.active&&streq(recents[0].name,"QUAYSIDE"));
}
static void preview_finish_route(void){
 /* Observe CALC before finishing, then let the shared HUD animate into ETA. */
 draw();hud_capture_ms+=1300u;draw();
 u32 deadline=s_now_ms()+120000u;
 while(route_status==R_CALC&&time_before_u32(s_now_ms(),deadline))route_service_until(s_now_ms()+12u);
 assert(route_status==R_READY&&route_n>1);
 struct TripTarget*t=trip_current();assert(t&&route_target_lat==t->lat&&route_target_lon==t->lon);
}
static void coordinate_route_start(void){
 coordinate_preview("review");gps.mph10=0;key_handle(K_ENTER);
 assert(screen==SCR_MAP&&trip.active&&!freecam&&streq(recents[0].name,"QUAYSIDE"));
 trip_service();trip_service();assert(route_status==R_CALC);
}
static void map_point_preview(const char*stage){
 clear_trip();gps.mph10=0;screen=SCR_MAP;hud_navigation.kind=HUD_NONE;
 settle(); /* Seed the persistent clock before any Explore/route transition. */
 u32 clock_at=hud_tween[HC_CLOCK].position_at;
 if(streq(stage,"driving"))return;
 key_handle(K_MAP);explore_sample(500);
 for(int i=0;i<4;i++)key_handle(K_LEFT);for(int i=0;i<2;i++)key_handle(K_UP);
 assert(freecam&&cam_lat!=car_lat()&&cam_lon!=car_lon());if(streq(stage,"selected"))return;
 s32 lat=cam_lat,lon=cam_lon;key_handle(K_ENTER);
 assert(modal==MOD_ROUTE_POINT&&pending_lat==lat&&pending_lon==lon&&!pending_point_stop);
 if(streq(stage,"confirm"))return;
 key_handle(K_ENTER);assert(screen==SCR_MAP&&trip.active&&trip.target[0].lat==lat&&trip.target[0].lon==lon);
 assert(!freecam&&freecam_transition_active()&&map_tilt_q8==0);
 assert(hud_tween[HC_CLOCK].position_at==clock_at&&hud_pose(&hud_tween[HC_CLOCK],hud_now_ms()).a==256);
 trip_service();trip_service();assert(screen==SCR_MAP&&!freecam&&route_status==R_CALC);
}
static void three_stops(void){trip.count=4;trip.active_idx=0;for(int i=0;i<3;i++){trip.target[i].lat=5497300+i*100;trip.target[i].lon=-161300-i*100;snprintf(trip.target[i].name,sizeof(trip.target[i].name),"Stop %d",i+1);}trip.target[3]=trip.target[0];copystr(trip.target[3].name,sizeof(trip.target[3].name),"Home");}
static void full_itinerary(void){three_stops();trip.target[TRIP_MAX_TARGETS-1]=trip.target[3];for(int i=3;i<TRIP_MAX_TARGETS-1;i++){trip.target[i]=trip.target[0];snprintf(trip.target[i].name,sizeof(trip.target[i].name),"Stop %d",i+1);}trip.count=TRIP_MAX_TARGETS;}
static void incoming(const char*name,char action){phone_dest.valid=1;phone_dest.action=action;phone_dest.lat=5497370;phone_dest.lon=-161331;copystr(phone_dest.name,sizeof(phone_dest.name),name);phone_destination_apply();}
static void incoming_sender(const char*name,int seq,char action,int lat,int lon){char packet[512];int n=snprintf(packet,sizeof(packet),"SEQ=%d\nACTION=%c\nLAT=%d\nLON=%d\nNAME=%s\n",seq,action,lat,lon,name);long fd=s_open(BT_NAV_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);assert(fd>=0);assert(s_write((int)fd,packet,n)==n);s_close((int)fd);poll_phone_destination();assert(phone_dest.valid);phone_destination_apply();}
static void show_camera(int kind,int avg){camera_visible=1;camera_average=avg;camera_current.kind=kind;camera_current.speed=30;camera_current.flags=2;camera_distance=avg?850:180;camera_average_speed=42;}
static void preview_track(const char*title,const char*artist){
 FILE*f=fopen(BT_STATUS_PATH,"w");assert(f);fputs("BT STREAMING\n",f);fclose(f);
 f=fopen(BT_META_PATH,"w");assert(f);fprintf(f,"DEVICE=Example iPhone\nCONNECTED=1\nPLAYING=1\nTITLE=%s\nARTIST=%s\nALBUM=\nPOS=0\nDUR=240\n",title,artist);fclose(f);
 f=fopen(BT_DEVLIST_PATH,"w");assert(f);fputs("COUNT=1\nDEV=00:11:22:33:44:55|1|Example iPhone\n",f);fclose(f);
 bt_poll_status();assert(hud_source.kind==HUD_TRACK);
}
static void preview_frequency(const char*frequency){
 int changed=0;process_property(65,(const u8*)frequency,slen(frequency),&changed);assert(changed);rebuild_source();
 assert(hud_source.kind==HUD_SOURCE);
}
static void show_phone_connection(void){
 FILE*f=fopen(BT_STATUS_PATH,"w");assert(f);fputs("BT CONNECTED\n",f);fclose(f);
 f=fopen(BT_META_PATH,"w");assert(f);fputs("DEVICE=Example iPhone\nCONNECTED=1\n",f);fclose(f);
 f=fopen(BT_DEVLIST_PATH,"w");assert(f);fputs("COUNT=1\nDEV=00:11:22:33:44:55|1|Example iPhone\n",f);fclose(f);
 bt_poll_status();assert(hud_connection.kind==HUD_CONNECTION);
}
static void boot_preview(unsigned elapsed){
 spl_render(gallery_night);boot_image_save(fb);boot_fade_load();assert(boot_fade.image);
 gps.fix=0;track_valid=1;trip.active=0;route_status=hud_observed_route=R_IDLE;
 route_n=0;memzero(&roadctx,sizeof(roadctx));
 car.tempok=0;car.source[0]=car.volume[0]=0;audio_source_live=0;
 btui.connected=btui.playing=0;bt_control_mode=0;memzero(&af,sizeof(af));
 hud_reset_animations();draw();hud_capture_ms+=elapsed;
}
static void gallery_reset(int night){
 hud_capture_clock=1;hud_capture_ms=s_now_ms();gallery_motion=0;diversion_preview_clear();ml_reset();
 bt_hud_link=0;bt_hud_peer[0]=0;bt_active_written=-1;coordinates_active=0;memzero(&coordinates,sizeof(coordinates));
 route_cancel();fixture();restore_route();atlas_active=0;atlas_future_m=atlas_future_cost=0;
 pref=(struct Prefs){50,1,0,0,0,night?THEME_NIGHT:THEME_DAY,0,1,0,0,CLIO_DAY_START,CLIO_NIGHT_START};apply_palette(night);
 mapok=1;screen=SCR_MAP;trip_pane=trip_tab=trip_focus=trip_right_focus=0;menu_focus=set_focus=0;
 bt_page_focus=bt_page_connected_choice=bt_page_dialog_choice=0;
 trip_pick_mode=dest_pick_action=DEST_PICK_NONE;modal_choice=0;modal_index=-1;pending_point_stop=0;
 memzero(&phone_dest,sizeof(phone_dest));phone_seq_seen_valid=0;arrival_modal_deadline=0;
 nrecents=8;nfaves=4;copystr(faves[0].name,sizeof(faves[0].name),"Home");copystr(faves[1].name,sizeof(faves[1].name),"Work");for(int i=0;i<4;i++){faves[i].lat=5500000+i*300;faves[i].lon=-165000-i*300;}
 kbd_focus=0;kbd_action=KBD_NAME_FAVE;kbd_fave_idx=-1;kbd_text[0]=0;theme_time_part=0;
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
static void gallery_step_frame(const char*suffix){char name[240];snprintf(name,sizeof(name),"%s/steps/%s-%s",gallery_night?"night":"day",gallery_scene,suffix);settle();snapshot(name);}
static void gallery_record_start(void){gallery_step=0;if(GALLERY_INPUTS){gallery_step_frame("before");gallery_record=1;}}
static void key_handle(int code){gallery_production_key_handle(code);if(gallery_record){char suffix[24];snprintf(suffix,sizeof(suffix),"%03d",gallery_step);gallery_step_frame(suffix);printf("INPUT|%s|%s|%03d|%d\n",gallery_night?"night":"day",gallery_scene,gallery_step++,code);}}
'''.replace('GALLERY_INPUTS',str(int(args.record_inputs)))
        program='#define CLIO_BOOT_ART '+json.dumps(str(ROOT/'clio843/src/boot_art.h'))+'\n#define key_handle gallery_production_key_handle\n'+harness+HELPERS+wrappers+'''
int main(int argc,char**argv){assert(argc==2);outdir=argv[1];setbuf(stdout,0);fixture();assert(load_font());assert(cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf"));assert(load_regions());assert(load_timezones());assert(load_map());assert(load_route_data());assert(route_start(5497370,-161331));u32 deadline=s_now_ms()+120000;while(route_status==R_CALC&&time_before_u32(s_now_ms(),deadline))route_service_until(s_now_ms()+12);assert(route_status==R_READY&&route_n>1);save_route();printf("Baseline real route: %d points, %u m, %u ds\\n",saved_n,saved_m,saved_cost);static const char*names[]={NAMES};
for(int night=0;night<2;night++){THEME_FILTER gallery_night=night;for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++){gallery_reset(night);gallery_scene=names[i];switch(i){CASES}if(gallery_motion)draw();else settle();char name[180];snprintf(name,sizeof(name),"%s/%s",night?"night":"day",names[i]);snapshot(name);printf("%s %s\\n",night?"night":"day",names[i]);}}return 0;}
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
            for mode in ('enter','leave'):
                count=len(EXPLORE_SAMPLES)
                images=[Image.open(out/theme/f'explore-{mode}-{i:02}.png').copy() for i in range(count)]
                images[0].save(out/f'explore-{mode}-{theme}.gif',save_all=True,append_images=images[1:],duration=[round((b-a)/10)*10 for a,b in zip(EXPLORE_SAMPLES,EXPLORE_SAMPLES[1:])]+[600],loop=0)
        write_gallery(out,manifest)
    print('Captures:',out/'manifest.json')


if __name__=='__main__':main()
