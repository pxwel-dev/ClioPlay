
"""Controlled native capture sequences for the 2.2.0 preview GIFs."""

DESTINATION_EVENTS = (
    (0, 'preview_prediction_seed();menu_open(P_TRIP,0,hud_now_ms());'),
    (1000, 'key_handle(K_DOWN);assert(sheet_focus==1);'),
    (2800, 'key_handle(K_ENTER);assert(trip.active&&streq(trip.target[0].name,"Work"));'),
    (4500, 'menu_open(P_TRIP,0,hud_now_ms());'),
)

# Synthetic fixture for rendering without installed map assets.
DIVERSION_EVENTS = (
    (0, 'preview_diversion_seed();'),
    (1400, 'menu_open(P_TRIP,trip.count+2,hud_now_ms());assert(traffic_offer_valid());'),
    (2600, 'key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3);'),
    (4000, 'key_handle(K_LEFT);assert(sheet_focus==0);'),
    (4700, 'key_handle(K_ENTER);assert(route_n==6&&!traffic_offer_valid());'),
)

# Production routing graph with controlled historical traffic observations.
MAP_DIVERSION_EVENTS = (
    (0, 'preview_map_diversion_seed();assert(mapok&&traffic_offer_valid());'),
    (1400, 'menu_open(P_TRIP,trip.count+2,hud_now_ms());assert(traffic_offer_valid());'),
    (2600, 'key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3);'),
    (4000, 'key_handle(K_LEFT);assert(sheet_focus==0);'),
    (4700,
     'int expected_n=route_n+traffic_offer_n-1-'
     '(traffic_join_idx-traffic_begin_idx);'
     'key_handle(K_ENTER);'
     'assert(mapok&&route_status==R_READY&&route_n==expected_n'
     '&&!traffic_offer_valid());'),
)
