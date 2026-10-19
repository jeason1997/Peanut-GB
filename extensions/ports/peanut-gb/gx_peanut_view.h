/* Include after peanut_gb.h and gx_gb_view.h. Borrow memory, copy tiny
 * registers/palettes; never copy the GB's RAM or cast the emulator struct. */
#ifndef GX_PEANUT_VIEW_H
#define GX_PEANUT_VIEW_H
#include "../../include/gx_gb_view.h"
static void gx_peanut_view(struct gx_gb_view *v, const struct gb_s *g)
{
 v->wram=g->wram;v->vram=g->vram;v->oam=g->oam;v->hram_io=g->hram_io;
 v->cpu_reg.bc.reg=g->cpu_reg.bc.reg;v->cpu_reg.pc.reg=g->cpu_reg.pc.reg;
 for(unsigned int i=0;i<4;i++)v->display.bg_palette[i]=g->display.bg_palette[i];
 for(unsigned int i=0;i<8;i++)v->display.sp_palette[i]=g->display.sp_palette[i];
}
#endif
