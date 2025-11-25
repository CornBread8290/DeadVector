#include "defs.h"

#define FONT_FIRST 32         
#define FONT_COUNT 96    
#define SHIFT_DOWN 3      

const uint8_t font_meta[FONT_COUNT/2] = {
    0x14, // 32:' ' (0), 33:'!' (1)
    0x53, // 34:'"' (3), 35:'#' (5)
    0x45, // 36:'$' (5), 37:'%' (4)
    0x15, // 38:'&' (5), 39:'\''(1)
    0x22, // 40:'(' (2), 41:')' (2)
    0x53, // 42:'*' (3), 43:'+' (5)
    0x5A, // 44:',' (shift1+width2=A), 45:'-' (5)
    0x41, // 46:'.' (1), 47:'/' (4)
    0x55, // 48:'0' (5), 49:'1' (5)
    0x45, // 50:'2' (5), 51:'3' (4)
    0x45, // 52:'4' (5), 53:'5' (4)
    0x55, // 54:'6' (5), 55:'7' (5)
    0x55, // 56:'8' (5), 57:'9' (5)
    0xA1, // 58:':' (1), 59:';' (shift1+width2=A)
    0x53, // 60:'<' (3), 61:'=' (5)
    0x53, // 62:'>' (3), 63:'?' (5)
    0x57, // 64:'@' (7), 65:'A' (5)
    0x55, // 66:'B' (5), 67:'C' (5)
    0x55, // 68:'D' (5), 69:'E' (5)   [was 0x05 before]
    0x55, // 70:'F' (5), 71:'G' (5)
    0x35, // 72:'H' (5), 73:'I' (3)
    0x45, // 74:'J' (5), 75:'K' (4)
    0x54, // 76:'L' (4), 77:'M' (5)
    0x55, // 78:'N' (5), 79:'O' (5)
    0x55, // 80:'P' (5), 81:'Q' (5)
    0x55, // 82:'R' (5), 83:'S' (5)
    0x55, // 84:'T' (5), 85:'U' (5)
    0x55, // 86:'V' (5), 87:'W' (5)
    0x55, // 88:'X' (5), 89:'Y' (5)
    0x35, // 90:'Z' (5), 91:'[' (3)
    0x34, // 92:'\\'(4), 93:']' (3)
    0x53, // 94:'^' (3), 95:'_' (5)
    0x52, // 96:'`' (2), 97:'a' (5)
    0x45, // 98:'b' (5), 99:'c' (4)
    0x45, // 100:'d'(5), 101:'e'(4)
    0xD4, // 102:'f'(4), 103:'g'(shift1+width5 = D)
    0x15, // 104:'h'(5), 105:'i'(1)
    0x4A, // 106:'j'(shift1+width2 = A), 107:'k'(4)
    0x71, // 108:'l'(1), 109:'m'(7)
    0x55, // 110:'n'(5), 111:'o'(5)
    0xCC, // 112:'p'(shift1+width4 = C), 113:'q'(shift1+width4 = C)
    0x34, // 114:'r'(4), 115:'s'(3)
    0x53, // 116:'t'(3), 117:'u'(5)
    0x75, // 118:'v'(5), 119:'w'(7)
    0xD5, // 120:'x'(5), 121:'y'(shift1+width5 = D)
    0x44, // 122:'z'(4), 123:'{'(4)
    0x41, // 124:'|'(1), 125:'}'(4)
    0x06, // 126:'~'(6), 127:DEL (0/unused)
};
const uint8_t font_bits[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,

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

    0b11111111,//E
    0b10001001,
    0b10001001,
    0b10001001,
    0b10000001,

    0b11111111,//F
    0b00001001,
    0b00001001,
    0b00001001,
    0b00000001,

    0b01111110,
    0b10000001,
    0b10000001,
    0b10010001,
    0b01110010,

    0b11111111,
    0b00001000,
    0b00001000,
    0b00001000,
    0b11111111,
    
    0b10000001,
    0b11111111,
    0b10000001,

    0b01100000,
    0b10000001,
    0b10000001,
    0b01111111,
    0b00000001,

    0b11111111,
    0b00001000,
    0b00110100,
    0b11000011,

    0b11111111,
    0b10000000,
    0b10000000,
    0b10000000,

    0b11111111,
    0b00000100,
    0b00001000,
    0b00000100,
    0b11111111,

    0b11111111,
    0b00000110,
    0b00011000,
    0b01100000,
    0b11111111,

    0b01111110,
    0b10000001,
    0b10000001,
    0b10000001,
    0b01111110,

    0b11111111,
    0b00010001,
    0b00010001,
    0b00010001,
    0b00001110,

    0b01111110,
    0b10000001,
    0b10100001,
    0b01000001,
    0b10111110,

    0b11111111,
    0b00010001,
    0b00010001,
    0b00010001,
    0b11101110,

    0b01000110,
    0b10001001,
    0b10001001,
    0b10001001,
    0b01110010,

    0b00000001,
    0b00000001,
    0b11111111,
    0b00000001,
    0b00000001,

    0b01111111,
    0b10000000,
    0b10000000,
    0b10000000,
    0b01111111,

    0b00000111,
    0b00111000,
    0b11000000,
    0b00111000,
    0b00000111,

    0b00111111,
    0b11000000,
    0b00100000,
    0b11000000,
    0b00111111,

    0b11000011,
    0b00100100,
    0b00011000,
    0b00100100,
    0b11000011,

    0b00000011,
    0b00001100,
    0b11110000,
    0b00001100,
    0b00000011,

    0b11000001,
    0b10100001,
    0b10011001,
    0b10000101,
    0b10000011,

    0b11111111,
    0b10000001,
    0b10000001,

    0b00000011,
    0b00001100,
    0b00110000,
    0b11000000,

    0b10000001,
    0b10000001,
    0b11111111,
    
    0b00000010,
    0b00000001,
    0b00000010,

    0b10000000,
    0b10000000,
    0b10000000,
    0b10000000,
    0b10000000,
    
    0b00000001,
    0b00000010,

    0b01110000,
    0b10001000,
    0b10001000,
    0b01111000,
    0b10000000,

    0b11111111,
    0b10001000,
    0b10001000,
    0b10001000,
    0b01110000,

    0b01110000,
    0b10001000,
    0b10001000,
    0b10001000,

    0b01110000,
    0b10001000,
    0b10001000,
    0b10001000,
    0b11111111,

    0b01110000,
    0b10101000,
    0b10101000,
    0b10110000,

    0b00001000,
    0b11111110,
    0b00001001,
    0b00001001,
    
    0b01001110, //g SHIFT
    0b10010001,
    0b10010001,
    0b10010001,
    0b01111111,

    0b11111111,
    0b00010000,
    0b00001000,
    0b00001000,
    0b11110000,

    0b11101000,

    0b10000000, //j SHIFT
    0b01111101,

    0b11111111,
    0b00010000,
    0b00101000,
    0b11000100,

    0b11111111,

    0b11111000,
    0b00001000,
    0b00001000,
    0b11110000,
    0b00001000,
    0b00001000,
    0b11110000,

    0b11111000,
    0b00010000,
    0b00001000,
    0b00001000,
    0b11110000,

    0b01110000,
    0b10001000,
    0b10001000,
    0b10001000,
    0b01110000,

    0b11111111, //p SHIFT
    0b00010001,
    0b00010001,
    0b00001110,
    
    0b00001110, //q SHIFT
    0b00010001,
    0b00010001,
    0b11111111,
    
    0b11111000,
    0b00010000,
    0b00001000,
    0b00001000,

    0b10010000,
    0b10101000,
    0b01001000,

    0b00001000,
    0b11111111,
    0b00001000,

    0b01111000,
    0b10000000,
    0b10000000,
    0b10000000,
    0b01111000,

    0b00011000,
    0b01100000,
    0b10000000,
    0b01100000,
    0b00011000,
    
    0b01111000,
    0b10000000,
    0b10000000,
    0b01111000,
    0b10000000,
    0b10000000,
    0b01111000,
    
    0b10001000,
    0b01010000,
    0b00100000,
    0b01010000,
    0b10001000,

    0b10000011, //y SHIFT
    0b01001100,
    0b00110000,
    0b00001100,
    0b00000011,

    0b11001000,
    0b10101000,
    0b10101000,
    0b10011000,

    0b00010000,
    0b01101100,
    0b10000010,
    0b10000010,

    0b11111111,

    0b10000010,
    0b10000010,
    0b01101100,
    0b00010000,
    
    0b00100000,
    0b00010000,
    0b00010000,
    0b00100000,
    0b00100000,
    0b00010000,




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
        if (!w) { continue; }

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
        pen += w + 1;
    }
    return pen - x;
}



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

// Static GL objects
static GLuint hud_vao = 0, hud_vbo = 0, hud_prog = 0, hud_tex = 0;
static GLint  u_screen_size = -1, u_pos_size = -1, u_sampler0 = -1, u_bg_rgba = -1;


static unsigned char* hud_buf = NULL;
static int hud_w = 512, hud_h = 64;
static int hud_dirty = 0;

static const char* HUD_VS =
"#version 330 core\n"
"layout(location=0)in vec2 a;layout(location=1)in vec2 b;"
"uniform vec2 u_screen_size;uniform vec4 u_pos_size;out vec2 v;"
"void main(){vec2 n=(u_pos_size.xy+a*u_pos_size.zw)/u_screen_size*2.0-1.0;"
"gl_Position=vec4(n.x,-n.y,0.0,1.0);v=b;}";

static const char* HUD_FS =
"#version 330 core\n"
"in vec2 v;out vec4 f;uniform sampler2D u_tex0;uniform vec4 u_bg_rgba;"
"void main(){vec4 b=u_bg_rgba;float a=texture(u_tex0,v).r;"
"f=vec4(mix(b.rgb,vec3(1.0),a),max(b.a,a));}";

static GLuint hud_compile(GLenum type, const char* src){
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok=0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){ glDeleteShader(s); return 0; }
    return s;
}
static GLuint hud_link(GLuint vs, GLuint fs){
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){ glDeleteProgram(p); return 0; }
    return p;
}

static void hud_upload_if_dirty(void){
    if (!hud_dirty || !hud_tex) return;
    glBindTexture(GL_TEXTURE_2D, hud_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, hud_w, hud_h, GL_RED, GL_UNSIGNED_BYTE, hud_buf);
    glBindTexture(GL_TEXTURE_2D, 0);
    hud_dirty = 0;
}

// Clear the mono buffer to zero (transparent)
void hud_clear(void){
    if (!hud_buf) return;
    memset(hud_buf, 0, (size_t)hud_w * (size_t)hud_h);
    hud_dirty = 1;
}

void hud_draw_string(int x, int y, const char* s){
    if (!hud_buf || !s) return;
    font_draw_text(hud_buf, hud_w, x, y, s);
    hud_dirty = 1;
}

void hud_init_minimal(void){
    if (hud_vao) return; // already initialized

    // Compile program
    GLuint vs = hud_compile(GL_VERTEX_SHADER,   HUD_VS);
    GLuint fs = hud_compile(GL_FRAGMENT_SHADER, HUD_FS);
    hud_prog = hud_link(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);


    // in hud_init_minimal(), after linking the program:
    u_screen_size = glGetUniformLocation(hud_prog, "u_screen_size");
    u_pos_size    = glGetUniformLocation(hud_prog, "u_pos_size");
    u_sampler0    = glGetUniformLocation(hud_prog, "u_tex0");
    u_bg_rgba     = glGetUniformLocation(hud_prog, "u_bg_rgba");

    // Allocate mono buffer
    if (!hud_buf) hud_buf = (unsigned char*)calloc((size_t)hud_w * (size_t)hud_h, 1);

    const float verts[] = {
        // x, y,  u, v
         0, 0,   0, 0,
         1, 0,   1, 0,
         1, 1,   1, 1,
         0, 0,   0, 0,
         1, 1,   1, 1,
         0, 1,   0, 1,
    };
    glGenVertexArrays(1, &hud_vao);
    glGenBuffers(1, &hud_vbo);
    glBindVertexArray(hud_vao);
    glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(sizeof(float)*2));
    glBindVertexArray(0);

    // Texture: GL_R8, nearest sampling
    glGenTextures(1, &hud_tex);
    glBindTexture(GL_TEXTURE_2D, hud_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, hud_w, hud_h, 0, GL_RED, GL_UNSIGNED_BYTE, hud_buf);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    hud_dirty = 1;
}

void draw_hud(void){
    if (!hud_vao) hud_init_minimal();

    // Upload texture if buffer changed
    hud_upload_if_dirty();

    GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
    float W = (float)vp[2], H = (float)vp[3];

    float x = 88.0f, y = 58.0f, w = (float)hud_w, h = (float)hud_h;

    // Minimal state setup
    GLboolean blend_was = glIsEnabled(GL_BLEND);
    GLint last_prog=0, last_active=0, last_tex2D=0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_prog);
    glGetIntegerv(GL_ACTIVE_TEXTURE,  &last_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(hud_prog);
    glUniform2f(u_screen_size, W, H);
    glUniform4f(u_pos_size, x, y, w, h);
    glUniform1i(u_sampler0, 0);

    glUniform4f(u_bg_rgba, 0.0f, 0.0f, 0.0f, 1.0f);

    glBindVertexArray(hud_vao);
    glBindTexture(GL_TEXTURE_2D, hud_tex);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Restore a little state
    glBindTexture(GL_TEXTURE_2D, (GLuint)last_tex2D);
    glUseProgram((GLuint)last_prog);
    if (!blend_was) glDisable(GL_BLEND);
    glActiveTexture((GLenum)last_active);
}
