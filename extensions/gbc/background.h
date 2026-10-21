/* CGB presentation-only background sampling. VRAM contains both 8KiB banks.
 * Outside the native view, the hardware tilemap can contain stale/repeated data. */
#ifndef GX_CGB_BACKGROUND_H
#define GX_CGB_BACKGROUND_H
#include <stdint.h>
static uint8_t gx_cgb_tile_pixel(const uint8_t *vram, unsigned lcdc,
    uint8_t tile, uint8_t attr, unsigned px, unsigned py)
{
    if(attr&0x20)px=7-px;
    if(attr&0x40)py=7-py;
    unsigned at=(lcdc&0x10)?tile*16:0x800+((tile+128)&255)*16;
    at+=((attr&8)?0x2000:0)+py*2;
    unsigned bit=7-px;
    return (uint8_t)((attr&7)*4+((vram[at]>>bit)&1)+(((vram[at+1]>>bit)&1)<<1));
}
static void gx_cgb_background(const uint8_t *vram,const uint8_t *io,
    uint8_t *out,uint16_t width,int32_t x,int32_t y)
{
    unsigned by=(uint8_t)(y+io[0x42]);
    for(unsigned i=0;i<width;i++) {
        unsigned bx=(uint8_t)(x+(int32_t)i+io[0x43]);
        unsigned cell=((io[0x40]&8)?0x1c00:0x1800)+(by>>3)*32+(bx>>3);
        out[i]=gx_cgb_tile_pixel(vram,io[0x40],vram[cell],vram[0x2000+cell],bx&7,by&7);
    }
}
#endif
