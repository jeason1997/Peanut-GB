#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
int main(void)
{
    struct gb_s *g=calloc(1,sizeof(*g));struct gbc_viewport *v=calloc(1,sizeof(*v));
    uint8_t *image=calloc(1,65536);assert(g&&v&&image);
    v->gb=g;v->view.wram=g->wram;v->view.vram=g->vram;v->view.hram_io=g->hram_io;
    v->silver_enabled=v->background_enabled=1;g->cgb.cgbMode=1;g->hram_io[0x40]=0x93;
    v->compose.width=320;v->compose.height=240;
    v->silver.rom=image;v->silver.rom_size=65536;v->silver.features=PGB_SILVER_SPRITES;
    v->silver.sprites_enabled=1;v->silver.frame_valid=v->silver.frame_ready=1;
    v->silver.frame_map.width=v->silver.frame_map.height=10;
    v->silver.object_count=1;
    image[0x404a]=0;image[0x404b]=0x41;image[0x4100]=1; /* one 8x8 facing */
    for(unsigned i=0;i<8;i++)image[0x5000+i*2]=255; /* opaque colour1 */
    g->wram[0x11f0]=1;g->wram[0x11f1]=1;g->wram[0x11f6]=2;
    struct pgb_silver_object *o=&v->silver.objects[0];
    o->map_index=1;o->live=1;o->graphics=0x5000;
    int cases[][4]={{-4,40,44,4},{156,40,44,4},{40,-4,0,8},{40,140,143,8},{40,40,44,0},{-8,40,44,0}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        uint8_t line[160]={0};o->x=cases[i][0];o->y=cases[i][1];
        gbc_view_foreground(v,line,160,(uint16_t)cases[i][2]);
        unsigned count=0;for(unsigned x=0;x<160;x++)if(line[x]){assert(line[x]==41);count++;}
        assert(count==(unsigned)cases[i][3]);
    }
    uint8_t line[160]={0};o->x=156;o->y=40;line[158]=63;
    gbc_view_foreground(v,line,160,44);assert(line[156]==41&&line[158]==63);
    memset(line,0,160);o->oam_pose=1;gbc_view_foreground(v,line,160,44);
    for(unsigned x=0;x<160;x++)assert(!line[x]); /* no second copy of native OBJ */
    o->oam_pose=0;v->silver.frame_valid=0;gbc_view_foreground(v,line,160,44);
    for(unsigned x=0;x<160;x++)assert(!line[x]);
    v->silver.frame_valid=1;v->silver.object_count=2;
    v->silver.objects[1]=*o;v->silver.objects[1].map_index=2;v->silver.objects[1].order=16;
    g->wram[0x1218]=1;g->wram[0x1219]=2;g->wram[0x121e]=3;
    memset(line,0,160);gbc_view_foreground(v,line,160,44);assert(line[157]==41);
    /* Higher-priority extra OBJ behind nonzero BG restores that original BG,
     * rather than revealing the lower-priority extra OBJ through it. */
    o->flags=0x80;for(unsigned i=0;i<8;i++)g->vram[i*2]=255;
    memset(line,1,160);line[158]=63;gbc_view_foreground(v,line,160,44);
    assert(line[157]==1&&line[158]==63);
    /* The game can reuse live slots during visible scanlines. A frame's pose
     * must keep its own palette even after the slot or current map changes. */
    v->silver.object_count=1;o->flags=0;o->map_group=26;o->map_number=3;
    g->wram[0x19c6]=26;g->wram[0x19c7]=3;g->wram[0x11f6]=2;
    gbc_silver_capture_palettes(v);assert(gbc_silver_frame_palette(v,0)==2);
    g->wram[0x11f6]=5;assert(gbc_silver_frame_palette(v,0)==2);
    memset(line,0,160);gbc_view_foreground(v,line,160,44);assert(line[157]==41);
    gbc_silver_capture_palettes(v);assert(gbc_silver_frame_palette(v,0)==5);
    g->wram[0x19c6]=24;o->oam_pose=1;o->order=8;g->oam[8*4+3]=7;
    gbc_silver_capture_palettes(v);assert(gbc_silver_frame_palette(v,0)==5); /* retained old-map pose */
    o->map_group=24;gbc_silver_capture_palettes(v);assert(gbc_silver_frame_palette(v,0)==7); /* new native OAM */
    /* Before a real slot exists only stationary declarations can be shown.
     * Walkers/spinners/scripts must not leave an old guessed pose behind. */
    for(unsigned movement=0;movement<37;movement++) {
        v->silver.object_count=2;v->silver.objects[1]=*o;
        o->live=0;o->map_index=1;v->silver.objects[1].live=1;
        g->wram[0x1438+16+4]=(uint8_t)movement;
        gbc_silver_remove_predictions(v);
        assert(v->silver.object_count==(unsigned)(gbc_silver_stationary(movement)?2:1));
        assert(v->silver.objects[v->silver.object_count-1].live);
        *o=v->silver.objects[v->silver.object_count-1];
    }
    free(image);free(g);free(v);
    puts("PASS sprite seams: four edges, native OBJ protection/priority, immutable per-frame palette, slot reuse and retained-map handoff");return 0;
}
