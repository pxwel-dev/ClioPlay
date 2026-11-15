/* Run existing remote workflows through the production 28-byte console decoder.
 * The control association is independent of csw_defaults: losing or changing a
 * default binding must fail the same behavioral assertions as a broken key. */
static void console_test_key(int code){
 static u32 now=1000;static int initialized;
 if(!initialized){csw_defaults(csw.map);initialized=1;}
 unsigned source=0,direction=5,rotation=3,pressed=0;
 switch(code){
 case K_ESC:source=1;break;case K_BACK:source=2;break;
 case K_SR:source=3;break;case K_MENU:source=5;break;case K_MAP:source=9;break;
 case K_UP:direction=1;break;case K_DOWN:direction=2;break;
 case K_RIGHT:direction=3;break;case K_LEFT:direction=4;break;
 case K_ENTER:pressed=1;break;
 default:assert(!"Remote action has no console test control");return;
 }
 u8 b[28]={0};b[0]=(u8)source;b[4]=(u8)direction;b[8]=(u8)rotation;
 b[16]=(u8)pressed;b[20]=1;now+=500;
 csw_feed(b,sizeof(b),now);
 int action=csw_next();assert(action==code);assert(!csw_next());key_handle(action);
 if(!source){b[4]=5;b[16]=0;csw_feed(b,sizeof(b),now+1);assert(!csw_next());}
}
#define key_handle(code) console_test_key(code)
