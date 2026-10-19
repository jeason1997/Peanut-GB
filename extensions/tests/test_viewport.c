#include "../include/gx_viewport.h"
#include <assert.h>
#include <stdio.h>
struct test {unsigned rows, begins, bg_pixels; uint16_t nw,nh,w,h,left,top;};
static void begin(void*u){((struct test*)u)->begins++;}
static void background(void*u,uint8_t*p,uint16_t n,int32_t x,int32_t y){
 struct test*t=u;t->bg_pixels+=n;
 for(unsigned i=0;i<n;i++) {assert(y<0||y>=t->nh||x+(int)i<0||x+(int)i>=t->nw);p[i]=7;}
}
static void emit(void*u,const uint8_t*p,uint16_t n,uint16_t y){
 struct test*t=u;assert(y==t->rows++&&n==t->w);
 for(unsigned x=0;x<n;x++)assert(p[x]==((y>=t->top&&y<t->top+t->nh&&x>=t->left&&x<t->left+t->nw)?3:7));
}
static void scene(unsigned nw,unsigned nh,unsigned w,unsigned h){
 uint8_t storage[1026],native[512];memset(storage,0xa5,sizeof(storage));memset(native,3,sizeof(native));
 struct test t={0,0,0,nw,nh,w,h,(w-nw)/2,(h-nh)/2};struct gx_viewport v;
 struct gx_callbacks cb={begin,background,NULL,emit};
 assert(!gx_viewport_init(&v,nw,nh,w,h,storage+1,w-1,GX_BACKGROUND,0,&t,cb));
 assert(gx_viewport_init(&v,nw,nh,w,h,storage+1,w,GX_BACKGROUND,0,&t,cb));
 assert(!gx_viewport_push(&v,0,native,nw));gx_viewport_begin(&v);
 assert(!gx_viewport_push(&v,1,native,nw));
 for(unsigned y=0;y<nh;y++)assert(gx_viewport_push(&v,y,native,nw));
 assert(!gx_viewport_push(&v,nh,native,nw));assert(t.begins==1&&t.rows==h&&t.bg_pixels==w*h-nw*nh);
 assert(storage[0]==0xa5&&storage[w+1]==0xa5);for(unsigned i=0;i<nw;i++)assert(native[i]==3);
}
static unsigned fg_calls;
static void foreground(void*u,uint8_t*p,uint16_t n,uint16_t y){(void)u;assert(n==2&&y==0);fg_calls++;p[0]=9;}
static void tiny_emit(void*u,const uint8_t*p,uint16_t n,uint16_t y){(void)u;assert(n==2&&y==0);assert(p[0]==(fg_calls?9:3)&&p[1]==3);}
static void foreground_check(void){struct gx_viewport v;uint8_t row[2],native[2]={3,3};struct gx_callbacks cb={NULL,NULL,foreground,tiny_emit};
 assert(gx_viewport_init(&v,2,1,2,1,row,2,0,0,NULL,cb));gx_viewport_begin(&v);assert(gx_viewport_push(&v,0,native,2));assert(!fg_calls);
 v.features=GX_FOREGROUND;gx_viewport_begin(&v);assert(gx_viewport_push(&v,0,native,2));assert(fg_calls==1&&native[0]==3);
}
int main(void){foreground_check();scene(160,144,160,144);scene(160,144,160,145);scene(160,144,160,160);scene(160,144,200,150);scene(160,144,321,241);scene(256,240,320,256);puts("PASS portable compositor: GB/NES dimensions, odd margins, ordering, exact margin work, bounds and immutable native input.");}
