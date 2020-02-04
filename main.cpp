#include <ggponet.h>
#include <imgui.h>
#include <imgui_impl_opengl2.h>
#include <imgui_impl_sdl.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <gl/GL.h>
#include "connection_report.h"
#include "game.h"
#include "utils.h"

enum ROLE_TYPE
{
	ROLE_TYPE_Spectator,
	ROLE_TYPE_Player,
};

typedef struct ClientInit
{
	unsigned short local_port;
	int num_players;
	ROLE_TYPE type;
	union
	{
		struct
		{
			char host_ip[128];
			unsigned short host_port;
		};
		struct
		{
			GGPOPlayer players[GGPO_MAX_SPECTATORS + GGPO_MAX_PLAYERS];
			int local_player;
			int num_spectators;
		};
	};
} ClientInit;

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

typedef struct GgpoHandles
{
	GGPOSession* session;
	GGPOPlayerHandle local_player;
} GgpoHandles;

typedef void (APIENTRY* PFNGLUSEPROGRAMPROC) (unsigned int);

typedef struct SdlHandles
{
	SDL_Window* window;
	SDL_GLContext gl_context;
	PFNGLUSEPROGRAMPROC glUseProgram;
	SDL_Renderer* renderer;
} SdlHandles;

static ConnectionReport connection_report;

static GgpoHandles ggpo;

// remotes[i] corresponds with connection_report.connections[i]
static GGPOPlayerHandle remotes[MAX_PARTICIPANTS];

static FrameReport frame_report;

static void set_connection_state(GGPOPlayerHandle handle, CONNECTION_STATE state)
{
	for (int i = 0; i < connection_report.num_participants; i++)
	{
		if (remotes[i] == handle)
		{
			connection_report.participants[i].connect_progress = 0;
			connection_report.participants[i].state = state;
			break;
		}
	}
}

static void update_connect_progress(GGPOPlayerHandle handle, int progress)
{
	for (int i = 0; i < connection_report.num_participants; i++)
	{
		if (remotes[i] == handle)
		{
			connection_report.participants[i].connect_progress = progress;
			break;
		}
	}
}

static bool __cdecl on_event(GGPOEvent *info)
{
	switch (info->code)
	{
	case GGPO_EVENTCODE_CONNECTED_TO_PEER:
		set_connection_state(
			info->u.connected.player, CONNECTION_STATE_synchronizing);
		break;

	case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
		update_connect_progress(
			info->u.synchronizing.player,
			info->u.synchronizing.count / info->u.synchronizing.total);
		break;

	case GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER:
		update_connect_progress(info->u.synchronized.player, 100);
		break;

	case GGPO_EVENTCODE_RUNNING:
		for (int i = 0; i < connection_report.num_participants; i++)
		{
			connection_report.participants[i].state = CONNECTION_STATE_running;
		}
		strcpy_s(connection_report.status, "");
		break;

	case GGPO_EVENTCODE_CONNECTION_INTERRUPTED:
		for (int i = 0; i < connection_report.num_participants; i++)
		{
			if (remotes[i] == info->u.connection_interrupted.player)
			{
				connection_report.participants[i].disconnect_start =
					SDL_GetTicks();
				connection_report.participants[i].disconnect_timeout =
					info->u.connection_interrupted.disconnect_timeout;
				connection_report.participants[i].state =
					CONNECTION_STATE_disconnecting;
				break;
			}
		}
		break;

	case GGPO_EVENTCODE_CONNECTION_RESUMED:
		set_connection_state(
			info->u.connection_resumed.player, CONNECTION_STATE_running);
		break;

	case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
		set_connection_state(
			info->u.disconnected.player, CONNECTION_STATE_disconnected);
		break;

	case GGPO_EVENTCODE_TIMESYNC:
		SDL_Delay(1000 * info->u.timesync.frames_ahead / 60);
		break;
	}

	return true;
}

static void setup_imgui_frame(SdlHandles handles)
{
	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame(handles.window);
	ImGui::NewFrame();
}

void draw_centered_text(SdlHandles handles, const char* str, int y)
{
	int w, h;
	SDL_GetWindowSize(handles.window, &w, &h);

	ImGui::GetForegroundDrawList()->AddText(
		ImVec2(w / 2 - ImGui::CalcTextSize(str).x / 2, (float)y),
		IM_COL32_WHITE,
		str);
}

void draw_checksum(SdlHandles handles, FrameInfo frame_info, int y)
{
	char checksum[128];

	sprintf_s(
		checksum,
		COUNT_OF(checksum), 
		"Frame: %04d Checksum: %08x", 
		frame_info.number,
		frame_info.hash);

	draw_centered_text(handles, checksum, y);
}

static void draw_gui(SdlHandles handles)
{
	draw_checksum(handles, frame_report.periodic, 18);
	draw_checksum(handles, frame_report.current, 34);
	draw_centered_text(handles, connection_report.status, 448);

	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	if (show_demo_window)
	{
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

		if (ImGui::Button("Button"))
		{
			counter++;
		}

		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);
		ImGui::Text(
			"Application average %.3f ms/frame (%.1f FPS)",
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		ImGui::End();
	}

	if (show_another_window)
	{
		ImGui::Begin("Another Window", &show_another_window);

		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
		{
			show_another_window = false;
		}

		ImGui::End();
	}

	ImGui::Render();
	handles.glUseProgram(0);
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

static void show_disconnected_player(GGPOErrorCode result, int player)
{
	char logbuf[128];

	if (GGPO_SUCCEEDED(result))
	{
		sprintf_s(
			logbuf, sizeof logbuf, "Disconnected player %d.\n", player);
	}
	else
	{
		sprintf_s(
			logbuf,
			sizeof logbuf,
			"Error while disconnecting player (err:%d).\n",
			result);
	}

	strcpy_s(connection_report.status, logbuf);
}

static void disconnect_player(int player)
{
	if (player < connection_report.num_participants)
	{
		GGPOErrorCode result = ggpo_disconnect_player(ggpo.session, remotes[player]);

		show_disconnected_player(result, player);
	}
}

static int client_process_event(SDL_Event e, SdlHandles sdl)
{
	switch (e.type) {
	case SDL_QUIT:
		return 1;
		break;

	case SDL_KEYDOWN:
		if (e.key.keysym.sym == SDLK_p)
		{
		}
		else if (e.key.keysym.sym == SDLK_ESCAPE)
		{
			return 1;
		}
		else if (e.key.keysym.sym >= SDLK_F1 && e.key.keysym.sym <= SDLK_F12)
		{
			disconnect_player(
				(int)(e.key.keysym.sym - SDLK_F1));
		}
		break;

	case SDL_WINDOWEVENT:
		switch (e.window.event)
		{
		case SDL_WINDOWEVENT_EXPOSED:
			SDL_UpdateWindowSurface(sdl.window);
			break;
		case SDL_WINDOWEVENT_CLOSE:
			return 1;
		}
		break;
	}

	ImGui_ImplSDL2_ProcessEvent(&e);

	return 0;
}

static void update_frame_report()
{
	frame_report.current.number = game_frame_number();
	frame_report.current.hash = game_state_hash();

	if ((frame_report.current.number % 90) == 0)
	{
		frame_report.periodic = frame_report.current;
	}
}

static bool __cdecl advance_frame(int flags)
{
	(void)flags;

	int disconnect_flags = 0;
	LocalInput inputs[MAX_PLAYERS] = { 0 };

	GGPOErrorCode result = ggpo_synchronize_input(
		ggpo.session,
		(void*)inputs,
		sizeof(LocalInput) * MAX_PLAYERS,
		&disconnect_flags);

	if (GGPO_SUCCEEDED(result))
	{
		step_game(inputs, disconnect_flags);
		ggpo_advance_frame(ggpo.session);
		update_frame_report();

		return true;
	}

	return false;
}

static void work(LocalInput *input)
{
	capture_input_state(input);

	GGPOErrorCode result = ggpo_add_local_input(
		ggpo.session,
		ggpo.local_player,
		input,
		sizeof(LocalInput));

	if (GGPO_SUCCEEDED(result))
	{
		advance_frame(0);
	}
}

static void main_loop(SdlHandles sdl)
{
	frame_report = { 0 };

	int start, next, now;
	start = next = now = SDL_GetTicks();

	LocalInput local_input = { 0 };

	while (1)
	{
		SDL_Event e;

		while (SDL_PollEvent(&e) != 0)
		{
			int quit = client_process_event(e, sdl);

			if (quit != 0)
			{
				return;
			}

			buffer_event(&e, &local_input);
		}

		now = SDL_GetTicks();
		ggpo_idle(ggpo.session, max(0, next - now - 1));

		if (now >= next)
		{
			work(&local_input);
			local_input = { 0 };
			next = now + (1000 / 60);
		}

		setup_imgui_frame(sdl);
		draw_game(sdl.renderer, &connection_report);
		draw_gui(sdl);

		SDL_GL_SwapWindow(sdl.window);
	}
}

static SdlHandles setup_sdl()
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	SDL_Window* window = SDL_CreateWindow(
		GAME_NAME,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		640,
		480,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);

	PFNGLUSEPROGRAMPROC glUseProgram =
		(PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");

	if (SDL_GL_SetSwapInterval(-1) != 0)
	{
		SDL_GL_SetSwapInterval(1);
	}

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

	return { window, gl_context, glUseProgram, renderer };
}

static void tear_down_sdl(SdlHandles sdl)
{
	SDL_GL_DeleteContext(sdl.gl_context);
	SDL_DestroyWindow(sdl.window);
	SDL_DestroyRenderer(sdl.renderer);
	SDL_Quit();
}

static void setup_imgui(SdlHandles sdl)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(sdl.window, sdl.gl_context);
	ImGui_ImplOpenGL2_Init();
}

static void tear_down_imgui()
{
	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

static void show_syntax_error()
{
	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		"Syntax: hey.exe <local port> <num players> ('local' | <remote ip>:<remote port>)*\n",
		"Could not start",
		NULL);
}

static int parse_args(int argc, char* args[], ClientInit *init)
{
	if (argc < 3)
	{
		show_syntax_error();
		return 1;
	}

	int offset = 1;

	init->local_port = (unsigned short)atoi(args[offset]);
	offset++;

	init->num_players = atoi(args[offset]);
	offset++;

	if (init->num_players < 0 || argc < offset + init->num_players)
	{
		show_syntax_error();
		return 1;
	}

	if (strcmp(args[offset], "spectate") == 0)
	{
		init->type = ROLE_TYPE_Spectator;

		int result = sscanf_s(
			args[offset + 1],
			"%[^:]:%hu",
			init->host_ip,
			(unsigned int)sizeof init->host_ip,
			&init->host_port);

		if (result != 2)
		{
			show_syntax_error();
			return 1;
		}

		return 0;
	}

	init->type = ROLE_TYPE_Player;

	GGPOPlayer* players = init->players;
	int i;
	for (i = 0; i < init->num_players; i++)
	{
		const char* arg = args[offset];
		offset++;

		players[i].size = sizeof(players[i]);
		players[i].player_num = i + 1;

		if (!strcmp(arg, "local"))
		{
			players[i].type = GGPO_PLAYERTYPE_LOCAL;
			init->local_player = i;

			continue;
		}

		players[i].type = GGPO_PLAYERTYPE_REMOTE;

		int result = sscanf_s(
			arg,
			"%[^:]:%hd",
			players[i].u.remote.ip_address,
			(unsigned int)sizeof players[i].u.remote.ip_address,
			&(players[i].u.remote.port));

		if (result != 2)
		{
			show_syntax_error();
			return 1;
		}
	}

	init->num_spectators = 0;

	while (offset < argc)
	{
		players[i].type = GGPO_PLAYERTYPE_SPECTATOR;

		int result = sscanf_s(
			args[offset],
			"%[^:]:%hd",
			players[i].u.remote.ip_address,
			(unsigned int)sizeof players[i].u.remote.ip_address,
			&players[i].u.remote.port);

		if (result != 2)
		{
			show_syntax_error();
			return 1;
		}

		offset++;
		i++;
		init->num_spectators++;
	}

	return 0;
}

static void adjust_window(SdlHandles const sdl, ClientInit const init)
{
	SDL_Point window_offsets[] = {
		{ 64,  64 },
		{ 740, 64 },
		{ 64,  600 },
		{ 740, 600 },
	};

	if (init.local_player < COUNT_OF(window_offsets))
	{
		SDL_SetWindowPosition(
			sdl.window,
			window_offsets[init.local_player].x,
			window_offsets[init.local_player].y);
	}
}

static bool __cdecl begin_game_callback(const char *game)
{
	return begin_game(game);
}

static bool __cdecl load_game_state_callback(unsigned char *buffer, int len)
{
	return load_game_state(buffer, len);
}

static bool __cdecl save_game_state_callback(
	unsigned char **buffer, int *len, int *checksum, int frame)
{
	return save_game_state(buffer, len, checksum, frame);
}

static void __cdecl free_buffer_callback(void *buffer)
{
	free_game_state(buffer);
}

static bool __cdecl log_game_state_callback(
	char *filename, unsigned char *buffer, int len)
{
	return log_game_state(filename, buffer, len);
}

static void setup_ggpo(ClientInit init)
{
	WSADATA wd = { 0 };
	WSAStartup(MAKEWORD(2, 2), &wd);

	GgpoHandles handles;
	GGPOSessionCallbacks cb;
	cb.on_event = on_event;
	cb.advance_frame = advance_frame;
	cb.begin_game = begin_game_callback;
	cb.load_game_state = load_game_state_callback;
	cb.free_buffer = free_buffer_callback;
	cb.log_game_state = log_game_state_callback;
	cb.save_game_state = save_game_state_callback;

	connection_report.num_participants = init.num_players;

	if (init.type == ROLE_TYPE_Spectator)
	{
		ggpo_start_spectating(
			&handles.session,
			&cb,
			GAME_NAME,
			init.num_players,
			sizeof(LocalInput),
			init.local_port,
			init.host_ip,
			init.host_port);

		strcpy_s(connection_report.status, "Starting new spectator session.");

		ggpo = handles;
		return;
	}

	GGPOErrorCode result = ggpo_start_session(
		&handles.session,
		&cb,
		GAME_NAME,
		init.num_players,
		sizeof(LocalInput),
		init.local_port);

	ggpo_set_disconnect_timeout(handles.session, 3000);
	ggpo_set_disconnect_notify_start(handles.session, 1000);

	for (int i = 0; i < init.num_players + init.num_spectators; i++)
	{
		GGPOPlayerHandle handle;

		result = ggpo_add_player(
			handles.session, init.players + i, &handle);

		remotes[i] = handle;

		// HACK: Slightly fragile cast.
		connection_report.participants[i].type =
			(PARTICIPANT_TYPE)init.players[i].type;

		if (init.players[i].type == GGPO_PLAYERTYPE_LOCAL)
		{
			handles.local_player = handle;
			connection_report.participants[i].connect_progress = 100;
			set_connection_state(handle, CONNECTION_STATE_connecting);
			ggpo_set_frame_delay(handles.session, handle, 2);
		}
		else
		{
			connection_report.participants[i].connect_progress = 0;
		}
	}

	strcpy_s(connection_report.status, "Connecting to peers.");

	ggpo = handles;
}

static void tear_down_ggpo()
{
	if (ggpo.session)
	{
		ggpo_close_session(ggpo.session);
		ggpo.session = NULL;
	}

	WSACleanup();
}

int main(int argc, char* args[])
{
	SdlHandles sdl = setup_sdl();

	ClientInit init;
	int result = parse_args(argc, args, &init);

	if (result != 0)
	{
		return result;
	}

	adjust_window(sdl, init);
	setup_ggpo(init);
	setup_imgui(sdl);
	setup_game(sdl.window, init.num_players);

	main_loop(sdl);

	tear_down_game();
	tear_down_imgui();
	tear_down_ggpo();
	tear_down_sdl(sdl);

	return 0;
}
