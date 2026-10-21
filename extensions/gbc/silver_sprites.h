/* CGB colour counterpart to the shared Silver presentation object decoder.
 * Exact JP Rev1 only. No OAM/VRAM writes and no emulation outside native PPU. */
#ifndef GX_CGB_SILVER_SPRITES_H
#define GX_CGB_SILVER_SPRITES_H
/* Only genuinely stationary declarations are safe before the game has a
 * live slot. Random walkers/spinners/scripts have no predictable pose. */
static int gbc_silver_stationary(unsigned movement)
{
    return movement==1||(movement>=6&&movement<=9);
}
static void gbc_silver_remove_predictions(struct gbc_viewport *v)
{
    unsigned n=0;
    for(unsigned i=0;i<v->silver.object_count;i++) {
        struct pgb_silver_object o=v->silver.objects[i];
        int keep=o.live;
        if(!keep&&o.map_group==v->view.wram[0x19c6]&&o.map_number==v->view.wram[0x19c7]&&o.map_index<16)
            keep=gbc_silver_stationary(v->view.wram[0x1438+o.map_index*16+4]);
        else if(!keep)for(unsigned j=0;j<v->silver.atlas_count;j++) {
            const struct pgb_silver_connection *c=&v->silver.atlas[j];
            if(c->group!=o.map_group||c->number!=o.map_number)continue;
            uint32_t records=0,base=0;
            unsigned count=pgb_silver_region_events(&v->silver,c,&records,&base);
            if(o.map_index>=2&&(unsigned)(o.map_index-2)<count)
                keep=gbc_silver_stationary(v->silver.rom[records+(o.map_index-2)*13+3]);
            break;
        }
        if(keep)v->silver.objects[n++]=o;
    }
    v->silver.object_count=n;
}
static uint8_t gbc_silver_terrain(struct gbc_viewport *v,int x,int y)
{
    int wx=v->silver.frame_map.origin_x+x,wy=v->silver.frame_map.origin_y+y;
    uint8_t tile=gbc_silver_cached_tile(v,wx,wy);
    return gx_cgb_tile_pixel(v->gb->vram,v->gb->hram_io[0x40],tile,v->tile_attr[tile],(unsigned)wx&7,(unsigned)wy&7);
}
static int gbc_silver_object_palette(struct gbc_viewport *v,const struct pgb_silver_object *o)
{
    const uint8_t *r=NULL;unsigned id,override;
    /* A retained old-map actor must never resolve through the new map's live
     * slots. Those slots can already be reused while its old pose is drawn. */
    int same_map=o->map_group==v->view.wram[0x19c6]&&o->map_number==v->view.wram[0x19c7];
    if(!o->preview&&same_map) {
        for(unsigned i=0;i<13;i++) {
            const uint8_t *live=v->view.wram+0x11f0+i*40;
            if(live[0]&&live[1]==o->map_index)return live[6]&7;
        }
        if(o->map_index>=16)return -1;
        r=v->view.wram+0x1438+o->map_index*16;id=r[1];override=r[8]>>4;
        for(unsigned depth=0;id>=0xf0&&depth<4;depth++)id=v->view.wram[0x1548+id-0xf0];
    } else {
        for(unsigned i=0;i<v->silver.atlas_count;i++) {
            const struct pgb_silver_connection *c=&v->silver.atlas[i];
            if(c->group!=o->map_group||c->number!=o->map_number)continue;
            uint32_t records=0,base=0;
            unsigned count=pgb_silver_region_events(&v->silver,c,&records,&base);
            if(o->map_index<2||(unsigned)(o->map_index-2)>=count)return -1;
            r=v->silver.rom+records+(o->map_index-2)*13;break;
        }
        if(!r)return -1;
        id=r[0];override=r[7]>>4;
    }
    if(override)return override&7;
    if(!id||id>=0xf0)return -1;
    return id<0x80?(v->silver.rom[0x147b0+(id-1)*6+5]&7):0;
}
static void gbc_silver_capture_palettes(struct gbc_viewport *v)
{
    for(unsigned i=0;i<v->silver.object_count;i++) {
        const struct pgb_silver_object *o=&v->silver.objects[i];
        unsigned cached=PGB_SILVER_MAX_OBJECTS;
        for(unsigned j=0;j<PGB_SILVER_MAX_OBJECTS;j++)
            if(v->palette_history[j].group==o->map_group&&v->palette_history[j].number==o->map_number&&
                v->palette_history[j].index==o->map_index){cached=j;break;}
        int same=o->map_group==v->view.wram[0x19c6]&&o->map_number==v->view.wram[0x19c7];
        int palette;
        if(!same&&cached<PGB_SILVER_MAX_OBJECTS)palette=v->palette_history[cached].palette;
        else if(same&&o->oam_pose&&o->order<40)palette=v->gb->oam[o->order*4+3]&7;
        else palette=gbc_silver_object_palette(v,o);
        if(palette<0&&cached<PGB_SILVER_MAX_OBJECTS)palette=v->palette_history[cached].palette;
        v->object_palettes[i]=(int8_t)palette;
        if(palette>=0&&o->map_group&&o->map_number) {
            if(cached==PGB_SILVER_MAX_OBJECTS)cached=v->palette_history_next++%PGB_SILVER_MAX_OBJECTS;
            v->palette_history[cached].group=o->map_group;v->palette_history[cached].number=o->map_number;
            v->palette_history[cached].index=o->map_index;v->palette_history[cached].palette=(uint8_t)palette;
        }
    }
    v->object_palettes_ready=1;
}
static int gbc_silver_frame_palette(struct gbc_viewport *v,unsigned i)
{
    return v->object_palettes_ready?v->object_palettes[i]:gbc_silver_object_palette(v,&v->silver.objects[i]);
}
static int gbc_part_order(const void *a,const void *b)
{
    const struct pgb_silver_part *pa=a,*pb=b;
    return (int)pa->order-(int)pb->order; /* CGB: OAM order, not DMG X priority. */
}
static void gbc_silver_sprite_layer(struct gbc_viewport *v,uint8_t *pixels,uint16_t width,int x,int y,int seams_only)
{
    struct pgb_silver_part parts[PGB_SILVER_MAX_OBJECTS*4];unsigned count=0;
    uint8_t native_pixels[160];
    const struct pgb_silver_jp *s=&v->silver;
    if(!s->sprites_enabled || !(s->features&PGB_SILVER_SPRITES) || !(v->gb->hram_io[0x40]&2))return;
    if(seams_only) {
        if(x!=0||width!=160||y<0||y>=144)return;
        memcpy(native_pixels,pixels,160);
    }
    for(unsigned i=0;i<s->object_count;i++) {
        const struct pgb_silver_object *o=&s->objects[i];
        if(seams_only&&o->oam_pose)continue; /* core already owns this exact pose */
        if(y<o->y-8||y>=o->y+32||o->x+32<=x||o->x-8>=x+width)continue;
        int palette=gbc_silver_frame_palette(v,i);if(palette<0)continue;
        unsigned at=pgb_silver_rom_word(s,0x404a+o->facing*2);
        if(at<0x4000||at>=0x4274)continue;
        unsigned n=s->rom[at++];if(n>16||at+n*4>0x4274)continue;
        if(seams_only) {
            int left=INT32_MAX,top=INT32_MAX,right=INT32_MIN,bottom=INT32_MIN;
            for(unsigned j=0;j<n;j++) {
                int px=o->x+pgb_silver_signed(s->rom[at+j*4+1]);
                int py=o->y+pgb_silver_signed(s->rom[at+j*4]);
                if(px<left)left=px;
                if(py<top)top=py;
                if(px+8>right)right=px+8;
                if(py+8>bottom)bottom=py+8;
            }
            if(left>=160||right<=0||top>=144||bottom<=0||
                (left>=0&&right<=160&&top>=0&&bottom<=144))continue;
        }
        for(unsigned j=0;j<n;j++,at+=4) {
            int py=o->y+pgb_silver_signed(s->rom[at]),px=o->x+pgb_silver_signed(s->rom[at+1]);
            if(y<py||y>=py+8||px+8<=x||px>=x+width)continue;
            unsigned flags=s->rom[at+2],tile=s->rom[at+3],row=(unsigned)(y-py);uint32_t graphics;
            if(flags&2)flags|=o->grass;
            flags=(flags&0xf0)|o->flags|(unsigned)palette;
            if(flags&0x40)row=7-row;
            if(s->rom[at+2]&4)graphics=tile*16;
            else if(o->graphics)graphics=0x80000000u|(o->graphics+(tile>=128?o->tiles+tile-128:tile)*16);
            else graphics=((o->tile+tile)&255)*16;
            graphics+=row*2;
            if((graphics&0x7fffffffu)+1>=((graphics&0x80000000u)?s->rom_size:0x2000))continue;
            if(count>=sizeof(parts)/sizeof(parts[0]))continue;
            parts[count].x=px;parts[count].graphics=graphics;parts[count].flags=(uint8_t)flags;
            parts[count++].order=(uint16_t)(o->order+j);
        }
    }
    qsort(parts,count,sizeof(parts[0]),gbc_part_order);
    while(count) {
        const struct pgb_silver_part *p=&parts[--count];
        const uint8_t *data=(p->graphics&0x80000000u)?s->rom+(p->graphics&0x7fffffffu):v->gb->vram+p->graphics;
        for(unsigned i=0;i<8;i++) {
            int sx=p->x+(int)i,dx=sx-x;if(dx<0||dx>=width)continue;
            if(seams_only&&native_pixels[dx]>=32)continue; /* player/real OBJ win */
            unsigned bit=(p->flags&0x20)?i:7-i,c=((data[0]>>bit)&1)|(((data[1]>>bit)&1)<<1);
            if(!c)continue;
            uint8_t bg=gbc_silver_terrain(v,sx,y);
            pixels[dx]=((p->flags&0x80)&&(bg&3))?(seams_only?native_pixels[dx]:bg):(uint8_t)(32+(p->flags&7)*4+c);
        }
    }
}
static void gbc_silver_sprites(struct gbc_viewport *v,uint8_t *pixels,uint16_t width,int x,int y)
{
    gbc_silver_sprite_layer(v,pixels,width,x,y,0);
}
#endif
