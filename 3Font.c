#include "defs.h"

#define FONT_FIRST 32          // ASCII ' '
#define FONT_COUNT 96          // printable ASCII 32..127
#define SHIFT_DOWN 3           // draw descenders 3px lower

const uint8_t font_meta[FONT_COUNT/2] = {
    0x10, // 32:' ' (0), 33:'!' (1)
    0x53, // 34:'"' (3), 35:'#' (5)
    0x45, // 36:'$' (5), 37:'%' (4)
    0x15, // 38:'&' (5), 39:'\''(1)
    0x22, // 40:'(' (2), 41:')' (2)
    0x53, // 42:'*' (3), 43:'+' (5)
    0x5A, // 44:',' (A=shift+width2), 45:'-' (5)
    0x41, // 46:'.' (1), 47:'/' (4)
    0x55, // 48:'0' (5), 49:'1' (5)
    0x45, // 50:'2' (5), 51:'3' (4)
    0x45, // 52:'4' (5), 53:'5' (4)
    0x55, // 54:'6' (5), 55:'7' (5)
    0x55, // 56:'8' (5), 57:'9' (5)
    0xA1, // 58:':' (1), 59:';' (A=shift+width2)
    0x53, // 60:'<' (3), 61:'=' (5)
    0x53, // 62:'>' (3), 63:'?' (5)
    0x57, // 64:'@' (7), 65:'A' (5)
    0x55, // 66:'B' (5), 67:'C' (5)
    0x05, // 68:'D' (5), 69:'E' (0 - not filled yet)
};

const uint8_t font_bits[] = {
    0b10011111,//!

    0b00000111,//"
    0b00000000, 
    0b00000111,

    0b00100100, //#
    0b11111111,
    0b00100100,
    0b11111111,
    0b00100100,

    0b01000100, //$
    0b01001010,
    0b11111111,
    0b01001010,
    0b00110010,

    0b11000011, //%
    0b00110000,
    0b00001100,
    0b11000011,

    0b01110110, //&
    0b10001001,
    0b10001001,
    0b01111100,
    0b10001000,

    0b00000011, //'

    0b01111110, //(
    0b10000001,

    0b10000001,//)
    0b01111110,

    0b00001010, //*
    0b00000100,
    0b00001010,

    0b00001000,//+
    0b00001000,
    0b00111110,
    0b00001000,
    0b00001000,

    0b00100000,//, Shift
    0b00011000,

    0b00001000,//-
    0b00001000,
    0b00001000,
    0b00001000,
    0b00001000,

    0b11000000,//.

    0b11000000,///
    0b00110000,
    0b00001100,
    0b00000011,

    0b01111110,//0
    0b10000001,
    0b10000001,
    0b10000001,
    0b01111110,

    0b10000100,//1
    0b10000010,
    0b11111111,
    0b10000000,
    0b10000000,

    0b10000010,//2
    0b11000001,
    0b10100001,
    0b10010001,
    0b10001110,

    0b10001001,//3
    0b10001001,
    0b10001001,
    0b01110110,

    0b00001111,//4
    0b00001000,
    0b00001000,
    0b00001000,
    0b11111111,

    0b10001111,//5
    0b10001001,
    0b10001001,
    0b01110001,

    0b01111100,//6
    0b10001010,
    0b10001001,
    0b10001001,
    0b01110000,

    0b10000001,
    0b01100001,
    0b00010001,
    0b00001101,
    0b00000011,

    0b01110110,
    0b10001001,
    0b10001001,
    0b10001001,
    0b01110110,

    0b00001110,
    0b00010001,
    0b00010001,
    0b00010001,
    0b11111110,

    0b01000100,

    0b00100000, //; Shift
    0b00011000,

    0b00001000,
    0b00010100,
    0b00100010,

    0b00100100,
    0b00100100,
    0b00100100,
    0b00100100,
    0b00100100,

    0b00100010,
    0b00010100,
    0b00001000,

    0b00000010,
    0b00000001,
    0b10110001,
    0b00001001,
    0b00000110,

    0b01111100,
    0b10000010,
    0b10010010,
    0b10101010,
    0b10111010,
    0b10100010,
    0b10011100,

    0b11110000,
    0b00101100,
    0b00100011,
    0b00101100,
    0b11110000,

    0b11111111,
    0b10001001,
    0b10001001,
    0b10001001,
    0b01110110,

    0b01111110,
    0b10000001,
    0b10000001,
    0b10000001,
    0b01000010,

    0b11111111,//D
    0b10000001,
    0b10000001,
    0b10000001,
    0b01111110,
};

int font_draw_text(uint8_t* dst, int pitch, int x, int y, const char* s) {
    int pen = x;
    const unsigned char* p = (const unsigned char*)s;

    while (*p) {
        unsigned c = *p++;
        if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT) continue;
        int gi = (int)c - FONT_FIRST;

        uint8_t mb = font_meta[gi >> 1];
        uint8_t nib = (gi & 1) ? (uint8_t)(mb >> 4) : (uint8_t)(mb & 0x0F);
        int w = nib & 7;
        if (!w) { pen += 0; continue; }

        int y0 = y + ((nib >> 3) ? SHIFT_DOWN : 0);

        unsigned off = 0;
        for (int g = 0; g < gi; g++) {
            uint8_t mb2 = font_meta[g >> 1];
            uint8_t nb2 = (g & 1) ? (uint8_t)(mb2 >> 4) : (uint8_t)(mb2 & 0x0F);
            off += (nb2 & 7);
        }

        const uint8_t* src = font_bits + off;
        for (int cx = 0; cx < w; cx++) {
            uint8_t col = *src++;
            for (int r = 0; r < 8; r++) {
                if (col & (1u << r)) {
                    dst[(y0 + r) * pitch + (pen + cx)] = 255;
                }
            }
        }
        pen += w;
    }
    return pen - x;
}



extern const uint8_t font_meta[];
int font_draw_text(uint8_t* dst, int pitch, int x, int y, const char* s);

static inline uint8_t meta_nib(int gi) {
    uint8_t b = font_meta[gi >> 1];
    return (gi & 1) ? (uint8_t)(b >> 4) : (uint8_t)(b & 0x0F);
}

#define FONT_FIRST 32
#define FONT_COUNT 96

static int font_text_w(const char* s) {
    int w = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
        int gi = (int)*p - FONT_FIRST;
        if ((unsigned)gi < FONT_COUNT) w += (meta_nib(gi) & 7);
    }
    return w;
}