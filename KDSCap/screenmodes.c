#include "screenmodes.h"

SDL_Rect getSrcRectForMode(screen_mode mode)
{
    SDL_Rect rect;

    switch (mode)
    {
        case Vertical:
            rect.x = 0;
            rect.y = 0;
            rect.w = DS_WIDTH;
            rect.h = DS_HEIGHT * 2;
            break;
        case Horizontal:
            rect.x = 0;
            rect.y = 0;
            rect.w = DS_WIDTH;
            rect.h = DS_HEIGHT;
            break;
        case GBATop:
            rect.x = (DS_WIDTH - GBA_WIDTH) / 2;
            rect.y = (DS_HEIGHT - GBA_HEIGHT) / 2;
            rect.w = GBA_WIDTH;
            rect.h = GBA_HEIGHT;
            break;
        case GBABottom:
            rect.x = (DS_WIDTH - GBA_WIDTH) / 2;
            rect.y = DS_HEIGHT + (DS_HEIGHT - GBA_HEIGHT) / 2;
            rect.w = GBA_WIDTH;
            rect.h = GBA_HEIGHT;
            break;
        default:
            rect.x = 0;
            rect.y = 0;
            rect.w = DS_WIDTH;
            rect.h = DS_HEIGHT * 2;
            break;
    }

    return rect;
}
