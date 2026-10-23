/* CPA1 bounded regional catalog. Cores are disjoint whole CPM Z0 tiles.
 * Cross-pack transfers use OSM identity and an exact default routing state. */
#ifndef HOST_TEST
#define ATLAS_ROOT "/mnt/sdcard/clio843/maps/"
#else
#define ATLAS_ROOT CLIO_HOST_ROOT "clio843/maps/"
#endif
#define ATLAS_MAX_REGIONS 256u
#define ATLAS_MAX_NODES 65534u
#define ATLAS_MAX_ATTACH 8192u
struct AtlasRegion {char path[24],name[24];s32 west,south,east,north;u32 first,count,res0,res1;};
struct AtlasNode {s32 lat,lon;u32 first,count,member,countmember;};
struct AtlasEdge {u32 to,cost,metres,region,arc,reserved;};
struct AtlasMember {u32 region,global,local,reserved;};
static int atlasfd=-1,atlas_ready=0,atlas_attempted=0,atlas_route_region=-1;
static u8*atlas_data=0;static u32 atlas_bytes=0,atlas_nr=0,atlas_nn=0,atlas_ne=0,atlas_nm=0,atlas_ro=0,atlas_no=0,atlas_eo=0,atlas_mo=0;
static struct AtlasRegion atlas_regions[ATLAS_MAX_REGIONS];
static char atlas_map_path[160],atlas_route_path[160],atlas_qadj_path[160];
static void atlas_path(char*out,unsigned cap,unsigned region,const char*name){out[0]=0;appendstr(out,cap,ATLAS_ROOT);if(region<atlas_nr){appendstr(out,cap,atlas_regions[region].path);appendstr(out,cap,"/");}appendstr(out,cap,name);}
static int atlas_owner(s32 lat,s32 lon){for(u32 i=0;i<atlas_nr;i++){struct AtlasRegion*r=&atlas_regions[i];if(lon>=r->west&&lon<r->east&&lat>=r->south&&lat<r->north)return(int)i;}return-1;}
static int atlas_set_paths(int region){if(region<0||(u32)region>=atlas_nr)return 0;atlas_route_region=region;atlas_path(atlas_map_path,sizeof(atlas_map_path),(u32)region,"map.cpm");atlas_path(atlas_route_path,sizeof(atlas_route_path),(u32)region,"route.cpr");atlas_path(atlas_qadj_path,sizeof(atlas_qadj_path),(u32)region,"route.qadj");return 1;}
static const struct AtlasNode*atlas_node(u32 n){return n<atlas_nn?(const struct AtlasNode*)(atlas_data+atlas_no+n*24u):0;}
static const struct AtlasEdge*atlas_edge(u32 n){return n<atlas_ne?(const struct AtlasEdge*)(atlas_data+atlas_eo+n*24u):0;}
static const struct AtlasMember*atlas_member(u32 n){return n<atlas_nm?(const struct AtlasMember*)(atlas_data+atlas_mo+n*16u):0;}
static u32 atlas_local(u32 global,u32 region){const struct AtlasNode*n=atlas_node(global);if(!n)return 0xffffffffu;for(u32 i=0;i<n->countmember;i++){const struct AtlasMember*m=(const struct AtlasMember*)(atlas_data+atlas_mo+atlas_nm*16u+(n->member+i)*16u);if(m->region==region)return m->local;}return 0xffffffffu;}
static int load_atlas(void){if(atlas_attempted)return atlas_ready;atlas_attempted=1;u8 h[64];atlasfd=(int)s_open(ATLAS_ROOT "atlas.cpa",O_RDONLY_,0);if(atlasfd<0)return 0;
 if(s_read(atlasfd,h,64)!=64||rd32u(h)!=0x31415043u||rd16(h+4)!=1||rd16(h+6)!=80)goto bad;
 atlas_nr=rd32u(h+8);atlas_nn=rd32u(h+12);atlas_ne=rd32u(h+16);atlas_nm=rd32u(h+20);atlas_ro=rd32u(h+24);atlas_no=rd32u(h+28);atlas_eo=rd32u(h+32);atlas_mo=rd32u(h+36);atlas_bytes=rd32u(h+40);
 if(!atlas_nr||atlas_nr>ATLAS_MAX_REGIONS||atlas_nn>ATLAS_MAX_NODES||atlas_ne>1000000u||atlas_nm>ATLAS_MAX_REGIONS*ATLAS_MAX_ATTACH||rd32u(h+48)>ATLAS_MAX_ATTACH||atlas_ro!=64||atlas_no!=atlas_ro+atlas_nr*80||atlas_eo!=atlas_no+atlas_nn*24||atlas_mo!=atlas_eo+atlas_ne*24||atlas_bytes!=atlas_mo+atlas_nm*32||atlas_bytes>32u*1024u*1024u||s_lseek(atlasfd,0,2)!=(long)atlas_bytes)goto bad;
 atlas_data=(u8*)s_mmap2(0,atlas_bytes,PROT_READ_,MAP_PRIVATE_,atlasfd,0);if(!atlas_data)goto bad;
 memzero(atlas_regions,sizeof(atlas_regions));for(u32 i=0;i<atlas_nr;i++){struct AtlasRegion*r=&atlas_regions[i];for(unsigned j=0;j<80;j++)((u8*)r)[j]=atlas_data[atlas_ro+i*80u+j];if(!r->path[0]||r->path[23]||r->name[23]||r->west>=r->east||r->south>=r->north||r->count>ATLAS_MAX_ATTACH||r->first>atlas_nm||r->count>atlas_nm-r->first)goto bad;for(unsigned j=0;r->path[j];j++)if(!((r->path[j]>='a'&&r->path[j]<='z')||(r->path[j]>='0'&&r->path[j]<='9')||r->path[j]=='_'||r->path[j]=='-'))goto bad;}
 for(u32 i=0;i<atlas_nn;i++){const struct AtlasNode*n=atlas_node(i);if(n->first>atlas_ne||n->count>atlas_ne-n->first||n->member>atlas_nm||n->countmember>atlas_nm-n->member)goto bad;}
 for(u32 i=0;i<atlas_ne;i++){const struct AtlasEdge*e=atlas_edge(i);if(e->to>=atlas_nn||e->region>=atlas_nr||e->cost==0xffffffffu)goto bad;}
 for(u32 i=0;i<atlas_nm*2u;i++){const struct AtlasMember*m=(const struct AtlasMember*)(atlas_data+atlas_mo+i*16u);if(m->region>=atlas_nr||m->global>=atlas_nn)goto bad;}
 atlas_ready=1;int owner=atlas_owner(gps.lat,gps.lon);atlas_set_paths(owner>=0?owner:0);return 1;
 bad:if(atlas_data){s_munmap(atlas_data,atlas_bytes);atlas_data=0;}s_close(atlasfd);atlasfd=-1;atlas_nr=atlas_nn=atlas_ne=atlas_nm=0;return 0;
}
static int atlas_active=0,atlas_probe=0;
static s32 atlas_dest_lat=0,atlas_dest_lon=0,atlas_origin_lat=0,atlas_origin_lon=0;
static int atlas_request(s32 lat,s32 lon);
static void atlas_cancel(void);
static void atlas_service_until(u32 deadline);
static u32 atlas_future_m=0,atlas_future_cost=0;
