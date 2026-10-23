#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static constexpr uint32_t NO=0xffffffffu;
static constexpr uint8_t ARC_SHORTCUT=1;
#pragma pack(push,1)
struct RawHdr { char magic[4]; uint32_t version,n,m,geom_points; };
struct RawArc { uint32_t u,v,cost,left,right; uint8_t cls,flags; uint16_t pad; };
struct OutHdr { char magic[4]; uint32_t version,n,base_m,arc_n,upf_n,upr_n; };
struct ArcDisk { uint32_t u,v,cost,left,right; uint8_t cls,flags; uint16_t pad; };
#pragma pack(pop)
struct Arc { uint32_t u,v,cost,left=NO,right=NO; uint8_t cls=7,flags=0; };
struct QN { uint32_t d,n; bool operator>(const QN&o)const{return d>o.d;} };
struct PQ { int64_t p; uint32_t n,gen; bool operator>(const PQ&o)const{return p>o.p || (p==o.p&&n>o.n);} };
struct UnionFind {
 std::vector<uint32_t> p,sz;
 explicit UnionFind(uint32_t n):p(n),sz(n,1){for(uint32_t i=0;i<n;i++)p[i]=i;}
 uint32_t find(uint32_t x){while(p[x]!=x){p[x]=p[p[x]];x=p[x];}return x;}
 void unite(uint32_t a,uint32_t b){a=find(a);b=find(b);if(a==b)return;if(sz[a]<sz[b])std::swap(a,b);p[b]=a;sz[a]+=sz[b];}
};

static int64_t clamp_prio(int64_t x){ return x<0?0:x; }

int main(int argc,char**argv){
 if(argc<3){std::cerr<<"usage: ch_preprocess RAW OUT [witness_settle_limit] [max_shortcut_ratio]\n";return 2;}
 const uint32_t witness_limit=argc>3?std::max(100u,(uint32_t)std::stoul(argv[3])):4000u;
 const double max_shortcut_ratio=argc>4?std::max(1.0,std::stod(argv[4])):6.0;
 std::ifstream f(argv[1],std::ios::binary); if(!f){std::cerr<<"open raw failed\n";return 2;}
 RawHdr h{}; f.read((char*)&h,sizeof(h)); if(std::memcmp(h.magic,"CRW4",4)||h.version!=1){std::cerr<<"bad CRW4 raw\n";return 2;}
 const uint32_t n=h.n, base_m=h.m;
 std::vector<Arc> arcs; arcs.reserve((size_t)base_m*2);
 std::vector<std::vector<uint32_t>> out(n),in(n);
 UnionFind uf(n);
 for(uint32_t i=0;i<base_m;i++){
  RawArc r{};f.read((char*)&r,sizeof(r));
  if(!f||r.u>=n||r.v>=n||r.u==r.v||!r.cost){std::cerr<<"bad raw arc "<<i<<"\n";return 2;}
  Arc a{r.u,r.v,r.cost,r.left,r.right,r.cls,r.flags};uint32_t id=(uint32_t)arcs.size();arcs.push_back(a);out[r.u].push_back(id);in[r.v].push_back(id);uf.unite(r.u,r.v);
 }
 std::unordered_map<uint32_t,uint32_t> root_sz;
 for(uint32_t i=0;i<n;i++)root_sz[uf.find(i)]++;
 std::vector<std::pair<uint32_t,uint32_t>> roots;roots.reserve(root_sz.size());for(auto &kv:root_sz)roots.push_back({kv.second,kv.first});
 std::sort(roots.begin(),roots.end(),[](auto&a,auto&b){return a.first>b.first;});
 std::unordered_map<uint32_t,uint32_t> compmap;for(uint32_t i=0;i<roots.size();i++)compmap[roots[i].second]=i;
 std::vector<uint32_t> comp(n);for(uint32_t i=0;i<n;i++)comp[i]=compmap[uf.find(i)];
 std::cerr<<"CPR2 v4 turn-aware compact CH: nodes="<<n<<" base_arcs="<<base_m<<" components="<<roots.size()<<" largest="<<(roots.empty()?0:roots[0].first)<<" witness_limit="<<witness_limit<<" max_shortcut_ratio="<<max_shortcut_ratio<<"\n";

 std::vector<uint8_t> portal(n,0);
 if(argc>5){std::ifstream pf(argv[5],std::ios::binary);uint32_t id;while(pf.read((char*)&id,4)){if(id>=n){std::cerr<<"bad portal node\n";return 2;}portal[id]=1;}}
 std::vector<uint8_t> contracted(n,0);std::vector<uint32_t> rank(n,NO),level(n,0),cnei(n,0),generation(n,0);
 std::vector<uint32_t> dist(n,0),stamp(n,0);uint32_t epoch=1;
 auto getd=[&](uint32_t x)->uint32_t{return stamp[x]==epoch?dist[x]:NO;};
 auto setd=[&](uint32_t x,uint32_t d){stamp[x]=epoch;dist[x]=d;};

 auto active_info=[&](uint32_t v,int &incn,int &outn,int &deg,int &bestcls,std::vector<uint32_t>*neigh){
  std::vector<uint32_t> ins,outs,all;bestcls=7;
  for(uint32_t id:in[v]){const Arc&a=arcs[id];if(contracted[a.u]||a.u==v)continue;ins.push_back(a.u);all.push_back(a.u);bestcls=std::min(bestcls,(int)a.cls);}
  for(uint32_t id:out[v]){const Arc&a=arcs[id];if(contracted[a.v]||a.v==v)continue;outs.push_back(a.v);all.push_back(a.v);bestcls=std::min(bestcls,(int)a.cls);}
  auto uniq=[](std::vector<uint32_t>&x){std::sort(x.begin(),x.end());x.erase(std::unique(x.begin(),x.end()),x.end());};uniq(ins);uniq(outs);uniq(all);
  incn=(int)ins.size();outn=(int)outs.size();deg=(int)all.size();if(neigh)*neigh=std::move(all);
 };
 auto priority=[&](uint32_t v)->int64_t{
  if(contracted[v])return std::numeric_limits<int64_t>::max()/4;
  int ni,no,deg,cls;active_info(v,ni,no,deg,cls,nullptr);
  int64_t potential=(int64_t)ni*(int64_t)no-(int64_t)ni-(int64_t)no;
  // Low degree/local roads first. Level and contracted-neighbour penalties prevent pathological deep stars.
  int64_t p=clamp_prio(potential)*14 + (int64_t)deg*5 + (int64_t)cnei[v]*18 + (int64_t)level[v]*4 + (int64_t)(8-cls)*8;
  if(deg==2)p=p>20?p-20:0;
  return p+(portal[v]?(1LL<<48):0);
 };
 std::priority_queue<PQ,std::vector<PQ>,std::greater<PQ>> pq;
 for(uint32_t v=0;v<n;v++)pq.push({priority(v),v,generation[v]});
 uint64_t shortcut_count=0,witness_runs=0,witness_settled=0,witness_capped=0;
 const uint64_t shortcut_guard=(uint64_t)((double)base_m*max_shortcut_ratio)+1024u;

 for(uint32_t oi=0;oi<n;){
  PQ top{};bool got=false;
  while(!pq.empty()){
   top=pq.top();pq.pop();if(contracted[top.n]||top.gen!=generation[top.n])continue;got=true;break;
  }
  if(!got){std::cerr<<"priority queue exhausted early\n";return 3;}
  uint32_t v=top.n;int64_t np=priority(v);
  // Lazy update: if another live candidate is clearly better, put this node back with fresh priority.
  int64_t nextp=std::numeric_limits<int64_t>::max()/4;
  while(!pq.empty()){
   const PQ&q=pq.top();if(contracted[q.n]||q.gen!=generation[q.n]){pq.pop();continue;}nextp=q.p;break;
  }
  if(np>nextp){generation[v]++;pq.push({np,v,generation[v]});continue;}

  std::unordered_map<uint32_t,uint32_t> inc,outc;
  inc.reserve(in[v].size()*2+1);outc.reserve(out[v].size()*2+1);
  for(uint32_t id:in[v]){const Arc&a=arcs[id];if(contracted[a.u]||a.u==v)continue;auto it=inc.find(a.u);if(it==inc.end()||a.cost<arcs[it->second].cost)inc[a.u]=id;}
  for(uint32_t id:out[v]){const Arc&a=arcs[id];if(contracted[a.v]||a.v==v)continue;auto it=outc.find(a.v);if(it==outc.end()||a.cost<arcs[it->second].cost)outc[a.v]=id;}
  std::vector<uint32_t> affected;affected.reserve(inc.size()+outc.size());for(auto&x:inc)affected.push_back(x.first);for(auto&x:outc)affected.push_back(x.first);std::sort(affected.begin(),affected.end());affected.erase(std::unique(affected.begin(),affected.end()),affected.end());

  for(auto &ikv:inc){
   uint32_t u=ikv.first,ia=ikv.second;
   struct Cand{uint32_t w,cost,oa;};std::vector<Cand> cand;cand.reserve(outc.size());uint32_t maxc=0;
   for(auto &okv:outc){uint32_t w=okv.first;if(w==u)continue;uint32_t oa=okv.second;uint64_t cc=(uint64_t)arcs[ia].cost+arcs[oa].cost;if(cc>0xffffffffu)continue;uint32_t c=(uint32_t)cc;cand.push_back({w,c,oa});maxc=std::max(maxc,c);}
   if(cand.empty())continue;
   ++witness_runs;if(++epoch==0){std::fill(stamp.begin(),stamp.end(),0);epoch=1;}
   std::priority_queue<QN,std::vector<QN>,std::greater<QN>> wq;setd(u,0);wq.push({0,u});uint32_t settled=0;bool capped=false;
   while(!wq.empty()){
    QN q=wq.top();wq.pop();if(q.d!=getd(q.n))continue;if(q.d>maxc)break;
    if(++settled>witness_limit){capped=true;witness_capped++;break;}
    for(uint32_t eid:out[q.n]){const Arc&a=arcs[eid];if(a.v==v||contracted[a.v]||contracted[a.u])continue;uint64_t nd=(uint64_t)q.d+a.cost;if(nd>maxc||nd>=0xffffffffu)continue;uint32_t od=getd(a.v);if(od==NO||(uint32_t)nd<od){setd(a.v,(uint32_t)nd);wq.push({(uint32_t)nd,a.v});}}
   }
   witness_settled+=settled;
   for(const Cand&c:cand){uint32_t wd=getd(c.w);if(!capped&&wd!=NO&&wd<=c.cost)continue;if(capped&&wd!=NO&&wd<=c.cost)continue;
    bool dominated=false;for(uint32_t eid:out[u]){const Arc&e=arcs[eid];if(!contracted[e.v]&&e.v==c.w&&e.cost<=c.cost){dominated=true;break;}}if(dominated)continue;
    Arc s{u,c.w,c.cost,ia,c.oa,(uint8_t)std::min((int)arcs[ia].cls,(int)arcs[c.oa].cls),ARC_SHORTCUT};uint32_t sid=(uint32_t)arcs.size();arcs.push_back(s);out[u].push_back(sid);in[c.w].push_back(sid);shortcut_count++;
    level[u]=std::max(level[u],level[v]+1);level[c.w]=std::max(level[c.w],level[v]+1);
    if(shortcut_count>shortcut_guard){std::cerr<<"ABORT shortcut explosion: shortcuts="<<shortcut_count<<" base="<<base_m<<" ratio="<<(double)shortcut_count/std::max(1u,base_m)<<" contracted="<<oi<<"/"<<n<<"\n";return 4;}
   }
  }
  contracted[v]=1;rank[v]=oi++;for(uint32_t x:affected){if(!contracted[x]){cnei[x]++;level[x]=std::max(level[x],level[v]+1);generation[x]++;pq.push({priority(x),x,generation[x]});}}
  if(oi%100000==0||oi==n){double r=(double)shortcut_count/std::max(1u,base_m);std::cerr<<"contracted "<<oi<<"/"<<n<<" arcs="<<arcs.size()<<" shortcuts="<<shortcut_count<<" ratio="<<r<<" witness_capped="<<witness_capped<<"\n";}
 }

 std::vector<std::vector<uint32_t>> upf(n),upr(n);
 for(uint32_t id=0;id<arcs.size();id++){const Arc&a=arcs[id];if(rank[a.u]<rank[a.v])upf[a.u].push_back(id);if(rank[a.v]<rank[a.u])upr[a.v].push_back(id);}
 std::vector<uint32_t> upfi(n+1),upri(n+1),upfr,uprr;uint64_t ufct=0,urct=0;
 for(uint32_t i=0;i<n;i++){upfi[i]=(uint32_t)ufct;ufct+=upf[i].size();upfr.insert(upfr.end(),upf[i].begin(),upf[i].end());upri[i]=(uint32_t)urct;urct+=upr[i].size();uprr.insert(uprr.end(),upr[i].begin(),upr[i].end());}upfi[n]=(uint32_t)ufct;upri[n]=(uint32_t)urct;
 OutHdr oh{{'C','H','0','4'},3,n,base_m,(uint32_t)arcs.size(),(uint32_t)upfr.size(),(uint32_t)uprr.size()};
 std::ofstream g(argv[2],std::ios::binary|std::ios::trunc);if(!g){std::cerr<<"open out failed\n";return 2;}g.write((char*)&oh,sizeof(oh));g.write((char*)rank.data(),rank.size()*4);g.write((char*)comp.data(),comp.size()*4);
 for(const Arc&a:arcs){ArcDisk d{a.u,a.v,a.cost,a.left,a.right,a.cls,a.flags,0};g.write((char*)&d,sizeof(d));}
 g.write((char*)upfi.data(),upfi.size()*4);g.write((char*)upfr.data(),upfr.size()*4);g.write((char*)upri.data(),upri.size()*4);g.write((char*)uprr.data(),uprr.size()*4);g.close();
 std::cerr<<"DONE nodes="<<n<<" base="<<base_m<<" arcs="<<arcs.size()<<" shortcuts="<<shortcut_count<<" shortcut_ratio="<<(double)shortcut_count/std::max(1u,base_m)<<" upf="<<upfr.size()<<" upr="<<uprr.size()<<" witness_runs="<<witness_runs<<" avg_witness_settled="<<(witness_runs?(double)witness_settled/witness_runs:0.0)<<" capped="<<witness_capped<<"\n";
 return 0;
}
