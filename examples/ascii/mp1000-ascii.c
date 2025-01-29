/*
    mp1000.c

    Stripped down MP1000 emulator running in a (xterm-256color) terminal.
*/
#include <stdint.h>
#include <stdbool.h>
#include <curses.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "chips/beeper.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "chips/mc6847.h"
#include "chips/mc6800.h"
#include "chips/mc6821.h"
#include "systems/mp1000.h"
#include "mp1000-roms.h"
#include <locale.h>

static mp1000_t mp1000;

// run the emulator and render-loop at 30fps
#define FRAME_USEC (33333)
// border size
#define BORDER_HORI (5)
#define BORDER_VERT (3)

// a signal handler for Ctrl-C, for proper cleanup
static int quit_requested = 0;
static void catch_sigint(int signo) {
    (void)signo;
    quit_requested = 1;
}

// conversion table from C64 font index to ASCII (the 'x' is actually the pound sign)
static char font_map[65] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[x]   !\"#$%&`()*+,-./0123456789:;<=>?";

// map C64 color numbers to xterm-256color colors
static int colors[16] = {
    71,     // green
    185,    // yellow
    18,     // blue
    88,     // red
    231,    // white
    73,     // cyan
    54,     // purple
    136,    // orange

    16,     // black
    16,     // black
    16,     // black
    16,     // black
    16,     // black
    16,     // black
    16,     // black
    16,     // black
};

static char* unicode_map[16] = {
  " ",   // 0000: No quadrants
  " ",   // 0001: Bottom-right
  "▄",   // 0010: Bottom-left
  "▄",   // 0011: Bottom-left and Bottom-right
  " ",   // 0100: Top-right
  " ",   // 0101: Top-right and Bottom-right
  "▄",   // 0110: Top-right and Bottom-left
  "▄",   // 0111: All but Top-left
  "▀",   // 1000: Top-left
  "▀",   // 1001: Top-left and Bottom-right
  "█",   // 1010: Top-left and Bottom-left
  "█",   // 1011: All but Top-right
  "▀",   // 1100: Top-left and Top-right
  "▀",   // 1101: Top-left, Top-right, and Bottom-right
  "█",   // 1110: Top-left, Top-right, and Bottom-left
  "█"    // 1111: All quadrants
};

static char* unicode_map2[16] = {
  " ",   // 0000: No quadrants
  "▄",   // 0001: Bottom-right
  " ",   // 0010: Bottom-left
  "▄",   // 0011: Bottom-left and Bottom-right
  "▀",   // 0100: Top-right
  "█",   // 0101: Top-right and Bottom-right
  "▀",   // 0110: Top-right and Bottom-left
  "█",   // 0111: All but Top-left
  " ",   // 1000: Top-left
  "▄",   // 1001: Top-left and Bottom-right
  " ",   // 1010: Top-left and Bottom-left
  "▄",   // 1011: All but Top-right
  "▀",   // 1100: Top-left and Top-right
  "█",   // 1101: Top-left, Top-right, and Bottom-right
  "▀",   // 1110: Top-left, Top-right, and Bottom-left
  "█"    // 1111: All quadrants
};

static void init_mp1000_colors(void) {
    start_color();
    for (int fg = 0; fg < 16; fg++) {
        for (int bg = 0; bg < 16; bg++) {
            int cp = (fg*16 + bg) + 1;
            init_pair(cp, colors[fg], colors[bg]);
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    mp1000_init(&mp1000, &(mp1000_desc_t){
        .roms = {
            .bios = { .ptr=dump_mp1000_bios_rom, .size=sizeof(dump_mp1000_bios_rom) },
            .basic = { .ptr=dump_mp1000_basic68_rom, .size=sizeof(dump_mp1000_basic68_rom) },
            .cart = { .ptr=dump_mp1000_basic80_rom, .size=sizeof(dump_mp1000_basic80_rom) },
//            .cart = { .ptr=dump_mp1000_mblock_rom, .size=sizeof(dump_mp1000_mblock_rom) },
        }
    });

    // install a Ctrl-C signal handler
    signal(SIGINT, catch_sigint);

    // setup curses
    setlocale(LC_ALL, "");
    initscr();
    init_mp1000_colors();
    noecho();
    curs_set(FALSE);
    cbreak();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    attron(A_BOLD);

    int speed = FRAME_USEC;
    int frames = 0;

    // run the emulation/input/render loop
    while (!quit_requested) {
        // tick the emulator for 1 frame
        mp1000_exec(&mp1000, speed);
        frames++;

        // keyboard input
        int ch = getch();
        if (ch != ERR) {
            switch (ch) {
                case 10:  ch = 0x0D; break; // ENTER
                case 127: ch = 0x01; break; // BACKSPACE
                case 27:  ch = 0x03; break; // ESCAPE
                case 260: ch = 0x08; break; // LEFT
                case 261: ch = 0x09; break; // RIGHT
                case 259: ch = 0x0B; break; // UP
                case 258: ch = 0x0A; break; // DOWN
                case ']': speed = 0; break;
                case '\'': speed = 1900000.0f/894750; break;
                case '/': speed = 30000000.0f/894750; break;
            }
            if (ch > 32) {
                if (islower(ch)) {
                    ch = toupper(ch);
                }
                else if (isupper(ch)) {
                    ch = tolower(ch);
                }
            }
            if (ch < 256) {
                mp1000_key_down(&mp1000, ch);
                mp1000_key_up(&mp1000, ch);
            }
        }

        // render the PETSCII buffer
        int cur_color_pair = -1;
        //int bg = c64.vic.gunit.bg[0] & 0xF;
        for (uint32_t yy = 0; yy < 16; yy++) {
            for (uint32_t xx = 0; xx < 32; xx++) {
                    // bitmap area (not border)
                    int x = xx; //- BORDER_HORI;
                    int y = yy; //- BORDER_VERT;

                    // get character index
                    uint16_t addr = y*32 + x;
                    uint8_t font_code = mem_rd(&mp1000.mem_vdg, addr);
                    char chr = font_map[font_code & 63];
                    int color_pair;
                    if (font_code & 128) {
                        int fg = (font_code >> 4) & 7;
                        color_pair = 16*fg + 0xF + 1;
                    } else {
                        color_pair = 0xF0 + 1;
                    }

                    // get color byte (only lower 4 bits wired)
                    //int fg = c64.color_ram[y*40+x] & 15;
                    if (color_pair != cur_color_pair) {
                        attron(COLOR_PAIR(color_pair));
                        cur_color_pair = color_pair;
                    }

                    // invert upper half of character set
                    //if (font_code > 127) {
                    //    attron(A_REVERSE);
                    //}
                    // padding to get proper aspect ratio
                    // character
                    if (font_code & 128) {
                        mvaddstr(yy, xx*2, unicode_map[font_code&0xF]);
                        mvaddstr(yy, xx*2+1, unicode_map2[font_code&0xF]);
                    } else {
                        mvaddch(yy, xx*2, ' ');
                        mvaddch(yy, xx*2+1, chr);
                    }

                    // invert upper half of character set
                    //if (font_code > 127) {
                    //    attroff(A_REVERSE);
                    //}
            }
        }

        char pcbuf[32];
        char spbuf[32];
        char abuf[32];
        char bbuf[32];
        char xbuf[32];
        char irbuf[32];
        char spval[32];
        char fbuf[32];
        char a4buf1[32];
        char a4buf2[32];
        char a4buf3[32];
        char a4buf4[32];

        snprintf(pcbuf, sizeof(pcbuf), "PC: %04x", mp1000.cpu.PC);
        snprintf(spbuf, sizeof(spbuf), "SP: %04x", mp1000.cpu.SP);
        snprintf(spval, sizeof(spval), "[SP]: %02x%02x", mem_rd(&mp1000.mem_cpu, mp1000.cpu.SP+1), mem_rd(&mp1000.mem_cpu, mp1000.cpu.SP+2));
        snprintf(abuf, sizeof(abuf), "A: %02x", mp1000.cpu.A);
        snprintf(bbuf, sizeof(bbuf), "B: %02x", mp1000.cpu.B);
        snprintf(xbuf, sizeof(xbuf), "X: %04x", mp1000.cpu.IX);
        snprintf(irbuf, sizeof(irbuf), "IR: %04x", mp1000.cpu.IR);
        snprintf(fbuf, sizeof(fbuf), "F: %4d", frames);

        snprintf(a4buf1, sizeof(a4buf1), "%02x %02x %02x %02x %02x %02x %02x %02x", mem_rd(&mp1000.mem_cpu, 0xA400),mem_rd(&mp1000.mem_cpu, 0xA401),mem_rd(&mp1000.mem_cpu, 0xA402),mem_rd(&mp1000.mem_cpu, 0xA403),mem_rd(&mp1000.mem_cpu, 0xA404),mem_rd(&mp1000.mem_cpu, 0xA405),mem_rd(&mp1000.mem_cpu, 0xA406),mem_rd(&mp1000.mem_cpu, 0xA407));
        snprintf(a4buf2, sizeof(a4buf2), "%02x %02x %02x %02x %02x %02x %02x %02x", mem_rd(&mp1000.mem_cpu, 0xA408),mem_rd(&mp1000.mem_cpu, 0xA409),mem_rd(&mp1000.mem_cpu, 0xA40A),mem_rd(&mp1000.mem_cpu, 0xA40B),mem_rd(&mp1000.mem_cpu, 0xA40C),mem_rd(&mp1000.mem_cpu, 0xA40D),mem_rd(&mp1000.mem_cpu, 0xA40E),mem_rd(&mp1000.mem_cpu, 0xA40F));
        snprintf(a4buf3, sizeof(a4buf3), "%02x %02x %02x %02x %02x %02x %02x %02x", mem_rd(&mp1000.mem_cpu, 0xA410),mem_rd(&mp1000.mem_cpu, 0xA411),mem_rd(&mp1000.mem_cpu, 0xA412),mem_rd(&mp1000.mem_cpu, 0xA413),mem_rd(&mp1000.mem_cpu, 0xA414),mem_rd(&mp1000.mem_cpu, 0xA415),mem_rd(&mp1000.mem_cpu, 0xA416),mem_rd(&mp1000.mem_cpu, 0xA417));
        snprintf(a4buf4, sizeof(a4buf4), "%02x %02x %02x %02x %02x %02x %02x %02x", mem_rd(&mp1000.mem_cpu, 0xA418),mem_rd(&mp1000.mem_cpu, 0xA419),mem_rd(&mp1000.mem_cpu, 0xA41A),mem_rd(&mp1000.mem_cpu, 0xA41B),mem_rd(&mp1000.mem_cpu, 0xA41C),mem_rd(&mp1000.mem_cpu, 0xA41D),mem_rd(&mp1000.mem_cpu, 0xA41E),mem_rd(&mp1000.mem_cpu, 0xA41F));

        mvaddstr(4, 80, pcbuf);
        mvaddstr(5, 80, spbuf);
        mvaddstr(6, 80, spval);
        mvaddstr(7, 80, abuf);
        mvaddstr(8, 80, bbuf);
        mvaddstr(9, 80, xbuf);
        mvaddstr(10, 80, irbuf);
        mvaddstr(11, 80, fbuf);
        mvaddstr(13, 80, a4buf1);
        mvaddstr(14, 80, a4buf2);
        mvaddstr(15, 80, a4buf3);
        mvaddstr(16, 80, a4buf4);

        refresh();
        //fprintf(stderr, "c->PC: %x\n", mp1000.cpu.PC);

        // pause until next frame
        usleep(FRAME_USEC);
    }
    endwin();
    return 0;
}
