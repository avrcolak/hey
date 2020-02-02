#ifndef _GAMESTATE_H_
#define _GAMESTATE_H_

#define PI              ((double)3.1415926)
#define STARTING_HEALTH        100
#define ROTATE_INCREMENT         3
#define SHIP_RADIUS             15
#define SHIP_WIDTH               8
#define SHIP_TUCK                3
#define SHIP_THRUST              0.06
#define SHIP_MAX_THRUST          4.0
#define SHIP_BREAK_SPEED         0.6
#define BULLET_SPEED             5
#define MAX_BULLETS             30
#define BULLET_COOLDOWN          8
#define BULLET_DAMAGE           10
#define MAX_SHIPS                4

enum INPUT
{
	INPUT_thrust = (1 << 0),
	INPUT_break = (1 << 1),
	INPUT_rotate_left = (1 << 2),
	INPUT_rotate_right = (1 << 3),
	INPUT_fire = (1 << 4),
	INPUT_bomb = (1 << 5),
};

typedef struct Position
{
	double x, y;
} Position;

typedef struct Velocity 
{
	double dx, dy;
} Velocity;

typedef struct Bullet 
{
	int active;
	Position position;
	Velocity velocity;
} Bullet;

typedef struct Ship 
{
	Position position;
	Velocity velocity;
	int radius;
	int heading;
	int health;
	int speed;
	int cooldown;
	Bullet bullets[MAX_BULLETS];
	int score;
} Ship;

typedef struct Bounds
{
	long left;
	long top;
	long right;
	long bottom;
} Bounds;

typedef struct GameState
{
	int frame_number;
	Bounds bounds;
	int num_ships;
	Ship ships[MAX_SHIPS];
} GameState;

#endif // ifndef _GAMESTATE_H_
