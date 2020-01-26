#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "windows_shims.h"
#include "sdl_renderer.h"
#include "vectorwar.h"

//#define SYNC_TEST    // test: turn on synctest
#define MAX_PLAYERS     64

GameState gs = { 0 };
Renderer* renderer = NULL;
GGPOSession* ggpo = NULL;

/*
 * Simple checksum function stolen from wikipedia:
 *
 *   http://en.wikipedia.org/wiki/Fletcher%27s_checksum
 */
int
fletcher32_checksum(short* data, size_t len)
{
	int sum1 = 0xffff, sum2 = 0xffff;

	while (len) {
		size_t tlen = len > 360 ? 360 : len;
		len -= tlen;
		do {
			sum1 += *data++;
			sum2 += sum1;
		} while (--tlen);
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	}

	/* Second reduction step to reduce sums to 16 bits */
	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	return sum2 << 16 | sum1;
}

/*
 * vw_begin_game_callback --
 *
 * The begin game callback.  We don't need to do anything special here,
 * so just return true.
 */
bool __cdecl
vw_begin_game_callback(const char*)
{
	return true;
}

/*
 * vw_advance_frame_callback --
 *
 * Notification from GGPO we should step foward exactly 1 frame
 * during a rollback.
 */
bool __cdecl
vw_advance_frame_callback(int)
{
	int inputs[MAX_SHIPS] = { 0 };
	int disconnect_flags;

	// Make sure we fetch new inputs from GGPO and use those to update
	// the game state instead of reading from the keyboard.
	ggpo_synchronize_input(ggpo, (void*)inputs, sizeof(int) * MAX_SHIPS, &disconnect_flags);
	VectorWar_AdvanceFrame(inputs, disconnect_flags);
	return true;
}

/*
 * vw_load_game_state_callback --
 *
 * Makes our current state match the state passed in by GGPO.
 */
bool __cdecl
vw_load_game_state_callback(unsigned char* buffer, int len)
{
	memcpy(&gs, buffer, len);
	return true;
}

/*
 * vw_save_game_state_callback --
 *
 * Save the current state to a buffer and return it to GGPO via the
 * buffer and len parameters.
 */
bool __cdecl
vw_save_game_state_callback(unsigned char** buffer, int* len, int* checksum, int)
{
	*len = sizeof(gs);
	*buffer = (unsigned char*)malloc(*len);
	if (!*buffer) {
		return false;
	}
	memcpy(*buffer, &gs, *len);
	*checksum = fletcher32_checksum((short*)*buffer, *len / 2);
	return true;
}

/*
 * vw_log_game_state --
 *
 * Log the gamestate.  Used by the synctest debugging tool.
 */
bool __cdecl
vw_log_game_state(char* filename, unsigned char* buffer, int)
{
	FILE* fp = nullptr;
	fopen_s(&fp, filename, "w");
	if (fp) {
		GameState* gamestate = (GameState*)buffer;
		fprintf(fp, "GameState object.\n");
		fprintf(fp, "  bounds: %d,%d x %d,%d.\n", gamestate->_bounds.left, gamestate->_bounds.top,
			gamestate->_bounds.right, gamestate->_bounds.bottom);
		fprintf(fp, "  num_ships: %d.\n", gamestate->_num_ships);
		for (int i = 0; i < gamestate->_num_ships; i++) {
			Ship* ship = gamestate->_ships + i;
			fprintf(fp, "  ship %d position:  %.4f, %.4f\n", i, ship->position.x, ship->position.y);
			fprintf(fp, "  ship %d velocity:  %.4f, %.4f\n", i, ship->velocity.dx, ship->velocity.dy);
			fprintf(fp, "  ship %d radius:    %d.\n", i, ship->radius);
			fprintf(fp, "  ship %d heading:   %d.\n", i, ship->heading);
			fprintf(fp, "  ship %d health:    %d.\n", i, ship->health);
			fprintf(fp, "  ship %d speed:     %d.\n", i, ship->speed);
			fprintf(fp, "  ship %d cooldown:  %d.\n", i, ship->cooldown);
			fprintf(fp, "  ship %d score:     %d.\n", i, ship->score);
			for (int j = 0; j < MAX_BULLETS; j++) {
				Bullet* bullet = ship->bullets + j;
				fprintf(fp, "  ship %d bullet %d: %.2f %.2f -> %.2f %.2f.\n", i, j,
					bullet->position.x, bullet->position.y,
					bullet->velocity.dx, bullet->velocity.dy);
			}
		}
		fclose(fp);
	}
	return true;
}

/*
 * vw_free_buffer --
 *
 * Free a save state buffer previously returned in vw_save_game_state_callback.
 */
void __cdecl
vw_free_buffer(void* buffer)
{
	free(buffer);
}

GGPOSessionCallbacks VectorWar_Callbacks()
{
	GGPOSessionCallbacks cb = { 0 };

	cb.begin_game = vw_begin_game_callback;
	cb.advance_frame = vw_advance_frame_callback;
	cb.load_game_state = vw_load_game_state_callback;
	cb.save_game_state = vw_save_game_state_callback;
	cb.free_buffer = vw_free_buffer;
	cb.log_game_state = vw_log_game_state;

	return cb;
}

void
VectorWar_Init(SDL_Window* window, int num_players)
{
	renderer = new SDLRenderer(window);

	gs.Init(window, num_players);
}

/*
 * VectorWar_DrawCurrentFrame --
 *
 * Draws the current frame without modifying the game state.
 */
void
VectorWar_DrawCurrentFrame(NonGameState ngs)
{
	if (renderer != nullptr) {
		renderer->Draw(gs, ngs);
	}
}

/*
 * VectorWar_AdvanceFrame --
 *
 * Advances the game state by exactly 1 frame using the inputs specified
 * for player 1 and player 2.
 */
void VectorWar_AdvanceFrame(int inputs[], int disconnect_flags)
{
	gs.Update(inputs, disconnect_flags);
}

void
VectorWar_Exit()
{
	memset(&gs, 0, sizeof(gs));

	delete renderer;
	renderer = NULL;
}
