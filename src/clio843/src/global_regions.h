/* CPG1 offline country polygons. No heap allocation, network or floating point.
 * Country boundaries are generalized: Auto has a manual unit override. */
#ifndef HOST_TEST
#define REGIONS_PATH "/mnt/sdcard/clio843/maps/regions.cpg"
#define CAMERAS_PATH "/mnt/sdcard/clio843/maps/cameras.cpc"
#else
#define REGIONS_PATH CLIO_HOST_ROOT "clio843/maps/regions.cpg"
#define CAMERAS_PATH CLIO_HOST_ROOT "clio843/maps/cameras.cpc"
#endif
static int regionfd=-1,region_flags=0;
static char region_iso[3]={0,0,0};
static u32 region_count=0,region_data=0,region_next_ms=0;
static s32 region_last_lat=0x7fffffff,region_last_lon=0x7fffffff;
static int load_regions(void){u8 h[16];regionfd=(int)s_open(REGIONS_PATH,O_RDONLY_,0);if(regionfd<0)return 0;if(s_read(regionfd,h,16)!=16||h[0]!='C'||h[1]!='P'||h[2]!='G'||h[3]!='1'||rd16(h+4)!=1||rd16(h+6)!=32)goto bad;region_count=rd32u(h+8);region_data=rd32u(h+12);if(region_count>200000||region_data!=16+region_count*32)goto bad;return 1;bad:s_close(regionfd);regionfd=-1;return 0;}
static int region_polygon(u32 off,u32 nr,s32 lat,s32 lon){
 if(nr>10000)return 0;int inside=0;s_lseek(regionfd,(long)(region_data+off),SEEK_SET_);
 for(u32 r=0;r<nr;r++){u8 b[8];if(s_read(regionfd,b,4)!=4)return 0;u32 n=rd32u(b);if(n<4||n>2000000)return 0;s32 la0=0,lo0=0;int ring=0;
 u8 points[1024];for(u32 i=0;i<n;){u32 count=n-i;if(count>128)count=128;if(s_read(regionfd,points,count*8)!=(long)(count*8))return 0;for(u32 j=0;j<count;j++,i++){const u8*p=points+j*8;s32 la=(s32)rd32u(p),lo=(s32)rd32u(p+4);if(i&&((la>lat)!=(la0>lat))){long long lhs=(long long)(lon-lo)*(la0-la),rhs=(long long)(lo0-lo)*(lat-la);if((la0>la)?lhs<rhs:lhs>rhs)ring=!ring;}la0=la;lo0=lo;}}
 inside^=ring;
 }return inside;
}
static void region_resolve(s32 lat,s32 lon){region_flags=0;region_iso[0]=region_iso[1]=0;if(regionfd<0){/* Old released maps were UK-only. Preserve their Auto display; cameras fail closed. */if(mapok&&mapfmt<=4&&mh.minlat>=4800000&&mh.maxlat<=6200000&&mh.minlon>=-1000000&&mh.maxlon<=300000){region_flags=5;region_iso[0]='G';region_iso[1]='B';}return;}
 for(u32 i=0;i<region_count;i++){u8 b[32];s_lseek(regionfd,(long)(16+i*32),SEEK_SET_);if(s_read(regionfd,b,32)!=32)return;s32 a=(s32)rd32u(b+4),l=(s32)rd32u(b+8),c=(s32)rd32u(b+12),d=(s32)rd32u(b+16);if(lat<a||lon<l||lat>c||lon>d)continue;if(region_polygon(rd32u(b+20),rd32u(b+24),lat,lon)){region_iso[0]=(char)b[0];region_iso[1]=(char)b[1];region_flags=b[2];return;}}
}
static void region_service(void){u32 now=s_now_ms();if(region_next_ms&&(s32)(now-region_next_ms)<0)return;region_next_ms=now+1000;if(!gps.fix)return;if(gps.lat==region_last_lat&&gps.lon==region_last_lon)return;region_last_lat=gps.lat;region_last_lon=gps.lon;region_resolve(gps.lat,gps.lon);}
static void append_metric_distance(char*b,unsigned cap,u32 m,int manoeuvre){if(manoeuvre&&m<70){appendstr(b,cap,ui_tr("NOW"));return;}if(m<1000){append_int(b,cap,(int)(manoeuvre?((m+5)/10)*10:m));appendstr(b,cap," M");return;}u32 km10=(m+50)/100;if(km10>=1000){append_int(b,cap,(int)((km10+5)/10));}else{append_int(b,cap,(int)(km10/10));appendstr(b,cap,".");char d[2]={(char)('0'+km10%10),0};appendstr(b,cap,d);}appendstr(b,cap," KM");}
