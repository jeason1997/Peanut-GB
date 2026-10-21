/* Real connected-map loads: smoother display cadence, identical execution
 * to a reference consuming every original CPU/PPU frame with the same input. */
#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
static struct gbc_viewport reference;
static uint8_t reference_save[32768];
static uint8_t reference_rom(struct gb_s*g,uint_fast32_t a)
{
    if(a>=rom_size)return 255;
    return reference.gb==g&&reference.silver_enabled?gbc_view_rom_read(&reference,g,a):rom[a];
}
static uint8_t reference_read(struct gb_s*g,uint_fast32_t a){(void)g;assert(a<32768);return reference_save[a];}
static void reference_write(struct gb_s*g,uint_fast32_t a,uint8_t b){(void)g;assert(a<32768);reference_save[a]=b;}
static void reference_line(struct gb_s*g,const uint8_t*p,uint_fast8_t y){(void)g;gbc_view_line(&reference,p,y);}
static unsigned clock_calls;
static double expired_budget(void){return clock_calls++?9.0:0.0;}
int main(int argc,char**argv)
{
    assert(argc==3);rom_size=1048576;ram_size=32768;rom=malloc(rom_size);ram=malloc(ram_size);assert(rom&&ram);
    FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,rom_size,f)==rom_size);fclose(f);
    f=fopen(argv[2],"rb");assert(f&&fread(ram,1,ram_size,f)==ram_size);fclose(f);memcpy(reference_save,ram,32768);
    struct gb_s *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));assert(a&&b);
    assert(!gb_init(a,read_rom,read_ram,write_ram,core_error,NULL));
    assert(!gb_init(b,reference_rom,reference_read,reference_write,core_error,NULL));
    output_width=320;output_height=240;output=calloc(320*240,4);viewport_row=malloc(320);
    uint32_t*other=calloc(320*240,4);uint8_t*row=malloc(320);assert(output&&viewport_row&&other&&row);
    assert(gbc_view_init(&viewport,a,rom,rom_size,320,240,output,viewport_row,1));
    assert(gbc_view_init(&reference,b,rom,rom_size,320,240,other,row,1));
    gb_init_lcd(a,draw);gb_init_lcd(b,reference_line);
    struct gbc_frame_runner runner={0};
    unsigned emulated=0,last_native=0,last_display=0,max_native=0,max_display=0;
    unsigned previous_native=0,previous_display=0;
    for(unsigned frame=0;frame<3200;frame++) {
        unsigned keys=0;
        if(frame>=120&&frame<460&&frame%60<4)keys=frame<180?8:1;
        if(frame>=1800&&frame<2010)keys=16;
        if(frame>=2010&&frame<2080)keys=64;
        if(frame>=2080&&frame<2440)keys=16;
        if(frame>=2440&&frame<2473)keys=128;
        if(frame>=2473&&frame<2750)keys=16;
        if(frame>=2800&&frame<3150)keys=32;
        a->direct.joypad=b->direct.joypad=(uint8_t)~keys;
        struct gbc_frame_runner limited=runner;
        unsigned extra=gbc_run_display_frame(&runner,&viewport,a,1,NULL);assert(extra<=6);
        int window=(frame>=2600&&frame<2660)||(frame>=2920&&frame<2960);
        for(unsigned i=0;i<=extra;i++) {
            if(!i&&limited.pending) {
                clock_calls=0;
                assert(!gbc_run_display_frame(&limited,&reference,b,1,expired_budget));
                assert(!limited.pending); /* exhausted host budget never adds frames */
            } else gb_run_frame(b);
            emulated++;
            if(b->hram_io[0x43]!=previous_native) {
                if(window&&last_native&&emulated-last_native>max_native)max_native=emulated-last_native;
                last_native=emulated;previous_native=b->hram_io[0x43];
            }
        }
        if(a->hram_io[0x43]!=previous_display) {
            if(window&&last_display&&frame-last_display>max_display)max_display=frame-last_display;
            last_display=frame;previous_display=a->hram_io[0x43];
        }
        /* RTC remains tied to presented real-time frames, not hidden loads. */
        if((uint64_t)(frame+1)*70224/4194304!=(uint64_t)frame*70224/4194304){gb_tick_rtc(a);gb_tick_rtc(b);}
        assert(!memcmp(&a->cpu_reg,&b->cpu_reg,sizeof(a->cpu_reg)));
        assert(!memcmp(a->wram,b->wram,sizeof(a->wram)));
        assert(!memcmp(a->vram,b->vram,sizeof(a->vram)));
        assert(!memcmp(a->oam,b->oam,sizeof(a->oam)));
        assert(!memcmp(a->hram_io,b->hram_io,sizeof(a->hram_io)));
        assert(!memcmp(&a->cgb,&b->cgb,sizeof(a->cgb)));
        assert(!memcmp(ram,reference_save,32768));
        assert(!memcmp(output,other,320*240*4));
        if(frame==1000&&(a->wram[0x19c6]!=26||a->wram[0x19c7]!=3||a->wram[0x19c9]!=10||a->wram[0x19c8]!=9)) {
            fprintf(stderr,"SKIP catchup: requires Cherrygrove group26/map3 x10,y9 test save\n");
            free(a);free(b);free(rom);free(ram);free(output);free(viewport_row);free(other);free(row);return 77;
        }
    }
    assert(runner.connections==2&&runner.extra_frames==6);
    assert(max_native==5&&max_display==2);
    /* A stale pending connection cannot accelerate an unrelated menu. */
    runner.pending=1;runner.group=a->wram[0x19c6];runner.number=a->wram[0x19c7];a->wram[0x1108]=1;
    assert(!gbc_run_display_frame(&runner,&viewport,a,1,NULL)&&!runner.pending);
    puts("PASS connection catchup: 2 loads, 6 hidden original frames; camera interval 5 -> 2 display frames; CPU/WRAM/VRAM/OAM/palettes/save/output identical to full execution; RTC real-time");
    free(a);free(b);free(rom);free(ram);free(output);free(viewport_row);free(other);free(row);return 0;
}
