#include <iostream>
#include <string>
#include <omp.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

struct AppContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    int window_w = 0;
    int window_h = 0;

    std::string dropped_file_path =
        "Brak upuszczonego pliku. Przeciągnij plik tutaj!";

    SDL_Texture* background_texture = nullptr;
};

SDL_Texture* MatToTexture(SDL_Renderer* renderer, const cv::Mat& mat) {
    if (!renderer || mat.empty()) {
        return nullptr;
    }

    cv::Mat rgbMat;

    if (mat.channels() == 3) {
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
    } else if (mat.channels() == 4) {
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGRA2RGBA);
    } else if (mat.channels() == 1) {
        cv::cvtColor(mat, rgbMat, cv::COLOR_GRAY2RGB);
    } else {
        return nullptr;
    }

    SDL_PixelFormat format =
        rgbMat.channels() == 4 ? SDL_PIXELFORMAT_RGBA32 : SDL_PIXELFORMAT_RGB24;

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        format,
        SDL_TEXTUREACCESS_STATIC,
        rgbMat.cols,
        rgbMat.rows
    );

    if (!texture) {
        std::cerr << "Nie udało się utworzyć tekstury SDL: "
                  << SDL_GetError() << std::endl;
        return nullptr;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);

    const int pitch = static_cast<int>(rgbMat.step);
    if (!SDL_UpdateTexture(texture, nullptr, rgbMat.data, pitch)) {
        std::cerr << "Nie udało się zaktualizować tekstury SDL: "
                  << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    return texture;
}

void DestroyBackgroundTexture(AppContext& ctx) {
    if (ctx.background_texture) {
        SDL_DestroyTexture(ctx.background_texture);
        ctx.background_texture = nullptr;
    }
}

void RenderFrame(AppContext& ctx) {
    SDL_GetWindowSize(ctx.window, &ctx.window_w, &ctx.window_h);

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (ctx.background_texture) {
        ImDrawList* background_draw_list = ImGui::GetBackgroundDrawList();

        ImVec2 p_min = ImVec2(0.0f, 0.0f);
        ImVec2 p_max = ImVec2(
            static_cast<float>(ctx.window_w),
            static_cast<float>(ctx.window_h)
        );

        background_draw_list->AddImage(
        (ImTextureID)(intptr_t)ctx.background_texture,
                p_min,
                p_max
            );
    }

    ImGui::Begin("Panel kontrolny");
    ImGui::Text("Witaj w aplikacji C++!");
    ImGui::Text("Rozdzielczosc okna: %d x %d", ctx.window_w, ctx.window_h);
    ImGui::End();

    ImGui::Begin("Obsługa plików");
    ImGui::TextWrapped("Ścieżka do ostatnio upuszczonego pliku:");
    ImGui::Separator();
    ImGui::TextColored(
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
        "%s",
        ctx.dropped_file_path.c_str()
    );
    ImGui::End();

    ImGui::Render();

    SDL_SetRenderDrawColor(ctx.renderer, 38, 38, 38, 255);
    SDL_RenderClear(ctx.renderer);

    ImGui_ImplSDLRenderer3_RenderDrawData(
        ImGui::GetDrawData(),
        ctx.renderer
    );

    SDL_RenderPresent(ctx.renderer);
}

int main(int argc, char* argv[]) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);
    cv::Mat image = cv::Mat::zeros(300, 300, CV_8UC3);
    cv::circle(image, cv::Point(150, 150), 100, cv::Scalar(0, 255, 0), -1);

    SDL_SetMainReady();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "Błąd SDL_Init: " << SDL_GetError() << std::endl;
        return -1;
    }

#pragma omp parallel
    {
#pragma omp master
        std::cout << "Liczba dostępnych wątków OpenMP: "
                  << omp_get_num_threads() << std::endl;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Projekt C++ (SDL3 + ImGui + OpenCV + OpenMP)",
        1280,
        720,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Nie udało się utworzyć okna: "
                  << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

    if (!renderer) {
        std::cerr << "Nie udało się utworzyć renderera SDL: "
                  << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    // (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    AppContext ctx{};
    ctx.window = window;
    ctx.renderer = renderer;
    SDL_GetWindowSize(window, &ctx.window_w, &ctx.window_h);

    ctx.background_texture = MatToTexture(renderer, image);

    const SDL_WindowID window_id = SDL_GetWindowID(window);

    bool running = true;

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == window_id) {
                running = false;
            }

            if (event.type == SDL_EVENT_DROP_FILE) {
                if (event.drop.data != nullptr) {
                    ctx.dropped_file_path = std::string(event.drop.data);

                    cv::Mat new_image = cv::imread(ctx.dropped_file_path);

                    if (!new_image.empty()) {
                        DestroyBackgroundTexture(ctx);
                        ctx.background_texture =
                            MatToTexture(renderer, new_image);
                    }
                }
            }
        }

        RenderFrame(ctx);
    }

    DestroyBackgroundTexture(ctx);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}