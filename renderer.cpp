#include <imgui.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game_state.h"
#include "connection_report.h"
#include "renderer.h"
#include "utils.h"

#define  PROGRESS_BAR_WIDTH        100
#define  PROGRESS_BAR_TOP_OFFSET    22
#define  PROGRESS_BAR_HEIGHT         8
#define  PROGRESS_TEXT_OFFSET       (PROGRESS_BAR_TOP_OFFSET + PROGRESS_BAR_HEIGHT + 4)

#define  PROGRESS_BAR_WIDTH        100
#define  PROGRESS_BAR_TOP_OFFSET    22
#define  PROGRESS_BAR_HEIGHT         8
#define  PROGRESS_TEXT_OFFSET       (PROGRESS_BAR_TOP_OFFSET + PROGRESS_BAR_HEIGHT + 4)

SDL_Color ship_colors[4] =
{
	{ 255, 0, 0, SDL_ALPHA_OPAQUE },
	{ 0, 255, 0, SDL_ALPHA_OPAQUE },
	{ 0, 0, 255, SDL_ALPHA_OPAQUE },
	{ 128, 128, 128, SDL_ALPHA_OPAQUE },
};

SDL_Color black = { 0, 0, 0, SDL_ALPHA_OPAQUE };
SDL_Color grey = { 128, 128, 128, SDL_ALPHA_OPAQUE };
SDL_Color white = { 255, 255, 255, SDL_ALPHA_OPAQUE };
SDL_Color red = { 255, 0, 0, SDL_ALPHA_OPAQUE };
SDL_Color safety_yellow = { 255, 192, 0, SDL_ALPHA_OPAQUE };

static void set_draw_color(SDL_Renderer *renderer, SDL_Color color)
{
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void draw_ship(SDL_Renderer *renderer, int which, GameState const *gs)
{
	Ship const *ship = gs->ships + which;

	SDL_Point shape[] =
	{
		{  SHIP_RADIUS,             0 },
		{ -SHIP_RADIUS,             SHIP_WIDTH },
		{  SHIP_TUCK - SHIP_RADIUS, 0 },
		{ -SHIP_RADIUS,            -SHIP_WIDTH },
		{  SHIP_RADIUS,             0 },
	};

	for (int i = 0; i < COUNT_OF(shape); i++)
	{
		double newx, newy;
		double cost, sint, theta;

		theta = (double)ship->heading * PI / 180;
		cost = ::cos(theta);
		sint = ::sin(theta);

		newx = shape[i].x * cost - shape[i].y * sint;
		newy = shape[i].x * sint + shape[i].y * cost;

		shape[i].x = (int)(newx + ship->position.x);
		shape[i].y = (int)(newy + ship->position.y);
	}

	SDL_RenderDrawLines(renderer, shape, 5);

	for (int i = 0; i < MAX_BULLETS; i++)
	{
		if (ship->bullets[i].active)
		{
			SDL_Rect rect =
			{
				(int)ship->bullets[i].position.x - 1,
				(int)ship->bullets[i].position.y - 1,
				2,
				2
			};

			set_draw_color(renderer, safety_yellow);
			SDL_RenderFillRect(renderer, &rect);
		}
	}

	SDL_Point text_offsets[] = 
	{
		{ gs->bounds.left + 2, gs->bounds.top + 2 },
		{ gs->bounds.right - 2, gs->bounds.top + 2 },
		{ gs->bounds.left + 2, gs->bounds.bottom - 2 },
		{ gs->bounds.right - 2, gs->bounds.bottom - 2 },
	};

	char buf[32];
	sprintf_s(buf, sizeof buf, "Hits: %d", ship->score);

	ImVec2 text_size = ImGui::CalcTextSize(buf);

	int ya[] = { 0, 0, -1, -1 };
	int xa[] = { 0, -1, 0, -1 };

	ImGui::GetForegroundDrawList()->AddText(
		ImVec2(
			text_offsets[which].x + text_size.x * xa[which],
			text_offsets[which].y + text_size.y * ya[which]),
		IM_COL32_WHITE,
		buf);
}

void draw_connect_state(
	SDL_Renderer *renderer, Ship const *ship, ConnectionInfo const *info)
{
	char status[64];
	*status = '\0';
	int progress = -1;
	bool local = info->type == PARTICIPANT_TYPE_local;

	switch (info->state) {
	case CONNECTION_STATE_connecting:
		sprintf_s(
			status, 
			sizeof status,
			local ? "Local Player" : "Connecting...");
		break;

	case CONNECTION_STATE_synchronizing:
		sprintf_s(
			status,
			sizeof status, 
			local ? "Local Player" : "Synchronizing...");
		progress = info->connect_progress;
		break;

	case CONNECTION_STATE_disconnected:
		sprintf_s(status, sizeof status, "Disconnected");
		break;

	case CONNECTION_STATE_disconnecting:
		sprintf_s(status, sizeof status, "Waiting for player...");
		progress = (SDL_GetTicks() - info->disconnect_start) * 
			100 / info->disconnect_timeout;
		break;
	}

	if (*status)
	{
		ImDrawList* draw_list = ImGui::GetForegroundDrawList();

		float x = (float)(ship->position.x - (double)ImGui::CalcTextSize(status).x / 2);

		draw_list->AddText(
			ImVec2(x, (float)(ship->position.y + (double)PROGRESS_TEXT_OFFSET)),
			IM_COL32_WHITE,
			status);
	}
	if (progress >= 0)
	{
		SDL_Color bar = info->state == CONNECTION_STATE_synchronizing 
			? white
			: red;

		SDL_Rect rc = 
		{ 
			(int)(ship->position.x - (PROGRESS_BAR_WIDTH / 2)),
			(int)(ship->position.y + PROGRESS_BAR_TOP_OFFSET),
			(int)PROGRESS_BAR_WIDTH,
			(int)PROGRESS_BAR_HEIGHT };

		set_draw_color(renderer, bar);
		SDL_RenderDrawRect(renderer, &rc);

		rc.w = min(100, progress) * PROGRESS_BAR_WIDTH / 100;
		rc = { rc.x + 1, rc.y + 1, rc.w - 1, rc.h - 1 };

		set_draw_color(renderer, bar);
		SDL_RenderFillRect(renderer, &rc);
	}
}

void draw(
	SDL_Renderer *renderer, GameState const *gs, ConnectionReport const *cr)
{
	set_draw_color(renderer, black);
	SDL_RenderClear(renderer);

	SDL_Rect bounds =
	{
		gs->bounds.left,
		gs->bounds.top,
		gs->bounds.right - gs->bounds.left,
		gs->bounds.bottom - gs->bounds.top
	};

	set_draw_color(renderer, white);
	SDL_RenderDrawRect(renderer, &bounds);

	for (int i = 0; i < gs->num_ships; i++)
	{
		set_draw_color(renderer, ship_colors[i]);
		draw_ship(renderer, i, gs);

		draw_connect_state(renderer, 
			&gs->ships[i],
			&cr->participants[i]);
	}

	SDL_RenderFlush(renderer);
}
