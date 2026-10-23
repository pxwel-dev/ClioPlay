#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

static uint16_t rd16(const unsigned char* p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t rd32(const unsigned char* p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int32_t rds32(const unsigned char* p){return (int32_t)rd32(p);}
static void wr16(unsigned char* p,uint16_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void wr32(unsigned char* p,uint32_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static void die(const std::string& s){std::fprintf(stderr,"build_qadj: %s\n",s.c_str());std::exit(2);}
static void write_all(int fd,const unsigned char* p,size_t n){while(n){ssize_t w=::write(fd,p,n);if(w<0){if(errno==EINTR)continue;die(std::string("write failed: ")+std::strerror(errno));}if(!w)die("short write");p+=w;n-=(size_t)w;}}

static const size_t CPR_HDR_SIZE=108;
static const size_t CPR_NODE_SIZE=40;
static const size_t CPR_BASE_SIZE=24;
static const size_t CPR_ARC_SIZE=24;
static const size_t CPR_POINT_SIZE=8;
static const size_t QAD7_HDR_SIZE=80;
static const size_t QADJ_ENTRY_SIZE=16;
static const size_t QREC7_SIZE=32;
static const size_t QAD7_POINT_SIZE=20;
static const uint32_t QAD_RECON_META=2u;
static const uint32_t QAD4_LOCAL_ARC_IDS=4u;
static const uint32_t QAD5_DIRECT_INDEX=8u;
static const uint32_t QAD5_RANK_ORDER=16u;
static const uint32_t QAD7_PACKET_GEOM=64u;
static const uint32_t QAD_NODE_MAX=32768u;
static const uint32_t ARC_SHORTCUT=1u;

static int coslat_q15(int32_t lat){int d=lat<0?-lat:lat;d/=100;if(d>=90000)return 0;static const uint16_t c[91]={32767,32762,32747,32722,32687,32642,32587,32523,32448,32364,32269,32165,32051,31927,31794,31650,31498,31335,31163,30982,30791,30591,30381,30162,29934,29697,29451,29196,28932,28659,28377,28087,27788,27481,27165,26841,26509,26169,25821,25465,25101,24730,24351,23964,23571,23170,22762,22347,21925,21497,21062,20621,20173,19720,19260,18794,18323,17846,17364,16876,16384,15886,15383,14876,14364,13848,13328,12803,12275,11743,11207,10668,10126,9580,9032,8481,7927,7371,6813,6252,5690,5126,4560,3993,3425,2856,2286,1715,1144,572,0};int i=d/1000,f=d%1000;return c[i]+((int)c[i+1]-c[i])*f/1000;}
static uint32_t iabs32(int32_t v){return (uint32_t)(v<0?-v:v);}
static uint32_t seg_units(int32_t la0,int32_t lo0,int32_t la1,int32_t lo1){int cq=coslat_q15((la0+la1)/2);int32_t dd=lo1-lo0;int32_t sx=(int32_t)(((int64_t)dd*cq)>>15);uint32_t dx=iabs32(sx),dy=iabs32(la1-la0),hi=dx>dy?dx:dy,lo=dx>dy?dy:dx;return hi+((lo*3u)>>3);}
static uint32_t sat_add(uint32_t a,uint32_t b){return a>0xffffffffu-b?0xffffffffu:a+b;}
static uint32_t ratio_q16(uint32_t num,uint32_t den){if(!den||!num)return 0;if(num>=den)return 65536u;while(den>65535u){den=(den+1u)>>1;num=(num+1u)>>1;if(num>=den)return 65536u;}return (num<<16)/den;}
static uint32_t seg_m(int32_t la0,int32_t lo0,int32_t la1,int32_t lo1){uint32_t q=seg_units(la0,lo0,la1,lo1);return (q*111u+50u)/100u;}

int main(int argc,char**argv){
 if(argc!=3){std::fprintf(stderr,"Usage: %s route.cpr route.qadj\n",argv[0]);return 2;}
 const char* in=argv[1];const char* out=argv[2];
 int fd=::open(in,O_RDONLY);if(fd<0)die(std::string("open route.cpr failed: ")+std::strerror(errno));
 struct stat st{};if(fstat(fd,&st)||st.st_size<(off_t)CPR_HDR_SIZE)die("route.cpr too small");size_t sz=(size_t)st.st_size;
 unsigned char* m=(unsigned char*)mmap(nullptr,sz,PROT_READ,MAP_PRIVATE,fd,0);if(m==MAP_FAILED)die(std::string("mmap failed: ")+std::strerror(errno));
 if(std::memcmp(m,"CPR2",4)||(rd16(m+4)!=4 && rd16(m+4)!=5))die("route.cpr is not CPR2 v4");
 uint32_t nodes=rd32(m+8),base_edges=rd32(m+16),route_edges=rd32(m+20),ch_arcs=rd32(m+32),upf_count=rd32(m+36),upr_count=rd32(m+40),geom_points=rd32(m+44);
 uint32_t node_off=rd32(m+60),base_off=rd32(m+68),arc_off=rd32(m+76),upf_off=rd32(m+80),upr_off=rd32(m+84),geom_off=rd32(m+88);
 auto inside=[&](uint64_t off,uint64_t bytes){return off<=sz&&bytes<=sz-off;};
 if(!nodes||!ch_arcs||nodes>=0x01000000u||!inside(node_off,(uint64_t)nodes*CPR_NODE_SIZE)||!inside(base_off,(uint64_t)base_edges*CPR_BASE_SIZE)||!inside(arc_off,(uint64_t)ch_arcs*CPR_ARC_SIZE)||!inside(upf_off,(uint64_t)upf_count*4)||!inside(upr_off,(uint64_t)upr_count*4)||!inside(geom_off,(uint64_t)geom_points*CPR_POINT_SIZE))die("route.cpr routing regions are invalid or node ids exceed QAD4 packing");
 if(sz>0xffffffffULL)die("route.cpr exceeds QAD4 32-bit identity field");
 const unsigned char* arcs=m+arc_off;const unsigned char* bases=m+base_off;const unsigned char* gp=m+geom_off;

 /* Sidecar-local arc IDs are assigned preorder from shortcut-DAG roots.  This
  * deliberately clusters a shortcut with the descendants the embedded runtime
  * will immediately unpack, without modifying route.cpr. */
 std::vector<unsigned char> is_child(ch_arcs,0);
 for(uint32_t aid=0;aid<ch_arcs;aid++){
   const unsigned char* a=arcs+(uint64_t)aid*CPR_ARC_SIZE;uint8_t flags=a[21];
   if(flags&ARC_SHORTCUT){uint32_t l=rd32(a+12),r=rd32(a+16);if(l>=ch_arcs||r>=ch_arcs)die("shortcut child arc id invalid");is_child[l]=1;is_child[r]=1;}
 }
 std::vector<uint32_t> old_to_new(ch_arcs,0xffffffffu),new_to_old;new_to_old.reserve(ch_arcs);std::vector<uint32_t> stack;stack.reserve(256);
 auto visit=[&](uint32_t root){if(old_to_new[root]!=0xffffffffu)return;stack.clear();stack.push_back(root);while(!stack.empty()){uint32_t x=stack.back();stack.pop_back();if(old_to_new[x]!=0xffffffffu)continue;old_to_new[x]=(uint32_t)new_to_old.size();new_to_old.push_back(x);const unsigned char* a=arcs+(uint64_t)x*CPR_ARC_SIZE;if(a[21]&ARC_SHORTCUT){uint32_t l=rd32(a+12),r=rd32(a+16);if(old_to_new[r]==0xffffffffu)stack.push_back(r);if(old_to_new[l]==0xffffffffu)stack.push_back(l);}}};
 for(uint32_t aid=0;aid<ch_arcs;aid++)if(!is_child[aid])visit(aid);
 for(uint32_t aid=0;aid<ch_arcs;aid++)visit(aid);
 if(new_to_old.size()!=ch_arcs)die("QAD4 reconstruction renumbering incomplete");

 std::vector<uint32_t> block_size(nodes),rank(nodes),node_offs(nodes);uint64_t sumf=0,sumr=0,data_bytes=0;
 for(uint32_t n=0;n<nodes;n++){
   const unsigned char* np=m+node_off+(uint64_t)n*CPR_NODE_SIZE;uint32_t ff=rd32(np+28),rf=rd32(np+36);uint32_t fc=rd16(np+32),rc=rd16(np+34);rank[n]=rd32(np+20);
   if(ff>upf_count||fc>upf_count-ff||rf>upr_count||rc>upr_count-rf)die("node upward adjacency range invalid");
   uint64_t bs=4ull+(uint64_t)(fc+rc)*QADJ_ENTRY_SIZE;if(bs>QAD_NODE_MAX)die("single-node QAD5 block exceeds runtime buffer");block_size[n]=(uint32_t)bs;sumf+=fc;sumr+=rc;data_bytes+=bs;
 }
 if(sumf!=upf_count||sumr!=upr_count)die("node counts do not cover CPR upward reference arrays exactly");
 if(data_bytes>0x7fffffffu)die("QAD5 query data exceeds runtime 2 GiB offset limit");
 std::vector<uint32_t> node_order(nodes);for(uint32_t n=0;n<nodes;n++)node_order[n]=n;
 std::sort(node_order.begin(),node_order.end(),[&](uint32_t a,uint32_t b){return rank[a]<rank[b];});
 uint64_t cur_off=0;for(uint32_t n:node_order){node_offs[n]=(uint32_t)cur_off;cur_off+=block_size[n];}if(cur_off!=data_bytes)die("QAD5 node offset accounting mismatch");
 std::vector<uint32_t> geom_units(base_edges,0);
 for(uint32_t b=0;b<base_edges;b++){
   const unsigned char* bp=bases+(uint64_t)b*CPR_BASE_SIZE;uint32_t gf=rd32(bp+4);uint32_t gc=rd16(bp+16);if(gc<2||gf>geom_points||gc>geom_points-gf)continue;
   uint32_t total=0;int32_t la0=rds32(gp+(uint64_t)gf*8),lo0=rds32(gp+(uint64_t)gf*8+4);
   for(uint32_t j=1;j<gc;j++){int32_t la1=rds32(gp+(uint64_t)(gf+j)*8),lo1=rds32(gp+(uint64_t)(gf+j)*8+4);total=sat_add(total,seg_units(la0,lo0,la1,lo1));la0=la1;lo0=lo1;}
   geom_units[b]=total;
 }

 /* PERF8/QAD7: self-contained reconstruction packets.  A packet is shared by
  * physical base edges that reference the same canonical geometry range, so
  * reverse directed edges do not duplicate their point stream.  Packets are
  * assigned in sidecar-local reconstruction order so the winning shortcut DAG
  * naturally walks nearby file pages. */
 struct PacketDesc{uint32_t gf,gc,off;};
 std::unordered_map<uint64_t,uint32_t> packet_by_geom;
 std::vector<PacketDesc> packets;packets.reserve(base_edges/2u+1u);
 std::vector<uint32_t> base_packet_off(base_edges,0xffffffffu);
 uint64_t packet_bytes=0,packet_points=0;
 for(uint32_t nid=0;nid<ch_arcs;nid++){
   uint32_t aid=new_to_old[nid];const unsigned char* a=arcs+(uint64_t)aid*CPR_ARC_SIZE;if(a[21]&ARC_SHORTCUT)continue;uint32_t bid=rd32(a+12);if(bid>=base_edges)continue;
   const unsigned char* bp=bases+(uint64_t)bid*CPR_BASE_SIZE;uint32_t gf=rd32(bp+4),gc=rd16(bp+16);if(gc<2||gf>geom_points||gc>geom_points-gf)die("QAD7 base geometry invalid");
   uint64_t key=((uint64_t)gf<<32)|gc;auto it=packet_by_geom.find(key);uint32_t off;
   if(it==packet_by_geom.end()){
     if(packet_bytes>0xffffffffULL-(uint64_t)gc*QAD7_POINT_SIZE)die("QAD7 packet region exceeds 32-bit offsets");
     off=(uint32_t)packet_bytes;packet_by_geom.emplace(key,off);packets.push_back(PacketDesc{gf,gc,off});packet_bytes+=(uint64_t)gc*QAD7_POINT_SIZE;packet_points+=gc;
   }else off=it->second;
   if(base_packet_off[bid]!=0xffffffffu&&base_packet_off[bid]!=off)die("QAD7 base packet mapping inconsistent");base_packet_off[bid]=off;
 }

 uint64_t index_off=QAD7_HDR_SIZE,index_bytes=(uint64_t)nodes*4u,data_off=index_off+index_bytes,rmeta_off=data_off+data_bytes,rmeta_bytes=(uint64_t)ch_arcs*QREC7_SIZE,packet_off=rmeta_off+rmeta_bytes,out_size=packet_off+packet_bytes;
 if(out_size>0x7fffffffULL)die("route.qadj would exceed runtime 2 GiB file-offset limit");
 std::string tmp=std::string(out)+".new";int ofd=::open(tmp.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);if(ofd<0)die(std::string("open output failed: ")+std::strerror(errno));
 unsigned char h[QAD7_HDR_SIZE]{};std::memcpy(h,"QAD7",4);wr16(h+4,7);wr16(h+6,(uint16_t)QADJ_ENTRY_SIZE);uint32_t hv[]={nodes,ch_arcs,upf_count,upr_count,route_edges,(uint32_t)sz,(uint32_t)index_off,(uint32_t)index_bytes,(uint32_t)data_off,(uint32_t)data_bytes,(uint32_t)rmeta_off,(uint32_t)rmeta_bytes,QAD_RECON_META|QAD4_LOCAL_ARC_IDS|QAD5_DIRECT_INDEX|QAD5_RANK_ORDER|QAD7_PACKET_GEOM,0u,(uint32_t)packet_off,(uint32_t)packet_bytes,(uint32_t)packet_points,QAD7_POINT_SIZE};for(int i=0;i<18;i++)wr32(h+8+i*4,hv[i]);write_all(ofd,h,sizeof(h));
 std::vector<unsigned char> ib((size_t)index_bytes);for(uint32_t n=0;n<nodes;n++)wr32(ib.data()+(uint64_t)n*4u,node_offs[n]);write_all(ofd,ib.data(),ib.size());
 const unsigned char* uf=m+upf_off;const unsigned char* ur=m+upr_off;std::vector<unsigned char> outbuf;outbuf.reserve(1024*1024+QAD_NODE_MAX);
 auto flush=[&](){if(!outbuf.empty()){write_all(ofd,outbuf.data(),outbuf.size());outbuf.clear();}};
 auto emit_ref=[&](uint32_t aid,bool reverse){if(aid>=ch_arcs)die("invalid CH arc id in upward reference array");const unsigned char* a=arcs+(uint64_t)aid*CPR_ARC_SIZE;uint32_t other=rd32(a+(reverse?0:4)),cost=rd32(a+8),local=old_to_new[aid];if(other>=nodes||local==0xffffffffu)die("unmapped QAD5 adjacency");size_t p=outbuf.size();outbuf.resize(p+QADJ_ENTRY_SIZE);wr32(outbuf.data()+p,other);wr32(outbuf.data()+p+4,cost);wr32(outbuf.data()+p+8,local);wr32(outbuf.data()+p+12,node_offs[other]);};
 for(uint32_t node:node_order){const unsigned char* np=m+node_off+(uint64_t)node*CPR_NODE_SIZE;uint32_t ff=rd32(np+28),rf=rd32(np+36);uint16_t fc=rd16(np+32),rc=rd16(np+34);size_t before=outbuf.size();outbuf.resize(before+4);wr16(outbuf.data()+before,fc);wr16(outbuf.data()+before+2,rc);for(uint32_t k=0;k<fc;k++)emit_ref(rd32(uf+(uint64_t)(ff+k)*4u),false);for(uint32_t k=0;k<rc;k++)emit_ref(rd32(ur+(uint64_t)(rf+k)*4u),true);if(outbuf.size()-before!=block_size[node])die("QAD5 node block size mismatch");if(outbuf.size()>=1024*1024)flush();}
 flush();
 std::vector<unsigned char> rb;rb.resize(1024*1024);size_t rpos=0;auto rflush=[&](){if(rpos){write_all(ofd,rb.data(),rpos);rpos=0;}};
 for(uint32_t nid=0;nid<ch_arcs;nid++){
   uint32_t aid=new_to_old[nid];const unsigned char* a=arcs+(uint64_t)aid*CPR_ARC_SIZE;uint32_t from=rd32(a),to=rd32(a+4),cost=rd32(a+8),left_old=rd32(a+12),right_old=rd32(a+16);uint8_t flags=a[21];if(to>=0x01000000u)die("QAD7 to-state exceeds packed 24-bit field");
   uint32_t left=left_old,right_geom=0,gf=0,gi=0,poff=0xffffffffu;
   if(flags&ARC_SHORTCUT){if(left_old>=ch_arcs||right_old>=ch_arcs)die("QAD7 shortcut child invalid");left=old_to_new[left_old];right_geom=old_to_new[right_old];}
   else if(left_old<base_edges){const unsigned char* bp=bases+(uint64_t)left_old*CPR_BASE_SIZE;uint16_t gc=rd16(bp+16);uint8_t cls=bp[20],bf=bp[21];gf=rd32(bp+4);right_geom=geom_units[left_old];gi=(uint32_t)gc|((uint32_t)bf<<16)|((uint32_t)cls<<24);poff=base_packet_off[left_old];if(poff==0xffffffffu)die("QAD7 base arc has no geometry packet");}
   uint32_t tf=to|((uint32_t)flags<<24);if(rpos+QREC7_SIZE>rb.size())rflush();wr32(rb.data()+rpos,from);wr32(rb.data()+rpos+4,cost);wr32(rb.data()+rpos+8,left);wr32(rb.data()+rpos+12,right_geom);wr32(rb.data()+rpos+16,tf);wr32(rb.data()+rpos+20,gf);wr32(rb.data()+rpos+24,gi);wr32(rb.data()+rpos+28,poff);rpos+=QREC7_SIZE;
 }
 rflush();
 std::vector<unsigned char> pb;pb.resize(1024*1024+QAD7_POINT_SIZE);size_t ppos=0;auto pflush=[&](){if(ppos){write_all(ofd,pb.data(),ppos);ppos=0;}};
 for(const PacketDesc&pd:packets){
   uint32_t gf=pd.gf,gc=pd.gc,total=0;std::vector<uint32_t> cum(gc,0);std::vector<uint16_t> mf(gc,0),mr(gc,0);int32_t la0=rds32(gp+(uint64_t)gf*8),lo0=rds32(gp+(uint64_t)gf*8+4);
   for(uint32_t j=1;j<gc;j++){int32_t la1=rds32(gp+(uint64_t)(gf+j)*8),lo1=rds32(gp+(uint64_t)(gf+j)*8+4);uint32_t u=seg_units(la0,lo0,la1,lo1),fm=seg_m(la0,lo0,la1,lo1),rm=seg_m(la1,lo1,la0,lo0);if(fm>65535u||rm>65535u)die("QAD7 geometry segment exceeds 16-bit metre field");total=sat_add(total,u);cum[j]=total;mf[j]=(uint16_t)fm;mr[j-1]=(uint16_t)rm;la0=la1;lo0=lo1;}
   for(uint32_t j=0;j<gc;j++){uint32_t qf=ratio_q16(cum[j],total),qr=ratio_q16(total>=cum[j]?total-cum[j]:0,total);uint16_t sf=(uint16_t)(qf>65535u?65535u:qf),sr=(uint16_t)(qr>65535u?65535u:qr),fl=(uint16_t)((qf>=65536u?1u:0u)|(qr>=65536u?2u:0u));if(ppos+QAD7_POINT_SIZE>pb.size())pflush();const unsigned char* pp=gp+(uint64_t)(gf+j)*8;wr32(pb.data()+ppos,rd32(pp));wr32(pb.data()+ppos+4,rd32(pp+4));wr16(pb.data()+ppos+8,sf);wr16(pb.data()+ppos+10,sr);wr16(pb.data()+ppos+12,mf[j]);wr16(pb.data()+ppos+14,mr[j]);wr16(pb.data()+ppos+16,fl);wr16(pb.data()+ppos+18,0);ppos+=QAD7_POINT_SIZE;}
 }
 pflush();
 if(fsync(ofd))die(std::string("fsync failed: ")+std::strerror(errno));
 off_t got=lseek(ofd,0,SEEK_END);if(got!=(off_t)out_size)die("route.qadj size mismatch after write");
 ::close(ofd);munmap(m,sz);::close(fd);if(::rename(tmp.c_str(),out))die(std::string("rename failed: ")+std::strerror(errno));
 std::fprintf(stderr,"QAD7: route=%zu bytes nodes=%u CH=%u forward=%u reverse=%u\n",sz,nodes,ch_arcs,upf_count,upr_count);
 std::fprintf(stderr,"QAD7: mmap index %.1f MiB, rank-ordered query data %.1f MiB\n",(double)index_bytes/1048576.0,(double)data_bytes/1048576.0);
 std::fprintf(stderr,"QAD7: reconstruction %.1f MiB + unified packets %.1f MiB (%zu unique polylines); wrote %.1f MiB\n",(double)rmeta_bytes/1048576.0,(double)packet_bytes/1048576.0,packets.size(),(double)out_size/1048576.0);return 0;
}
