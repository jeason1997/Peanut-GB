/* Framebuffer packing must honor channel layout, stride, and pan offsets. */
#define main fb_frontend_main
#include "frontend_fb.c"
#undef main
#include <assert.h>
int main(void) {
    fb_flush=0;
    uint32_t source[4]={0xffff0000,0xff00ff00,0xff0000ff,0xffffffff};
    uint8_t memory[64],stage[16];
    output=source;output_width=output_height=2;fb_memory=memory;packed_frame=stage;
    fb_var.xoffset=1;fb_var.yoffset=1;fb_var.bits_per_pixel=16;
    fb_var.red=(struct fb_bitfield){11,5,0};fb_var.green=(struct fb_bitfield){5,6,0};fb_var.blue=(struct fb_bitfield){0,5,0};
    fb_fix.line_length=8;
    for(int fast_path=0;fast_path<2;fast_path++) {
        memset(memory,0xa5,sizeof(memory));rgb565=fast_path;fb_present();
        uint16_t values[4]={0xf800,0x07e0,0x001f,0xffff};
        for(unsigned i=0;i<64;i++) {
            unsigned char expected=0xa5;
            for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++)for(unsigned b=0;b<2;b++)if(i==(y+1)*8+(x+1)*2+b)expected=((uint8_t*)&values[y*2+x])[b];
            assert(memory[i]==expected);
        }
    }
    for(unsigned c=0;c<256;c++) {
        source[0]=0xff000000u|c*0x010101;
        rgb565=0;fb_present();uint16_t generic;memcpy(&generic,stage,2);
        rgb565=1;fb_present();uint16_t fast;memcpy(&fast,stage,2);assert(generic==fast);
    }
    fb_var.bits_per_pixel=32;fb_var.red=(struct fb_bitfield){16,8,0};fb_var.green=(struct fb_bitfield){8,8,0};fb_var.blue=(struct fb_bitfield){0,8,0};fb_var.transp=(struct fb_bitfield){24,8,0};fb_fix.line_length=12;rgb565=0;
    memset(memory,0xa5,sizeof(memory));fb_present();
    for(unsigned y=0;y<2;y++)assert(!memcmp(memory+(y+1)*12+4,source+y*2,8));
    puts("PASS framebuffer RGB565 fast/generic, ARGB8888, stride, offsets and guards");
    return 0;
}
