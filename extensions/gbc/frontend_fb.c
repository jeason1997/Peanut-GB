#define _POSIX_C_SOURCE 200809L
/* Optional Linux fbdev frontend; shares the unchanged GBC viewport and frame runner.
 * Serial controls: WASD, Z/X, E=Start, R=Select, Q=quit. Saves are read-only by default. */
#define PEANUT_FULL_GBC_SUPPORT 1
#define ENABLE_SOUND 0
#define __not_in_flash_func(x) x
#define RGB555_TO_RGB444(c) ((((c)>>3)&0xf00)|(((c)>>2)&0xf0)|(((c)>>1)&0xf))
#include "peanut_gbc.h"
#include "viewport.h"
#include "frame_runner.h"
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/resource.h>

static uint8_t *rom, *ram;
static size_t rom_size, ram_size;

static struct gbc_viewport viewport;
static uint32_t *output;
static uint8_t *viewport_row;
static unsigned output_width=320,output_height=240;
static unsigned errors;
static int profile_enabled;
static uint64_t profile_draw_ticks;
struct frame_profile {unsigned long frames,over_budget;double run_ms,draw_ms,present_ms,max_ms;};
static void profile_record(struct frame_profile *p,double run,double draw_time,double present)
{
    p->frames++;p->run_ms+=run;p->draw_ms+=draw_time;p->present_ms+=present;
    if(run+present>p->max_ms)p->max_ms=run+present;
    if(run+present>70224.0/4194304.0*1000)p->over_budget++;
}
static void profile_report(const char *name,const struct frame_profile *p)
{
    if(p->frames)printf("profile=%s samples=%lu run_avg_ms=%.3f draw_avg_ms=%.3f present_avg_ms=%.3f work_max_ms=%.3f over_budget=%lu\n",
        name,p->frames,p->run_ms/p->frames,p->draw_ms/p->frames,p->present_ms/p->frames,p->max_ms,p->over_budget);
}
static double monotonic_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000.0+t.tv_nsec/1e6;
}
static int parse_viewport(const char *text,unsigned *w,unsigned *h) {
    char *end;errno=0;
    if(*text<'0'||*text>'9')return 0;
    unsigned long a=strtoul(text,&end,10);
    if(errno||*end!='x'||end[1]<'0'||end[1]>'9')return 0;
    unsigned long b=strtoul(end+1,&end,10);
    if(errno||*end||a<160||b<144||a>65535||b>65535||(uint64_t)a*b>16777216)return 0;
    *w=(unsigned)a;*h=(unsigned)b;return 1;
}
static uint64_t ticks(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
static void pause_ms(double ms) {struct timespec t={(time_t)(ms/1000),(long)((ms-(time_t)(ms/1000)*1000)*1e6)};nanosleep(&t,NULL);}
static volatile sig_atomic_t stopped;
static void stop(int sig){(void)sig;stopped=1;}
static int fb_fd,console_fd=-1,console_mode=-1;
static void restore_console(void) {if(console_fd>=0){if(console_mode>=0)ioctl(console_fd,KDSETMODE,console_mode);close(console_fd);console_fd=-1;}}
static struct fb_var_screeninfo fb_var;
static struct fb_fix_screeninfo fb_fix;
static uint8_t *fb_memory, *packed_frame;
static uint64_t convert_ticks,write_ticks;
static int rgb565,fb_flush=1;
static uint32_t component(unsigned c,struct fb_bitfield f) {return f.length?((c*((1u<<f.length)-1)+127)/255)<<f.offset:0;}
static void fb_open(void) {
    fb_fd=open("/dev/fb0",O_RDWR);
    if(fb_fd<0||ioctl(fb_fd,FBIOGET_VSCREENINFO,&fb_var)||ioctl(fb_fd,FBIOGET_FSCREENINFO,&fb_fix)){perror("fb0 query");exit(1);}
    printf("fb=%s %ux%u virtual=%ux%u offset=%u,%u bpp=%u stride=%u bytes=%u RGB=%u/%u,%u/%u,%u/%u type=%u visual=%u\n",fb_fix.id,fb_var.xres,fb_var.yres,fb_var.xres_virtual,fb_var.yres_virtual,fb_var.xoffset,fb_var.yoffset,fb_var.bits_per_pixel,fb_fix.line_length,fb_fix.smem_len,fb_var.red.offset,fb_var.red.length,fb_var.green.offset,fb_var.green.length,fb_var.blue.offset,fb_var.blue.length,fb_fix.type,fb_fix.visual);
    if(fb_var.xres!=output_width||fb_var.yres!=output_height||fb_fix.type!=FB_TYPE_PACKED_PIXELS||fb_fix.visual!=FB_VISUAL_TRUECOLOR||(fb_var.bits_per_pixel!=16&&fb_var.bits_per_pixel!=32)||fb_var.red.msb_right||fb_var.green.msb_right||fb_var.blue.msb_right||fb_var.red.length>8||fb_var.green.length>8||fb_var.blue.length>8||fb_var.transp.length>8||fb_var.red.offset+fb_var.red.length>fb_var.bits_per_pixel||fb_var.green.offset+fb_var.green.length>fb_var.bits_per_pixel||fb_var.blue.offset+fb_var.blue.length>fb_var.bits_per_pixel||fb_var.transp.offset+fb_var.transp.length>fb_var.bits_per_pixel||(uint64_t)(fb_var.yoffset+output_height-1)*fb_fix.line_length+(uint64_t)(fb_var.xoffset+output_width)*(fb_var.bits_per_pixel/8)>fb_fix.smem_len||(uint64_t)(fb_var.xoffset+output_width)*(fb_var.bits_per_pixel/8)>fb_fix.line_length){fprintf(stderr,"Unsupported framebuffer geometry/format\n");exit(1);}
    packed_frame=malloc((size_t)output_width*output_height*(fb_var.bits_per_pixel/8));
    if(!packed_frame){perror("packed buffer");exit(1);}
    rgb565=fb_var.bits_per_pixel==16&&fb_var.red.offset==11&&fb_var.red.length==5&&fb_var.green.offset==5&&fb_var.green.length==6&&fb_var.blue.offset==0&&fb_var.blue.length==5&&!fb_var.transp.length;
    console_fd=open("/dev/tty1",O_RDWR);
    if(console_fd>=0&&ioctl(console_fd,KDGETMODE,&console_mode)==0) {
        if(ioctl(console_fd,KDSETMODE,KD_GRAPHICS)) {perror("console graphics mode");exit(1);}
        atexit(restore_console);
    }
    if(ioctl(fb_fd,FBIOBLANK,FB_BLANK_UNBLANK))perror("fb unblank");
    fb_memory=mmap(NULL,fb_fix.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fb_fd,0);
    if(fb_memory==MAP_FAILED){perror("fb mmap");exit(1);}
}
static void fb_present(void) {
    unsigned bytes=fb_var.bits_per_pixel/8;
    uint64_t begin=profile_enabled?ticks():0;
    for(size_t i=0;i<(size_t)output_width*output_height;i++) {
        uint32_t c=output[i],packed;
        if(rgb565) packed=(((c>>16)&255)*31+127)/255<<11|(((c>>8)&255)*63+127)/255<<5|((c&255)*31+127)/255;
        else packed=component((c>>16)&255,fb_var.red)|component((c>>8)&255,fb_var.green)|component(c&255,fb_var.blue)|component(255,fb_var.transp);
        if(bytes==2){uint16_t v=(uint16_t)packed;memcpy(packed_frame+i*bytes,&v,2);}else memcpy(packed_frame+i*bytes,&packed,4);
    }
    uint64_t converted=profile_enabled?ticks():0;
    for(unsigned y=0;y<output_height;y++)memcpy(fb_memory+(y+fb_var.yoffset)*fb_fix.line_length+fb_var.xoffset*bytes,packed_frame+(size_t)y*output_width*bytes,output_width*bytes);
    /* fbdev mmap defers dirty pages (50ms on this DRM driver). Flush that
     * queue each display frame; this still does not fence physical scanout. */
    if(fb_flush&&fsync(fb_fd)){perror("fb fsync");exit(1);}
    if(profile_enabled){convert_ticks+=converted-begin;write_ticks+=ticks()-converted;}
}
static uint8_t read_rom(struct gb_s *g, uint_fast32_t a) {
    if(a>=rom_size)return 0xff;
    return viewport.gb==g&&viewport.silver_enabled?gbc_view_rom_read(&viewport,g,a):rom[a];
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
    (void)g;
    if (y >= 144) return;
    uint64_t started=profile_enabled?ticks():0;
    if(viewport.output)gbc_view_line(&viewport,p,y);
    if(profile_enabled)profile_draw_ticks+=ticks()-started;
}
static void snapshot(const char *path) {
    FILE *f=fopen(path,"wb"); if (!f) { perror(path); exit(1); }
    fprintf(f,"P6\n%u %u\n255\n",output_width,output_height);
    for(unsigned y=0;y<output_height;y++) for(unsigned x=0;x<output_width;x++) {
        unsigned c=output[(size_t)y*output_width+x]; unsigned char p[3]={c>>16,c>>8,c};
        if(fwrite(p,1,3,f)!=3) exit(1);
    }
    if(fclose(f)) exit(1);
}
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    const char *rom_path=NULL,*save=NULL,*shot=NULL,*script=NULL;
    unsigned long frames=0;
    int fast=0,write_save=0,background=1,sprites=1,catchup=1;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--fast")) fast=1;
        else if(!strcmp(argv[i],"--deferred-fb")) fb_flush=0;
        else if(!strcmp(argv[i],"--write-save")) write_save=1;
        else if(!strcmp(argv[i],"--no-background")) background=0;
        else if(!strcmp(argv[i],"--terrain-only")) sprites=0;
        else if(!strcmp(argv[i],"--no-transition-catchup")) catchup=0;
        else if(!strcmp(argv[i],"--profile")) profile_enabled=1;
        else if(i+1<argc&&!strcmp(argv[i],"--viewport")) {
            unsigned w,h;
            if(!parse_viewport(argv[++i],&w,&h)) {
                fprintf(stderr,"Invalid viewport: use WIDTHxHEIGHT, >=160x144, dimensions <=65535, <=16777216 pixels\n");return 1;
            }
            output_width=w;output_height=h;
        }
        else if(i+1<argc&&!strcmp(argv[i],"--frames")) frames=strtoul(argv[++i],NULL,10);
        else if(i+1<argc&&!strcmp(argv[i],"--save")) save=argv[++i];
        else if(i+1<argc&&!strcmp(argv[i],"--snapshot")) shot=argv[++i];
        else if(i+1<argc&&!strcmp(argv[i],"--input")) script=argv[++i];
        else if(argv[i][0]!='-'&&!rom_path) rom_path=argv[i];
        else { fprintf(stderr,"Unknown/incomplete argument: %s\n",argv[i]); return 1; }
    }
    if(!rom_path || (write_save&&!save)) {
        fprintf(stderr,"Usage: %s ROM [--viewport WxH] [--no-background] [--terrain-only] [--no-transition-catchup] [--profile] [--save COPY] [--write-save] [--frames N] [--fast] [--deferred-fb] [--input SCRIPT] [--snapshot PPM]\n",argv[0]); return 1;
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
    output=calloc((size_t)output_width*output_height,sizeof(*output));viewport_row=malloc(output_width);
    if(!output||!viewport_row||!gbc_view_init(&viewport,g,rom,rom_size,(uint16_t)output_width,(uint16_t)output_height,output,viewport_row,background))return 1;
    if(!sprites)pgb_silver_jp_configure(&viewport.silver,viewport.silver.features&~(PGB_SILVER_SPRITES|PGB_SILVER_NEIGHBOR_NPCS),PGB_SILVER_MAX_OBJECTS);
    printf("viewport=%ux%u backend=%s background=%d\n",output_width,output_height,viewport.silver_enabled?"silver-jp-rev1-terrain":"VRAM (may be stale)",background);
    gb_init_lcd(g,draw);
    FILE *input=script?fopen(script,"r"):NULL;
    if(script&&!input){perror(script);return 1;}
    unsigned long next=0; unsigned mask=0; int has=input&&fscanf(input,"%lu %x",&next,&mask)==2;
    fb_open();
    struct termios old_term, raw_term; int terminal=isatty(STDIN_FILENO);
    if(terminal) {tcgetattr(0,&old_term);raw_term=old_term;raw_term.c_lflag &= ~(ICANON|ECHO);raw_term.c_cc[VMIN]=0;raw_term.c_cc[VTIME]=0;tcsetattr(0,TCSANOW,&raw_term);}
    int old_flags=fcntl(0,F_GETFL);fcntl(0,F_SETFL,old_flags|O_NONBLOCK);
    signal(SIGINT,stop);signal(SIGTERM,stop);
    uint64_t rtc_seconds=0;
    uint64_t start=ticks(); unsigned long count=0; int quit=0; uint8_t scripted=255;
    struct gbc_frame_runner runner={0};
    struct frame_profile all_profile={0},map_profile={0};
    double tick_ms=0.000001;
    while(!quit&&(!frames||count<frames)) {
        unsigned keys=0; char key;
        /* Serial keys are pulses (8 display frames); script masks remain held. */
        static unsigned held=0;static unsigned long release=0;
        while(read(0,&key,1)==1) {
            if(key=='q')quit=1;
            unsigned m=key=='d'?16:key=='a'?32:key=='w'?64:key=='s'?128:key=='z'?1:key=='x'?2:key=='e'?8:key=='r'?4:0;
            if(m){held=m;release=count+8;printf("input frame=%lu mask=%02x\n",count,m);}
        }
        if(count>=release)held=0;
        keys=held;
        while(has&&next<=count){scripted=(uint8_t)~mask;has=fscanf(input,"%lu %x",&next,&mask)==2;}
        if(stopped)quit=1;
        g->direct.joypad=scripted & (uint8_t)~keys;
        uint64_t frame_started=profile_enabled?ticks():0;profile_draw_ticks=0;
        gbc_run_display_frame(&runner,&viewport,g,catchup,monotonic_ms); count++;
        /* RTC follows monotonic wall time, including when the board falls behind. */
        uint64_t real_seconds=(ticks()-start)/1000000000;
        while(rtc_seconds<real_seconds){gb_tick_rtc(g);rtc_seconds++;}
        uint64_t run_finished=profile_enabled?ticks():0;
        fb_present();
        if(viewport.silver_enabled&&viewport.silver.frame_valid) {
            static unsigned previous=~0u;
            unsigned map=((unsigned)g->wram[0x19c6]<<8)|g->wram[0x19c7];
            if(map!=previous){printf("map frame=%lu id=%u/%u player=%u,%u catchups=%lu extra=%lu\n",count,map>>8,map&255,g->wram[0x19c9],g->wram[0x19c8],runner.connections,runner.extra_frames);previous=map;}
        }
        if(profile_enabled) {
            double run=(run_finished-frame_started)*tick_ms,draw_time=profile_draw_ticks*tick_ms;
            double present=(ticks()-run_finished)*tick_ms;
            profile_record(&all_profile,run,draw_time,present);
            if(viewport.silver.frame_valid)profile_record(&map_profile,run,draw_time,present);
        }
        if(!fast){double due=count*(70224.0/4194304.0)*1000;double elapsed=(ticks()-start)/1e6;if(due>elapsed)pause_ms(due-elapsed);}
    }

    double elapsed=(ticks()-start)/1e9;
    struct rusage usage; getrusage(RUSAGE_SELF,&usage);
    printf("frames=%lu wall=%.3fs cpu=%.3fs fps=%.1f context=%zu rom=%zu save=%zu framebuffer=%zu maxrss=%ldKiB cgb=%u double=%u PC=%04x errors=%u\n",count,elapsed,usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6,count/elapsed,sizeof(*g),rom_size,ram_size,(size_t)fb_fix.smem_len,usage.ru_maxrss,g->cgb.cgbMode,g->cgb.doubleSpeed,g->cpu_reg.pc.reg,errors);
    if(shot)snapshot(shot);
    if(profile_enabled){profile_report("all",&all_profile);profile_report("map",&map_profile);}
    if(profile_enabled)printf("fb_convert_avg_ms=%.3f fb_copy_flush_avg_ms=%.3f (mmap copy/flush, excludes asynchronous SPI scanout)\n",convert_ticks/1e6/count,write_ticks/1e6/count);
    printf("connection_catchup=%d completed=%lu extra_emulated_frames=%lu\n",catchup,runner.connections,runner.extra_frames);
    printf("extension map_frames=%lu colour_valid_frames=%lu palette_mismatches=%u rows=%lu margin_sprite_pixels=%lu native_seam_pixels=%lu output_bytes=%zu row_bytes=%u adapter_bytes=%zu\n",viewport.map_frames,viewport.valid_frames,viewport.palette_mismatches,viewport.emitted_rows,viewport.margin_sprite_pixels,viewport.native_seam_pixels,(size_t)output_width*output_height*sizeof(*output),output_width,sizeof(viewport));
    if(viewport.silver_enabled)printf("silver map=%u/%u player=%u,%u\n",g->wram[0x19c6],g->wram[0x19c7],g->wram[0x19c9],g->wram[0x19c8]);
    if(write_save){f=fopen(save,"wb");if(!f||fwrite(ram,1,ram_size,f)!=ram_size||fclose(f))return 1;}
    if(input)fclose(input);
    if(terminal)tcsetattr(0,TCSANOW,&old_term);
    fcntl(0,F_SETFL,old_flags);munmap(fb_memory,fb_fix.smem_len);close(fb_fd);
    free(packed_frame);free(output);free(viewport_row);free(g);free(ram);free(rom);return 0;
}
