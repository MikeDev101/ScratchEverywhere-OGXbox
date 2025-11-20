#pragma once
#if defined(_XBOX) || defined(__XBOX__)
    #include <SDL.h>
#else
    #include <SDL2/SDL.h>
#endif
#ifdef ENABLE_AUDIO
#include <SDL_mixer.h>
#endif
#include <SDL_ttf.h>

extern int windowWidth;
extern int windowHeight;
extern SDL_Window *window;
extern SDL_Renderer *renderer;

std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight);
