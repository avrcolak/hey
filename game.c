#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "connection_report.h"
#include "game.h"
#include "game_state.h"
#include "renderer.h"
#include "utils.h"

GameState gs = { 0 };

static double degtorad(double deg)
{
	return PI * deg / 180;
}

static double distance(Position* lhs, Position* rhs)
{
	double x = rhs->x - lhs->x;
	double y = rhs->y - lhs->y;
	return sqrt(x * x + y * y);
}

static void inflate(Bounds* bounds, int dx, int dy)
{
	bounds->left -= dx;
	bounds->top -= dy;
	bounds->right += dx;
	bounds->bottom += dy;
}

static void init_game_state(SDL_Window* window, int num_players)
{
	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	Bounds bounds = { 0, 0, w, h };
	gs.bounds = bounds;

	inflate(&gs.bounds, -8, -8);

	int r = h / 4;
	gs.frame_number = 0;
	gs.num_ships = num_players;

	for (int i = 0; i < gs.num_ships; i++)
	{
		int heading = i * 360 / num_players;
		double theta = (double)heading * PI / 180;
		double cost = cos(theta);
		double sint = sin(theta);

		gs.ships[i].position.x = (w / 2) + r * cost;
		gs.ships[i].position.y = (h / 2) + r * sint;
		gs.ships[i].heading = (heading + 180) % 360;
		gs.ships[i].health = STARTING_HEALTH;
		gs.ships[i].radius = SHIP_RADIUS;
	}

	inflate(&gs.bounds, -8, -8);
}

static void get_ship_ai(int i, double *heading, double *thrust, int *fire)
{
	*heading = (gs.ships[i].heading + 5) % 360;
	*thrust = 0;
	*fire = 0;
}

static void parse_ship_inputs(
	int inputs, int i, double *heading, double *thrust, int *fire)
{
	Ship *ship = gs.ships + i;

	if (inputs & INPUT_rotate_right) 
	{
		*heading = (ship->heading + ROTATE_INCREMENT) % 360;
	}
	else if (inputs & INPUT_rotate_left) 
	{
		*heading = (ship->heading - ROTATE_INCREMENT + 360) % 360;
	}
	else 
	{
		*heading = ship->heading;
	}

	if (inputs & INPUT_thrust) 
	{
		*thrust = SHIP_THRUST;
	}
	else if (inputs & INPUT_break) 
	{
		*thrust = -SHIP_THRUST;
	}
	else {
		*thrust = 0;
	}

	*fire = inputs & INPUT_fire;
}

static void move_ship(int which, double heading, double thrust, int fire)
{
	Ship* ship = gs.ships + which;

	ship->heading = (int)heading;

	if (ship->cooldown == 0)
	{
		if (fire)
		{
			for (int i = 0; i < MAX_BULLETS; i++)
			{
				double dx = cos(degtorad(ship->heading));
				double dy = sin(degtorad(ship->heading));

				if (!ship->bullets[i].active)
				{
					ship->bullets[i].active = true;
					ship->bullets[i].position.x = ship->position.x + (ship->radius * dx);
					ship->bullets[i].position.y = ship->position.y + (ship->radius * dy);
					ship->bullets[i].velocity.dx = ship->velocity.dx + (BULLET_SPEED * dx);
					ship->bullets[i].velocity.dy = ship->velocity.dy + (BULLET_SPEED * dy);
					ship->cooldown = BULLET_COOLDOWN;
					break;
				}
			}
		}
	}

	if (thrust)
	{
		double dx = thrust * cos(degtorad(heading));
		double dy = thrust * sin(degtorad(heading));

		ship->velocity.dx += dx;
		ship->velocity.dy += dy;

		double mag = sqrt(
			ship->velocity.dx * ship->velocity.dx +
			ship->velocity.dy * ship->velocity.dy);

		if (mag > SHIP_MAX_THRUST)
		{
			ship->velocity.dx = (ship->velocity.dx * SHIP_MAX_THRUST) / mag;
			ship->velocity.dy = (ship->velocity.dy * SHIP_MAX_THRUST) / mag;
		}
	}

	ship->position.x += ship->velocity.dx;
	ship->position.y += ship->velocity.dy;

	if (ship->position.x - ship->radius < gs.bounds.left ||
		ship->position.x + ship->radius > gs.bounds.right)
	{
		ship->velocity.dx *= -1;
		ship->position.x += (ship->velocity.dx * 2);
	}
	if (ship->position.y - ship->radius < gs.bounds.top ||
		ship->position.y + ship->radius > gs.bounds.bottom)
	{
		ship->velocity.dy *= -1;
		ship->position.y += (ship->velocity.dy * 2);
	}
	for (int i = 0; i < MAX_BULLETS; i++)
	{
		Bullet* bullet = ship->bullets + i;
		if (bullet->active)
		{
			bullet->position.x += bullet->velocity.dx;
			bullet->position.y += bullet->velocity.dy;

			if (bullet->position.x < gs.bounds.left ||
				bullet->position.y < gs.bounds.top ||
				bullet->position.x > gs.bounds.right ||
				bullet->position.y > gs.bounds.bottom)
			{
				bullet->active = false;
			}
			else
			{
				for (int j = 0; j < gs.num_ships; j++)
				{
					Ship* other = gs.ships + j;

					if (distance(&bullet->position, &other->position) <
						other->radius)
					{
						ship->score++;
						other->health -= BULLET_DAMAGE;
						bullet->active = false;
						break;
					}
				}
			}
		}
	}
}

static void update_game_state(int const *inputs, int disconnect_flags)
{
	gs.frame_number++;

	for (int i = 0; i < gs.num_ships; i++)
	{
		double thrust, heading;
		int fire;

		if (disconnect_flags & (1 << i))
		{
			get_ship_ai(i, &heading, &thrust, &fire);
		}
		else
		{
			parse_ship_inputs(inputs[i], i, &heading, &thrust, &fire);
		}

		move_ship(i, heading, thrust, fire);

		if (gs.ships[i].cooldown)
		{
			gs.ships[i].cooldown--;
		}
	}
}

// See http://en.wikipedia.org/wiki/Fletcher%27s_checksum.
static int fletcher32_checksum(short const *data, size_t len)
{
	int sum1 = 0xffff, sum2 = 0xffff;

	while (len)
	{
		size_t tlen = len > 360 ? 360 : len;
		len -= tlen;

		do
		{
			sum1 += *data++;
			sum2 += sum1;
		} while (--tlen);

		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	}

	// Second reduction step to reduce sums to 16 bits.
	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	return sum2 << 16 | sum1;
}

void buffer_event(SDL_Event const *e, LocalInput* input)
{
	(void)input; (void)e;
}

void capture_input_state(LocalInput *input)
{
	static const struct
	{
		int key;
		int input;
	} inputtable[] =
	{
	   { SDL_SCANCODE_UP,       INPUT_thrust },
	   { SDL_SCANCODE_DOWN,     INPUT_break },
	   { SDL_SCANCODE_LEFT,     INPUT_rotate_left },
	   { SDL_SCANCODE_RIGHT,    INPUT_rotate_right },
	   { SDL_SCANCODE_D,        INPUT_fire },
	   { SDL_SCANCODE_S,        INPUT_bomb },
	};

	const Uint8* states = SDL_GetKeyboardState(NULL);
	int inputs = 0;

	for (int i = 0; i < COUNT_OF(inputtable); i++)
	{
		if (states[inputtable[i].key])
		{
			inputs |= inputtable[i].input;
		}
	}

	input->inputs = inputs;
}

void step_game(LocalInput const* inputs, int disconnect_flags)
{
	int gs_inputs[MAX_PLAYERS] = { 0 };

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		gs_inputs[i] = inputs[i].inputs;
	}

	update_game_state(gs_inputs, disconnect_flags);
}

void draw_game(SDL_Renderer* renderer, ConnectionReport const* connection_report)
{
	draw(renderer, &gs, connection_report);
}

void setup_game(SDL_Window* window, int num_players)
{
	init_game_state(window, num_players);
}

void tear_down_game()
{
	memset(&gs, 0, sizeof(gs));
}

int begin_game(const char* game)
{
	(void)game;
	return true;
}

int load_game_state(unsigned char* buffer, int len)
{
	memcpy(&gs, buffer, len);
	return true;
}

int save_game_state(unsigned char** buffer, int* len, int* checksum, int frame)
{
	(void)frame;

	*len = sizeof(gs);
	*buffer = (unsigned char*)malloc(*len);

	if (!*buffer)
	{
		return false;
	}

	memcpy(*buffer, &gs, *len);
	*checksum = fletcher32_checksum((short*)*buffer, *len / 2);

	return true;
}

int log_game_state(char* filename, unsigned char* buffer, int len)
{
	(void)len;

	FILE* fp = NULL;
	fopen_s(&fp, filename, "w");

	if (fp)
	{
		GameState* in_gs = (GameState*)buffer;

		fprintf(fp, "GameState object.\n");
		fprintf(fp, "  bounds: %d,%d x %d,%d.\n",
			in_gs->bounds.left,
			in_gs->bounds.top,
			in_gs->bounds.right,
			in_gs->bounds.bottom);

		fprintf(fp, "  num_ships: %d.\n", in_gs->num_ships);

		for (int i = 0; i < in_gs->num_ships; i++)
		{
			Ship* ship = in_gs->ships + i;

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

			for (int j = 0; j < MAX_BULLETS; j++)
			{
				Bullet* bullet = ship->bullets + j;
				fprintf(
					fp,
					"  ship %d bullet %d: %.2f %.2f -> %.2f %.2f.\n",
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

void free_game_state(void* buffer)
{
	free(buffer);
}

int game_frame_number()
{
	return gs.frame_number;
}

int game_state_hash()
{
	return fletcher32_checksum((short*)&gs, sizeof(gs) / 2);
}
