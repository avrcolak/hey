#include <SDL.h>
#include <stdio.h>
#include <math.h>
#include "windows.h"
#include "vectorwar.h"
#include "sdl_renderer.h"

#define  PROGRESS_BAR_WIDTH        100
#define  PROGRESS_BAR_TOP_OFFSET    22
#define  PROGRESS_BAR_HEIGHT         8
#define  PROGRESS_TEXT_OFFSET       (PROGRESS_BAR_TOP_OFFSET + PROGRESS_BAR_HEIGHT + 4)

#define  PROGRESS_BAR_WIDTH        100
#define  PROGRESS_BAR_TOP_OFFSET    22
#define  PROGRESS_BAR_HEIGHT         8
#define  PROGRESS_TEXT_OFFSET       (PROGRESS_BAR_TOP_OFFSET + PROGRESS_BAR_HEIGHT + 4)

SDL_Rect ToSDLRect(RECT rect)
{
	SDL_Rect sdl_rect;

	sdl_rect.x = rect.left;
	sdl_rect.y = rect.top;
	sdl_rect.w = rect.right - rect.left;
	sdl_rect.h = rect.bottom - rect.top;

	return sdl_rect;
}

SDLRenderer::SDLRenderer(SDL_Window* window) :
	_window(window)
{
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

	_renderer = SDL_CreateRenderer(window, -1, 0);

	*_status = '\0';

	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	_rc.right = w;
	_rc.bottom = h;

	_shipColors[0] = { 255, 0, 0, SDL_ALPHA_OPAQUE };
	_shipColors[1] = { 0, 255, 0, SDL_ALPHA_OPAQUE };
	_shipColors[2] = { 0, 0, 255, SDL_ALPHA_OPAQUE };
	_shipColors[3] = { 128, 128, 128, SDL_ALPHA_OPAQUE };

	_red = { 255, 0, 0, SDL_ALPHA_OPAQUE };
	_bullet = { 255, 192, 0, SDL_ALPHA_OPAQUE };
}

SDLRenderer::~SDLRenderer()
{
	SDL_DestroyRenderer(_renderer);
}

void
SDLRenderer::Draw(GameState& gs, NonGameState& ngs)
{
	SDL_SetRenderDrawColor(_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(_renderer);

	SDL_Rect bounds = ToSDLRect(gs._bounds);

	SDL_SetRenderDrawColor(_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(_renderer, &bounds);

	for (int i = 0; i < gs._num_ships; i++) {
		SDL_SetRenderDrawColor(_renderer, _shipColors[i].r, _shipColors[i].g, _shipColors[i].b, _shipColors[i].a);
		DrawShip(i, gs);
		DrawConnectState(gs._ships[i], ngs.players[i]);
	}

	SDL_RenderFlush(_renderer);
	RenderChecksum(40, ngs.periodic);
}

void
SDLRenderer::RenderChecksum(int y, NonGameState::ChecksumInfo& info)
{
	char checksum[128];
	sprintf_s(checksum, ARRAYSIZE(checksum), "Frame: %04d  Checksum: %08x", info.framenumber, info.checksum);
}

void
SDLRenderer::SetStatusText(const char* text)
{
	strcpy_s(_status, text);
}

void
SDLRenderer::DrawShip(int which, GameState& gs)
{
	Ship* ship = gs._ships + which;
	RECT bullet = { 0 };
	SDL_Point shape[] = {
	   { SHIP_RADIUS,           0 },
	   { -SHIP_RADIUS,          SHIP_WIDTH },
	   { SHIP_TUCK - SHIP_RADIUS, 0 },
	   { -SHIP_RADIUS,          -SHIP_WIDTH },
	   { SHIP_RADIUS,           0 },
	};
	SDL_Point text_offsets[] = {
	   { gs._bounds.left + 2, gs._bounds.top + 2 },
	   { gs._bounds.right - 2, gs._bounds.top + 2 },
	   { gs._bounds.left + 2, gs._bounds.bottom - 2 },
	   { gs._bounds.right - 2, gs._bounds.bottom - 2 },
	};
	char buf[32];
	int i;

	for (i = 0; i < ARRAYSIZE(shape); i++) {
		double newx, newy;
		double cost, sint, theta;

		theta = (double)ship->heading * PI / 180;
		cost = ::cos(theta);
		sint = ::sin(theta);

		newx = shape[i].x * cost - shape[i].y * sint;
		newy = shape[i].x * sint + shape[i].y * cost;

		shape[i].x = newx + ship->position.x;
		shape[i].y = newy + ship->position.y;
	}
	SDL_RenderDrawLines(_renderer, shape, 5);

	for (i = 0; i < MAX_BULLETS; i++) {
		if (ship->bullets[i].active) {
			bullet.left = ship->bullets[i].position.x - 1;
			bullet.right = ship->bullets[i].position.x + 1;
			bullet.top = ship->bullets[i].position.y - 1;
			bullet.bottom = ship->bullets[i].position.y + 1;

			SDL_Rect rect = ToSDLRect(bullet);

			SDL_SetRenderDrawColor(_renderer, _bullet.r, _bullet.g, _bullet.b, _bullet.a);
			SDL_RenderFillRect(_renderer, &rect);
		}
	}
	sprintf_s(buf, ARRAYSIZE(buf), "Hits: %d", ship->score);
}

void
SDLRenderer::DrawConnectState(Ship& ship, PlayerConnectionInfo& info)
{
	char status[64];
	static const char* statusStrings[] = {
	   "Connecting...",
	   "Synchronizing...",
	   "",
	   "Disconnected.",
	};
	int progress = -1;

	*status = '\0';
	switch (info.state) {
	case Connecting:
		sprintf_s(status, ARRAYSIZE(status), (info.type == GGPO_PLAYERTYPE_LOCAL) ? "Local Player" : "Connecting...");
		break;

	case Synchronizing:
		progress = info.connect_progress;
		sprintf_s(status, ARRAYSIZE(status), (info.type == GGPO_PLAYERTYPE_LOCAL) ? "Local Player" : "Synchronizing...");
		break;

	case Disconnected:
		sprintf_s(status, ARRAYSIZE(status), "Disconnected");
		break;

	case Disconnecting:
		sprintf_s(status, ARRAYSIZE(status), "Waiting for player...");
		progress = (SDL_GetTicks() - info.disconnect_start) * 100 / info.disconnect_timeout;
		break;
	}

	if (*status) {

	}
	if (progress >= 0) {
		SDL_Color bar;
		if (info.state == Synchronizing)
		{
			bar = { 255, 255, 255, SDL_ALPHA_OPAQUE };
		}
		else
		{
			bar = _red;
		}
		RECT rc = { (LONG)(ship.position.x - (PROGRESS_BAR_WIDTH / 2)),
					(LONG)(ship.position.y + PROGRESS_BAR_TOP_OFFSET),
					(LONG)(ship.position.x + (PROGRESS_BAR_WIDTH / 2)),
					(LONG)(ship.position.y + PROGRESS_BAR_TOP_OFFSET + PROGRESS_BAR_HEIGHT) };
		SDL_Rect rect = ToSDLRect(rc);

		SDL_SetRenderDrawColor(_renderer, 128, 128, 128, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(_renderer, &rect);

		rc.right = rc.left + min(100, progress) * PROGRESS_BAR_WIDTH / 100;
		InflateRect(&rc, -1, -1);
		rect = ToSDLRect(rc);

		SDL_SetRenderDrawColor(_renderer, bar.r, bar.g, bar.b, SDL_ALPHA_OPAQUE);
		SDL_RenderFillRect(_renderer, &rect);
	}
}
