/* Generic DMG background from existing VRAM. Pixels may be stale outside
 * hardware view: this cannot discover a game's complete world map. */
#ifndef GX_GB_BACKGROUND_H
#define GX_GB_BACKGROUND_H
#include "gx_gb_view.h"
static void gx_gb_background(const struct gx_gb_view *g, uint8_t *out,
 uint16_t width, int32_t x, int32_t y, int layer_tags)
{
 unsigned int lcdc=g->hram_io[0x40];
 uint8_t by=(uint8_t)(y+g->hram_io[0x42]);
 for(unsigned int i=0;i<width;i++) {
  unsigned int colour=0;
  if(lcdc&1) {
   uint8_t bx=(uint8_t)(x+(int32_t)i+g->hram_io[0x43]);
   unsigned int tile=g->vram[((lcdc&8)?0x1c00:0x1800)+(by>>3)*32+(bx>>3)];
   unsigned int at=(lcdc&16)?tile*16:0x800+((tile+128)&255)*16;
   at+=(by&7)*2;
   unsigned int shift=7-(bx&7);
   colour=((g->vram[at]>>shift)&1)|(((g->vram[at+1]>>shift)&1)<<1);
  }
  out[i]=g->display.bg_palette[colour]|(layer_tags?GX_GB_PALETTE_BG:0);
 }
}
#endif
