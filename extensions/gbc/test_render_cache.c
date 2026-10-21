/* Compare fast rendering with the original scalar samplers, including
 * negative coordinates, cache collisions and changes between scanlines. */
#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
int main(void)
{
    struct gb_s *g=calloc(1,sizeof(*g));struct gbc_viewport *v=calloc(1,sizeof(*v));
    uint8_t *image=calloc(1,1048576);assert(g&&v&&image);
    v->gb=g;gx_peanut_view(&v->view,g);v->silver.rom=image;v->silver.rom_size=1048576;
    gx_cgb_row_decoder_init(&v->tile_decoder);
    for(unsigned pair=0;pair<65536;pair++)for(unsigned variant=0;variant<8;variant++) {
        unsigned lcdc=(pair&1)?0x10:0,tile=(pair>>5)&255,py=(pair>>2)&7;
        unsigned attr=((pair>>8)&7)|((variant&1)?8:0)|((variant&2)?32:0)|((variant&4)?64:0);
        unsigned row=(attr&64)?7-py:py;
        unsigned at=((lcdc&16)?tile*16:0x800+((tile+128)&255)*16)+((attr&8)?0x2000:0)+row*2;
        g->vram[at]=(uint8_t)pair;g->vram[at+1]=(uint8_t)(pair>>8);
        uint8_t pixels[10];memset(pixels,0xa5,sizeof(pixels));
        unsigned first=(pair+variant)&7,count=8-first;
        gx_cgb_tile_span(&v->tile_decoder,g->vram,lcdc,(uint8_t)tile,(uint8_t)attr,py,first,count,pixels+1);
        assert(pixels[0]==0xa5&&pixels[count+1]==0xa5);
        for(unsigned i=0;i<count;i++)assert(pixels[1+i]==gx_cgb_tile_pixel(g->vram,lcdc,(uint8_t)tile,(uint8_t)attr,first+i,py));
    }
    v->silver.features=PGB_SILVER_STABLE_BORDER;v->silver.atlas_count=2;
    v->silver.frame_map.width=v->silver.frame_map.height=20;v->silver.frame_map.definitions=0x2000;
    for(unsigned i=0;i<2048;i++)image[0x2000+i]=(uint8_t)(i*37+(i>>4)*13);
    for(unsigned i=0;i<400;i++)image[0x10000+i]=v->silver.frame_blocks[i]=(uint8_t)(i%127+1);
    v->silver.atlas[0]=(struct pgb_silver_connection){.x=0,.y=0,.width=20,.height=20,.blocks=0x10000,.border=2,.group=26,.number=3};
    v->silver.atlas[1]=(struct pgb_silver_connection){.x=20,.y=0,.width=20,.height=20,.blocks=0x10000,.border=7,.group=24,.number=3};
    for(int y=-65;y<710;y+=7)for(int x=-2100;x<6500;x+=13)
        assert(gbc_silver_cached_tile(v,x,y)==pgb_silver_world_tile(&v->silver,&v->view,&v->silver.frame_map,x,y));
    uint8_t before=gbc_silver_cached_tile(v,96,96);
    v->silver.frame_blocks[3*20+3]++;
    gbc_silver_reset_terrain(v); /* changes become visible on the next frame */
    assert(gbc_silver_cached_tile(v,96,96)!=before);
    assert(gbc_silver_cached_tile(v,96,96)==pgb_silver_world_tile(&v->silver,&v->view,&v->silver.frame_map,96,96));
    v->silver.atlas[0].border=11;gbc_silver_reset_terrain(v);
    assert(gbc_silver_cached_tile(v,-320,-320)==pgb_silver_world_tile(&v->silver,&v->view,&v->silver.frame_map,-320,-320));
    /* Palette writes must affect the very next emitted row, without stale
     * cached BG/OBJ colours; switching to DMG and blank pixels also work. */
    uint32_t output_pixels[64*3];uint8_t indices[64];
    v->output=output_pixels;v->compose.width=64;v->silver_enabled=0;
    for(unsigned i=0;i<64;i++)indices[i]=(uint8_t)i;
    indices[63]=255;
    for(unsigned y=0;y<3;y++) {
        g->cgb.cgbMode=y<2;
        for(unsigned i=0;i<64;i++)g->cgb.fixPalette[i]=(uint16_t)((i*513+y*911)&32767);
        gbc_view_emit(v,indices,64,(uint16_t)y);
        for(unsigned i=0;i<64;i++)assert(output_pixels[y*64+i]==gbc_rgb(g,indices[i]));
    }
    free(g);free(v);free(image);
    puts("PASS render caches: 524288 bitplane rows vs scalar, signed/banked/XY/palette/clipping, terrain negative/collision/frame invalidation, scanline BG/OBJ writes and DMG switch");
    return 0;
}
