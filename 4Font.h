#ifndef FONT_H
#define FONT_H

#include <stdint.h>

int font_draw_text(uint8_t* dst, int pitch, int x, int y, const char* s);
void hud_init_minimal(void);
void hud_clear(void);
void hud_draw_string(int x, int y, const char* s);
void draw_hud(void);
void hud_fill_rect(int x, int y, int w, int h, uint8_t v);

#endif // FONT_H
