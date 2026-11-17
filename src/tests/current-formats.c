/* Current file-format gates, independent of map downloads. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
#include <sys/stat.h>

static void put(const char*path,const void*data,size_t n){
 FILE*f=fopen(path,"wb");assert(f&&fwrite(data,1,n,f)==n);assert(fclose(f)==0);
}
int main(void){
 assert(mkdir(CLIO_HOST_ROOT "clio843/maps",0700)==0);
 struct CpmHeader m={0};memcpy(m.magic,"CPM5",4);m.ver=5;m.maxlvl=6;
 m.index_off=m.data_off=sizeof(m);
 put(MAP_PATH,&m,sizeof(m));assert(load_map()&&mapok);
 for(int i=1;i<5;i++){
  m.magic[3]='0'+i;m.ver=i;put(MAP_PATH,&m,sizeof(m));
  assert(!load_map()&&!mapok&&mapfd==-1);
 }
 m.magic[3]='5';m.ver=5;put(MAP_PATH,&m,sizeof(m));assert(load_map());
 /* Minimal layout fixtures test the header gates, not graph correctness. */
 struct {struct CprHeader h;struct CprPoint p;} r={0};
 memcpy(r.h.magic,"CPR2",4);r.h.ver=5;
 r.h.nodes=r.h.base_edges=r.h.ch_arcs=r.h.geom_points=1;r.h.geom_off=sizeof(r.h);
 put(ROUTE_PATH,&r,sizeof(r));assert(load_route_data()&&routeok&&!qadjok);
 struct {struct QAdj7Header h;u32 idx,block;struct QRec7 rec;} q={0};
 memcpy(q.h.q.magic,"QAD7",4);q.h.q.ver=7;q.h.q.entry_size=sizeof(struct QAdj7Entry);
 q.h.q.nodes=q.h.q.ch_arcs=1;q.h.q.route_bytes=sizeof(r);
 q.h.q.flags=QAD_RECON_META|QAD_LOCAL_ARC_IDS|QAD_DIRECT_INDEX|QAD_RANK_ORDER|QAD7_PACKET_GEOM;
 q.h.q.index_off=sizeof(q.h);q.h.q.index_bytes=4;q.h.q.data_off=q.h.q.index_off+4;
 q.h.q.data_bytes=4;q.h.q.rmeta_off=q.h.q.data_off+4;q.h.q.rmeta_bytes=sizeof(q.rec);
 q.h.packet_off=q.h.q.rmeta_off+q.h.q.rmeta_bytes;q.h.packet_entry_size=QAD7_POINT_SIZE;
 assert(sizeof(q)==q.h.packet_off);put(QADJ_PATH,&q,sizeof(q));assert(load_route_data()&&qadjok&&qadjmode==7);
 for(int i=1;i<7;i++){
  q.h.q.magic[3]='0'+i;q.h.q.ver=i;put(QADJ_PATH,&q,sizeof(q));
  assert(load_route_data()&&routeok&&!qadjok&&!qadjmode&&qadjfd==-1);
 }
 q.h.q.magic[3]='7';q.h.q.ver=7;put(QADJ_PATH,&q,sizeof(q));assert(load_route_data()&&qadjok);
 r.h.ver=4;put(ROUTE_PATH,&r,sizeof(r));assert(!load_route_data()&&!routeok&&!qadjok&&routefd==-1&&qadjfd==-1);
 r.h.ver=5;put(ROUTE_PATH,&r,sizeof(r));assert(load_route_data()&&qadjok);
 struct SavedPlace p={12345,-54321,"Current favourite"},got[2];
 write_places(FAVES_PATH,&p,1);assert(read_places(FAVES_PATH,got,2)==1&&!strcmp(got[0].name,p.name));
 FILE*f=fopen(FAVES_PATH,"r+b");assert(f);u32 old=0x31504c43u;assert(fwrite(&old,4,1,f)==1);fclose(f);
 assert(read_places(FAVES_PATH,got,2)==0);f=fopen(FAVES_PATH,"rb");u32 magic=0;assert(f&&fread(&magic,4,1,f)==1&&magic==old);fclose(f);
 write_places(FAVES_PATH,&p,1);assert(read_places(FAVES_PATH,got,2)==1);
 assert(truncate(FAVES_PATH,8)==0&&read_places(FAVES_PATH,got,2)==0);
 s_close(mapfd);qadj_unmap();route_geom_unmap();s_close(qadjfd);s_close(routefd);
 unlink(FAVES_PATH);unlink(MAP_PATH);unlink(ROUTE_PATH);unlink(QADJ_PATH);rmdir(CLIO_HOST_ROOT "clio843/maps");
 puts("Current formats: CPM5, CPR2 v5, QAD7, CLP2 accepted; obsolete versions and truncated places rejected, no migration; current CPR2 fallback preserved.");
 return 0;
}
