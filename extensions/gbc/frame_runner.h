/* Connected-map loading catch-up. Execute the original CPU/PPU frames, but
 * finish a bounded loading interval before presenting the next image. */
#ifndef GX_GBC_FRAME_RUNNER_H
#define GX_GBC_FRAME_RUNNER_H
struct gbc_frame_runner {
    unsigned pending,group,number;
    unsigned long extra_frames,connections;
};
static int gbc_connection_ready(const struct gbc_viewport *v,const struct gb_s *g)
{
    return g->wram[0x114b]==2&&v->silver.frame_valid&&v->compose.next_y==144&&
        v->silver.history_group==g->wram[0x19c6]&&v->silver.history_number==g->wram[0x19c7];
}
static unsigned gbc_run_display_frame(struct gbc_frame_runner *r,struct gbc_viewport *v,
    struct gb_s *g,int enabled,double (*clock_ms)(void))
{
    double start=clock_ms?clock_ms():0;
    unsigned extra=0,old_group=g->wram[0x19c6],old_number=g->wram[0x19c7];
    int old_valid=v->silver.frame_valid;
    gb_run_frame(g);
    if(!enabled||!v->silver_enabled||!v->background_enabled||
        (v->compose.width==160&&v->compose.height==144)) {r->pending=0;return 0;}
    if(r->pending) {
        /* Doors, warps, menus and subsequent unrelated map changes cannot
         * inherit this connected-map fast path. Never skip game instructions. */
        if(g->wram[0x19c6]==r->group&&g->wram[0x19c7]==r->number&&
            !g->wram[0x1108]&&(g->wram[0xfdf]&0x41)==1&&
            (g->hram_io[0x40]&0x99)==0x81) {
            while(!gbc_connection_ready(v,g)&&extra<6&&
                (g->wram[0x114b]==1||g->wram[0x114b]==2)&&
                (!clock_ms||clock_ms()-start<8.0)) {
                gb_run_frame(g);extra++;
                if(g->wram[0x19c6]!=r->group||g->wram[0x19c7]!=r->number||g->wram[0x1108])break;
            }
            if(gbc_connection_ready(v,g))r->connections++;
        }
        r->pending=0;
    /* JP Rev1 hMapEntryMethod=$FFA1, MAPSETUP_CONNECTION=$F7;
     * ROM $96696: F0 A1 FE F7 confirms the connection-only check. */
    } else if(old_valid&&g->wram[0x114b]==1&&g->hram_io[0xa1]==0xf7&&
        (old_group!=g->wram[0x19c6]||old_number!=g->wram[0x19c7])) {
        for(unsigned i=0;i<4;i++) {
            const struct pgb_silver_connection *c=&v->silver.connections[i];
            if(c->blocks&&c->group==g->wram[0x19c6]&&c->number==g->wram[0x19c7]) {
                r->pending=1;r->group=c->group;r->number=c->number;break;
            }
        }
    }
    r->extra_frames+=extra;
    return extra;
}
#endif
