/* Exact Silver JP Rev1: roofs keep the palette of their spatial owner.
 * RoofPals=$B934 (ROM bank 2), wTimeOfDayPal=$D55B. No palette/RAM writes. */
#ifndef GX_CGB_SILVER_ROOFS_H
#define GX_CGB_SILVER_ROOFS_H
static unsigned gbc_silver_owner_group(const struct gbc_viewport *v,int x,int y)
{
    const struct pgb_silver_jp *s=&v->silver;
    int bx=pgb_silver_floor32(x),by=pgb_silver_floor32(y);
    unsigned group=s->history_group,best_key=~0u;
    uint64_t best=UINT64_MAX;
    for(unsigned i=0;i<s->atlas_count;i++) {
        const struct pgb_silver_connection *c=&s->atlas[i];
        int dx=bx<c->x?c->x-bx:bx>=c->x+c->width?bx-c->x-c->width+1:0;
        int dy=by<c->y?c->y-by:by>=c->y+c->height?by-c->y-c->height+1:0;
        uint64_t d=(uint64_t)dx*dx+(uint64_t)dy*dy;
        unsigned key=c->group*256u+c->number;
        if(d<best||(d==best&&key<best_key)){best=d;best_key=key;group=c->group;}
    }
    return group;
}
static uint16_t gbc_silver_roof_rgb555(const struct gbc_viewport *v,unsigned group,unsigned colour)
{
    unsigned night=v->roof_time_ready?v->roof_night:(v->view.wram[0x155b]&3)>=2;
    unsigned at=0xb934+group*8+night*4+(colour-1)*2;
    unsigned raw=v->silver.rom[at]|(v->silver.rom[at+1]<<8);
    return (uint16_t)(((raw&31)<<10)|(raw&0x3e0)|((raw>>10)&31));
}
static uint32_t gbc_view_colour(struct gbc_viewport *v,uint8_t p,unsigned x,unsigned y)
{
    uint32_t native=gbc_rgb(v->gb,p);
    if(!v->silver_enabled||!v->background_enabled||!v->gb->cgb.cgbMode||
        !v->silver.frame_valid||(p!=25&&p!=26)||
        (v->compose.width==160&&v->compose.height==144))return native;
    int wx=v->silver.frame_map.origin_x+(int)x-v->compose.left;
    int wy=v->silver.frame_map.origin_y+(int)y-v->compose.top;
    /* All eight pixels of a terrain tile have the same map owner. Avoid a
     * full atlas search per roof pixel; reset this cache at each frame begin. */
    int tx=wx>=0?wx/8:-((7-wx)/8),ty=wy>=0?wy/8:-((7-wy)/8);
    if(!v->roof_cache_valid||v->roof_cache_x!=tx||v->roof_cache_y!=ty) {
        uint8_t tile=gbc_silver_cached_tile(v,wx,wy);
        unsigned slot=gbc_silver_cached_cell(v,wx,wy);
        if(v->roof_tile[tile]&&v->terrain_cache[slot].owner==255)
            v->terrain_cache[slot].owner=(uint8_t)gbc_silver_owner_group(v,wx,wy);
        v->roof_cache_group=v->roof_tile[tile]?v->terrain_cache[slot].owner:0;
        v->roof_cache_x=tx;v->roof_cache_y=ty;v->roof_cache_valid=1;
    }
    unsigned group=v->roof_cache_group;
    if(!group||group>26)return native;
    /* Silver uploads the new hardware palette after changing map IDs. On a
     * return trip the spatial owner can already be "current" while the old
     * green palette is still installed. BGP=$e4 is the game's normal phase:
     * always use the owner's ROM colours there, even during that upload gap.
     * Other BGP mappings are the game's fade/flash phases and stay untouched. */
    if(v->gb->hram_io[0x47]!=0xe4)return native;
    unsigned c=gbc_silver_roof_rgb555(v,group,p-24);
    return 0xff000000u|(((c>>10)&31)*255/31<<16)|(((c>>5)&31)*255/31<<8)|((c&31)*255/31);
}
#endif
