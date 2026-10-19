/* Real-ROM test through the neutral view + compositor, no core viewport API. */
#define ENABLE_SOUND 0
#include "../../peanut_gb.h"
#include "../games/pokemon_silver_jp.h"
#include "../ports/peanut-gb/gx_peanut_view.h"
#include "../include/gx_viewport.h"
#include <assert.h>
#include <stdio.h>
struct host {
 struct gb_s gb;struct gx_gb_view view;struct pgb_silver_jp silver;struct gx_viewport viewport;
 uint8_t *rom,save[32768],row[200];unsigned rows,valid,extra,frames;
};
static uint8_t rr(struct gb_s*g,uint_fast32_t a){struct host*h=g->direct.priv;h->view.cpu_reg.pc.reg=g->cpu_reg.pc.reg;h->view.cpu_reg.bc.reg=g->cpu_reg.bc.reg;return pgb_silver_jp_rom_read(&h->silver,&h->view,a);}
static uint8_t sr(struct gb_s*g,uint_fast32_t a){return ((struct host*)g->direct.priv)->save[a];}
static void sw(struct gb_s*g,uint_fast32_t a,uint8_t v){((struct host*)g->direct.priv)->save[a]=v;}
static void fail(struct gb_s*g,enum gb_error_e e,uint16_t a){(void)g;(void)e;(void)a;abort();}
static void begin(void*u){struct host*h=u;gx_peanut_view(&h->view,&h->gb);h->valid+=pgb_silver_begin_frame(&h->silver,&h->view);}
static void bg(void*u,uint8_t*p,uint16_t n,int32_t x,int32_t y){struct host*h=u;gx_peanut_view(&h->view,&h->gb);pgb_silver_jp_background(&h->silver,&h->view,p,n,x,y);h->extra+=n;}
static void fg(void*u,uint8_t*p,uint16_t n,uint16_t y){struct host*h=u;assert(n==160);gx_peanut_view(&h->view,&h->gb);pgb_silver_jp_foreground(&h->silver,&h->view,p,y);}
static void emit(void*u,const uint8_t*p,uint16_t n,uint16_t y){struct host*h=u;(void)p;assert(n==200&&y<150);h->rows++;}
static void line(struct gb_s*g,const uint8_t*p,uint_fast8_t y){struct host*h=g->direct.priv;if(y==0)gx_viewport_begin(&h->viewport);
/* LCD enable can start a partial frame; wait for its first complete row 0. */
if(!h->viewport.frame_open) return;
if(!gx_viewport_push(&h->viewport,y,p,160)) h->viewport.frame_open=0;}
int main(int argc,char**argv){
 static struct host h;assert(argc==3);h.rom=malloc(1048576);assert(h.rom);FILE*f=fopen(argv[1],"rb");assert(f);assert(fread(h.rom,1,1048576,f)==1048576);fclose(f);
 f=fopen(argv[2],"rb");assert(f);assert(fread(h.save,1,sizeof(h.save),f)==sizeof(h.save));fclose(f);
 assert(pgb_silver_jp_init(&h.silver,h.rom,1048576));pgb_silver_jp_viewport(&h.silver,200,150);
 assert(gb_init(&h.gb,rr,sr,sw,fail,&h)==GB_INIT_NO_ERROR);gx_peanut_view(&h.view,&h.gb);
 struct gx_callbacks cb={begin,bg,fg,emit};assert(gx_viewport_init(&h.viewport,160,144,200,150,h.row,sizeof(h.row),3,0,&h,cb));gb_init_lcd(&h.gb,line);
 for(h.frames=0;h.frames<1800;h.frames++){unsigned k=255;if(h.frames>=120&&h.frames<460&&h.frames%60<4)k&=~(h.frames<180?JOYPAD_START:JOYPAD_A);h.gb.direct.joypad=k;gb_run_frame(&h.gb);}
 assert(h.valid>500&&h.rows>150*1000);printf("PASS neutral host + native callback: %u valid scenes, %u emitted rows, %u margin pixels.\n",h.valid,h.rows,h.extra);free(h.rom);
}
