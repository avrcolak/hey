#include "connection_report.h"
#include "game.h"
#include <ggponet.h>
#include <imgui.h>
#include <imgui_impl_opengl2.h>
#include <imgui_impl_sdl.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <gl/GL.h>

#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

typedef void (APIENTRYP PFNGLUSEPROGRAMPROC) (unsigned int);

typedef struct GgpoHandles
{
	GGPOSession* session;
	GGPOPlayerHandle local_player;
} GgpoHandles;

GgpoHandles ggpo_handles;

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

// remotes[i] corresponds with connection_report.connections[i]
GGPOPlayerHandle remotes[MAX_REMOTES];

ConnectionReport connection_report;

void set_connection_state(GGPOPlayerHandle handle, ConnectionState state) {
	for (int i = 0; i < connection_report.num_players; i++) {
		if (remotes[i] == handle) {
			connection_report.players[i].connect_progress = 0;
			connection_report.players[i].state = state;
			break;
		}
	}
}

void update_connect_progress(GGPOPlayerHandle handle, int progress) {
	for (int i = 0; i < connection_report.num_players; i++) {
		if (remotes[i] == handle) {
			connection_report.players[i].connect_progress = progress;
			break;
		}
	}
}

bool __cdecl on_event(GGPOEvent* info)
{
	int progress;

	switch (info->code) {
	case GGPO_EVENTCODE_CONNECTED_TO_PEER:
		set_connection_state(
			info->u.connected.player, CONNECTION_STATE_Synchronizing);
		break;
	case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
		progress =
			100 * info->u.synchronizing.count / info->u.synchronizing.total;
		update_connect_progress(info->u.synchronizing.player, progress);
		break;
	case GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER:
		update_connect_progress(info->u.synchronized.player, 100);
		break;
	case GGPO_EVENTCODE_RUNNING:
		for (int i = 0; i < connection_report.num_players; i++) {
			connection_report.players[i].state = CONNECTION_STATE_Running;
		}
		// renderer->SetStatusText("");
		break;
	case GGPO_EVENTCODE_CONNECTION_INTERRUPTED:
		for (int i = 0; i < connection_report.num_players; i++) {
			if (remotes[i] == info->u.connection_interrupted.player) {
				connection_report.players[i].disconnect_start =
					SDL_GetTicks();
				connection_report.players[i].disconnect_timeout =
					info->u.connection_interrupted.disconnect_timeout;
				connection_report.players[i].state =
					CONNECTION_STATE_Disconnecting;
				break;
			}
		}
		break;
	case GGPO_EVENTCODE_CONNECTION_RESUMED:
		set_connection_state(
			info->u.connection_resumed.player, CONNECTION_STATE_Running);
		break;
	case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
		set_connection_state(
			info->u.disconnected.player, CONNECTION_STATE_Disconnected);
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
	SDL_Renderer* renderer;
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
		sprintf_s(
			logbuf, ARRAYSIZE(logbuf), "Disconnected player %d.\n", player);
	}
	else {
		sprintf_s(
			logbuf,
			ARRAYSIZE(logbuf),
			"Error while disconnecting player (err:%d).\n",
			result);
	}

	// renderer->SetStatusText(logbuf);
}

void disconnect_player(int player, GGPOSession* ggpo)
{
	if (player < connection_report.num_players) {
		GGPOErrorCode result =
			ggpo_disconnect_player(ggpo, remotes[player]);

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
	(void)flags;
	int disconnect_flags = 0;
	LocalInput inputs[MAX_PLAYERS] = { 0 };

	GGPOErrorCode result = ggpo_synchronize_input(
		ggpo_handles.session,
		(void*)inputs,
		sizeof(LocalInput) * MAX_PLAYERS,
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

		draw_game(sdl_handles.renderer, &connection_report);
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

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

	return { window, gl_context, glUseProgram, renderer };
}

void tear_down_sdl(SdlHandles handles)
{
	SDL_GL_DeleteContext(handles.gl_context);
	SDL_DestroyWindow(handles.window);
	SDL_DestroyRenderer(handles.renderer);
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

	if (strcmp(args[offset], "spectate") == 0) {
		init->role_init.type = ROLE_TYPE_Spectator;

		int result = sscanf_s(
			args[offset + 1],
			"%[^:]:%hu",
			init->role_init.spectator_init.host_ip,
			(unsigned int)ARRAYSIZE(init->role_init.spectator_init.host_ip),
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

			int result = sscanf_s(
				arg,
				"%[^:]:%hd",
				players[i].u.remote.ip_address,
				(unsigned int)ARRAYSIZE(players[i].u.remote.ip_address),
				&players[i].u.remote.port);

			if (result != 2) {
				show_syntax_error();

				return 1;
			}
		}

		init->role_init.player_init.num_spectators = 0;

		while (offset < argc) {
			players[i].type = GGPO_PLAYERTYPE_SPECTATOR;

			int result = sscanf_s(
				args[offset++],
				"%[^:]:%hd",
				players[i].u.remote.ip_address,
				(unsigned int)ARRAYSIZE(players[i].u.remote.ip_address),
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

bool __cdecl begin_game_callback(const char* game)
{
	return begin_game(game);
}

bool __cdecl load_game_state_callback(unsigned char* buffer, int len)
{
	return load_game_state(buffer, len);
}

bool __cdecl save_game_state_callback(unsigned char** buffer, int* len, int* checksum, int frame)
{
	return save_game_state(buffer, len, checksum, frame);
}

void __cdecl free_buffer_callback(void* buffer)
{
	free_game_state(buffer);
}

bool __cdecl log_game_state_callback(char* filename, unsigned char* buffer, int len)
{
	return log_game_state(filename, buffer, len);
}

GgpoHandles setup_ggpo(ClientInit init, SDL_Window* window)
{
	WSADATA wd = { 0 };
	WSAStartup(MAKEWORD(2, 2), &wd);

	GgpoHandles handles;
	GGPOSessionCallbacks cb;;
	cb.on_event = on_event;
	cb.advance_frame = advance_frame;
	cb.begin_game = begin_game_callback;
	cb.load_game_state = load_game_state_callback;
	cb.free_buffer = free_buffer_callback;
	cb.log_game_state = log_game_state_callback;
	cb.save_game_state = save_game_state_callback;

	connection_report.num_players = init.num_players;

	if (init.role_init.type == ROLE_TYPE_Spectator) {
		SpectatorInit spectator_init = init.role_init.spectator_init;

		ggpo_start_spectating(
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

		remotes[i] = handle;

		// HACK: Slightly fragile cast.
		connection_report.players[i].type = (ConnectionType)player_init.players[i].type;

		if (player_init.players[i].type == GGPO_PLAYERTYPE_LOCAL) {
			handles.local_player = handle;
			connection_report.players[i].connect_progress = 100;
			set_connection_state(handle, CONNECTION_STATE_Connecting);
			ggpo_set_frame_delay(handles.session, handle, 2);
		}
		else {
			connection_report.players[i].connect_progress = 0;
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