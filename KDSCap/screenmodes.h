#pragma once
#include <SDL_rect.h>

#define DS_WIDTH 256
#define DS_HEIGHT 192
#define GBA_WIDTH 240
#define GBA_HEIGHT 160

typedef enum
{
    Vertical,
    Horizontal,
    GBATop,
    GBABottom
} screen_mode;
#define NUM_SCREEN_MODES 4

SDL_Rect getSrcRectForMode(screen_mode mode);
