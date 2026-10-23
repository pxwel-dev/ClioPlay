/* Bounded allocator and small libc surface for the freestanding text engine.
 * Its shaping and bidi work run only on the UI's single cooperative thread. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#if defined(HOST_TEST) && !defined(CLIO_TEST_POOL)
void*hb_malloc_impl(size_t n){return malloc(n);}
void*hb_calloc_impl(size_t n,size_t s){return calloc(n,s);}
void*hb_realloc_impl(void*p,size_t n){return realloc(p,n);}
void hb_free_impl(void*p){free(p);}
#else
#define POOL_BYTES (2u*1024u*1024u)
static union {uint64_t alignment;unsigned char bytes[POOL_BYTES];} pool __attribute__((aligned(16)));
struct Block {uint32_t size,used;struct Block*next,*prev;} __attribute__((aligned(16)));
static struct Block*head;
void*hb_malloc_impl(size_t n){if(!n)n=1;if(n>POOL_BYTES-64)return 0;n=(n+15)&~15u;if(!head){head=(struct Block*)pool.bytes;head->size=POOL_BYTES-sizeof(*head);head->next=head->prev=0;head->used=0;}for(struct Block*b=head;b;b=b->next)if(!b->used&&b->size>=n){if(b->size>=n+sizeof(*b)+16){struct Block*c=(struct Block*)((unsigned char*)(b+1)+n);c->size=b->size-n-sizeof(*b);c->used=0;c->next=b->next;c->prev=b;if(c->next)c->next->prev=c;b->next=c;b->size=n;}b->used=1;return b+1;}return 0;}
void hb_free_impl(void*p){if(!p)return;struct Block*b=(struct Block*)p-1;b->used=0;if(b->next&&!b->next->used){struct Block*c=b->next;b->size+=sizeof(*b)+c->size;b->next=c->next;if(b->next)b->next->prev=b;}if(b->prev&&!b->prev->used){struct Block*c=b->prev;c->size+=sizeof(*b)+b->size;c->next=b->next;if(c->next)c->next->prev=c;}}
void*hb_calloc_impl(size_t n,size_t s){if(s&&n>POOL_BYTES/s)return 0;size_t size=n*s;unsigned char*p=hb_malloc_impl(size);if(p)for(size_t i=0;i<size;i++)p[i]=0;return p;}
void*hb_realloc_impl(void*p,size_t n){if(!p)return hb_malloc_impl(n);if(!n){hb_free_impl(p);return 0;}struct Block*b=(struct Block*)p-1;if(b->size>=n)return p;void*q=hb_malloc_impl(n);if(q){memcpy(q,p,b->size);hb_free_impl(p);}return q;}
unsigned cliou_heap_used(void){unsigned n=0;for(struct Block*b=head;b;b=b->next)if(b->used)n+=b->size;return n;}
#ifndef HOST_TEST
void*malloc(size_t n){return hb_malloc_impl(n);}void*calloc(size_t n,size_t s){return hb_calloc_impl(n,s);}void*realloc(void*p,size_t n){return hb_realloc_impl(p,n);}void free(void*p){hb_free_impl(p);}
void*memset(void*p,int value,size_t n){unsigned char*d=p;while(n--)*d++=(unsigned char)value;return p;}
void*memmove(void*d,const void*s,size_t n){unsigned char*a=d;const unsigned char*b=s;if(a<b)for(size_t i=0;i<n;i++)a[i]=b[i];else while(n){n--;a[n]=b[n];}return d;}
int memcmp(const void*a,const void*b,size_t n){const unsigned char*x=a,*y=b;while(n--){if(*x!=*y)return*x-*y;x++;y++;}return 0;}
size_t strlen(const char*s){size_t n=0;while(s[n])n++;return n;}
int strcmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return(unsigned char)*a-(unsigned char)*b;}
int strncmp(const char*a,const char*b,size_t n){while(n&&*a&&*a==*b){a++;b++;n--;}return n?(unsigned char)*a-(unsigned char)*b:0;}
char*strncpy(char*d,const char*s,size_t n){size_t i=0;for(;i<n&&s[i];i++)d[i]=s[i];for(;i<n;i++)d[i]=0;return d;}
char*strchr(const char*s,int c){for(;;s++){if(*s==(char)c)return(char*)s;if(!*s)return 0;}}
char*strstr(const char*s,const char*n){size_t len=strlen(n);for(;*s;s++)if(!strncmp(s,n,len))return(char*)s;return len?0:(char*)s;}
int abs(int x){return x<0?-x:x;}
static void swap_bytes(unsigned char*a,unsigned char*b,size_t size){while(size--){unsigned char c=*a;*a++=*b;*b++=c;}}
void qsort(void*data,size_t n,size_t size,int(*cmp)(const void*,const void*)){unsigned char*b=data;if(n<2||!size)return;/* Bounded insertion sort; shaping sorts only short glyph/feature arrays. */for(size_t i=1;i<n;i++){size_t j=i;while(j&&cmp(b+(j-1)*size,b+j*size)>0){swap_bytes(b+(j-1)*size,b+j*size,size);j--;}}}
long strtol(const char*s,char**end,int base){int neg=0;unsigned long v=0;if(*s=='-'){neg=1;s++;}if(!base)base=10;while(*s){int d=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='z'?*s-'a'+10:*s>='A'&&*s<='Z'?*s-'A'+10:99;if(d>=base)break;v=v*(unsigned)base+(unsigned)d;s++;}if(end)*end=(char*)s;return neg?-(long)v:(long)v;}
unsigned long strtoul(const char*s,char**end,int base){return(unsigned long)strtol(s,end,base);}
float fabsf(float x){union {float f;uint32_t u;}v={x};v.u&=0x7fffffffu;return v.f;}
double fabs(double x){union {double d;uint64_t u;}v={x};v.u&=0x7fffffffffffffffull;return v.d;}
static float round_float(float x,int ceil_mode){union {float f;uint32_t u;}v={x};unsigned sign=v.u>>31;int e=(int)((v.u>>23)&255)-127;if(e>=23)return x;if(e<0){if(!(v.u&0x7fffffffu))return x;v.u=(ceil_mode?!sign:sign)?(sign?0xbf800000u:0x3f800000u):(sign?0x80000000u:0);return v.f;}unsigned mask=(1u<<(23-e))-1;if(!(v.u&mask))return x;if(ceil_mode?!sign:sign)v.u+=1u<<(23-e);v.u&=~mask;return v.f;}
float floorf(float x){return round_float(x,0);}float ceilf(float x){return round_float(x,1);}
/* These services are compiled out of HB_TINY shaping but may be referenced by
 * diagnostic-only sections before linker garbage collection. */
int snprintf(char*d,size_t cap,const char*format,...){(void)format;if(cap)d[0]=0;return 0;}
void abort(void){for(;;){}}
#endif
#endif
