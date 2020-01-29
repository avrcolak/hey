#ifndef _SDL_RENDERER_H_
#define _SDL_RENDERER_H_

#include <SDL.h>
#include "windows_shims.h"

/*
 * sdl_renderer.h --
 *
 * A simple C++ renderer that uses SDL to render the game state.
 *
 */

class SDLRenderer {
public:
    SDLRenderer(SDL_Window* window);
    ~SDLRenderer();

    virtual void Draw(struct GameState const *gs, struct ConnectionReport const *connection_report);
    virtual void SetStatusText(char const *text);

protected:
    void DrawShip(int which, GameState const *gs);
    void DrawConnectState(struct Ship const *ship, struct ConnectionInfo const *info);
    void CreateFont();

    SDL_Renderer* _renderer;
    SDL_Window*   _window;
    RECT          _rc;
    char          _status[1024];
    SDL_Color     _shipColors[4];
    SDL_Color     _red;
    SDL_Color     _bullet;
};

#endif
