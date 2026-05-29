#include <iostream>
#include <string>
#include <omp.h>
#include <opencv2/opencv.hpp>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

struct AppContext {
    SDL_Window* window;
    int window_w;
    int window_h;
    int display_w;
    int display_h;
    std::string dropped_file_path = "Brak upuszczonego pliku. Przeciągnij plik tutaj!";
    GLuint background_texture = 0;
};

GLuint MatToTexture(const cv::Mat& mat) {
    if (mat.empty()) return 0;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    cv::Mat rgbMat;
    cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbMat.cols, rgbMat.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbMat.data);

    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

void RenderFrame(AppContext& ctx) {
    SDL_GetWindowSize(ctx.window, &ctx.window_w, &ctx.window_h);
    SDL_GetWindowSizeInPixels(ctx.window, &ctx.display_w, &ctx.display_h);

    ImGuiIO& io = ImGui::GetIO();
    if (ctx.window_w > 0 && ctx.window_h > 0) {
        io.DisplaySize = ImVec2((float)ctx.window_w, (float)ctx.window_h);
        io.DisplayFramebufferScale = ImVec2((float)ctx.display_w / ctx.window_w, (float)ctx.display_h / ctx.window_h);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (ctx.background_texture != 0) {
        ImDrawList* background_draw_list = ImGui::GetBackgroundDrawList();
        ImVec2 p_min = ImVec2(0.0f, 0.0f);
        ImVec2 p_max = ImVec2((float)ctx.window_w, (float)ctx.window_h);
        background_draw_list->AddImage((ImTextureID)(intptr_t)ctx.background_texture, p_min, p_max);
    }

    ImGui::Begin("Panel kontrolny");
    ImGui::Text("Witaj w aplikacji C++!");
    ImGui::Text("Rozdzielczosc okna: %d x %d", ctx.window_w, ctx.window_h);
    ImGui::End();

    ImGui::Begin("Obsługa plików");
    ImGui::TextWrapped("Ścieżka do ostatnio upuszczonego pliku:");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", ctx.dropped_file_path.c_str());
    ImGui::End();

    ImGui::Render();
    glViewport(0, 0, ctx.display_w, ctx.display_h);

    glClearColor(0.15f, 0.15f, 0.15f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(ctx.window);
}

bool SDLCALL WindowEventWatcher(void* userdata, SDL_Event* event) {
    AppContext* ctx = static_cast<AppContext*>(userdata);
    SDL_WindowID window_id = SDL_GetWindowID(ctx->window);

    if (event->window.windowID == window_id) {
        if (event->type == SDL_EVENT_WINDOW_EXPOSED ||
            event->type == SDL_EVENT_WINDOW_RESIZED ||
            event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {

            RenderFrame(*ctx);
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    cv::Mat image = cv::Mat::zeros(300, 300, CV_8UC3);
    cv::circle(image, cv::Point(150, 150), 100, cv::Scalar(0, 255, 0), -1);

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "Blad SDL_Init: " << SDL_GetError() << std::endl;
        return -1;
    }

#pragma omp parallel
    {
#pragma omp master
        std::cout << "Liczba dostepnych watkow OpenMP: " << omp_get_num_threads() << std::endl;
    }

    const char* glsl_version = "#version 130";
#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    glsl_version = "#version 150";
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_Window* window = SDL_CreateWindow("Projekt C++ (SDL3 + ImGui + OpenCV + OpenMP)", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Nie udalo sie utworzyc okna: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
#if defined(__APPLE__)
    SDL_GL_SetSwapInterval(0);
#else
    SDL_GL_SetSwapInterval(1);
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    AppContext ctx{};
    ctx.window = window;
    SDL_GetWindowSize(window, &ctx.window_w, &ctx.window_h);
    SDL_GetWindowSizeInPixels(window, &ctx.display_w, &ctx.display_h);
    ctx.background_texture = MatToTexture(image);
    const SDL_WindowID window_id = SDL_GetWindowID(window);

    SDL_AddEventWatch(WindowEventWatcher, &ctx);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT)
                running = false;

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window_id)
                running = false;

            if (event.type == SDL_EVENT_DROP_FILE) {
                if (event.drop.data != nullptr) {
                    ctx.dropped_file_path = std::string(event.drop.data);

                    cv::Mat new_image = cv::imread(ctx.dropped_file_path);
                    if (!new_image.empty()) {
                        if (ctx.background_texture != 0) {
                            glDeleteTextures(1, &ctx.background_texture);
                            ctx.background_texture = 0;
                        }
                        ctx.background_texture = MatToTexture(new_image);
                    }

                }
            }
        }

        RenderFrame(ctx);
    }

    SDL_RemoveEventWatch(WindowEventWatcher, &ctx);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (ctx.background_texture != 0) {
        glDeleteTextures(1, &ctx.background_texture);
        ctx.background_texture = 0;
    }

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}