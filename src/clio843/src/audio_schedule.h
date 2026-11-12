/* Elapsed-time maintenance, independent of media/socket wakeups. Polling audio
 * stays immediate; missed maintenance intervals coalesce instead of replaying. */
struct AudioPeriodic {u32 at;int initialized;};
static u32 audio_periodic(struct AudioPeriodic*p,u32 now,u32 period){
 if(!p->initialized){p->initialized=1;p->at=now;return 1;}
 u32 elapsed=now-p->at;if(elapsed<period)return 0;
 u32 steps=elapsed/period;p->at+=steps*period;return steps;
}
static int audio_periodic_wait(const struct AudioPeriodic*p,u32 now,u32 period){
 if(!p->initialized)return 0;
 u32 elapsed=now-p->at;return elapsed>=period?0:(int)(period-elapsed);
}
static int audio_countdown(int remaining,u32 steps){
 if(remaining<=0)return remaining;
 return steps>=(u32)remaining?0:remaining-(int)steps;
}
