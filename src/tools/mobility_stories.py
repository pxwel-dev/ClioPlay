
"""Controlled native capture sequences for the 2.2.0 preview GIFs."""

DESTINATION_EVENTS = (
    (0, 'preview_prediction_seed();bt_control_mode=0;key_handle(K_BACK);assert(bt_control_mode);'),
    (1400, 'key_handle(K_BACK);assert(!bt_control_mode);'),
    (2600, 'settle();assert(hud_suggestion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==4&&sheet_focus==0);'),
    (4400, 'key_handle(K_ENTER);assert(trip.active&&streq(trip.target[0].name,"Work")&&!mob_suggest_valid);'),
)

# Controlled stationary startup; a named Recent supplies the editable draft.
# No waiting for the three-second stationary gate in the published playback.
FREQUENT_PLACE_EVENTS = (
    (0, 'preview_frequent_seed();'),
    (900, 'preview_frequent_refresh();key_handle(K_LEFT);assert(sheet_focus==0);'),
    (1600, 'preview_frequent_refresh();key_handle(K_ENTER);assert(sheet_page==P_NAME&&keyboard_origin==KBD_NAME_FREQUENT&&streq(kbd_text,"Work 2")&&!nfaves);'),
    (2600, 'preview_frequent_refresh();key_handle(K_ESC);assert(streq(kbd_text,"Work "));'),
    (3000, 'key_handle(K_ESC);assert(streq(kbd_text,"Work"));'),
    (3500, 'preview_frequent_refresh();key_handle(K_UP);for(int i=0;i<5&&pk_focus!=PK_SAVE;i++)key_handle(K_RIGHT);assert(pk_focus==PK_SAVE);'),
    (4200, 'key_handle(K_ENTER);assert(screen==SCR_MAP&&nfaves==1&&streq(faves[0].name,"Work")&&!trip.active&&streq(notice_text,ui_tr("Favourite saved")));'),
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
