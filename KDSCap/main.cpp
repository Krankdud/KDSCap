#include <SDL.h>
#include <cstdio>
#include <stdexcept>
#include <mutex>
#include "audiorecorder.h"
#include "videoencoder.h"
#include "win_dscapture.h"
#include "screenmodes.h"

const int SCREEN_WIDTH = DS_WIDTH;
const int SCREEN_HEIGHT = DS_HEIGHT * 2;

bool init(SDL_Window** window, SDL_Renderer** renderer, SDL_Texture** texture);
void captureFrame(DSCapture& capture, VideoEncoder& encoder, uint16_t* dsFrameBuffer, SDL_Texture* texture);
void destroyAll(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture);
void resizeWindow(SDL_Window* window, int scale, screen_mode mode);
void setWindowTitle(SDL_Window* window, screen_mode mode, unsigned int framerate);

int main(int argc, char* argv[])
{
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Texture* texture = NULL;
    SDL_Event event;
    uint16_t* dsFrameBuffer = new uint16_t[DS_WIDTH * DS_HEIGHT * 2];
    bool quit = false;
    int scale = 1;
    screen_mode screenMode = Vertical;
    unsigned int frameCounter = 0;
    unsigned int lastTime = 0;
    unsigned int currentTime;
    
    DSCapture dscapture;

    if (!init(&window, &renderer, &texture))
    {
        return 1;
    }

    VideoEncoder videoEncoder;
    AudioRecorder audioRecorder;
    audioRecorder.start();

    dscapture.startCapture();

    while (!quit)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.repeat) continue;

                switch (event.key.keysym.sym)
                {
                    case SDLK_1:
                        scale = 1;
                        resizeWindow(window, scale, screenMode);
                        break;
                    case SDLK_2:
                        scale = 2;
                        resizeWindow(window, scale, screenMode);
                        break;
                    case SDLK_3:
                        scale = 3;
                        resizeWindow(window, scale, screenMode);
                        break;
                    case SDLK_4:
                        scale = 4;
                        resizeWindow(window, scale, screenMode);
                        break;
                    case SDLK_m:
                        screenMode = static_cast<screen_mode>((screenMode + 1) % NUM_SCREEN_MODES);
                        resizeWindow(window, scale, screenMode);
                        break;
                }
            }
        }

        captureFrame(dscapture, videoEncoder, dsFrameBuffer, texture);

        SDL_Rect src = getSrcRectForMode(screenMode);
        if (screenMode == Horizontal)
        {
            SDL_Rect dest;
            dest.x = dest.y = 0;
            dest.w = DS_WIDTH * scale;
            dest.h = DS_HEIGHT * scale;
            SDL_RenderCopy(renderer, texture, &src, &dest);
            src.y += DS_HEIGHT;
            dest.x += DS_WIDTH * scale;
            SDL_RenderCopy(renderer, texture, &src, &dest);
        }
        else
        {
            SDL_RenderCopy(renderer, texture, &src, NULL);
        }
        SDL_RenderPresent(renderer);

        ++frameCounter;
        currentTime = SDL_GetTicks();
        if (currentTime - lastTime >= 1000)
        {
            setWindowTitle(window, screenMode, frameCounter);
            frameCounter = 0;
            lastTime = currentTime;
        }
    }

    audioRecorder.stop();
    dscapture.endCapture();

    destroyAll(window, renderer, texture);

    delete [] dsFrameBuffer;

    SDL_Quit();
    return 0;
}

// Initializes SDL and DS capture device.
// Returns false if an error occurred.
bool init(SDL_Window ** window, SDL_Renderer ** renderer, SDL_Texture ** texture)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        printf("Could not initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    *window = SDL_CreateWindow("KDSCap - Vertical DS", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (*window == NULL)
    {
        printf("Could not create window: %s\n", SDL_GetError());
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (*renderer == NULL)
    {
        printf("Could not create renderer: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    *texture = SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, DS_WIDTH, DS_HEIGHT * 2);
    if (*texture == NULL)
    {
        printf("Could not create texture: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void captureFrame(DSCapture& capture, VideoEncoder& encoder, uint16_t* dsFrameBuffer, SDL_Texture* texture)
{
    static void* pixels;
    static int pitch;

    capture.grabFrame(dsFrameBuffer);
    encoder.sendFrame(dsFrameBuffer);

    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    memcpy(pixels, dsFrameBuffer, DS_WIDTH * DS_HEIGHT * 2 * 2);
    SDL_UnlockTexture(texture);
}

// Destroys SDL objects
void destroyAll(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture)
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

void resizeWindow(SDL_Window* window, int scale, screen_mode mode)
{
    switch (mode)
    {
        case Vertical:
            SDL_SetWindowSize(window, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale);
            break;
        case Horizontal:
            SDL_SetWindowSize(window, DS_WIDTH * 2 * scale, DS_HEIGHT * scale);
            break;
        case GBABottom:
            SDL_SetWindowSize(window, GBA_WIDTH * scale, GBA_HEIGHT * scale);
            break;
        case GBATop:
            SDL_SetWindowSize(window, GBA_WIDTH * scale, GBA_HEIGHT * scale);
            break;
    }
}

void setWindowTitle(SDL_Window* window, screen_mode mode, unsigned int framerate)
{
    char buffer[80];
    switch (mode)
    {
        case Vertical:
            snprintf(buffer, 80, "KDSCap - Vertical DS - %d fps", framerate);
            break;
        case Horizontal:
            snprintf(buffer, 80, "KDSCap - Horizontal DS - %d fps", framerate);
            break;
        case GBABottom:
            snprintf(buffer, 80, "KDSCap - GBA Bottom - %d fps", framerate);
            break;
        case GBATop:
            snprintf(buffer, 80, "KDSCap - GBA Top - %d fps", framerate);
            break;
    }
    SDL_SetWindowTitle(window, buffer);
}
