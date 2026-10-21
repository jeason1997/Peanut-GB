/* Actual JP Rev1 moving-NPC handoff: replay complete live actors across the
 * border as an oracle, including pixels where the core's OAM has dropped out. */
#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
static struct gbc_viewport oracle;
static unsigned frame;
static unsigned long samples,raw_gaps,corrected_mismatches;
static unsigned long slot_changes,retained_palettes;
static unsigned long outside_live,outside_motion,handoffs;
static struct pgb_silver_object previous[PGB_SILVER_MAX_OBJECTS];
static unsigned previous_count;
static int previous_x,previous_y;
static int raw_start[PGB_SILVER_MAX_OBJECTS],frozen[PGB_SILVER_MAX_OBJECTS];
static void check_line(struct gb_s *g,const uint8_t *p,uint_fast8_t y)
{
    draw(g,p,y);
    if(!y) {
        for(unsigned i=0;i<viewport.silver.object_count;i++) {
            const struct pgb_silver_object *o=&viewport.silver.objects[i];
            int outside=o->x<0||o->x>=160||o->y<0||o->y>=144;
            if(o->live&&o->map_index&&outside)outside_live++;
            /* A speculative moving object must never be replaced at another
             * position. Current-map moving actors already have real slots. */
            if(!o->live&&o->map_group==g->wram[0x19c6]&&o->map_number==g->wram[0x19c7])
                assert(gbc_silver_stationary(g->wram[0x1438+o->map_index*16+4]));
            for(unsigned j=0;j<previous_count;j++) {
                const struct pgb_silver_object *p=&previous[j];
                if(p->map_group!=o->map_group||p->map_number!=o->map_number||p->map_index!=o->map_index)continue;
                if(o->live&&p->live&&o->map_index&&viewport.silver.frame_valid&&
                    o->map_group==g->wram[0x19c6]&&o->map_number==g->wram[0x19c7]) {
                    int dx=o->x+viewport.silver.frame_map.origin_x-p->x-previous_x;
                    int dy=o->y+viewport.silver.frame_map.origin_y-p->y-previous_y;
                    assert(abs(dx)<=4&&abs(dy)<=4);
                    if(outside&&(dx||dy))outside_motion++;
                    if(o->oam_pose!=p->oam_pose)handoffs++;
                }
                break;
            }
            raw_start[i]=gbc_silver_object_palette(&viewport,&viewport.silver.objects[i]);
            frozen[i]=gbc_silver_frame_palette(&viewport,i);
        }
        previous_count=viewport.silver.frame_valid?viewport.silver.object_count:0;
        memcpy(previous,viewport.silver.objects,previous_count*sizeof(*previous));
        previous_x=viewport.silver.frame_map.origin_x;previous_y=viewport.silver.frame_map.origin_y;
        oracle=viewport;oracle.silver.object_count=0;
        for(unsigned i=0;i<viewport.silver.object_count;i++)if(viewport.silver.objects[i].live) {
            oracle.object_palettes[oracle.silver.object_count]=viewport.object_palettes[i];
            oracle.silver.objects[oracle.silver.object_count++]=viewport.silver.objects[i];
        }
    }
    if(!viewport.silver.frame_valid)return;
    for(unsigned i=0;i<viewport.silver.object_count;i++) {
        const struct pgb_silver_object *o=&viewport.silver.objects[i];
        int raw=gbc_silver_object_palette(&viewport,o);
        assert(gbc_silver_frame_palette(&viewport,i)==frozen[i]);
        if(raw!=raw_start[i])slot_changes++;
        if(raw<0&&frozen[i]>=0)retained_palettes++;
    }
    uint8_t row[160];gx_cgb_background(g->vram,g->hram_io,row,160,0,y);
    gbc_silver_sprites(&oracle,row,160,0,y); /* full actor; no native-edge clipping */
    for(unsigned x=0;x<160;x++)if((x<16||x>=144||y<16||y>=128)&&(p[x]>=32||row[x]>=32)) {
        samples++;raw_gaps+=p[x]!=row[x];
        uint32_t got=output[(y+48)*320+x+80];
        uint32_t expected=gbc_view_colour(&oracle,row[x],x+80,y+48);
        if(got!=expected) {
            if(corrected_mismatches<4)fprintf(stderr,"seam mismatch frame=%u xy=%u,%u core=%u full-actor=%u\n",frame,x,(unsigned)y,p[x],row[x]);
            corrected_mismatches++;
        }
        if(p[x]>=32)assert(got==gbc_rgb(g,p[x]));
    }
}
int main(int argc,char **argv)
{
    assert(argc==3);rom_size=1048576;rom=malloc(rom_size);ram_size=32768;ram=malloc(ram_size);assert(rom&&ram);
    FILE*f=fopen(argv[1],"rb");assert(f);assert(fread(rom,1,rom_size,f)==rom_size);fclose(f);
    f=fopen(argv[2],"rb");assert(f);assert(fread(ram,1,ram_size,f)==ram_size);fclose(f);
    struct gb_s*g=calloc(1,sizeof(*g));assert(g);assert(!gb_init(g,read_rom,read_ram,write_ram,core_error,NULL));
    output_width=320;output_height=240;output=calloc(320*240,4);viewport_row=malloc(320);assert(output&&viewport_row);
    assert(gbc_view_init(&viewport,g,rom,rom_size,320,240,output,viewport_row,1));assert(viewport.silver_enabled);
    gb_init_lcd(g,check_line);
    for(frame=0;frame<4000;frame++) {
        unsigned keys=0;
        if(frame>=120&&frame<460&&frame%60<4)keys=frame<180?8:1;
        if(frame>=1800&&frame<2010)keys=16;
        if(frame>=2010&&frame<2080)keys=64;
        if(frame>=2080&&frame<2440)keys=16;
        if(frame>=2440&&frame<2473)keys=128;
        if(frame>=2473&&frame<2750)keys=16;
        if(frame>=2800&&frame<3150)keys=32;
        g->direct.joypad=(uint8_t)~keys;gb_run_frame(g);
        if(frame==1000&&(g->wram[0x19c6]!=26||g->wram[0x19c7]!=3||g->wram[0x19c9]!=10||g->wram[0x19c8]!=9)) {
            fprintf(stderr,"SKIP moving seam route: requires Cherrygrove group26/map3 x10,y9 test save\n");
            free(g);free(rom);free(ram);free(output);free(viewport_row);return 77;
        }
    }
    assert(samples>1000&&!corrected_mismatches&&slot_changes&&retained_palettes);
    assert(outside_live>100&&outside_motion>10&&handoffs>0);
    printf("PASS Silver moving seams: %lu border samples, raw core/replay gaps=%lu, corrected mismatches=%lu; original OBJ protected\n",samples,raw_gaps,corrected_mismatches);
    printf("PASS actual temporal handoff: %lu scanline slot changes, %lu retained-palette samples; frame palettes immutable\n",slot_changes,retained_palettes);
    printf("PASS real NPC lifetime: %lu outside-live samples, %lu outside movement samples, %lu OAM handoffs; no position jumps (>4px/frame)\n",outside_live,outside_motion,handoffs);
    free(g);free(rom);free(ram);free(output);free(viewport_row);return 0;
}
