/* Intentionally no Peanut-GB include: compile in a foreign host. */
#include "../games/pokemon_silver_jp.h"
#include "../include/gx_gb_background.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 struct pgb_silver_jp s;uint8_t rom[32]={0},ram[8192]={0},vram[8192]={0},oam[160]={0},io[256]={0},row[200];
 struct gx_gb_view g={0};g.wram=ram;g.vram=vram;g.oam=oam;g.hram_io=io;
 g.display.bg_palette[0]=2;gx_gb_background(&g,row,200,-20,-3,1);
 for(unsigned i=0;i<200;i++)assert(row[i]==(2|GX_GB_PALETTE_BG));
 assert(!pgb_silver_jp_init(&s,rom,sizeof(rom)));assert(!s.supported);
 assert(pgb_silver_jp_configure(&s,0,28));assert(!pgb_silver_jp_configure(&s,PGB_SILVER_SPRITES,28));
 assert(!pgb_silver_jp_configure(&s,PGB_SILVER_TERRAIN|PGB_SILVER_NEIGHBOR_NPCS,28));
 assert(!pgb_silver_jp_configure(&s,PGB_SILVER_ALL,27));
 assert(!pgb_silver_jp_configure(&s,PGB_SILVER_ALL,PGB_SILVER_MAX_OBJECTS+1));
 assert(pgb_silver_jp_configure(&s,PGB_SILVER_TERRAIN,28));
 assert(!pgb_silver_jp_background(&s,&g,row,200,-20,-3));
 assert(pgb_silver_jp_configure(&s,0,28));pgb_silver_jp_viewport(&s,200,150);
 for(unsigned i=0;i<32;i++)assert(pgb_silver_jp_rom_read(&s,&g,i)==rom[i]);
 printf("PASS portable configuration: state=%zu, object=%zu, part=%zu, worst parts stack=%zu bytes; objects=%u maps=%u cache=%u.\n",sizeof(s),sizeof(struct pgb_silver_object),sizeof(struct pgb_silver_part),sizeof(struct pgb_silver_part)*PGB_SILVER_MAX_OBJECTS*4,PGB_SILVER_MAX_OBJECTS,PGB_SILVER_MAX_MAPS,PGB_SILVER_BLOCK_CACHE);
}
