#ifndef _GAME_H
#define _GAME_H

#ifdef __cplusplus
extern "C" 
{
#endif

#define MAX_PLAYERS 4
#define GAME_NAME   "vectorwar"

typedef struct LocalInput {
	int inputs;
} LocalInput;

extern const char game_name[];

int game_frame_number();

int game_state_hash();

void setup_game(struct SDL_Window *window, int num_players);

void tear_down_game();

void buffer_event(union SDL_Event const *event, LocalInput *input);

void capture_input_state(LocalInput *input);

void step_game(LocalInput const *inputs, int disconnect_flags);

void draw_game(
	struct SDL_Renderer *renderer, 
	struct ConnectionReport const *connection_report);

// The following are wrapped before being passed to GGPO to match the exact
// signature. For intended semantics, see GGPOSessionCallbacks in ggponet.h.

int begin_game(const char *game);

int load_game_state(unsigned char *buffer, int len);

int save_game_state(
	unsigned char **buffer,
	int *len, 
	int *checksum, 
	int frame);

void free_game_state(void *buffer);

int log_game_state(char *filename, unsigned char *buffer, int len);

#ifdef __cplusplus
}
#endif

#endif // ifndef _GAME_H
