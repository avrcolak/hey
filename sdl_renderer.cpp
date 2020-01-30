#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "connection_report.h"
#include "gamestate.h"
#include "sdl_renderer.h"
#include "utils.h"

#define  PROGRESS_BAR_WIDTH        100
#define  PROGRESS_BAR_TOP_OFFSET    22
#define  PROGRESS_BAR_HEIGHT         8
#define  PROGRESS_TEXT_OFFSET       (PROGRESS_BAR_TOP_OFFSET + PROGRESS_BAR_HEIGHT + 4)

#define  PROGRESS_BAR_WIDTH        100
#define  PROGRESS_BAR_TOP_OFFSET    22
#define  PROGRESS_BAR_HEIGHT         8
#define  PROGRESS_TEXT_OFFSET       (PROGRESS_BAR_TOP_OFFSET + PROGRESS_BAR_HEIGHT + 4)

SDLRenderer::SDLRenderer(SDL_Window* window)
{
	_window = window;

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

	_renderer = SDL_CreateRenderer(window, -1, 0);

	*_status = '\0';

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
SDLRenderer::Draw(GameState const* gs, ConnectionReport const* connection_report)
{
	SDL_SetRenderDrawColor(_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(_renderer);

	SDL_Rect bounds =
	{
		gs->_bounds.left,
		gs->_bounds.top,
		gs->_bounds.right - gs->_bounds.left,
		gs->_bounds.bottom - gs->_bounds.top
	};

	SDL_SetRenderDrawColor(_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(_renderer, &bounds);

	for (int i = 0; i < gs->_num_ships; i++) {
		SDL_SetRenderDrawColor(_renderer, _shipColors[i].r, _shipColors[i].g, _shipColors[i].b, _shipColors[i].a);
		DrawShip(i, gs);
		DrawConnectState(&gs->_ships[i], &connection_report->players[i]);
	}

	SDL_RenderFlush(_renderer);
}

void
SDLRenderer::SetStatusText(char const* text)
{
	strcpy(_status, text);
}

void
SDLRenderer::DrawShip(int which, GameState const* gs)
{
	Ship const* ship = gs->_ships + which;

	SDL_Point shape[] = {
	   { SHIP_RADIUS,           0 },
	   { -SHIP_RADIUS,          SHIP_WIDTH },
	   { SHIP_TUCK - SHIP_RADIUS, 0 },
	   { -SHIP_RADIUS,          -SHIP_WIDTH },
	   { SHIP_RADIUS,           0 },
	};

	SDL_Point text_offsets[] = {
	   { gs->_bounds.left + 2, gs->_bounds.top + 2 },
	   { gs->_bounds.right - 2, gs->_bounds.top + 2 },
	   { gs->_bounds.left + 2, gs->_bounds.bottom - 2 },
	   { gs->_bounds.right - 2, gs->_bounds.bottom - 2 },
	};

	for (int i = 0; i < ARRAYSIZE(shape); i++) {
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

	for (int i = 0; i < MAX_BULLETS; i++) {
		if (ship->bullets[i].active) {
			SDL_Rect rect =
			{
				ship->bullets[i].position.x - 1,
				ship->bullets[i].position.y - 1,
				2,
				2
			};

			SDL_SetRenderDrawColor(_renderer, _bullet.r, _bullet.g, _bullet.b, _bullet.a);
			SDL_RenderFillRect(_renderer, &rect);
		}
	}

	char buf[32];
	sprintf_s(buf, ARRAYSIZE(buf), "Hits: %d", ship->score);
}

void
SDLRenderer::DrawConnectState(Ship const* ship, ConnectionInfo const* info)
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
	switch (info->state) {
	case CONNECTION_STATE_Connecting:
		sprintf_s(status, ARRAYSIZE(status), (info->type == PLAYER_TYPE_Local) ? "Local Player" : "Connecting...");
		break;

	case CONNECTION_STATE_Synchronizing:
		progress = info->connect_progress;
		sprintf_s(status, ARRAYSIZE(status), (info->type == PLAYER_TYPE_Local) ? "Local Player" : "Synchronizing...");
		break;

	case CONNECTION_STATE_Disconnected:
		sprintf_s(status, ARRAYSIZE(status), "Disconnected");
		break;

	case CONNECTION_STATE_Disconnecting:
		sprintf_s(status, ARRAYSIZE(status), "Waiting for player...");
		progress = (SDL_GetTicks() - info->disconnect_start) * 100 / info->disconnect_timeout;
		break;
	}

	if (*status) {

	}
	if (progress >= 0) {
		SDL_Color bar;
		if (info->state == CONNECTION_STATE_Synchronizing) {
			bar = { 255, 255, 255, SDL_ALPHA_OPAQUE };
		}
		else {
			bar = _red;
		}

		SDL_Rect rc = { (int)(ship->position.x - (PROGRESS_BAR_WIDTH / 2)),
						(int)(ship->position.y + PROGRESS_BAR_TOP_OFFSET),
						(int)PROGRESS_BAR_WIDTH,
						(int)PROGRESS_BAR_HEIGHT };

		SDL_SetRenderDrawColor(_renderer, 128, 128, 128, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(_renderer, &rc);

		rc.w = min(100, progress) * PROGRESS_BAR_WIDTH / 100;
		rc = { rc.x + 1, rc.y + 1, rc.w - 1, rc.h - 1 };

		SDL_SetRenderDrawColor(_renderer, bar.r, bar.g, bar.b, SDL_ALPHA_OPAQUE);
		SDL_RenderFillRect(_renderer, &rc);
	}
}
