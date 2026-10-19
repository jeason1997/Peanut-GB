/* Portable presentation-only viewport compositor. C99, no heap, no SDL.
 * Coordinates passed to background/foreground are relative to native top-left.
 * The emulator retains ownership of its hardware frame and timing. */
#ifndef GX_VIEWPORT_H
#define GX_VIEWPORT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum gx_features { GX_BACKGROUND = 1, GX_FOREGROUND = 2 };
struct gx_callbacks {
 void (*begin)(void *user);
 void (*background)(void *user, uint8_t *pixels, uint16_t width, int32_t x, int32_t y);
 void (*foreground)(void *user, uint8_t *native_copy, uint16_t width, uint16_t y);
 void (*emit)(void *user, const uint8_t *pixels, uint16_t width, uint16_t y);
};
struct gx_viewport {
 uint16_t native_width, native_height, width, height, left, top, next_y;
 unsigned int features;
 uint8_t blank, frame_open;
 uint8_t *row;
 void *user;
 struct gx_callbacks callbacks;
};
static int gx_viewport_init(struct gx_viewport *v, uint16_t nw, uint16_t nh,
 uint16_t width, uint16_t height, uint8_t *row, size_t row_bytes,
 unsigned int features, uint8_t blank, void *user, struct gx_callbacks callbacks)
{
 if(!v || !nw || !nh || width < nw || height < nh || !row || row_bytes < width ||
    !callbacks.emit || (features & ~(GX_BACKGROUND | GX_FOREGROUND))) return 0;
 memset(v, 0, sizeof(*v));
 v->native_width=nw; v->native_height=nh; v->width=width; v->height=height;
 v->left=(width-nw)/2; v->top=(height-nh)/2; v->row=row;
 v->features=features; v->blank=blank; v->user=user; v->callbacks=callbacks;
 return 1;
}
static void gx_viewport_begin(struct gx_viewport *v)
{
 v->next_y=0; v->frame_open=1;
 if(v->callbacks.begin) v->callbacks.begin(v->user);
}
static void gx_viewport_span(struct gx_viewport *v, uint16_t offset,
 uint16_t width, int32_t y)
{
 if(!width) return;
 memset(v->row+offset,v->blank,width);
 if((v->features & GX_BACKGROUND) && v->callbacks.background)
  v->callbacks.background(v->user,v->row+offset,width,(int32_t)offset-v->left,y);
}
/* Feed ordered native rows. Returns 0 on a missing/duplicate/out-of-order row.
 * Interlace/frame-skip ports must provide completed rows or use a whole-frame
 * frontend; do not silently treat partial PPU output as a complete frame. */
static int gx_viewport_push(struct gx_viewport *v, uint16_t y,
 const uint8_t *native, size_t native_bytes)
{
 if(!v->frame_open || y!=v->next_y || y>=v->native_height ||
    !native || native_bytes<v->native_width) return 0;
 if(!y) for(uint16_t row=0;row<v->top;row++) {
  gx_viewport_span(v,0,v->width,(int32_t)row-v->top);
  v->callbacks.emit(v->user,v->row,v->width,row);
 }
 gx_viewport_span(v,0,v->left,y);
 uint16_t right=v->left+v->native_width;
 gx_viewport_span(v,right,v->width-right,y);
 memcpy(v->row+v->left,native,v->native_width);
 if((v->features & GX_FOREGROUND) && v->callbacks.foreground)
  v->callbacks.foreground(v->user,v->row+v->left,v->native_width,y);
 v->callbacks.emit(v->user,v->row,v->width,v->top+y);
 v->next_y++;
 if(v->next_y==v->native_height) {
  for(uint32_t row=(uint32_t)v->top+v->native_height;row<v->height;row++) {
   gx_viewport_span(v,0,v->width,(int32_t)row-v->top);
   v->callbacks.emit(v->user,v->row,v->width,(uint16_t)row);
  }
  v->frame_open=0;
 }
 return 1;
}
#endif
