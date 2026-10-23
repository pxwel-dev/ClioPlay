/* Bounded UTF-8 decoding shared by the UI and Bluetooth metadata receiver. */
#ifndef CLIO_UTF8_H
#define CLIO_UTF8_H
static unsigned utf8_span(const char*s){unsigned c=(unsigned char)s[0],n;if(!c)return 0;if(c<128)return 1;n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;if(!n)return 0;for(unsigned i=1;i<n;i++)if(!s[i]||((unsigned char)s[i]&192)!=128)return 0;if(n==3&&((c==0xe0&&(unsigned char)s[1]<0xa0)||(c==0xed&&(unsigned char)s[1]>=0xa0)))return 0;if(n==4&&((c==0xf0&&(unsigned char)s[1]<0x90)||(c==0xf4&&(unsigned char)s[1]>=0x90)))return 0;return n;}
static unsigned utf8_copy(char*d,unsigned cap,const char*s,unsigned maxchars){unsigned out=0,chars=0;if(!cap)return 0;while(s&&*s&&chars<maxchars){unsigned n=utf8_span(s);if(!n){s++;continue;}if(out+n>=cap)break;for(unsigned i=0;i<n;i++)d[out++]=*s++;chars++;}d[out]=0;return out;}
static int utf8_code(const char**p){const char*s=*p;unsigned n=utf8_span(s);if(!n){if(*s)(*p)++;return 0xfffd;}unsigned cp=(unsigned char)*s;if(n>1){cp&=(1u<<(7-n))-1;for(unsigned i=1;i<n;i++)cp=(cp<<6)|((unsigned char)s[i]&63);}*p+=n;return(int)cp;}
static void utf8_backspace(char*s){unsigned n=0;while(s[n])n++;if(!n)return;n--;while(n&&((unsigned char)s[n]&192)==128)n--;s[n]=0;}
static unsigned utf8_count(const char*s){unsigned n=0;while(s&&*s){unsigned span=utf8_span(s);s+=span?span:1;n++;}return n;}
static int utf8_native(const char*s){while(s&&*s){int cp=utf8_code(&s);if(cp>126&&cp!=176&&cp!=8226&&cp!=8230)return 1;}return 0;}
#endif
