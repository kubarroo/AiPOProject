#include <iostream>
#include <string>
#include <omp.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include "seam_carver.hpp"
#include "fast_carver.hpp"

struct AppContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    int window_w = 0;
    int window_h = 0;

    std::string dropped_file_path =
        "Brak upuszczonego pliku. Przeciągnij plik tutaj!";

    SDL_Texture* background_texture = nullptr;
    cv::Mat current_image;
    
    SeamCarver carver;
    FastCarver fast_carver;
    bool use_seam_carving = true;
    
    Axis current_axis = Axis::HORIZONTAL;
    int target_pct = 100;
    
    int tex_w = 0;
    int tex_h = 0;
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

void ShowDockSpace()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("DockSpace", nullptr, window_flags);

    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

    static bool first_frame = true;
    if (first_frame)
    {
        first_frame = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID center;
        ImGuiID right;

        ImGui::DockBuilderSplitNode(
            dockspace_id,
            ImGuiDir_Right,
            0.25f,
            &right,
            &center
        );

        ImGui::DockBuilderDockWindow("Obraz", center);
        ImGui::DockBuilderDockWindow("Panel Sterowania", right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id);

    ImGui::End();
}

void RenderFrame(AppContext& ctx) {
    SDL_GetWindowSize(ctx.window, &ctx.window_w, &ctx.window_h);

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ShowDockSpace();

    ImGui::Begin("Obraz");
    if (ctx.background_texture) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float w = ctx.tex_w > 0 ? (float)ctx.tex_w : avail.x;
        float h = ctx.tex_h > 0 ? (float)ctx.tex_h : avail.y;

        ImGui::Image(
            (ImTextureID)(intptr_t)ctx.background_texture,
            ImVec2(w, h)
        );
    }
    ImGui::End();

    ImGui::Begin("Panel Sterowania");
    ImGui::TextWrapped("Ścieżka do ostatnio upuszczonego pliku:");
    ImGui::Separator();
    ImGui::TextColored(
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
        "%s",
        ctx.dropped_file_path.c_str()
    );
    
    if (!ctx.current_image.empty()) {
        ImGui::Separator();
        ImGui::Text("Rozmiar bazowy: %dx%d", ctx.current_image.cols, ctx.current_image.rows);
        ImGui::TextColored(ImVec4(1,1,0,1), "Precomputed Real-Time Seam Carving");
        
        int last_axis = (int)ctx.current_axis;
        ImGui::RadioButton("Oś Pozioma (Szerokość)", (int*)&ctx.current_axis, (int)Axis::HORIZONTAL);
        ImGui::RadioButton("Oś Pionowa (Wysokość)", (int*)&ctx.current_axis, (int)Axis::VERTICAL);
        
        if (last_axis != (int)ctx.current_axis) {
            std::cout << "Zmieniono oś. Rozpoczynam prekomputację..." << std::endl;
            uint64_t start = SDL_GetTicks();
            ctx.fast_carver.precompute(ctx.current_image, ctx.carver, ctx.current_axis);
            uint64_t end = SDL_GetTicks();
            std::cout << "Prekomputacja zakończona w " << (end - start) << " ms." << std::endl;
            
            ctx.target_pct = 100;
            int max_size = (ctx.current_axis == Axis::HORIZONTAL) ? ctx.current_image.cols : ctx.current_image.rows;
            cv::Mat scaled = ctx.fast_carver.getRealTimeImage(max_size);
            DestroyBackgroundTexture(ctx);
            ctx.background_texture = MatToTexture(ctx.renderer, scaled);
            ctx.tex_w = scaled.cols;
            ctx.tex_h = scaled.rows;
        }
        
        int max_size = (ctx.current_axis == Axis::HORIZONTAL) ? ctx.current_image.cols : ctx.current_image.rows;
        const char* slider_label = (ctx.current_axis == Axis::HORIZONTAL) ? "Rozmiar poziomy (%)" : "Rozmiar pionowy (%)";
        
        if (ImGui::SliderInt(slider_label, &ctx.target_pct, 1, 200)) {
            int target_size = std::max(1, (max_size * ctx.target_pct) / 100);
            cv::Mat scaled;
            if (ctx.use_seam_carving) {
                scaled = ctx.fast_carver.getRealTimeImage(target_size);
            } else {
                if (ctx.current_axis == Axis::HORIZONTAL) {
                    cv::resize(ctx.current_image, scaled, cv::Size(target_size, ctx.current_image.rows));
                } else {
                    cv::resize(ctx.current_image, scaled, cv::Size(ctx.current_image.cols, target_size));
                }
            }
            DestroyBackgroundTexture(ctx);
            ctx.background_texture = MatToTexture(ctx.renderer, scaled);
            ctx.tex_w = scaled.cols;
            ctx.tex_h = scaled.rows;
        }
    }
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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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
                        ctx.current_image = new_image;
                        ctx.carver.setImage(new_image);
                        
                        std::cout << "Rozpoczynam prekomputację wszystkich szwów. Proszę czekać..." << std::endl;
                        uint64_t start = SDL_GetTicks();
                        ctx.fast_carver.precompute(new_image, ctx.carver, ctx.current_axis);
                        uint64_t end = SDL_GetTicks();
                        std::cout << "Prekomputacja zakończona w " << (end - start) << " ms." << std::endl;
                        
                        ctx.target_pct = 100;
                        ctx.tex_w = new_image.cols;
                        ctx.tex_h = new_image.rows;
                        
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