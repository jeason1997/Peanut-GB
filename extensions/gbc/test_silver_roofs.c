/* Mixed map owners share hardware palette 6; display must not share roof RGB. */
#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
static uint32_t rgb(unsigned c){return 0xff000000u|(((c>>10)&31)*255/31<<16)|(((c>>5)&31)*255/31<<8)|((c&31)*255/31);}
int main(int argc,char **argv)
{
    assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);
    rom=malloc(1048576);assert(rom);assert(fread(rom,1,1048576,f)==1048576);fclose(f);
    struct gb_s *g=calloc(1,sizeof(*g));struct gbc_viewport *v=calloc(1,sizeof(*v));assert(g&&v);
    v->gb=g;v->view.wram=g->wram;v->silver.rom=rom;v->silver.rom_size=1048576;
    assert(pgb_silver_jp_init(&v->silver,rom,1048576));v->silver_enabled=v->background_enabled=1;
    /* JP Rev1 ROM bytes independently identified from roofs.pal. */
    assert(pgb_silver_rom_word(&v->silver,0xb934+24*8)==0x3bf4);
    assert(pgb_silver_rom_word(&v->silver,0xb934+26*8)==0x71df);
    /* Synthetic geometry over a private ROM copy, no original file writes. */
    memset(rom+0x2000,1,2048);rom[0x1000]=rom[0x1001]=rom[0x1002]=rom[0x1003]=1;
    g->cgb.cgbMode=1;g->hram_io[0x47]=0xe4;v->compose.width=320;v->compose.height=240;
    v->silver.frame_valid=1;v->silver.features=PGB_SILVER_STABLE_BORDER;v->silver.atlas_count=2;
    v->silver.frame_map.definitions=0x2000;v->roof_tile[1]=1;
    v->silver.atlas[0]=(struct pgb_silver_connection){.x=0,.y=0,.width=2,.height=2,.blocks=0x1000,.border=1,.group=26,.number=3};
    v->silver.atlas[1]=(struct pgb_silver_connection){.x=2,.y=0,.width=2,.height=2,.blocks=0x1000,.border=1,.group=24,.number=3};
    for(unsigned night=0;night<2;night++)for(unsigned colour=1;colour<=2;colour++) {
        g->wram[0x155b]=night?2:0;
        unsigned pink=gbc_silver_roof_rgb555(v,26,colour),green=gbc_silver_roof_rgb555(v,24,colour);
        assert(pink!=green);
        for(unsigned pass=0;pass<4;pass++) {
            unsigned current=(pass&1)?24:26;g->wram[0x19c6]=(uint8_t)current;
            g->cgb.fixPalette[24+colour]=gbc_silver_roof_rgb555(v,current,colour);
            assert(gbc_view_colour(v,24+colour,0,0)==rgb(pink));
            assert(gbc_view_colour(v,24+colour,64,0)==rgb(green));
        }
        /* Map ID can switch first; old or half-uploaded hardware colour must
         * not flash into the new current town's roof, on either direction. */
        g->wram[0x19c6]=26;g->cgb.fixPalette[24+colour]=green;
        assert(gbc_view_colour(v,24+colour,0,0)==rgb(pink));
        g->cgb.fixPalette[24+colour]=0x1234;
        assert(gbc_view_colour(v,24+colour,0,0)==rgb(pink));
        g->wram[0x19c6]=24;g->hram_io[0x47]=0;g->cgb.fixPalette[24+colour]=0; /* fade */
        assert(gbc_view_colour(v,24+colour,0,0)==0xff000000u);
        g->hram_io[0x47]=0xe4;
    }
    assert(gbc_view_colour(v,32+25,0,0)==gbc_rgb(g,32+25)); /* OBJ palette unaffected */
    v->silver.frame_valid=0;assert(gbc_view_colour(v,25,0,0)==gbc_rgb(g,25));
    free(g);free(v);free(rom);puts("PASS spatial roof colours: Cherrygrove/New Bark owners, both colours, day/night, repeated switches, fades and OBJ exclusion");return 0;
}
