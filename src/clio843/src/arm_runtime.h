#ifndef HOST_TEST
/* Freestanding ARM EABI compiler-runtime shims for the device build. */
void *memcpy(void *d0,const void *s0,unsigned n){u8*d=(u8*)d0;const u8*s=(const u8*)s0;while(n--)*d++=*s++;return d0;}
__attribute__((naked)) void __aeabi_uidivmod(void){__asm__ volatile(
  "cmp r1,#0\n"
  "beq 2f\n"
  "mov r2,#0\n"
  "mov r3,#0\n"
  "mov r12,#32\n"
  "1:\n"
  "mov r3,r3,lsl #1\n"
  "orr r3,r3,r0,lsr #31\n"
  "mov r0,r0,lsl #1\n"
  "cmp r3,r1\n"
  "subcs r3,r3,r1\n"
  "adc r2,r2,r2\n"
  "subs r12,r12,#1\n"
  "bne 1b\n"
  "mov r0,r2\n"
  "mov r1,r3\n"
  "bx lr\n"
  "2:\n"
  "mov r0,#0\n"
  "mov r1,#0\n"
  "bx lr\n");}
__attribute__((naked)) void __aeabi_uidiv(void){__asm__ volatile("b __aeabi_uidivmod\n");}
__attribute__((naked)) void __aeabi_idivmod(void){__asm__ volatile(
  "push {r4,r5,lr}\n"
  "mov r4,r0\n"
  "eor r5,r0,r1\n"
  "cmp r0,#0\n"
  "rsblt r0,r0,#0\n"
  "cmp r1,#0\n"
  "rsblt r1,r1,#0\n"
  "bl __aeabi_uidivmod\n"
  "cmp r5,#0\n"
  "rsblt r0,r0,#0\n"
  "cmp r4,#0\n"
  "rsblt r1,r1,#0\n"
  "pop {r4,r5,pc}\n");}
__attribute__((naked)) void __aeabi_idiv(void){__asm__ volatile("b __aeabi_idivmod\n");}
#endif
