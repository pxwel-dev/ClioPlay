/* PCM-only copies: aligned words on ARMv4T, byte-safe fallback otherwise.
 * may_alias permits accessing the PCM byte array through word lvalues. */
typedef u32 AudioPcmWord __attribute__((__may_alias__));
static void audio_pcm_copy(u8*d,const u8*s,u32 n){
 if((((unsigned long)d)|((unsigned long)s)|n)&3u){memcopy(d,s,n);return;}
 AudioPcmWord*to=(AudioPcmWord*)d;const AudioPcmWord*from=(const AudioPcmWord*)s;
 while(n){*to++=*from++;n-=4u;}
}
/* Keep the primary at about 70 ms at every accepted output rate. */
static u32 audio_primary_frames(int div){return div==4?256u:div==2?512u:1024u;}
/* queued is a bounded secondary-ring byte count. The primary may still hold
 * another 70 ms after GETPOS has advanced; leave a 40 ms scheduling margin.
 * A floor avoids reacting to short packet gaps. Unsigned time handles wrap. */
static int audio_idle_expired(u32 now,u32 feed_at,u32 queued,u32 bytes_per_second){
 if(!bytes_per_second)return 0;
 u32 drain=udiv32(queued*1000u+bytes_per_second-1u,bytes_per_second)+70u+40u;
 if(drain<250u)drain=250u;
 u32 elapsed=now-feed_at;return !(elapsed&0x80000000u)&&elapsed>=drain;
}
