#include <iostream>
#include <chrono>
#include <thread>

#include <SDL.h>
#include <GL/glew.h>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "imfilebrowser.h"
#include "panaroiacontroller.h"

void displayGameWindow(PanaroiaController &controller);
void displayKeypad(PanaroiaController &controller);
void displayRomController(PanaroiaController &controller, ImGui::FileBrowser &fileDialog);

// copied from imgui sdl sample
int main(int, char**)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "Error: "  << SDL_GetError() << std::endl;
        return -1;
    }

    PanaroiaController controller;

    const char* glslVersion = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Panaroia lib example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, windowFlags);
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(0);

    bool err = glewInit() != GLEW_OK;

    if (err) {
        std::cerr << "Failed to initialize OpenGL loader!" << std::endl;
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init(glslVersion);

    ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ImGui::FileBrowser fileDialog;
    fileDialog.SetTitle("Select the ROM file...");
    fileDialog.ClearSelected();

    // Main loop
    bool done = false;
    while (!done) {
        controller.step();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;

            if (!controller.running()) {
                break;
            }

            if (event.type == SDL_KEYDOWN) {
                controller.keyDown(event.key.keysym.sym);
            } else if (event.type == SDL_KEYUP) {
                controller.keyUp(event.key.keysym.sym);
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        displayRomController(controller, fileDialog);

        if (fileDialog.HasSelected()) {
            controller.setCurrentRom(fileDialog.GetSelected().string());
            fileDialog.ClearSelected();
        }

        displayGameWindow(controller);

        displayKeypad(controller);

        fileDialog.Display();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        std::this_thread::sleep_for(std::chrono::microseconds(1200));
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

void displayKeypad(PanaroiaController &controller)
{
    ImGui::SetNextWindowSize(ImVec2(130, 150), ImGuiCond_Always);
    ImGui::Begin("Keypad");

    static bool showSdlMappings = false;
    ImGui::Checkbox("SDL Mappings", &showSdlMappings);

    auto buttons = { 0x1, 0x2, 0x3, 0xc,
                     0x4, 0x5, 0x6, 0xd,
                     0x7, 0x8, 0x9, 0xe,
                     0xa, 0x0, 0xb, 0xf};
    int i = 1;
    for (auto button : buttons) {
        char state = controller.inputState(button);
        if (state == 1) {
            ImGui::PushStyleColor(ImGuiCol_Button,(ImVec4)ImColor::HSV(7.0f, 0.6f, 0.6f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,(ImVec4)ImColor::HSV(0.0f, 0.0f, 0.0f));
        }

        std::stringstream stream;
        stream << std::hex << button;
        ImGui::Button(showSdlMappings ? SDL_GetKeyName(controller.keyMapping(button)) : stream.str().c_str());
        if (i++ % 4 != 0) {
            ImGui::SameLine();
        }
    }

    ImGui::End();
}

void displayGameWindow(PanaroiaController &controller)
{
    ImGui::SetNextWindowSize(ImVec2(655, 360), ImGuiCond_Always);
    ImGui::Begin("Game window");
    unsigned char *screen = controller.screen();

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int i = 0; i < 64; ++i) {
        for (int j = 0; j < 32; ++j) {
            ImVec2 topLeft = ImVec2(pos.x + i * 10, pos.y + j * 10.0);
            ImVec2 bottomRight = ImVec2(topLeft.x + 10.0, topLeft.y + 10.0);
            unsigned short pnriaIndex = i + j * 64;

            if (screen[pnriaIndex] == 1) {
                drawList->AddRectFilled(topLeft, bottomRight, IM_COL32(255, 0, 255, 255));
            }
        }
    }

    ImGui::End();
}

void displayRomController(PanaroiaController &controller, ImGui::FileBrowser &fileDialog)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(150, 70), ImGuiCond_FirstUseEver);

    static bool romControl;
    ImGui::Begin("ROM", &romControl, ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Click here..."))
        {
            if (ImGui::MenuItem("Open..")) {
                fileDialog.Open();
            }

            if (ImGui::MenuItem("Restart")) {
                controller.reset();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();

        ImGui::Text("Current ROM: %s", controller.currentRom().c_str());
    }

    ImGui::End();
}
