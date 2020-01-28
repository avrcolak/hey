#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <imgui.h>
#include <imgui_impl_opengl2.h>
#include <imgui_impl_sdl.h>
#include "nongamestate.h"
#include "game.h"
#include <gl/GL.h>

#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

typedef void (APIENTRYP PFNGLUSEPROGRAMPROC) (unsigned int);

typedef struct FrameInfo
{
	int number;
	int hash;
} FrameInfo;

typedef struct FrameReport
{
	FrameInfo current;
	FrameInfo periodic;
} FrameReport;

FrameReport frame_report;

typedef struct GgpoHandles
{
	GGPOSession* session;
	GGPOPlayerHandle local_player;
} GgpoHandles;

GgpoHandles ggpo_handles;

NonGameState ngs = { 0 };

bool __cdecl on_event(GGPOEvent* info)
{
	int progress;

	switch (info->code) {
	case GGPO_EVENTCODE_CONNECTED_TO_PEER:
		ngs.SetConnectState(info->u.connected.player, Synchronizing);
		break;
	case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
		progress = 100 * info->u.synchronizing.count / info->u.synchronizing.total;
		ngs.UpdateConnectProgress(info->u.synchronizing.player, progress);
		break;
	case GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER:
		ngs.UpdateConnectProgress(info->u.synchronized.player, 100);
		break;
	case GGPO_EVENTCODE_RUNNING:
		ngs.SetConnectState(Running);
		// renderer->SetStatusText("");
		break;
	case GGPO_EVENTCODE_CONNECTION_INTERRUPTED:
		ngs.SetDisconnectTimeout(
			info->u.connection_interrupted.player,
			SDL_GetTicks(),
			info->u.connection_interrupted.disconnect_timeout);
		break;
	case GGPO_EVENTCODE_CONNECTION_RESUMED:
		ngs.SetConnectState(info->u.connection_resumed.player, Running);
		break;
	case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
		ngs.SetConnectState(info->u.disconnected.player, Disconnected);
		break;
	case GGPO_EVENTCODE_TIMESYNC:
		SDL_Delay(1000 * info->u.timesync.frames_ahead / 60);
		break;
	}

	return true;
}

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

void show_disconnected_player(GGPOErrorCode result, int player)
{
	char logbuf[128];

	if (GGPO_SUCCEEDED(result)) {
		sprintf(logbuf, "Disconnected player %d.\n", player);
	}
	else {
		sprintf(logbuf, "Error while disconnecting player (err:%d).\n", result);
	}

	// renderer->SetStatusText(logbuf);
}

void disconnect_player(int player, GGPOSession* ggpo)
{
	if (player < ngs.num_players) {
		GGPOErrorCode result =
			ggpo_disconnect_player(ggpo, ngs.players[player].handle);

		show_disconnected_player(result, player);
	}
}

int client_process_event(SDL_Event e, SdlHandles sdl_handles)
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
			disconnect_player(
				(int)(e.key.keysym.sym - SDLK_F1), ggpo_handles.session);
		}
		break;
	case SDL_WINDOWEVENT:
		switch (e.window.event)
		{
		case SDL_WINDOWEVENT_EXPOSED:
			SDL_UpdateWindowSurface(sdl_handles.window);
			break;
		case SDL_WINDOWEVENT_CLOSE:
			return 1;
		}
		break;
	}

	ImGui_ImplSDL2_ProcessEvent(&e);

	return 0;
}

void update_frame_report(int frame_number, int hash)
{
	frame_report.current.number = frame_number;
	frame_report.current.hash = hash;

	if ((frame_number % 90) == 0) {
		frame_report.periodic = frame_report.current;
	}
}

bool __cdecl advance_frame(int flags)
{
	int disconnect_flags = 0;
	LocalInput inputs[2] = { 0 };

	GGPOErrorCode result = ggpo_synchronize_input(
		ggpo_handles.session,
		(void*)inputs,
		sizeof(LocalInput) * 2,
		&disconnect_flags);

	if (GGPO_SUCCEEDED(result)) {
		step_game(inputs, disconnect_flags);
		ggpo_advance_frame(ggpo_handles.session);

		int frame_number = game_frame_number();
		int hash = game_state_hash();
		update_frame_report(frame_number, hash);

		return true;
	}

	return false;
}

void work(LocalInput* local_input)
{
	capture_input_state(local_input);

	GGPOErrorCode result = ggpo_add_local_input(
		ggpo_handles.session,
		ggpo_handles.local_player,
		local_input,
		sizeof(LocalInput));

	if (GGPO_SUCCEEDED(result)) {
		advance_frame(0);
	}
}

void client_loop(SdlHandles sdl_handles)
{
	int start, next, now;

	start = next = now = SDL_GetTicks();
	LocalInput local_input = { 0 };

	while (1) {
		SDL_Event e;

		while (SDL_PollEvent(&e) != 0) {
			int quit = client_process_event(e, sdl_handles);

			if (quit) {
				return;
			}

			buffer_event(&e, &local_input);
		}

		now = SDL_GetTicks();
		ggpo_idle(ggpo_handles.session, max(0, next - now - 1));

		if (now >= next) {
			work(&local_input);
			local_input = { 0 };
			next = now + (1000 / 60);
		}

		draw_game(&ngs);
		draw_gui(sdl_handles);

		SDL_GL_SwapWindow(sdl_handles.window);
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

GgpoHandles setup_ggpo(ClientInit init, SDL_Window* window)
{
	WSADATA wd = { 0 };
	WSAStartup(MAKEWORD(2, 2), &wd);

	GgpoHandles handles;
	GGPOSessionCallbacks cb;;
	cb.on_event = on_event;
	cb.advance_frame = advance_frame;
	cb.begin_game = begin_game;
	cb.load_game_state = load_game_state;
	cb.free_buffer = free_game_state;
	cb.log_game_state = log_game_state;
	cb.save_game_state = save_game_state;

	ngs.num_players = init.num_players;

	if (init.role_init.type == ROLE_TYPE_Spectator) {
		SpectatorInit spectator_init = init.role_init.spectator_init;

		GGPOErrorCode result = ggpo_start_spectating(
			&handles.session,
			&cb,
			"vectorwar",
			init.num_players,
			sizeof(LocalInput),
			init.local_port,
			spectator_init.host_ip,
			spectator_init.host_port);

		// renderer->SetStatusText("Starting new spectator session");

		return handles;
	}

	PlayerInit player_init = init.role_init.player_init;

	SDL_Point window_offsets[] = {
		{ 64,  64 },
		{ 740, 64 },
		{ 64,  600 },
		{ 740, 600 },
	};

	GGPOErrorCode result = ggpo_start_session(
		&handles.session,
		&cb, 
		"vectorwar",
		init.num_players, 
		sizeof(LocalInput),
		init.local_port);

	ggpo_set_disconnect_timeout(handles.session, 3000);
	ggpo_set_disconnect_notify_start(handles.session, 1000);

	for (int i = 0; i < init.num_players + player_init.num_spectators; i++) {
		GGPOPlayerHandle handle;

		result = ggpo_add_player(
			handles.session, player_init.players + i, &handle);

		ngs.players[i].handle = handle;
		ngs.players[i].type = player_init.players[i].type;

		if (player_init.players[i].type == GGPO_PLAYERTYPE_LOCAL) {
			handles.local_player = handle;

			ngs.players[i].connect_progress = 100;
			ngs.SetConnectState(handle, Connecting);
			ggpo_set_frame_delay(handles.session, handle, 2);
		}
		else {
			ngs.players[i].connect_progress = 0;
		}
	}

	// renderer->SetStatusText("Connecting to peers.");

	int local_player = player_init.local_player;

	if (local_player < ARRAYSIZE(window_offsets)) {
		SDL_SetWindowPosition(
			window,
			window_offsets[local_player].x,
			window_offsets[local_player].y);
	}

	return handles;
}

void tear_down_ggpo()
{
	if (ggpo_handles.session) {
		ggpo_close_session(ggpo_handles.session);
		ggpo_handles.session = NULL;
	}

	WSACleanup();
}

int main(int argc, char* args[])
{
	SdlHandles sdl_handles = setup_sdl();

	ClientInit init;
	int result = parse_args(argc, args, &init);

	if (result != 0) {
		return result;
	}

	ggpo_handles = setup_ggpo(init, sdl_handles.window);

	setup_imgui(sdl_handles);
	setup_game(sdl_handles.window, init.num_players);

	frame_report = { 0 };

	client_loop(sdl_handles);

	tear_down_game();
	tear_down_imgui();
	tear_down_ggpo();
	tear_down_sdl(sdl_handles);

	return 0;
}