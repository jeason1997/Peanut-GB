/* Optional frontend adapter; the GBC core and the DMG game backend stay intact. */
#ifndef GX_CGB_VIEWPORT_H
#define GX_CGB_VIEWPORT_H
#include "../include/gx_viewport.h"
#include "../include/gx_gb_background.h"
#include "../games/pokemon_silver_jp.h"
#include "../ports/peanut-gb/gx_peanut_view.h"
#include "background.h"
struct gbc_viewport {
    struct gx_viewport compose;
    struct gx_gb_view view;
    struct pgb_silver_jp silver;
    struct gb_s *gb;
    uint32_t *output;
    uint8_t *row;
    unsigned long valid_frames, map_frames, emitted_rows, margin_sprite_pixels, native_seam_pixels;
    unsigned palette_mismatches;
    uint8_t tile_attr[256];
    uint8_t roof_tile[256];
    int roof_cache_x,roof_cache_y,roof_cache_valid;
    unsigned roof_cache_group;
    uint8_t roof_night,roof_time_ready;
    int8_t object_palettes[PGB_SILVER_MAX_OBJECTS];
    struct { uint8_t group,number,index,palette; } palette_history[PGB_SILVER_MAX_OBJECTS];
    unsigned palette_history_next;
    int object_palettes_ready;
    int silver_enabled, background_enabled;
};
#include "silver_sprites.h"
/* Use the same exact-ROM NPC lifetime policy as the DMG frontend. Rendering
 * alone cannot predict the random walks of objects that the game deletes. */
static uint8_t gbc_view_rom_read(struct gbc_viewport *v,struct gb_s *g,uint_fast32_t a)
{
    if(v->gb!=g||!v->silver_enabled||!v->background_enabled)return v->silver.rom[a];
    v->view.wram=g->wram;
    v->view.cpu_reg.pc.reg=g->cpu_reg.pc.reg;
    v->view.cpu_reg.bc.reg=g->cpu_reg.bc.reg;
    return pgb_silver_jp_rom_read(&v->silver,&v->view,a);
}
static uint32_t gbc_rgb(struct gb_s *g,uint8_t p)
{
    if(p==255)return 0xff000000u; /* presentation-only blank sentinel */
    if(g->cgb.cgbMode) {
        unsigned c=g->cgb.fixPalette[p&63];
        return 0xff000000u|(((c>>10)&31)*255/31<<16)|(((c>>5)&31)*255/31<<8)|((c&31)*255/31);
    }
    unsigned c=255-(p&3)*85;
    return 0xff000000u|(c<<16)|(c<<8)|c;
}
#include "silver_roofs.h"
static void gbc_view_begin(void *user)
{
    struct gbc_viewport *v=user;struct gb_s *g=v->gb;
    v->roof_cache_valid=0;
    v->object_palettes_ready=0;
    v->roof_time_ready=0;
    gx_peanut_view(&v->view,g);
    if(!v->silver_enabled || !v->background_enabled || (v->compose.width==160&&v->compose.height==144))return;
    /* Silver Rev1's map state is in fixed C000 plus physical WRAM bank 1,
     * not whichever temporary bank the CPU selects for palette operations. */
    if(!pgb_silver_begin_frame(&v->silver,&v->view))return;
    v->map_frames++;
    if(!g->cgb.cgbMode){v->valid_frames++;return;}
    /* Exact-ROM tileset palette table: wTilesetPalettes=$D0C1, ROM bank $02.
     * map_palettes.asm: packed nibbles, bit3 -> MapGroupPalettes (all roof=6).
     * Validate against the native visible attribute map before extending. */
    unsigned ptr=pgb_silver_word(&v->view,0xd0c1);
    if(ptr<0x4000||ptr>0x7f80){v->silver.frame_valid=0;return;}
    for(unsigned i=0;i<256;i++) {
        unsigned p=v->silver.rom[2*0x4000+ptr-0x4000+i/2];
        p=(p>>((i&1)*4))&15;
        v->tile_attr[i]=(uint8_t)((p&8)?6:p);
        v->roof_tile[i]=(uint8_t)(v->tile_attr[i]==6);
    }
    unsigned bad=0;
    for(unsigned y=0;y<144;y+=8)for(unsigned x=0;x<160;x+=8) {
        unsigned by=(y+g->hram_io[0x42])&255,bx=(x+g->hram_io[0x43])&255;
        unsigned cell=0x1800+(by>>3)*32+(bx>>3);
        if(g->vram[cell+0x2000]!=v->tile_attr[g->vram[cell]])bad++;
    }
    v->palette_mismatches=bad;
    if(bad){v->silver.frame_valid=0;return;}
    gbc_silver_remove_predictions(v);
    v->roof_night=(uint8_t)((v->view.wram[0x155b]&3)>=2);v->roof_time_ready=1;
    gbc_silver_capture_palettes(v);
    v->valid_frames++;
}
static void gbc_view_background(void *user,uint8_t *out,uint16_t n,int32_t x,int32_t y)
{
    struct gbc_viewport *v=user;struct gb_s *g=v->gb;
    if(!v->background_enabled || !(g->hram_io[0x40]&0x80))return;
    if(v->silver_enabled) {
        /* Menus/battles/transitions stay blank, never guessed from old VRAM. */
        if(!v->silver.frame_valid)return;
        if(!g->cgb.cgbMode){pgb_silver_jp_background(&v->silver,&v->view,out,n,x,y);return;}
        const struct pgb_silver_map *m=&v->silver.frame_map;
        uint8_t tile=0;
        for(unsigned i=0;i<n;i++) {
            int wx=m->origin_x+x+(int)i,wy=m->origin_y+y;
            if(!i || !(wx&7))tile=pgb_silver_world_tile(&v->silver,&v->view,m,wx,wy);
            out[i]=gx_cgb_tile_pixel(g->vram,g->hram_io[0x40],tile,v->tile_attr[tile],(unsigned)wx&7,(unsigned)wy&7);
        }
        gbc_silver_sprites(v,out,n,x,y);
    } else if(g->cgb.cgbMode)gx_cgb_background(g->vram,g->hram_io,out,n,x,y);
    else gx_gb_background(&v->view,out,n,x,y,0);
}
static void gbc_view_emit(void *user,const uint8_t *p,uint16_t n,uint16_t y)
{
    struct gbc_viewport *v=user;
    for(unsigned x=0;x<n;x++) {
        v->output[(size_t)y*v->compose.width+x]=gbc_view_colour(v,p[x],x,y);
        if(v->gb->cgb.cgbMode&&p[x]>=32&&p[x]<64&&
            (x<v->compose.left||x>=(unsigned)v->compose.left+160||y<v->compose.top||y>=(unsigned)v->compose.top+144))v->margin_sprite_pixels++;
    }
    v->emitted_rows++;
}
static void gbc_view_foreground(void *user,uint8_t *p,uint16_t width,uint16_t y)
{
    struct gbc_viewport *v=user;
    if(v->background_enabled&&v->silver_enabled&&v->silver.frame_valid&&v->gb->cgb.cgbMode&&
        (v->silver.features&PGB_SILVER_SPRITES)&&(v->compose.width!=160||v->compose.height!=144)) {
        uint8_t before[160];if(width!=160)return;memcpy(before,p,160);
        gbc_silver_sprite_layer(v,p,width,0,y,1);
        for(unsigned i=0;i<160;i++)v->native_seam_pixels+=before[i]!=p[i];
    }
}
static int gbc_view_init(struct gbc_viewport *v,struct gb_s *g,const uint8_t *rom,
    size_t bytes,uint16_t w,uint16_t h,uint32_t *output,uint8_t *row,int background)
{
    /* The shared header also exports legacy DMG-only entry points. */
    (void)pgb_silver_jp_rom_read;(void)pgb_silver_jp_foreground;(void)pgb_silver_jp_margins;
    memset(v,0,sizeof(*v));v->gb=g;v->output=output;v->row=row;v->background_enabled=background;
    gx_peanut_view(&v->view,g);
    v->silver_enabled=pgb_silver_jp_init(&v->silver,rom,bytes);
    pgb_silver_jp_configure(&v->silver,PGB_SILVER_ALL,PGB_SILVER_MAX_OBJECTS);
    pgb_silver_jp_viewport(&v->silver,w,h);
    struct gx_callbacks cb={gbc_view_begin,gbc_view_background,gbc_view_foreground,gbc_view_emit};
    return gx_viewport_init(&v->compose,160,144,w,h,row,w,GX_BACKGROUND|GX_FOREGROUND,255,v,cb);
}
static void gbc_view_line(struct gbc_viewport *v,const uint8_t *p,unsigned y)
{
    if(!y)gx_viewport_begin(&v->compose);
    if(v->compose.frame_open&&!gx_viewport_push(&v->compose,(uint16_t)y,p,160))v->compose.frame_open=0;
}
#endif
