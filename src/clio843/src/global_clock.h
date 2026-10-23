/* CPT1 offline timezone polygons and UTC transitions. Manual offset is fallback.
 * UTC seconds are unsigned, so the clock table is not limited by signed 2038. */
#ifndef HOST_TEST
#define TIMEZONES_PATH "/mnt/sdcard/clio843/maps/timezones.cpt"
#else
#define TIMEZONES_PATH CLIO_HOST_ROOT "clio843/maps/timezones.cpt"
#endif
static int clockfd=-1,clock_offset_seconds=0,clock_valid=0,clock_zone=-1;
static u32 clock_polys=0,clock_zones=0,clock_po=0,clock_zo=0,clock_go=0,clock_to=0,clock_start=0,clock_end=0,clock_next_ms=0,clock_size=0;
static s32 clock_lat=0x7fffffff,clock_lon=0x7fffffff;
static char clock_name[49];
static u32 clock_u32(const u8*p){return(u32)p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24;}
static int clock_read(u32 off,void*p,u32 n){if(off>clock_size||n>clock_size-off)return 0;return s_lseek(clockfd,(long)off,SEEK_SET_)>=0&&s_read(clockfd,p,n)==(long)n;}
static int load_timezones(void){u8 h[40];clockfd=(int)s_open(TIMEZONES_PATH,O_RDONLY_,0);if(clockfd<0)return 0;long n=s_lseek(clockfd,0,2);if(n<40||n>0x7fffffff)goto bad;clock_size=(u32)n;if(!clock_read(0,h,40)||h[0]!='C'||h[1]!='P'||h[2]!='T'||h[3]!='1'||h[4]!=1||h[5]||h[6]!=32||h[7])goto bad;
 clock_polys=clock_u32(h+8);clock_zones=clock_u32(h+12);clock_po=clock_u32(h+16);clock_zo=clock_u32(h+20);clock_go=clock_u32(h+24);clock_to=clock_u32(h+28);clock_start=clock_u32(h+32);clock_end=clock_u32(h+36);
 if(clock_polys>100000||clock_zones>1024||clock_po!=40||clock_zo!=clock_po+clock_polys*32||clock_go!=clock_zo+clock_zones*64||clock_to<clock_go||clock_to>clock_size||clock_end<=clock_start)goto bad;return 1;
 bad:s_close(clockfd);clockfd=-1;clock_valid=0;return 0;}
static u32 clock_gps_epoch(void){if(gps.year<2020||gps.year>2060||gps.hour<0||gps.hour>23||gps.min<0||gps.min>59||gps.sec<0||gps.sec>60)return 0;int d=day_of_year0();if(d<0)return 0;u32 days=18262u+(u32)d;for(int y=2020;y<gps.year;y++)days+=leap_year(y)?366:365;return days*86400u+(u32)gps.hour*3600u+(u32)gps.min*60u+(u32)(gps.sec==60?59:gps.sec);}
static int clock_polygon(u32 off,u32 nr,s32 lat,s32 lon){if(nr>10000||off>clock_to-clock_go)return 0;u32 pos=clock_go+off;int inside=0;u8 b[1024];for(u32 r=0;r<nr;r++){if(!clock_read(pos,b,4))return 0;pos+=4;u32 n=clock_u32(b);if(n<4||n>2000000)return 0;s32 la0=0,lo0=0;int ring=0;for(u32 i=0;i<n;){u32 count=n-i;if(count>128)count=128;if(!clock_read(pos,b,count*8))return 0;pos+=count*8;for(u32 j=0;j<count;j++,i++){const u8*p=b+j*8;s32 la=(s32)clock_u32(p),lo=(s32)clock_u32(p+4);if(i&&((la>lat)!=(la0>lat))){long long a=(long long)(lon-lo)*(la0-la),c=(long long)(lo0-lo)*(lat-la);if((la0>la)?a<c:a>c)ring=!ring;}la0=la;lo0=lo;}}inside^=ring;}return inside;}
static int clock_resolve(s32 lat,s32 lon){u8 b[32];for(u32 i=0;i<clock_polys;i++){if(!clock_read(clock_po+i*32,b,32))return-1;s32 a=(s32)clock_u32(b),l=(s32)clock_u32(b+4),c=(s32)clock_u32(b+8),d=(s32)clock_u32(b+12);if(lat<a||lon<l||lat>c||lon>d)continue;u32 z=clock_u32(b+24);if(z<clock_zones&&clock_polygon(clock_u32(b+16),clock_u32(b+20),lat,lon))return(int)z;}return-1;}
static int clock_transition(int zid,u32 utc,int*out){u8 b[64];if(zid<0||(u32)zid>=clock_zones||utc<clock_start||utc>=clock_end||!clock_read(clock_zo+(u32)zid*64,b,64))return 0;u32 first=clock_u32(b+48),count=clock_u32(b+52);int initial=(s32)clock_u32(b+56);if(first>(clock_size-clock_to)/8||count>(clock_size-clock_to)/8-first)return 0;for(int i=0;i<48;i++)clock_name[i]=(char)b[i];clock_name[48]=0;u32 lo=0,hi=count;while(lo<hi){u32 mid=lo+(hi-lo)/2;if(!clock_read(clock_to+(first+mid)*8,b,8))return 0;if(clock_u32(b)<=utc)lo=mid+1;else hi=mid;}if(lo){if(!clock_read(clock_to+(first+lo-1)*8,b,8))return 0;initial=(s32)clock_u32(b+4);}if(initial< -43200||initial>50400)return 0;*out=initial;return 1;}
static void clock_service(void){if(!pref.clock_auto||clockfd<0)return;u32 now=s_now_ms();if(clock_next_ms&&(s32)(now-clock_next_ms)<0)return;clock_next_ms=now+1000;if(!gps.fix)return;u32 utc=clock_gps_epoch();if(!utc)return;if(gps.lat!=clock_lat||gps.lon!=clock_lon){clock_zone=clock_resolve(gps.lat,gps.lon);clock_lat=gps.lat;clock_lon=gps.lon;}int offset;if(!clock_transition(clock_zone,utc,&offset)){clock_valid=0;return;}clock_valid=1;clock_offset_seconds=offset;int minutes=offset/60;if(pref.hoff*60+pref.moff!=minutes){pref.hoff=minutes/60;pref.moff=minutes%60;prefs_mark_dirty();}}
