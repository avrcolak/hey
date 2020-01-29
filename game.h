#ifndef _GAME_H
#define _GAME_H

typedef struct LocalInput {
    int inputs;
} LocalInput;

void buffer_event(union SDL_Event const *e, LocalInput* input);

void capture_input_state(LocalInput *input);

void setup_game(struct SDL_Window *window, int num_players);

void tear_down_game();

void step_game(LocalInput const *inputs, int disconnect_flags);

void draw_game(struct ConnectionReport const *connection_report);

int game_frame_number();

int game_state_hash();

bool __cdecl begin_game(const char*);

bool __cdecl load_game_state(unsigned char *buffer, int len);

bool __cdecl save_game_state(unsigned char **buffer, int *len, int *checksum, int);

void __cdecl free_game_state(void *buffer);

bool __cdecl log_game_state(char *filename, unsigned char *buffer, int);

#endif
