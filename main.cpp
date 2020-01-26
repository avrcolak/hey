#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <imgui.h>
#include <imgui_impl_opengl2.h>
#include <imgui_impl_sdl.h>
#include "vectorwar.h"
#include <gl/GL.h>

#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

typedef void (APIENTRYP PFNGLUSEPROGRAMPROC) (unsigned int);

typedef struct SdlHandles
{
	SDL_Window* window;
	SDL_GLContext gl_context;
	PFNGLUSEPROGRAMPROC glUseProgram;
} SdlHandles;

void draw_gui(SdlHandles handles)
{
	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame(handles.window);
	ImGui::NewFrame();

	if (show_demo_window) {
		ImGui::ShowDemoWindow(&show_demo_window);
	}

	{
		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, world!");

		ImGui::Text("This is some useful text.");
		ImGui::Checkbox("Demo Window", &show_demo_window);
		ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
		ImGui::ColorEdit3("clear color", (float*)&clear_color);

		if (ImGui::Button("Button")) {
			counter++;
		}

		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	if (show_another_window) {
		ImGui::Begin("Another Window", &show_another_window);
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			show_another_window = false;
		ImGui::End();
	}

	ImGui::Render();

	handles.glUseProgram(0);
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

typedef struct Inputs {
	SDL_Window* window;
} Inputs;

void step_game(Inputs inputs)
{
	VectorWar_RunFrame(inputs.window);
}

void draw_game()
{
	VectorWar_DrawCurrentFrame();
}

int client_process_event(SDL_Event e, SdlHandles handles, Inputs* inputs)
{
	switch (e.type) {
	case SDL_QUIT:
		return 1;
		break;
	case SDL_KEYDOWN:
		if (e.key.keysym.sym == SDLK_p) {
		}
		else if (e.key.keysym.sym == SDLK_ESCAPE) {
			return 1;
		}
		else if (e.key.keysym.sym >= SDLK_F1 && e.key.keysym.sym <= SDLK_F12) {
			VectorWar_DisconnectPlayer((int)(e.key.keysym.sym - SDLK_F1));
		}
		break;
	case SDL_WINDOWEVENT:
		switch (e.window.event)
		{
		case SDL_WINDOWEVENT_EXPOSED:
			SDL_UpdateWindowSurface(handles.window);
			break;
		case SDL_WINDOWEVENT_CLOSE:
			return 1;
		}
		break;
	}

	inputs->window = handles.window; // TODO
	ImGui_ImplSDL2_ProcessEvent(&e);

	return 0;
}

void client_loop(SdlHandles handles, GGPOSession* ggpo)
{
	int start, next, now;

	start = next = now = SDL_GetTicks();

	while (1) {
		SDL_Event e;
		Inputs inputs;

		while (SDL_PollEvent(&e) != 0) {
			int quit = client_process_event(e, handles, &inputs);

			if (quit) {
				return;
			}
		}

		now = SDL_GetTicks();
		ggpo_idle(ggpo, max(0, next - now - 1));

		if (now >= next) {
			step_game(inputs);
			next = now + (1000 / 60);
		}

		draw_game();
		draw_gui(handles);

		SDL_GL_SwapWindow(handles.window);
	}
}

SdlHandles setup_sdl()
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	SDL_Window* window = SDL_CreateWindow("vectorwar",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		640,
		480,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);

	PFNGLUSEPROGRAMPROC glUseProgram = (PFNGLUSEPROGRAMPROC)
		SDL_GL_GetProcAddress("glUseProgram");

	if (SDL_GL_SetSwapInterval(-1) != 0)
	{
		SDL_GL_SetSwapInterval(1);
	}

	return { window, gl_context, glUseProgram };
}

void tear_down_sdl(SdlHandles handles)
{
	SDL_GL_DeleteContext(handles.gl_context);
	SDL_DestroyWindow(handles.window);
	SDL_Quit();
}

void setup_winsock()
{
	WSADATA wd = { 0 };
	WSAStartup(MAKEWORD(2, 2), &wd);
}

void tear_down_winsock()
{
	WSACleanup();
}

void setup_imgui(SdlHandles sdl_handles)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(sdl_handles.window, sdl_handles.gl_context);
	ImGui_ImplOpenGL2_Init();
}

void tear_down_imgui()
{
	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

enum ROLE_TYPE
{
	ROLE_TYPE_Spectator,
	ROLE_TYPE_Player,
};

typedef struct SpectatorInit
{
	ROLE_TYPE type;
	char host_ip[128];
	unsigned short host_port;
} SpecatatorInit;

typedef struct PlayerInit
{
	ROLE_TYPE type;
	GGPOPlayer players[GGPO_MAX_SPECTATORS + GGPO_MAX_PLAYERS];
	int local_player;
	int num_spectators;
} PlayerInit;

typedef union RoleInit
{
	ROLE_TYPE type;
	PlayerInit player_init;
	SpectatorInit spectator_init;
} RoleInit;

typedef struct ClientInit
{
	unsigned short local_port;
	int num_players;
	RoleInit role_init;
} ClientInit;

void show_syntax_error()
{
	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		"Syntax: vectorwar.exe <local port> <num players> ('local' | <remote ip>:<remote port>)*\n",
		"Could not start",
		NULL);
}

int parse_args(int argc, char* args[], ClientInit* init)
{
	if (argc < 3) {
		show_syntax_error();

		return 1;
	}

	int offset = 1;

	init->local_port = (unsigned short)atoi(args[offset++]);
	init->num_players = atoi(args[offset++]);

	if (init->num_players < 0 || argc < offset + init->num_players) {
		show_syntax_error();

		return 1;
	}

	char wide_ip_buffer[128];
	unsigned int wide_ip_buffer_size = (unsigned int)ARRAYSIZE(wide_ip_buffer);

	if (strcmp(args[offset], "spectate") == 0) {
		init->role_init.type = ROLE_TYPE_Spectator;

		int result = sscanf(
			args[offset + 1],
			"%[^:]:%hu",
			init->role_init.spectator_init.host_ip,
			&init->role_init.spectator_init.host_port);

		if (result != 2) {
			show_syntax_error();

			return 1;
		}

		return 0;
	}
	else {
		init->role_init.type = ROLE_TYPE_Player;

		GGPOPlayer* players = init->role_init.player_init.players;
		int i;

		for (i = 0; i < init->num_players; i++) {
			const char* arg = args[offset++];

			players[i].size = sizeof(players[i]);
			players[i].player_num = i + 1;

			if (!strcmp(arg, "local")) {
				players[i].type = GGPO_PLAYERTYPE_LOCAL;
				init->role_init.player_init.local_player = i;

				continue;
			}

			players[i].type = GGPO_PLAYERTYPE_REMOTE;

			int result = sscanf(
				arg,
				"%[^:]:%hd",
				players[i].u.remote.ip_address,
				&players[i].u.remote.port);

			if (result != 2) {
				show_syntax_error();

				return 1;
			}
		}

		init->role_init.player_init.num_spectators = 0;

		while (offset < argc) {
			players[i].type = GGPO_PLAYERTYPE_SPECTATOR;

			int result = sscanf(
				args[offset++],
				"%[^:]:%hd",
				players[i].u.remote.ip_address,
				&players[i].u.remote.port);

			if (result != 2) {
				show_syntax_error();

				return 1;
			}

			i++;
			init->role_init.player_init.num_spectators++;
		}

		return 0;
	}
}

GGPOSession* setup_vector_war(ClientInit init, SDL_Window* window)
{
	if (init.role_init.type == ROLE_TYPE_Spectator) {
		SpectatorInit spectator_init = init.role_init.spectator_init;

		GGPOSession* ggpo = VectorWar_InitSpectator(
			window,
			init.local_port,
			init.num_players,
			spectator_init.host_ip,
			spectator_init.host_port);

		return ggpo;
	}

	PlayerInit player_init = init.role_init.player_init;

	SDL_Point window_offsets[] = {
		{ 64,  64 },
		{ 740, 64 },
		{ 64,  600 },
		{ 740, 600 },
	};

	GGPOSession* ggpo = VectorWar_Init(
		window,
		init.local_port,
		init.num_players,
		player_init.players,
		player_init.num_spectators);

	int local_player = player_init.local_player;

	if (local_player < ARRAYSIZE(window_offsets)) {
		SDL_SetWindowPosition(
			window,
			window_offsets[local_player].x,
			window_offsets[local_player].y);
	}

	return ggpo;
}

void tear_down_game()
{
	VectorWar_Exit();
}

void tear_down_ggpo(GGPOSession* ggpo)
{
	if (ggpo) {
		ggpo_close_session(ggpo);
		ggpo = NULL;
	}
}

int main(int argc, char* args[])
{
	SdlHandles sdl_handles = setup_sdl();

	ClientInit init;
	int result = parse_args(argc, args, &init);

	if (result != 0) {
		return result;
	}

	setup_winsock();
	setup_imgui(sdl_handles);
	GGPOSession* ggpo = setup_vector_war(init, sdl_handles.window);

	client_loop(sdl_handles, ggpo);

	tear_down_ggpo(ggpo);
	tear_down_game();
	tear_down_imgui();
	tear_down_winsock();
	tear_down_sdl(sdl_handles);

	return 0;
}