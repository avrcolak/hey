#include "game.h"
#include "sdl_renderer.h"
#include <string.h>
#include <stdio.h>
#include "gamestate.h"

GameState gs = { 0 };
SDLRenderer* renderer = NULL;

/*
 * See http://en.wikipedia.org/wiki/Fletcher%27s_checksum
 */
int fletcher32_checksum(short* data, size_t len)
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

void buffer_event(SDL_Event const* e, LocalInput* input)
{

}

void capture_input_state(LocalInput* input)
{
	static const struct {
		int      key;
		int      input;
	} inputtable[] = {
	   { SDL_SCANCODE_UP,       INPUT_THRUST },
	   { SDL_SCANCODE_DOWN,     INPUT_BREAK },
	   { SDL_SCANCODE_LEFT,     INPUT_ROTATE_LEFT },
	   { SDL_SCANCODE_RIGHT,    INPUT_ROTATE_RIGHT },
	   { SDL_SCANCODE_D,        INPUT_FIRE },
	   { SDL_SCANCODE_S,        INPUT_BOMB },
	};

	const Uint8* states = SDL_GetKeyboardState(NULL);
	int inputs = 0;

	for (int i = 0; i < sizeof(inputtable) / sizeof(inputtable[0]); i++) {
		if (states[inputtable[i].key]) {
			inputs |= inputtable[i].input;
		}
	}

	input->inputs = inputs;
}

void step_game(LocalInput const *inputs, int disconnect_flags)
{
	int gs_inputs[] = { inputs[0].inputs, inputs[1].inputs };
	gs.Update(gs_inputs, disconnect_flags);
}

void draw_game(ConnectionReport const* connection_report)
{
	if (renderer != nullptr) {
		renderer->Draw(&gs, connection_report);
	}
}

void setup_game(SDL_Window* window, int num_players)
{
	renderer = new SDLRenderer(window);
	gs.Init(window, num_players);
}

void tear_down_game()
{
	memset(&gs, 0, sizeof(gs));

	delete renderer;
	renderer = NULL;
}

bool __cdecl begin_game(const char*)
{
	return true;
}

bool __cdecl load_game_state(unsigned char* buffer, int len)
{
	memcpy(&gs, buffer, len);
	return true;
}

bool __cdecl save_game_state(unsigned char** buffer, int* len, int* checksum, int)
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

bool __cdecl log_game_state(char* filename, unsigned char* buffer, int)
{
	FILE* fp = nullptr;
	fopen_s(&fp, filename, "w");
	if (fp) {
		GameState* gamestate = (GameState*)buffer;

		fprintf(fp, "GameState object.\n");
		fprintf(fp, "  bounds: %d,%d x %d,%d.\n",
			gamestate->_bounds.left,
			gamestate->_bounds.top,
			gamestate->_bounds.right,
			gamestate->_bounds.bottom);

		fprintf(fp, "  num_ships: %d.\n", gamestate->_num_ships);

		for (int i = 0; i < gamestate->_num_ships; i++) {
			Ship* ship = gamestate->_ships + i;

			fprintf(fp, "  ship %d position:  %.4f, %.4f\n",
				i, ship->position.x, ship->position.y);

			fprintf(fp, "  ship %d velocity:  %.4f, %.4f\n",
				i, ship->velocity.dx, ship->velocity.dy);

			fprintf(fp, "  ship %d radius:    %d.\n", i, ship->radius);
			fprintf(fp, "  ship %d heading:   %d.\n", i, ship->heading);
			fprintf(fp, "  ship %d health:    %d.\n", i, ship->health);
			fprintf(fp, "  ship %d speed:     %d.\n", i, ship->speed);
			fprintf(fp, "  ship %d cooldown:  %d.\n", i, ship->cooldown);
			fprintf(fp, "  ship %d score:     %d.\n", i, ship->score);

			for (int j = 0; j < MAX_BULLETS; j++) {
				Bullet* bullet = ship->bullets + j;
				fprintf(fp, "  ship %d bullet %d: %.2f %.2f -> %.2f %.2f.\n",
					i,
					j,
					bullet->position.x, bullet->position.y,
					bullet->velocity.dx, bullet->velocity.dy);
			}
		}
		fclose(fp);
	}
	return true;
}

void __cdecl free_game_state(void* buffer)
{
	free(buffer);
}

int game_frame_number()
{
	return gs._framenumber;
}

int game_state_hash()
{
	return fletcher32_checksum((short*)&gs, sizeof(gs) / 2);
}