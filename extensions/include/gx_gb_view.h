/* Host-neutral, borrowed read-only view of a DMG emulator at a scanline.
 * Host must expose these exact decoded layouts; never cast a foreign context.
 * Palette values are 0..3, optional layer tags use bits 4..5.
 * No RAM copying, ownership transfer or emulator execution is required. */
#ifndef GX_GB_VIEW_H
#define GX_GB_VIEW_H
#include <stdint.h>
#define GX_GB_HEIGHT 144
#define GX_GB_VRAM_SIZE 0x2000
#define GX_GB_PALETTE_BG 0x20
#define GX_GB_PALETTE_ALL 0x30
struct gx_gb_view {
 const uint8_t *wram; /* 0x2000 bytes: CPU C000..DFFF (DMG only) */
 const uint8_t *vram; /* 0x2000 bytes: CPU 8000..9FFF */
 const uint8_t *oam; /* 160 bytes: CPU FE00..FE9F */
 const uint8_t *hram_io; /* 256 bytes: CPU FF00..FFFF, not just HRAM */
 struct { struct { uint16_t reg; } bc, pc; } cpu_reg;
 struct { uint8_t bg_palette[4], sp_palette[8]; } display;
};
#endif
