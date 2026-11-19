
"""Controlled native capture sequences for the 2.2.0 preview GIFs."""

DESTINATION_EVENTS = (
    (0, 'preview_prediction_seed();bt_control_mode=0;key_handle(K_BACK);assert(bt_control_mode);'),
    (1400, 'key_handle(K_BACK);assert(!bt_control_mode);'),
    (2600, 'settle();assert(hud_suggestion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==4&&sheet_focus==0);'),
    (4400, 'key_handle(K_ENTER);assert(trip.active&&streq(trip.target[0].name,"Work")&&!mob_suggest_valid);'),
)

# Synthetic fixture for rendering without installed map assets.
DIVERSION_EVENTS = (
    (0, 'preview_diversion_seed();bt_control_mode=0;key_handle(K_BACK);assert(bt_control_mode);'),
    (1400, 'key_handle(K_BACK);assert(!bt_control_mode&&traffic_offer_valid());'),
    (2600, 'settle();assert(streq(hud_tween[HC_STATUS].target.traffic_hint,"OK to review"));assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3&&sheet_focus==0);'),
    (4400, 'key_handle(K_ENTER);assert(route_n==6&&!traffic_offer_valid());'),
)

# Production routing graph with controlled historical traffic observations.
MAP_DIVERSION_EVENTS = (
    (0, 'preview_map_diversion_seed();bt_control_mode=0;key_handle(K_BACK);assert(mapok&&traffic_offer_valid()&&bt_control_mode);'),
    (1400, 'key_handle(K_BACK);assert(!bt_control_mode&&traffic_offer_valid());'),
    (2600, 'settle();assert(streq(hud_tween[HC_STATUS].target.traffic_hint,"OK to review"));assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3&&sheet_focus==0);'),
    (4400,
     'int expected_n=route_n+traffic_offer_n-1-'
     '(traffic_join_idx-traffic_begin_idx);'
     'key_handle(K_ENTER);'
     'assert(mapok&&route_status==R_READY&&route_n==expected_n'
     '&&!traffic_offer_valid());'),
)
