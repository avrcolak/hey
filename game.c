#include <SDL.h>
#include <string.h>
#include <stdio.h>
#include "connection_report.h"
#include "game.h"
#include "game_state.h"
#include "renderer.h"
#include <stdbool.h>

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

void inflate(Bounds* bounds, int dx, int dy)
{
	bounds->left -= dx;
	bounds->top -= dy;
	bounds->right += dx;
	bounds->bottom += dy;
}

void init_game_state(SDL_Window* window, int num_players)
{
	int i, w, h, r;

	SDL_GetWindowSize(window, (int*)&gs._bounds.right, (int*)&gs._bounds.bottom);
	gs._bounds.top = 0;
	gs._bounds.left = 0;
	inflate(&gs._bounds, -8, -8);

	w = gs._bounds.right - gs._bounds.left;
	h = gs._bounds.bottom - gs._bounds.top;
	r = h / 4;

	gs._framenumber = 0;
	gs._num_ships = num_players;
	for (i = 0; i < gs._num_ships; i++) {
		int heading = i * 360 / num_players;
		double cost, sint, theta;

		theta = (double)heading * PI / 180;
		cost = cos(theta);
		sint = sin(theta);

		gs._ships[i].position.x = (w / 2) + r * cost;
		gs._ships[i].position.y = (h / 2) + r * sint;
		gs._ships[i].heading = (heading + 180) % 360;
		gs._ships[i].health = STARTING_HEALTH;
		gs._ships[i].radius = SHIP_RADIUS;
	}

	inflate(&gs._bounds, -8, -8);
}

void get_ship_ai(int i, double* heading, double* thrust, int* fire)
{
	*heading = (gs._ships[i].heading + 5) % 360;
	*thrust = 0;
	*fire = 0;
}

void parse_ship_inputs(int inputs, int i, double* heading, double* thrust, int* fire)
{
	Ship* ship = gs._ships + i;

	if (inputs & INPUT_ROTATE_RIGHT) {
		*heading = (ship->heading + ROTATE_INCREMENT) % 360;
	}
	else if (inputs & INPUT_ROTATE_LEFT) {
		*heading = (ship->heading - ROTATE_INCREMENT + 360) % 360;
	}
	else {
		*heading = ship->heading;
	}

	if (inputs & INPUT_THRUST) {
		*thrust = SHIP_THRUST;
	}
	else if (inputs & INPUT_BREAK) {
		*thrust = -SHIP_THRUST;
	}
	else {
		*thrust = 0;
	}
	*fire = inputs & INPUT_FIRE;
}

void move_ship(int which, double heading, double thrust, int fire)
{
	Ship* ship = gs._ships + which;

	ship->heading = (int)heading;

	if (ship->cooldown == 0) {
		if (fire) {
			for (int i = 0; i < MAX_BULLETS; i++) {
				double dx = cos(degtorad(ship->heading));
				double dy = sin(degtorad(ship->heading));
				if (!ship->bullets[i].active) {
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

	if (thrust) {
		double dx = thrust * cos(degtorad(heading));
		double dy = thrust * sin(degtorad(heading));

		ship->velocity.dx += dx;
		ship->velocity.dy += dy;
		double mag = sqrt(ship->velocity.dx * ship->velocity.dx +
			ship->velocity.dy * ship->velocity.dy);
		if (mag > SHIP_MAX_THRUST) {
			ship->velocity.dx = (ship->velocity.dx * SHIP_MAX_THRUST) / mag;
			ship->velocity.dy = (ship->velocity.dy * SHIP_MAX_THRUST) / mag;
		}
	}

	ship->position.x += ship->velocity.dx;
	ship->position.y += ship->velocity.dy;

	if (ship->position.x - ship->radius < gs._bounds.left ||
		ship->position.x + ship->radius > gs._bounds.right) {
		ship->velocity.dx *= -1;
		ship->position.x += (ship->velocity.dx * 2);
	}
	if (ship->position.y - ship->radius < gs._bounds.top ||
		ship->position.y + ship->radius > gs._bounds.bottom) {
		ship->velocity.dy *= -1;
		ship->position.y += (ship->velocity.dy * 2);
	}
	for (int i = 0; i < MAX_BULLETS; i++) {
		Bullet* bullet = ship->bullets + i;
		if (bullet->active) {
			bullet->position.x += bullet->velocity.dx;
			bullet->position.y += bullet->velocity.dy;
			if (bullet->position.x < gs._bounds.left ||
				bullet->position.y < gs._bounds.top ||
				bullet->position.x > gs._bounds.right ||
				bullet->position.y > gs._bounds.bottom) {
				bullet->active = false;
			}
			else {
				for (int j = 0; j < gs._num_ships; j++) {
					Ship* other = gs._ships + j;
					if (distance(&bullet->position, &other->position) < other->radius) {
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

void update_game_state(int inputs[], int disconnect_flags)
{
	gs._framenumber++;
	for (int i = 0; i < gs._num_ships; i++) {
		double thrust, heading;
		int fire;

		if (disconnect_flags & (1 << i)) {
			get_ship_ai(i, &heading, &thrust, &fire);
		}
		else {
			parse_ship_inputs(inputs[i], i, &heading, &thrust, &fire);
		}
		move_ship(i, heading, thrust, fire);

		if (gs._ships[i].cooldown) {
			gs._ships[i].cooldown--;
		}
	}
}

// See http://en.wikipedia.org/wiki/Fletcher%27s_checksum.
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

	// Second reduction step to reduce sums to 16 bits.
	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	return sum2 << 16 | sum1;
}

void buffer_event(SDL_Event const* e, LocalInput* input)
{
	(void)input;
	(void)e;
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

void step_game(LocalInput const* inputs, int disconnect_flags)
{
	int gs_inputs[MAX_PLAYERS] = { 0 };

	for (int i = 0; i < MAX_PLAYERS; i++) {
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
	if (!*buffer) {
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
	if (fp) {
		GameState* in_gs = (GameState*)buffer;

		fprintf(fp, "GameState object.\n");
		fprintf(fp, "  bounds: %d,%d x %d,%d.\n",
			in_gs->_bounds.left,
			in_gs->_bounds.top,
			in_gs->_bounds.right,
			in_gs->_bounds.bottom);

		fprintf(fp, "  num_ships: %d.\n", in_gs->_num_ships);

		for (int i = 0; i < in_gs->_num_ships; i++) {
			Ship* ship = in_gs->_ships + i;

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

void free_game_state(void* buffer)
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