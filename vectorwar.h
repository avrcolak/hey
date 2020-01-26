#ifndef _VECTORWAR_H
#define _VECTORWAR_H

#include "nongamestate.h"
#include "ggponet.h"

/*
 * vectorwar.h --
 *
 * Interface to the vector war application.
 *
 */

enum VectorWarInputs {
   INPUT_THRUST            = (1 << 0),
   INPUT_BREAK             = (1 << 1),
   INPUT_ROTATE_LEFT       = (1 << 2),
   INPUT_ROTATE_RIGHT      = (1 << 3),
   INPUT_FIRE              = (1 << 4),
   INPUT_BOMB              = (1 << 5),
};

void VectorWar_Init(SDL_Window* window, int num_players);
void VectorWar_DrawCurrentFrame(NonGameState ngs);
void VectorWar_AdvanceFrame(int inputs[], int disconnect_flags);
void VectorWar_Exit();
GGPOSessionCallbacks VectorWar_Callbacks();

#define ARRAY_SIZE(n)      (sizeof(n) / sizeof(n[0]))
#define FRAME_DELAY        2

#endif
