/* Behavioral smoke tests, not a hardware conformance suite. */
#define main frontend_main
#include "frontend.c"
#undef main
#include <assert.h>
int main(void) {
    rom_size=8*1024*1024;rom=calloc(1,rom_size);ram_size=32768;ram=calloc(1,ram_size);
    assert(rom&&ram);
    rom[0x143]=0xc0;rom[0x147]=0x1b;rom[0x148]=8;rom[0x149]=3;
    for(unsigned i=0x134;i<=0x14c;i++)rom[0x14d]-=rom[i]+1;
    struct gb_s *g=calloc(1,sizeof(*g));assert(g);
    assert(gb_init(g,read_rom,read_ram,write_ram,core_error,NULL)==0);
    assert(g->cgb.cgbMode&&gb_get_save_size(g)==32768);
    rom[0x4000]=17;rom[256*0x4000]=93;
    __gb_write(g,0x2000,1);assert(__gb_read(g,0x4000)==17);
    __gb_write(g,0x2000,0);__gb_write(g,0x3000,1);assert(__gb_read(g,0x4000)==93);
    __gb_write(g,0xff4f,0);__gb_write(g,0x8000,12);
    __gb_write(g,0xff4f,1);__gb_write(g,0x8000,34);
    assert(__gb_read(g,0x8000)==34);__gb_write(g,0xff4f,0);assert(__gb_read(g,0x8000)==12);
    __gb_write(g,0xff70,1);__gb_write(g,0xd000,56);
    __gb_write(g,0xff70,7);__gb_write(g,0xd000,78);
    assert(__gb_read(g,0xd000)==78);__gb_write(g,0xff70,0);assert(__gb_read(g,0xd000)==56);
    __gb_write(g,0xff68,0x80);__gb_write(g,0xff69,0x1f);__gb_write(g,0xff69,0);
    assert(g->cgb.fixPalette[0]==0x7c00&&g->cgb.BGPaletteID==2);
    uint8_t line[160]={0};draw(g,line,0);assert(pixels[0][0]==0xffff0000u);
    __gb_write(g,0xff6a,0xbf);__gb_write(g,0xff6b,0);assert(g->cgb.OAMPaletteID==0);
    for(unsigned i=0;i<16;i++)__gb_write(g,0xc000+i,0x80+i);
    __gb_write(g,0xff51,0xc0);__gb_write(g,0xff52,0);__gb_write(g,0xff53,0);__gb_write(g,0xff54,0x10);__gb_write(g,0xff55,0);
    for(unsigned i=0;i<16;i++)assert(__gb_read(g,0x8010+i)==0x80+i);
    assert(__gb_read(g,0xff55)==0xff);
    rom[0x100]=0x10;g->cpu_reg.pc.reg=0x100;__gb_write(g,0xff4d,1);__gb_step_cpu(g);
    assert(g->cgb.doubleSpeed==1&&!g->cgb.doubleSpeedPrep);
    gb_reset(g);assert(g->cgb.cgbMode&&!g->cgb.doubleSpeed);
    free(g);free(rom);free(ram);puts("GBC MBC5/VRAM/WRAM/palette/RGB/GDMA/KEY1/reset smoke PASS");return 0;
}
