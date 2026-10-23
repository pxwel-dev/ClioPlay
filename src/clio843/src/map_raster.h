/* Integer coverage rasterization for the existing 480x202 RGB565 map.
 * No supersampled framebuffer, floating point, extra map reads or heap storage. */
#ifndef CLIO_MAP_RASTER_H
#define CLIO_MAP_RASTER_H
#ifndef MAP_ANTIALIAS
#define MAP_ANTIALIAS 1
#endif
/* One coverage plane packs six road classes into 30 bits per map pixel.
 * Segments/features of the same class take the maximum coverage, then blend
 * once. This prevents bright/dark blobs at shared endpoints and road junctions.
 * The plane is reused for context strokes, roads, route and vehicle outline. */
#if MAP_ANTIALIAS
static u32 map_coverage[W*(MAP_STRIP_Y-40)];
static int map_dirty_lo[MAP_STRIP_Y-40],map_dirty_hi[MAP_STRIP_Y-40];
static u16 map_coverage_cols[6];
static int map_mask_active=0,map_mask_shift=0,map_mask_count=0;
static void map_mask_begin(int kind){
 for(int y=0;y<MAP_STRIP_Y-40;y++){map_dirty_lo[y]=W;map_dirty_hi[y]=-1;}
 map_mask_active=1;map_mask_shift=0;
 if(kind==0){map_mask_count=3;map_coverage_cols[0]=C_WATER;map_coverage_cols[1]=C_GREEN;map_coverage_cols[2]=C_BLUE;}
 else if(kind==1){map_mask_count=6;map_coverage_cols[0]=C_ROAD;map_coverage_cols[1]=C_DIM;map_coverage_cols[2]=C_MUTED;map_coverage_cols[3]=C_WHITE;map_coverage_cols[4]=C_BLUE;map_coverage_cols[5]=C_CYAN;}
 else{map_mask_count=1;map_coverage_cols[0]=kind==2?C_ROUTE:C_CAR;}
}
static void map_mask_select(int slot){map_mask_shift=slot*5;}
#else
#define map_mask_begin(kind) ((void)0)
#define map_mask_select(slot) ((void)0)
#define map_mask_end() ((void)0)
#endif
/* Packed red/blue lanes have guard bits for the weighted sum. */
static void map_blend(int x,int y,u16 c,int a){
 if(a<=0||(unsigned)x>=W||y<40||y>=MAP_STRIP_Y)return;
#if MAP_ANTIALIAS
 if(map_mask_active){
  int row=y-40;u32*p=&map_coverage[row*W+x],value=(u32)((a*31+16)>>5),old=(*p>>map_mask_shift)&31u;
  if(value>old)*p=(*p&~(31u<<map_mask_shift))|(value<<map_mask_shift);
  if(x<map_dirty_lo[row])map_dirty_lo[row]=x;if(x>map_dirty_hi[row])map_dirty_hi[row]=x;return;
 }
#endif
 u16*d=&fb[y*W+x];if(a>=32){*d=c;return;}if(*d==c)return;
 u32 v=*d,rb=(((v&0xf81fu)*(u32)(32-a)+(c&0xf81fu)*(u32)a)>>5)&0xf81fu;
 u32 g=(((v&0x07e0u)*(u32)(32-a)+(c&0x07e0u)*(u32)a)>>5)&0x07e0u;
 *d=(u16)(rb|g);
}
#if MAP_ANTIALIAS
static void map_mask_end(void){
 map_mask_active=0;
 for(int row=0;row<MAP_STRIP_Y-40;row++)for(int x=map_dirty_lo[row];x<=map_dirty_hi[row];x++){
  u32*p=&map_coverage[row*W+x],bits=*p;if(!bits)continue;*p=0;
  /* Blend active classes locally, loading/storing the framebuffer once. */
  u16 out=fb[(row+40)*W+x];int slot=0;
  while(bits){int a=(int)(bits&31u);if(a){u16 c=map_coverage_cols[slot];
    if(a==31)out=c;
    else{a+=a>>4;u32 rb=(((out&0xf81fu)*(u32)(32-a)+(c&0xf81fu)*(u32)a)>>5)&0xf81fu;
      u32 g=(((out&0x07e0u)*(u32)(32-a)+(c&0x07e0u)*(u32)a)>>5)&0x07e0u;out=(u16)(rb|g);}
   }bits>>=5;slot++;
  }fb[(row+40)*W+x]=out;
 }
}
#endif
static void map_solid(int x,int y,int w,int h,u16 c){
 if(x<0){w+=x;x=0;}if(y<40){h+=y-40;y=40;}if(x+w>W)w=W-x;if(y+h>MAP_STRIP_Y)h=MAP_STRIP_Y-y;if(w<=0||h<=0)return;
#if MAP_ANTIALIAS
 if(map_mask_active){u32 bits=31u<<map_mask_shift;
  for(int row=y-40;row<y-40+h;row++){if(x<map_dirty_lo[row])map_dirty_lo[row]=x;if(x+w-1>map_dirty_hi[row])map_dirty_hi[row]=x+w-1;
   u32*p=&map_coverage[row*W+x];for(int i=0;i<w;i++)p[i]|=bits;
  }return;
 }
#endif
 fill(x,y,w,h,c);
}
/* Subpixel round caps use a small read-only square-root table. Geometry is
 * kept in 1/256-pixel coordinates until coverage is written; short route
 * segments therefore do not snap into a chain of square or circular blobs. */
static int map_round_coverage_q8(int dx,int dy,int width){
 int outer=(width+1)*128;
 if(dx<=-outer||dx>=outer||dy<=-outer||dy>=outer)return 0;
 int d=dx*dx+dy*dy;if(d>=outer*outer)return 0;
 static const u16 distance[2305]={0,16,22,27,32,35,39,42,45,48,50,53,55,57,59,61,64,65,67,69,71,73,75,76,78,80,81,83,84,86,87,89,90,91,93,94,96,97,98,99,101,102,103,104,106,107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,128,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,144,145,146,147,148,149,150,150,151,152,153,154,155,155,156,157,158,159,160,160,161,162,163,163,164,165,166,167,167,168,169,170,170,171,172,173,173,174,175,176,176,177,178,178,179,180,181,181,182,183,183,184,185,185,186,187,187,188,189,189,190,191,192,192,193,193,194,195,195,196,197,197,198,199,199,200,201,201,202,203,203,204,204,205,206,206,207,208,208,209,209,210,211,211,212,212,213,214,214,215,215,216,217,217,218,218,219,219,220,221,221,222,222,223,224,224,225,225,226,226,227,227,228,229,229,230,230,231,231,232,232,233,234,234,235,235,236,236,237,237,238,238,239,240,240,241,241,242,242,243,243,244,244,245,245,246,246,247,247,248,248,249,249,250,250,251,251,252,252,253,253,254,254,255,256,256,256,257,257,258,258,259,259,260,260,261,261,262,262,263,263,264,264,265,265,266,266,267,267,268,268,269,269,270,270,271,271,272,272,272,273,273,274,274,275,275,276,276,277,277,278,278,278,279,279,280,280,281,281,282,282,283,283,283,284,284,285,285,286,286,287,287,288,288,288,289,289,290,290,291,291,291,292,292,293,293,294,294,295,295,295,296,296,297,297,298,298,298,299,299,300,300,301,301,301,302,302,303,303,304,304,304,305,305,306,306,306,307,307,308,308,309,309,309,310,310,311,311,311,312,312,313,313,313,314,314,315,315,315,316,316,317,317,317,318,318,319,319,320,320,320,321,321,321,322,322,323,323,323,324,324,325,325,325,326,326,327,327,327,328,328,329,329,329,330,330,331,331,331,332,332,332,333,333,334,334,334,335,335,336,336,336,337,337,337,338,338,339,339,339,340,340,340,341,341,342,342,342,343,343,343,344,344,345,345,345,346,346,346,347,347,347,348,348,349,349,349,350,350,350,351,351,352,352,352,353,353,353,354,354,354,355,355,355,356,356,357,357,357,358,358,358,359,359,359,360,360,360,361,361,362,362,362,363,363,363,364,364,364,365,365,365,366,366,366,367,367,368,368,368,369,369,369,370,370,370,371,371,371,372,372,372,373,373,373,374,374,374,375,375,375,376,376,376,377,377,377,378,378,378,379,379,379,380,380,380,381,381,381,382,382,382,383,383,384,384,384,384,385,385,385,386,386,386,387,387,387,388,388,388,389,389,389,390,390,390,391,391,391,392,392,392,393,393,393,394,394,394,395,395,395,396,396,396,397,397,397,398,398,398,399,399,399,400,400,400,400,401,401,401,402,402,402,403,403,403,404,404,404,405,405,405,406,406,406,406,407,407,407,408,408,408,409,409,409,410,410,410,411,411,411,411,412,412,412,413,413,413,414,414,414,415,415,415,416,416,416,416,417,417,417,418,418,418,419,419,419,419,420,420,420,421,421,421,422,422,422,423,423,423,423,424,424,424,425,425,425,426,426,426,426,427,427,427,428,428,428,429,429,429,429,430,430,430,431,431,431,432,432,432,432,433,433,433,434,434,434,434,435,435,435,436,436,436,437,437,437,437,438,438,438,439,439,439,439,440,440,440,441,441,441,441,442,442,442,443,443,443,443,444,444,444,445,445,445,445,446,446,446,447,447,447,448,448,448,448,449,449,449,449,450,450,450,451,451,451,451,452,452,452,453,453,453,453,454,454,454,455,455,455,455,456,456,456,457,457,457,457,458,458,458,459,459,459,459,460,460,460,460,461,461,461,462,462,462,462,463,463,463,464,464,464,464,465,465,465,465,466,466,466,467,467,467,467,468,468,468,468,469,469,469,470,470,470,470,471,471,471,471,472,472,472,473,473,473,473,474,474,474,474,475,475,475,475,476,476,476,477,477,477,477,478,478,478,478,479,479,479,480,480,480,480,481,481,481,481,482,482,482,482,483,483,483,483,484,484,484,485,485,485,485,486,486,486,486,487,487,487,487,488,488,488,488,489,489,489,490,490,490,490,491,491,491,491,492,492,492,492,493,493,493,493,494,494,494,494,495,495,495,496,496,496,496,497,497,497,497,498,498,498,498,499,499,499,499,500,500,500,500,501,501,501,501,502,502,502,502,503,503,503,503,504,504,504,504,505,505,505,505,506,506,506,506,507,507,507,507,508,508,508,508,509,509,509,509,510,510,510,510,511,511,511,512,512,512,512,512,513,513,513,513,514,514,514,514,515,515,515,515,516,516,516,516,517,517,517,517,518,518,518,518,519,519,519,519,520,520,520,520,521,521,521,521,522,522,522,522,523,523,523,523,524,524,524,524,525,525,525,525,526,526,526,526,527,527,527,527,528,528,528,528,528,529,529,529,529,530,530,530,530,531,531,531,531,532,532,532,532,533,533,533,533,534,534,534,534,534,535,535,535,535,536,536,536,536,537,537,537,537,538,538,538,538,539,539,539,539,539,540,540,540,540,541,541,541,541,542,542,542,542,543,543,543,543,544,544,544,544,544,545,545,545,545,546,546,546,546,547,547,547,547,547,548,548,548,548,549,549,549,549,550,550,550,550,551,551,551,551,551,552,552,552,552,553,553,553,553,554,554,554,554,554,555,555,555,555,556,556,556,556,557,557,557,557,557,558,558,558,558,559,559,559,559,560,560,560,560,560,561,561,561,561,562,562,562,562,562,563,563,563,563,564,564,564,564,565,565,565,565,565,566,566,566,566,567,567,567,567,567,568,568,568,568,569,569,569,569,569,570,570,570,570,571,571,571,571,571,572,572,572,572,573,573,573,573,573,574,574,574,574,575,575,575,575,576,576,576,576,576,577,577,577,577,577,578,578,578,578,579,579,579,579,579,580,580,580,580,581,581,581,581,581,582,582,582,582,583,583,583,583,583,584,584,584,584,585,585,585,585,585,586,586,586,586,587,587,587,587,587,588,588,588,588,588,589,589,589,589,590,590,590,590,590,591,591,591,591,592,592,592,592,592,593,593,593,593,593,594,594,594,594,595,595,595,595,595,596,596,596,596,596,597,597,597,597,598,598,598,598,598,599,599,599,599,599,600,600,600,600,601,601,601,601,601,602,602,602,602,602,603,603,603,603,603,604,604,604,604,605,605,605,605,605,606,606,606,606,606,607,607,607,607,608,608,608,608,608,609,609,609,609,609,610,610,610,610,610,611,611,611,611,611,612,612,612,612,613,613,613,613,613,614,614,614,614,614,615,615,615,615,615,616,616,616,616,616,617,617,617,617,618,618,618,618,618,619,619,619,619,619,620,620,620,620,620,621,621,621,621,621,622,622,622,622,622,623,623,623,623,624,624,624,624,624,625,625,625,625,625,626,626,626,626,626,627,627,627,627,627,628,628,628,628,628,629,629,629,629,629,630,630,630,630,630,631,631,631,631,631,632,632,632,632,632,633,633,633,633,633,634,634,634,634,634,635,635,635,635,635,636,636,636,636,636,637,637,637,637,637,638,638,638,638,638,639,639,639,639,640,640,640,640,640,640,641,641,641,641,641,642,642,642,642,642,643,643,643,643,643,644,644,644,644,644,645,645,645,645,645,646,646,646,646,646,647,647,647,647,647,648,648,648,648,648,649,649,649,649,649,650,650,650,650,650,651,651,651,651,651,652,652,652,652,652,653,653,653,653,653,654,654,654,654,654,655,655,655,655,655,656,656,656,656,656,656,657,657,657,657,657,658,658,658,658,658,659,659,659,659,659,660,660,660,660,660,661,661,661,661,661,662,662,662,662,662,662,663,663,663,663,663,664,664,664,664,664,665,665,665,665,665,666,666,666,666,666,667,667,667,667,667,667,668,668,668,668,668,669,669,669,669,669,670,670,670,670,670,671,671,671,671,671,672,672,672,672,672,672,673,673,673,673,673,674,674,674,674,674,675,675,675,675,675,675,676,676,676,676,676,677,677,677,677,677,678,678,678,678,678,679,679,679,679,679,679,680,680,680,680,680,681,681,681,681,681,682,682,682,682,682,682,683,683,683,683,683,684,684,684,684,684,685,685,685,685,685,685,686,686,686,686,686,687,687,687,687,687,688,688,688,688,688,688,689,689,689,689,689,690,690,690,690,690,690,691,691,691,691,691,692,692,692,692,692,693,693,693,693,693,693,694,694,694,694,694,695,695,695,695,695,695,696,696,696,696,696,697,697,697,697,697,697,698,698,698,698,698,699,699,699,699,699,699,700,700,700,700,700,701,701,701,701,701,701,702,702,702,702,702,703,703,703,703,703,704,704,704,704,704,704,705,705,705,705,705,705,706,706,706,706,706,707,707,707,707,707,707,708,708,708,708,708,709,709,709,709,709,709,710,710,710,710,710,711,711,711,711,711,711,712,712,712,712,712,713,713,713,713,713,713,714,714,714,714,714,715,715,715,715,715,715,716,716,716,716,716,716,717,717,717,717,717,718,718,718,718,718,718,719,719,719,719,719,720,720,720,720,720,720,721,721,721,721,721,721,722,722,722,722,722,723,723,723,723,723,723,724,724,724,724,724,724,725,725,725,725,725,726,726,726,726,726,726,727,727,727,727,727,727,728,728,728,728,728,729,729,729,729,729,729,730,730,730,730,730,730,731,731,731,731,731,731,732,732,732,732,732,733,733,733,733,733,733,734,734,734,734,734,734,735,735,735,735,735,736,736,736,736,736,736,737,737,737,737,737,737,738,738,738,738,738,738,739,739,739,739,739,739,740,740,740,740,740,741,741,741,741,741,741,742,742,742,742,742,742,743,743,743,743,743,743,744,744,744,744,744,744,745,745,745,745,745,746,746,746,746,746,746,747,747,747,747,747,747,748,748,748,748,748,748,749,749,749,749,749,749,750,750,750,750,750,750,751,751,751,751,751,752,752,752,752,752,752,753,753,753,753,753,753,754,754,754,754,754,754,755,755,755,755,755,755,756,756,756,756,756,756,757,757,757,757,757,757,758,758,758,758,758,758,759,759,759,759,759,759,760,760,760,760,760,760,761,761,761,761,761,761,762,762,762,762,762,762,763,763,763,763,763,763,764,764,764,764,764,764,765,765,765,765,765,765,766,766,766,766,766,766,767,767,767,767,767,768};
 int a=(outer-(int)distance[d>>8]+4)>>3;return a>32?32:a;
}
static void map_dot_q8(int x,int y,u16 c,int width){
 int r=width/2+1,cx=x>>8,cy=y>>8;
 for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)map_blend(cx+i,cy+j,c,map_round_coverage_q8((cx+i)*256-x,(cy+j)*256-y,width));
}
/* Binary fractional division avoids an overflowing numerator and needs only
 * sixteen iterations once per segment, with no division inside the pixel loop. */
static int map_ratio_q16(u32 n,u32 d){
 if(!n||!d)return 0;if(n>=d)return 65536;u32 q=0,r=n;
 for(int i=0;i<16;i++){q<<=1;r<<=1;if(r>=d){r-=d;q++;}}
 if(r>=d-r)q++;return (int)q;
}
#include "map_thin_coverage.h"
#if MAP_ANTIALIAS
/* Minor-road tables already contain the low five-bit coverage lane. Bounds
 * are clipped by the caller; avoid generic blend/colour/slot work per fringe. */
static inline __attribute__((always_inline)) void map_minor_store(int x,int y,int a){
 int row=y-40;u32*p=&map_coverage[row*W+x];
 if((unsigned)a>(*p&31u))*p=(*p&~31u)|(u32)a;
 if(x<map_dirty_lo[row])map_dirty_lo[row]=x;if(x>map_dirty_hi[row])map_dirty_hi[row]=x;
}
#else
#define map_minor_store(x,y,a) ((void)0)
#endif
static int map_minor_cap(int dx,int dy){
 if(dx<=-768||dx>=768||dy<=-768||dy>=768)return 0;
 unsigned d=(unsigned)(dx*dx+dy*dy+256)>>9;return d<1025?map_minor_caps[d]:0;
}
static void map_line_q8(int x0,int y0,int x1,int y1,u16 c,int wid){
 if(!MAP_ANTIALIAS){line1(x0>>8,y0>>8,x1>>8,y1>>8,c,wid);return;}
 int soft=0;
#if MAP_ANTIALIAS
 /* Only the thinnest road class gets the wider reconstruction filter. */
 soft=wid==1&&map_mask_active&&map_mask_count==6&&map_mask_shift==0;
#endif
 int dx=x1-x0,dy=y1-y0,steep=(dy<0?-dy:dy)>(dx<0?-dx:dx);
 if(steep){int t=x0;x0=y0;y0=t;t=x1;x1=y1;y1=t;}
 if(x0>x1){int t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;}
 dx=x1-x0;dy=y1-y0;
 int stroke_width=(wid/2)*2+1,cap=stroke_width/2+1;
 if(!dx&&soft){int cx=x0>>8,cy=y0>>8;
  for(int j=-3;j<=3;j++)for(int i=-3;i<=3;i++){
   int a=map_minor_cap((cx+i)*256-x0,(cy+j)*256-y0);
   if(!a)continue;
   if(steep){if((unsigned)(cy+j)<W&&cx+i>=40&&cx+i<MAP_STRIP_Y)map_minor_store(cy+j,cx+i,a);}
   else if((unsigned)(cx+i)<W&&cy+j>=40&&cy+j<MAP_STRIP_Y)map_minor_store(cx+i,cy+j,a);
  }return;
 }
 if(!dx){if(steep)map_dot_q8(y0,x0,c,stroke_width);else map_dot_q8(x0,y0,c,stroke_width);return;}
 /* Axis-aligned bodies remain fast spans; only their round caps need coverage. */
 if(!soft&&!dy&&!((x0|x1|y0)&255)){int r=stroke_width/2;
  if(steep){map_solid((y0>>8)-r,x0>>8,stroke_width,(dx>>8)+1,c);map_dot_q8(y0,x0,c,stroke_width);map_dot_q8(y1,x1,c,stroke_width);}
  else{map_solid(x0>>8,(y0>>8)-r,(dx>>8)+1,stroke_width,c);map_dot_q8(x0,y0,c,stroke_width);map_dot_q8(x1,y1,c,stroke_width);}return;
 }
 int first=(x0>>8)-(soft?2:cap),last=(x1>>8)+(soft?2:cap),majorlo=steep?40:0,majorhi=steep?MAP_STRIP_Y-1:W-1;
 if(first<majorlo)first=majorlo;if(last>majorhi)last=majorhi;if(first>last)return;
 int slope=map_ratio_q16((u32)(dy<0?-dy:dy),(u32)dx),step=dy<0?-slope:slope;
 static const u16 norm[17]={256,256,257,260,263,268,273,279,286,293,301,310,320,329,340,350,362};
 int idx=slope>>12,length=norm[idx];if(idx<16)length+=((norm[idx+1]-length)*(slope&4095))>>12;
 /* Preserve the old nominal odd stroke sizes (1,3,5), but correct thickness
  * for slope instead of repeatedly stamping overlapping square pixels. */
 int half=stroke_width*length*128;
 long long pos=(long long)y0*256+(((long long)first*256-x0)*step>>8);
 int minorlo=steep?0:40,minorhi=steep?W-1:MAP_STRIP_Y-1;
 /* One-pixel roads need the area on both axes: a centre-column sample
  * overstates diagonal centres and snaps their shoulders into visible steps.
  * Minor roads additionally use a broader, isotropic reconstruction filter:
  * the softer shoulders reduce visible stair steps instead of merely making
  * coverage more accurate. Wider roads retain their fast opaque spans. */
 int thin_idx=(slope+1024)>>11,thin_limit=soft?177:113;
 const u8*thin=soft?map_minor_coverage[thin_idx]:map_thin_coverage[thin_idx];
 int thin_radius=soft?((int)map_minor_radius[thin_idx]*1024+1024):half+(slope>>1)+32768+1024;
 long long thin_len=(long long)dx*dx+(long long)dy*dy,thin_dotstep=(long long)dy*256;
 for(int major=first;major<=last;major++,pos+=step){
  if(stroke_width==1){
   if(pos<(long long)minorlo*65536-thin_radius||pos>(long long)minorhi*65536+thin_radius)continue;
   int center=(int)pos;
   int begin=(center-thin_radius+65535)>>16,end=(center+thin_radius)>>16;
   if(begin<minorlo)begin=minorlo;if(end>minorhi)end=minorhi;
   int endpoint=major*256<x0+(soft?512:256)||major*256>x1-(soft?512:256);
   long long dot=endpoint?((long long)major*256-x0)*dx+((long long)begin*256-y0)*dy:0;
   for(int minor=begin;minor<=end;minor++,dot+=thin_dotstep){
    int delta=minor*65536-center;if(delta<0)delta=-delta;
    unsigned d=(unsigned)((delta+512)>>10);int cover=d<(unsigned)thin_limit?thin[d]:0;
    if(!cover)continue;
    if(endpoint){int round=32;
     if(dot<0)round=soft?map_minor_cap(major*256-x0,minor*256-y0):map_round_coverage_q8(major*256-x0,minor*256-y0,1);
     else if(dot>thin_len)round=soft?map_minor_cap(major*256-x1,minor*256-y1):map_round_coverage_q8(major*256-x1,minor*256-y1,1);
     if(round<cover)cover=round;
    }
    if(soft){if(steep)map_minor_store(minor,major,cover);else map_minor_store(major,minor,cover);}
    else if(steep)map_blend(minor,major,c,cover);else map_blend(major,minor,c,cover);
   }continue;
  }
  long long low=pos-half+32768,high=pos+half+32768;
  int lo=(int)(low>>16),hi=(int)(high>>16);
  int a=(int)((65536-(low-(long long)lo*65536)+1024)>>11);
  int b=(int)(((high-(long long)hi*65536)+1024)>>11);
  /* Near endpoints, clamp the band to a round cap rather than letting a
   * square or diagonal corner poke out beyond the road's radius. */
  if(major*256<x0+cap*256||major*256>x1-cap*256){
   int begin=lo,end=hi;if(begin<minorlo)begin=minorlo;if(end>minorhi)end=minorhi;
   for(int minor=begin;minor<=end;minor++){
    int cover=minor==lo?a:(minor==hi?b:32),round=32;
    long long startdot=((long long)major*256-x0)*dx+((long long)minor*256-y0)*dy;
    long long enddot=((long long)major*256-x1)*dx+((long long)minor*256-y1)*dy;
    if(startdot<0)round=map_round_coverage_q8(major*256-x0,minor*256-y0,stroke_width);
    else if(enddot>0)round=map_round_coverage_q8(major*256-x1,minor*256-y1,stroke_width);
    if(round<cover)cover=round;if(steep)map_blend(minor,major,c,cover);else map_blend(major,minor,c,cover);
   }continue;
  }
  if(lo==hi){int cover=(int)((high-low+1024)>>11);if(steep)map_blend(lo,major,c,cover);else map_blend(major,lo,c,cover);continue;}
  if(steep){map_blend(lo,major,c,a);map_blend(hi,major,c,b);}
  else{map_blend(major,lo,c,a);map_blend(major,hi,c,b);}
  int begin=lo+1,end=hi;if(begin<minorlo)begin=minorlo;if(end>minorhi+1)end=minorhi+1;
  if(begin<end){if(steep)map_solid(begin,major,end-begin,1,c);else map_solid(major,begin,1,end-begin,c);}

 }
}
static void map_line(int x0,int y0,int x1,int y1,u16 c,int wid){map_line_q8(x0*256,y0*256,x1*256,y1*256,c,wid);}
/* RDP in bounded 255-point chunks: retain every visible bend, while
 * avoiding repeated round-cap work for dense collinear route/road geometry.
 * The L1-normal error bound is <=sqrt(2)/8 pixel. Backtracking is preserved. */
static void map_stroke_polyline(const int*x,const int*y,int n,u16 col,int wid){
 u8 keep[255],stack_a[255],stack_b[255];
 for(int i=0;i<n;i++)keep[i]=!MAP_ANTIALIAS;keep[0]=keep[n-1]=1;
 if(MAP_ANTIALIAS){int pending=1;stack_a[0]=0;stack_b[0]=(u8)(n-1);
  while(pending){int a=stack_a[--pending],b=stack_b[pending],at=-1;
   long long dx=(long long)x[b]-x[a],dy=(long long)y[b]-y[a],len=dx*dx+dy*dy;
   long long den=(dx<0?-dx:dx)+(dy<0?-dy:dy);if(!den)den=1;
   long long worst=32*den;
   for(int i=a+1;i<b;i++){long long ix=(long long)x[i]-x[a],iy=(long long)y[i]-y[a],dot=ix*dx+iy*dy,error;
    if(!len||dot<0){if(ix<0)ix=-ix;if(iy<0)iy=-iy;error=(ix>iy?ix:iy)*den;}
    else if(dot>len){ix=(long long)x[i]-x[b];iy=(long long)y[i]-y[b];if(ix<0)ix=-ix;if(iy<0)iy=-iy;error=(ix>iy?ix:iy)*den;}
    else{error=ix*dy-iy*dx;if(error<0)error=-error;}
    if(error>worst){worst=error;at=i;}
   }
   if(at>=0){keep[at]=1;if(at>a+1){stack_a[pending]=(u8)a;stack_b[pending++]=(u8)at;}if(b>at+1){stack_a[pending]=(u8)at;stack_b[pending++]=(u8)b;}}
  }
 }
 int prev=0;for(int i=1;i<n;i++)if(keep[i]){
  if(!((x[prev]<-20*256&&x[i]<-20*256)||(x[prev]>(W+20)*256&&x[i]>(W+20)*256)||(y[prev]<35*256&&y[i]<35*256)||(y[prev]>(MAP_STRIP_Y+3)*256&&y[i]>(MAP_STRIP_Y+3)*256)))map_line_q8(x[prev],y[prev],x[i],y[i],col,wid);
  prev=i;
 }
}
/* Four vertical coverage samples only along a polygon's fringe. Fully covered
 * interiors remain one solid span; no full-image blur or supersampling pass. */
static void map_area_fringe(int y,int begin,int end,const int*l,const int*r,u16 col){
 for(int x=begin;x<end;x++){
  int covered=0,px0=x*4096,px1=px0+4096;
  for(int s=0;s<4;s++){int a=l[s]>px0?l[s]:px0,b=r[s]<px1?r[s]:px1;if(b>a)covered+=b-a;}
  map_blend(x,y,col,(covered+256)>>9);
 }
}
static void map_area_span(int y,int left,int right,int ls,int rs,u16 col){
 if(!ls&&!rs&&!((left|right)&4095)){int a=left>>12,b=right>>12;if(a<0)a=0;if(b>W)b=W;if(a<b)fill(a,y,b-a,1,col);return;}
 int l[4],r[4],minl=0x7fffffff,maxl=(-2147483647-1),minr=0x7fffffff,maxr=(-2147483647-1);
 for(int s=0;s<4;s++){
  int off=s*2-3;l[s]=left+(int)(((long long)ls*off)>>3);r[s]=right+(int)(((long long)rs*off)>>3);
  if(l[s]<minl)minl=l[s];if(l[s]>maxl)maxl=l[s];if(r[s]<minr)minr=r[s];if(r[s]>maxr)maxr=r[s];
 }
 int begin=minl>>12,end=(maxr+4095)>>12,solid0=(maxl+4095)>>12,solid1=minr>>12;
 if(begin<0)begin=0;if(end>W)end=W;if(begin>=end)return;
 if(solid0<begin)solid0=begin;if(solid1>end)solid1=end;
 if(solid0<solid1){
  fill(solid0,y,solid1-solid0,1,col);
  map_area_fringe(y,begin,solid0,l,r,col);map_area_fringe(y,solid1,end,l,r,col);
 }else map_area_fringe(y,begin,end,l,r,col);
}
#endif
