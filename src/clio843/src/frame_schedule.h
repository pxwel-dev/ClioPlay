/* Independent clocks; expired frames are skipped, never replayed in a burst. */
#define FRAME_MODEL_MS 100u
#define FRAME_UI_MS 67u /* Approximately 15 Hz on the millisecond clock. */
#define FRAME_MAP_MS 100u
#define FRAME_CALC_MAP_MS 200u
#define FRAME_ROUTE_CHUNK_MS 5u
#define FRAME_GUARD_MS 2u
struct FrameClock {u32 next;};
static int frame_due(u32 now,u32 deadline){return (s32)(now-deadline)>=0;}
static u32 frame_after(u32 deadline,u32 now,u32 period){
 if(!frame_due(now,deadline))return deadline;
 return deadline+((now-deadline)/period+1u)*period;
}
/* Calculation caps everything; otherwise steady values share the map rate. */
static u32 frame_ui_period(int calculating,int transition){
 return calculating?FRAME_CALC_MAP_MS:transition?FRAME_UI_MS:FRAME_MAP_MS;
}
static void frame_advance_ui(struct FrameClock*c,u32 now,int calculating,int transition){
 c->next=frame_after(c->next,now,frame_ui_period(calculating,transition));
}
static u32 frame_earlier(u32 a,u32 b){return (s32)(a-b)<0?a:b;}
static u32 frame_route_budget(u32 now,u32 deadline){
 if(frame_due(now,deadline)||deadline-now<=FRAME_GUARD_MS)return 0;
 u32 ms=deadline-now-FRAME_GUARD_MS;return ms<FRAME_ROUTE_CHUNK_MS?ms:FRAME_ROUTE_CHUNK_MS;
}
static int frame_ease(u32 elapsed,u32 duration){
 if(elapsed>=duration)return 256;u32 t=elapsed*256u/duration;
 return (int)((t*t*(768u-2u*t)+32768u)>>16);
}
