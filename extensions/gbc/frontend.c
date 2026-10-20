/* Optional native-resolution GBC frontend. No implicit save writes. */
#define PEANUT_FULL_GBC_SUPPORT 1
#define ENABLE_SOUND 0
#define __not_in_flash_func(x) x
#define RGB555_TO_RGB444(c) ((((c)>>3)&0xf00)|(((c)>>2)&0xf0)|(((c)>>1)&0xf))
#include "peanut_gbc.h"
#include <SDL.h>
#include <stdio.h>
#include <errno.h>
#include <sys/resource.h>

static uint8_t *rom, *ram;
static size_t rom_size, ram_size;
static uint32_t pixels[144][160];
static unsigned errors;
static uint8_t read_rom(struct gb_s *g, uint_fast32_t a) {
    (void)g; return a < rom_size ? rom[a] : 0xff;
}
static uint8_t read_ram(struct gb_s *g, uint_fast32_t a) {
    (void)g; return a < ram_size ? ram[a] : 0xff;
}
static void write_ram(struct gb_s *g, uint_fast32_t a, uint8_t v) {
    (void)g; if (a < ram_size) ram[a] = v;
}
static void core_error(struct gb_s *g, enum gb_error_e e, uint16_t a) {
    fprintf(stderr, "Core error %d at %04x PC=%04x\n", e, a, g->cpu_reg.pc.reg);
    errors++; exit(2);
}
static void draw(struct gb_s *g, const uint8_t *p, uint_fast8_t y) {
    if (y >= 144) return;
    for (unsigned x=0; x<160; x++) {
        unsigned r,b,v;
        if (g->cgb.cgbMode) {
            unsigned c=g->cgb.fixPalette[p[x]&63];
            r=((c>>10)&31)*255/31; v=((c>>5)&31)*255/31; b=(c&31)*255/31;
        } else r=v=b=255-(p[x]&3)*85;
        pixels[y][x]=0xff000000u|(r<<16)|(v<<8)|b;
    }
}
static void snapshot(const char *path) {
    FILE *f=fopen(path,"wb"); if (!f) { perror(path); exit(1); }
    fprintf(f,"P6\n160 144\n255\n");
    for(unsigned y=0;y<144;y++) for(unsigned x=0;x<160;x++) {
        unsigned c=pixels[y][x]; unsigned char p[3]={c>>16,c>>8,c};
        if(fwrite(p,1,3,f)!=3) exit(1);
    }
    if(fclose(f)) exit(1);
}
int main(int argc,char **argv) {
    const char *rom_path=NULL,*save=NULL,*shot=NULL,*script=NULL;
    unsigned long frames=0;
    int hidden=0,fast=0,write_save=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--hidden")) hidden=1;
        else if(!strcmp(argv[i],"--fast")) fast=1;
        else if(!strcmp(argv[i],"--write-save")) write_save=1;
        else if(i+1<argc&&!strcmp(argv[i],"--frames")) frames=strtoul(argv[++i],NULL,10);
        else if(i+1<argc&&!strcmp(argv[i],"--save")) save=argv[++i];
        else if(i+1<argc&&!strcmp(argv[i],"--snapshot")) shot=argv[++i];
        else if(i+1<argc&&!strcmp(argv[i],"--input")) script=argv[++i];
        else if(argv[i][0]!='-'&&!rom_path) rom_path=argv[i];
        else { fprintf(stderr,"Unknown/incomplete argument: %s\n",argv[i]); return 1; }
    }
    if(!rom_path || (write_save&&!save) || (hidden&&!frames)) {
        fprintf(stderr,"Usage: %s ROM [--save COPY] [--write-save] [--hidden --frames N] [--fast] [--input SCRIPT] [--snapshot PPM]\n",argv[0]); return 1;
    }
    if(save&&!strcmp(save,rom_path)) return 1;
    FILE *f=fopen(rom_path,"rb"); if(!f) {perror(rom_path);return 1;}
    if(fseek(f,0,SEEK_END)||ftell(f)<0x150) return 1;
    rom_size=(size_t)ftell(f); rewind(f); rom=malloc(rom_size);
    if(!rom||fread(rom,1,rom_size,f)!=rom_size) return 1;
    fclose(f);
    /* The imported core indexes fixed header tables during gb_init. */
    if(rom[0x148]>8 || rom[0x149]>5 || rom_size<((size_t)32768<<rom[0x148])) {
        fprintf(stderr,"Unsupported or truncated ROM size/header\n"); return 1;
    }
    struct gb_s *g=calloc(1,sizeof(*g)); if(!g) return 1;
    if(gb_init(g,read_rom,read_ram,write_ram,core_error,NULL)) {fprintf(stderr,"Unsupported ROM\n");return 1;}
    ram_size=gb_get_save_size(g); ram=malloc(ram_size?ram_size:1); if(!ram)return 1;
    memset(ram,0xff,ram_size);
    if(save) { f=fopen(save,"rb"); if(f) { if(fread(ram,1,ram_size,f)!=ram_size||fgetc(f)!=EOF){fprintf(stderr,"Save size mismatch\n");return 1;} fclose(f); } else if(errno!=ENOENT){perror(save);return 1;} }
    gb_init_lcd(g,draw);
    FILE *input=script?fopen(script,"r"):NULL;
    if(script&&!input){perror(script);return 1;}
    unsigned long next=0; unsigned mask=0; int has=input&&fscanf(input,"%lu %x",&next,&mask)==2;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) {fprintf(stderr,"%s\n",SDL_GetError());return 1;}
    SDL_Window *window=SDL_CreateWindow("Peanut GBC (experimental, silent)",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,160,144,hidden?SDL_WINDOW_HIDDEN:0);
    SDL_Renderer *renderer=window?SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE):NULL;
    SDL_Texture *texture=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,160,144):NULL;
    if(!texture){fprintf(stderr,"%s\n",SDL_GetError());return 1;}
    uint64_t start=SDL_GetPerformanceCounter(); unsigned long count=0; int quit=0; uint8_t scripted=255;
    while(!quit&&(!frames||count<frames)) {
        SDL_Event event; while(SDL_PollEvent(&event)) if(event.type==SDL_QUIT)quit=1;
        const uint8_t *k=SDL_GetKeyboardState(NULL);
        if(k[SDL_SCANCODE_ESCAPE])quit=1;
        while(has&&next<=count){scripted=(uint8_t)~mask;has=fscanf(input,"%lu %x",&next,&mask)==2;}
        unsigned keys=(k[SDL_SCANCODE_Z]?1:0)|(k[SDL_SCANCODE_X]?2:0)|(k[SDL_SCANCODE_BACKSPACE]?4:0)|(k[SDL_SCANCODE_RETURN]?8:0)|(k[SDL_SCANCODE_RIGHT]?16:0)|(k[SDL_SCANCODE_LEFT]?32:0)|(k[SDL_SCANCODE_UP]?64:0)|(k[SDL_SCANCODE_DOWN]?128:0);
        g->direct.joypad=scripted & (uint8_t)~keys;
        gb_run_frame(g); count++;
        /* RTC uses emulated frame time; CPU double speed must not double it. */
        if((uint64_t)count*70224/4194304 != (uint64_t)(count-1)*70224/4194304)gb_tick_rtc(g);
        if(!hidden){SDL_UpdateTexture(texture,NULL,pixels,160*4);SDL_RenderCopy(renderer,texture,NULL,NULL);SDL_RenderPresent(renderer);}
        if(!fast){double due=count*(70224.0/4194304.0)*1000;double elapsed=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();if(due>elapsed)SDL_Delay((uint32_t)(due-elapsed));}
    }
    double elapsed=(SDL_GetPerformanceCounter()-start)/(double)SDL_GetPerformanceFrequency();
    struct rusage usage; getrusage(RUSAGE_SELF,&usage);
    printf("frames=%lu wall=%.3fs cpu=%.3fs fps=%.1f context=%zu rom=%zu save=%zu framebuffer=%zu maxrss=%ldKiB cgb=%u double=%u PC=%04x errors=%u\n",count,elapsed,usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6,count/elapsed,sizeof(*g),rom_size,ram_size,sizeof(pixels),usage.ru_maxrss,g->cgb.cgbMode,g->cgb.doubleSpeed,g->cpu_reg.pc.reg,errors);
    if(shot)snapshot(shot);
    if(write_save){f=fopen(save,"wb");if(!f||fwrite(ram,1,ram_size,f)!=ram_size||fclose(f))return 1;}
    if(input)fclose(input);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();free(g);free(ram);free(rom);return 0;
}
