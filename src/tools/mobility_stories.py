
"""Controlled native capture sequences for the 2.2.0 preview GIFs."""

DESTINATION_EVENTS = (
    (0, 'preview_prediction_seed();menu_open(P_TRIP,0,hud_now_ms());'),
    (1000, 'key_handle(K_DOWN);assert(sheet_focus==1);'),
    (2800, 'key_handle(K_ENTER);assert(trip.active&&streq(trip.target[0].name,"Work"));'),
    (4500, 'menu_open(P_TRIP,0,hud_now_ms());'),
)

# Synthetic fixture for rendering without installed map assets.
DIVERSION_EVENTS = (
    (0, 'preview_diversion_seed();bt_control_mode=0;key_handle(K_BACK);assert(bt_control_mode);'),
    (5000, 'hud_prepare(active_zoom(),hud_now_ms(),1);assert(hud_tween[HC_STATUS].target.flags&HUD_TRAFFIC_COMPACT);'),
    (6400, 'key_handle(K_BACK);assert(!bt_control_mode&&traffic_offer_valid());'),
    (7600, 'settle();assert(hud_tween[HC_STATUS].target.flags&HUD_TRAFFIC_COMPACT);assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3&&sheet_focus==0);'),
    (9400, 'key_handle(K_ENTER);assert(route_n==6&&!traffic_offer_valid());'),
)

# Production routing graph with controlled historical traffic observations.
MAP_DIVERSION_EVENTS = (
    (0, 'preview_map_diversion_seed();bt_control_mode=0;key_handle(K_BACK);assert(mapok&&traffic_offer_valid()&&bt_control_mode);'),
    (5000, 'hud_prepare(active_zoom(),hud_now_ms(),1);assert(hud_tween[HC_STATUS].target.flags&HUD_TRAFFIC_COMPACT);'),
    (6400, 'key_handle(K_BACK);assert(!bt_control_mode&&traffic_offer_valid());'),
    (7600, 'settle();assert(hud_tween[HC_STATUS].target.flags&HUD_TRAFFIC_COMPACT);assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3&&sheet_focus==0);'),
    (9400,
     'int expected_n=route_n+traffic_offer_n-1-'
     '(traffic_join_idx-traffic_begin_idx);'
     'key_handle(K_ENTER);'
     'assert(mapok&&route_status==R_READY&&route_n==expected_n'
     '&&!traffic_offer_valid());'),
)

# Skip static idle time in the published GIF, not in the native HUD clock.
DIVERSION_GIF_SKIP_FROM = 1000
DIVERSION_GIF_SKIP_TO = 4500
def diversion_playback_ms(ms):
    return ms - max(0, min(ms, DIVERSION_GIF_SKIP_TO) - DIVERSION_GIF_SKIP_FROM)
