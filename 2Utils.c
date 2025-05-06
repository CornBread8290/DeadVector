#include "defs.h"
static uint32_t seed = 12355;

uint32_t xorshift32() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}
float xorshift32f() {
    return (float)xorshift32() / 4294967295.0f;
}
float meTanf(float num){
    return sin(num) / cos(num);
}