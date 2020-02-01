#ifndef _RENDERER_H_
#define _RENDERER_H_

#ifdef __cplusplus
extern "C" {
#endif

void draw(struct SDL_Renderer *renderer, struct GameState const *gs, struct ConnectionReport const *connection_report);

#ifdef __cplusplus
}
#endif

#endif