#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
int main(void) {
    unsigned w,h;
    assert(parse_viewport("320x240",&w,&h)&&w==320&&h==240);
    assert(parse_viewport("161x145",&w,&h)&&w==161&&h==145);
    const char *invalid[]={"159x144","160x143","-160x144","320x240x","999999999999999999999x240","65536x144","65535x65535","320x-240"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)assert(!parse_viewport(invalid[i],&w,&h));
    struct gb_s *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*g));assert(g&&before);
    g->cgb.cgbMode=1;g->hram_io[0x40]=0x91;
    /* Tile 0 in bank 1, palette 3, XY flip. Unique bottom-right pixel. */
    g->vram[0x1800]=0;g->vram[0x3800]=0x6b;g->vram[0x2000+14]=1;
    uint8_t row[8];gx_cgb_background(g->vram,g->hram_io,row,8,0,0);
    assert(row[0]==13&&row[1]==12);
    g->vram[0x3800]=3;g->vram[0]=0x80;
    gx_cgb_background(g->vram,g->hram_io,row,8,0,0);assert(row[0]==13&&row[1]==12);
    /* LCDC bit0 does not disable CGB background. */
    g->hram_io[0x40]&=~1u;gx_cgb_background(g->vram,g->hram_io,row,8,0,0);assert(row[0]==13);
    unsigned dims[][2]={{160,144},{161,145},{200,150},{320,240},{321,241},{640,480}};
    for(unsigned i=0;i<sizeof(dims)/sizeof(dims[0]);i++) {
        w=dims[i][0];h=dims[i][1];
        uint32_t *fb=malloc(((size_t)w*h+2)*sizeof(*fb));uint8_t *line=malloc(w+2);
        assert(fb&&line);fb[0]=fb[(size_t)w*h+1]=0xdeadbeef;line[0]=line[w+1]=0xa5;
        struct gbc_viewport *v=calloc(1,sizeof(*v));assert(v);
        assert(gbc_view_init(v,g,NULL,0,w,h,fb+1,line+1,1));
        memcpy(before,g,sizeof(*g));
        uint8_t native[160];for(unsigned x=0;x<160;x++)native[x]=x%64;
        for(unsigned j=0;j<64;j++)g->cgb.fixPalette[j]=(uint16_t)(j*513);
        memcpy(before,g,sizeof(*g));
        for(unsigned y=0;y<144;y++)gbc_view_line(v,native,y);
        assert(!memcmp(before,g,sizeof(*g)));assert(v->emitted_rows==h);
        for(unsigned y=0;y<144;y++)for(unsigned x=0;x<160;x++)
            assert(fb[1+(size_t)(y+(h-144)/2)*w+x+(w-160)/2]==gbc_rgb(g,native[x]));
        assert(fb[0]==0xdeadbeef&&fb[(size_t)w*h+1]==0xdeadbeef&&line[0]==0xa5&&line[w+1]==0xa5);
        free(v);free(fb);free(line);
    }
    free(g);free(before);puts("PASS CGB viewport: arbitrary/odd sizes, bank/XY/palette, native equality, immutable core, buffer guards");return 0;
}
