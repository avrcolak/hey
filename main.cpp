#if defined(_DEBUG)
#   include <crtdbg.h>
#endif
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

PFNGLUSEPROGRAMPROC glUseProgram;

void
PostQuitMessageShim()
{
	SDL_Event e;
	e.type = SDL_QUIT;
	SDL_PushEvent(&e);
}

void
MainWindowProc(SDL_Window* window, SDL_Event e)
{
	switch (e.type) {
	case SDL_KEYDOWN:
		if (e.key.keysym.sym == SDLK_p) {
		}
		else if (e.key.keysym.sym == SDLK_ESCAPE) {
			PostQuitMessageShim();
		}
		else if (e.key.keysym.sym >= SDLK_F1 && e.key.keysym.sym <= SDLK_F12) {
			VectorWar_DisconnectPlayer((int)(e.key.keysym.sym - SDLK_F1));
		}
		break;
	case SDL_WINDOWEVENT:
		switch (e.window.event)
		{
		case SDL_WINDOWEVENT_EXPOSED:
			SDL_UpdateWindowSurface(window);
			break;
		case SDL_WINDOWEVENT_CLOSE:
			PostQuitMessageShim();
		}
		break;
	}
}

void
RunMainLoop(SDL_Window* window)
{
	int start, next, now;

	start = next = now = SDL_GetTicks();

	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	while (1) {
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0) {
			ImGui_ImplSDL2_ProcessEvent(&e);
			MainWindowProc(window, e);
			if (e.type == SDL_QUIT)
			{
				return;
			}
		}

		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &show_another_window);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		// 3. Show another simple window.
		if (show_another_window)
		{
			ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("Hello from another window!");
			if (ImGui::Button("Close Me"))
				show_another_window = false;
			ImGui::End();
		}

		now = SDL_GetTicks();
		VectorWar_Idle(max(0, next - now - 1));
		if (now >= next) {
			VectorWar_RunFrame(window);

			ImGui::Render();
			//glViewport(0, 0, 640, 480);
			//glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			//glClear(GL_COLOR_BUFFER_BIT);
			glUseProgram(0);
			ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

			SDL_GL_SwapWindow(window);
			next = now + (1000 / 60);
		}
	}
}

void
Syntax()
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
		"Syntax: vectorwar.exe <local port> <num players> ('local' | <remote ip>:<remote port>)*\n",
		"Could not start", NULL);
}

int main(int argc, char* args[])
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

	SDL_Window* window = SDL_CreateWindow("vectorwar", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, window_flags);

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);

	glUseProgram = (PFNGLUSEPROGRAMPROC) SDL_GL_GetProcAddress("glUseProgram");

	if (SDL_GL_SetSwapInterval(-1) != 0)
	{
		SDL_GL_SetSwapInterval(1);
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL2_Init();

	int offset = 1, local_player = 0;
	WSADATA wd = { 0 };
	char wide_ip_buffer[128];
	unsigned int wide_ip_buffer_size = (unsigned int)ARRAYSIZE(wide_ip_buffer);

	WSAStartup(MAKEWORD(2, 2), &wd);

	SDL_Point window_offsets[] = {
		{ 64,  64 },   // player 1
		{ 740, 64 },   // player 2
		{ 64,  600 },  // player 3
		{ 740, 600 },  // player 4
	};

#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	if (argc < 3) {
		Syntax();
		return 1;
	}
	unsigned short local_port = (unsigned short)atoi(args[offset++]);
	int num_players = atoi(args[offset++]);
	if (num_players < 0 || argc < offset + num_players) {
		Syntax();
		return 1;
	}
	if (strcmp(args[offset], "spectate") == 0) {
		char host_ip[128];
		unsigned short host_port;
		if (sscanf_s(args[offset + 1], "%[^:]:%hu", wide_ip_buffer, wide_ip_buffer_size, &host_port) != 2) {
			Syntax();
			return 1;
		}
		strcpy_s(host_ip, wide_ip_buffer);
		VectorWar_InitSpectator(window, local_port, num_players, host_ip, host_port);
	}
	else {
		GGPOPlayer players[GGPO_MAX_SPECTATORS + GGPO_MAX_PLAYERS];

		int i;
		for (i = 0; i < num_players; i++) {
			const char* arg = args[offset++];

			players[i].size = sizeof(players[i]);
			players[i].player_num = i + 1;
			if (!strcmp(arg, "local")) {
				players[i].type = GGPO_PLAYERTYPE_LOCAL;
				local_player = i;
				continue;
			}

			players[i].type = GGPO_PLAYERTYPE_REMOTE;
			if (sscanf_s(arg, "%[^:]:%hd", wide_ip_buffer, wide_ip_buffer_size, &players[i].u.remote.port) != 2) {
				Syntax();
				return 1;
			}
			strcpy_s(players[i].u.remote.ip_address, wide_ip_buffer);
		}
		// these are spectators...
		int num_spectators = 0;
		while (offset < argc) {
			players[i].type = GGPO_PLAYERTYPE_SPECTATOR;
			if (sscanf_s(args[offset++], "%[^:]:%hd", wide_ip_buffer, wide_ip_buffer_size, &players[i].u.remote.port) != 2) {
				Syntax();
				return 1;
			}
			strcpy_s(players[i].u.remote.ip_address, wide_ip_buffer);
			i++;
			num_spectators++;
		}

		if (local_player < sizeof(window_offsets) / sizeof(window_offsets[0])) {
			SDL_SetWindowPosition(window, window_offsets[local_player].x, window_offsets[local_player].y);
		}

		VectorWar_Init(window, local_port, num_players, players, num_spectators);
	}

	RunMainLoop(window);
	VectorWar_Exit();
	WSACleanup();

	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}