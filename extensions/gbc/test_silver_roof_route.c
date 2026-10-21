/* Real inputs: Cherrygrove -> Route29 -> Cherrygrove. No state injection. */
#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
static uint8_t indices[320*240];
static void capture(void *user,const uint8_t *p,uint16_t n,uint16_t y)
{
    assert(n==320&&y<240);memcpy(indices+y*320,p,n);gbc_view_emit(user,p,n,y);
}
static uint32_t reference_pink(unsigned colour,unsigned night)
{
    unsigned a=0xb934+26*8+night*4+(colour-1)*2;
    unsigned c=rom[a]|rom[a+1]<<8;
    return 0xff000000u|((c&31)*255/31<<16)|(((c>>5)&31)*255/31<<8)|(((c>>10)&31)*255/31);
}
int main(int argc,char **argv)
{
    assert(argc==3);rom_size=1048576;rom=malloc(rom_size);ram_size=32768;ram=malloc(ram_size);assert(rom&&ram);
    FILE*f=fopen(argv[1],"rb");assert(f);assert(fread(rom,1,rom_size,f)==rom_size);fclose(f);
    f=fopen(argv[2],"rb");assert(f);assert(fread(ram,1,ram_size,f)==ram_size);fclose(f);
    struct gb_s*g=calloc(1,sizeof(*g));assert(g);assert(!gb_init(g,read_rom,read_ram,write_ram,core_error,NULL));
    output_width=320;output_height=240;output=calloc(320*240,4);viewport_row=malloc(320);assert(output&&viewport_row);
    assert(gbc_view_init(&viewport,g,rom,rom_size,320,240,output,viewport_row,1));assert(viewport.silver_enabled);
    viewport.compose.callbacks.emit=capture;gb_init_lcd(g,draw);
    unsigned enter=0,returned=0;unsigned long samples=0,colours[2]={0},return_lag_pixels=0;
    for(unsigned frame=0;frame<4000;frame++) {
        unsigned key=0;
        if(frame>=120&&frame<460&&frame%60<4)key=frame<180?8:1;
        if(frame>=1800&&frame<2010)key=16;
        if(frame>=2010&&frame<2080)key=64;
        if(frame>=2080&&frame<2440)key=16;
        if(frame>=2440&&frame<2473)key=128;
        if(frame>=2473&&frame<2750)key=16;
        if(frame>=2800&&frame<3150)key=32;
        g->direct.joypad=(uint8_t)~key;gb_run_frame(g);
        if(frame==1000&&(g->wram[0x19c6]!=26||g->wram[0x19c7]!=3||g->wram[0x19c9]!=10||g->wram[0x19c8]!=9)) {
            fprintf(stderr,"SKIP scripted roof route: requires Cherrygrove group26/map3 at x10,y9 after Continue; use the recorded test save copy\n");
            free(g);free(rom);free(ram);free(output);free(viewport_row);return 77;
        }
        if(g->wram[0x19c6]==24)enter=1;
        if(enter&&g->wram[0x19c6]==26)returned=1;
        if((g->wram[0x19c6]==24||g->wram[0x19c6]==26)&&viewport.silver.frame_valid&&
            g->hram_io[0x47]==0xe4) {
            /* Independently restrict to the actual old city's atlas rectangle,
             * not the current group or the colour-selection helper. */
            for(unsigned i=0;i<viewport.silver.atlas_count;i++) {
                const struct pgb_silver_connection*c=&viewport.silver.atlas[i];
                if(c->group!=26||c->number!=3)continue;
                for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++) {
                    unsigned p=indices[y*320+x];if(p!=25&&p!=26)continue;
                    int wx=viewport.silver.frame_map.origin_x+(int)x-80;
                    int wy=viewport.silver.frame_map.origin_y+(int)y-48;
                    if(wx<c->x*32||wx>=(c->x+c->width)*32||wy<c->y*32||wy>=(c->y+c->height)*32)continue;
                    if(g->wram[0x19c6]==26&&gbc_rgb(g,(uint8_t)p)!=reference_pink(p-24,(g->wram[0x155b]&3)>=2))return_lag_pixels++;
                    if(output[y*320+x]!=reference_pink(p-24,(g->wram[0x155b]&3)>=2)) {
                        uint8_t tile=pgb_silver_world_tile(&viewport.silver,&viewport.view,&viewport.silver.frame_map,wx,wy);
                        fprintf(stderr,"frame=%u xy=%u,%u p=%u tile=%02x roof=%u owner=%u got=%08x want=%08x hw=%04x expected-current=%04x\n",frame,x,y,p,tile,viewport.roof_tile[tile],gbc_silver_owner_group(&viewport,wx,wy),output[y*320+x],reference_pink(p-24,(g->wram[0x155b]&3)>=2),g->cgb.fixPalette[p],gbc_silver_roof_rgb555(&viewport,24,p-24));
                        abort();
                    }
                    samples++;colours[p-25]++;
                }
            }
        }
        if(frame==2600)snapshot("build-extensions/gbc-silver-viewport/roof-before.ppm");
        if(frame==2648)snapshot("build-extensions/gbc-silver-viewport/roof-after.ppm");
        if(frame==3100)snapshot("build-extensions/gbc-silver-viewport/roof-return.ppm");
    }
    assert(enter&&returned&&samples>100&&colours[0]&&colours[1]&&return_lag_pixels);
    printf("PASS real roof roundtrip: %lu stable city pixels, including %lu delayed-palette pixels on return (%lu/%lu colours)\n",samples,return_lag_pixels,colours[0],colours[1]);
    free(g);free(rom);free(ram);free(output);free(viewport_row);return 0;
}
