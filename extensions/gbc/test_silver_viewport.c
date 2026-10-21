/* Real ROM: compare expanded core against an unextended reference every frame. */
#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
struct test_host {struct gb_s gb;uint8_t save[32768];};
static uint8_t raw_rom(struct gb_s *g,uint_fast32_t a){(void)g;return a<rom_size?rom[a]:0xff;}
static uint8_t test_read(struct gb_s *g,uint_fast32_t a){assert(a<32768);return ((struct test_host*)g->direct.priv)->save[a];}
static void test_write(struct gb_s *g,uint_fast32_t a,uint8_t b){assert(a<32768);((struct test_host*)g->direct.priv)->save[a]=b;}
static unsigned long native_rows;
static unsigned long corrected_roof_pixels;
static unsigned long seam_pixels;
static void checked_line(struct gb_s *g,const uint8_t *p,uint_fast8_t y)
{
    draw(g,p,y);
    if(y<144&&viewport.compose.next_y==y+1) {
        uint8_t expected[160];memcpy(expected,p,160);gbc_view_foreground(&viewport,expected,160,(uint16_t)y);
        for(unsigned x=0;x<160;x++) {
            unsigned ox=viewport.compose.left+x,oy=viewport.compose.top+y;
            uint32_t c=output[(size_t)oy*output_width+ox];
            assert(c==gbc_view_colour(&viewport,expected[x],ox,oy));
            if(expected[x]!=p[x]){assert(p[x]<32&&expected[x]>=32&&expected[x]<64);seam_pixels++;}
            else if(c!=gbc_rgb(g,p[x])){assert(p[x]==25||p[x]==26);corrected_roof_pixels++;}
            if(p[x]>=32)assert(c==gbc_rgb(g,p[x])); /* original OBJ must survive */
        }
        native_rows++;
    }
}
int main(int argc,char **argv)
{
    assert(argc==3);
    FILE*f=fopen(argv[1],"rb");assert(f);rom_size=1048576;rom=malloc(rom_size);assert(rom);
    assert(fread(rom,1,rom_size,f)==rom_size&&fgetc(f)==EOF);fclose(f);
    struct test_host *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));assert(a&&b);
    f=fopen(argv[2],"rb");assert(f);assert(fread(a->save,1,32768,f)==32768);fclose(f);memcpy(b->save,a->save,32768);
    /* This test isolates display-only work. The production NPC ROM-read
     * policy intentionally changes game state and is tested separately. */
    assert(!gb_init(&a->gb,raw_rom,test_read,test_write,core_error,a));
    assert(!gb_init(&b->gb,raw_rom,test_read,test_write,core_error,b));
    assert(a->gb.cgb.cgbMode);
    output_width=320;output_height=240;output=calloc(320*240,4);viewport_row=malloc(320);assert(output&&viewport_row);
    assert(gbc_view_init(&viewport,&a->gb,rom,rom_size,320,240,output,viewport_row,1));assert(viewport.silver_enabled);
    gb_init_lcd(&a->gb,checked_line);gb_init_lcd(&b->gb,NULL);
    /* Reference still renders internally, but without a host callback. */
    for(unsigned i=0;i<8000;i++) {
        unsigned keys=0;
        if(i>=120&&i<460&&i%60<4)keys=i<180?8:1;
        if(i>=2000&&i<2300)keys=64;
        if(i>=2600&&i<2800)keys=32;
        if(i==3200||i==3800)keys=8;
        if(i==3500||i==4000||i==4500)keys=2;
        a->gb.direct.joypad=b->gb.direct.joypad=(uint8_t)~keys;
        gb_run_frame(&a->gb);gb_run_frame(&b->gb);
        assert(!memcmp(&a->gb.cpu_reg,&b->gb.cpu_reg,sizeof(a->gb.cpu_reg)));
        assert(!memcmp(a->gb.wram,b->gb.wram,sizeof(a->gb.wram)));
        assert(!memcmp(a->gb.vram,b->gb.vram,sizeof(a->gb.vram)));
        assert(!memcmp(a->gb.oam,b->gb.oam,sizeof(a->gb.oam)));
        assert(!memcmp(a->gb.hram_io,b->gb.hram_io,sizeof(a->gb.hram_io)));
        assert(!memcmp(&a->gb.cgb,&b->gb.cgb,sizeof(a->gb.cgb)));
        assert(!memcmp(a->save,b->save,32768));
    }
    assert(viewport.valid_frames>1000&&native_rows>144*1000);
    assert(viewport.margin_sprite_pixels>0);
    printf("PASS Silver CGB 8000 frames: core/CPU/VRAM/WRAM/OAM/save identical, %lu native rows checked (%lu roof/%lu seam corrections, original OBJ protected), %lu valid extension frames, %lu margin sprite pixels\n",native_rows,corrected_roof_pixels,seam_pixels,viewport.valid_frames,viewport.margin_sprite_pixels);
    free(a);free(b);free(rom);free(output);free(viewport_row);return 0;
}
