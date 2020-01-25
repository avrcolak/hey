#ifndef _SDL_RENDERER_H_
#define _SDL_RENDERER_H_

#include "renderer.h"

/*
 * sdl_renderer.h --
 *
 * A simple C++ renderer that uses SDL to render the game state.
 *
 */

class SDLRenderer : public Renderer {
public:
	SDLRenderer(SDL_Window* window);
	~SDLRenderer();

	virtual void Draw(GameState& gs, NonGameState& ngs);
	virtual void SetStatusText(const char* text);

protected:
	void RenderChecksum(int y, NonGameState::ChecksumInfo& info);
	void DrawShip(int which, GameState& gamestate);
	void DrawConnectState(Ship& ship, PlayerConnectionInfo& info);
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
