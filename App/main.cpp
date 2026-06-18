#include "SDL3/SDL_dialog.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <omp.h>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <utility>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "fast_carver.hpp"
#include "seam_carver.hpp"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_internal.h>

struct LoadedImageResult {
    bool success = false;
    std::string path;
    std::string error;
    cv::Mat image;
    SeamCarver carver;
    FastCarver fast_carver;
    long long elapsed_ms = 0;
};

struct SdlStringDeleter {
    void operator()(char *value) const {
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc): SDL strings must be released with SDL_free.
        SDL_free(value);
    }
};

using SdlStringPtr = std::unique_ptr<char, SdlStringDeleter>;

struct AppContext {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    Uint32 open_image_event = 0;

    int window_w = 0;
    int window_h = 0;

    std::string dropped_file_path = "No file dropped. Drag and drop a file here!";

    SDL_Texture *background_texture = nullptr;
    cv::Mat current_image;
    bool loading_image = false;
    std::string loading_text = "Precomputing image. Please wait...";
    std::future<LoadedImageResult> image_load_task;

    SeamCarver carver;
    FastCarver fast_carver;
    bool use_seam_carving = true;

    Axis current_axis = Axis::HORIZONTAL;
    int target_pct = 100;

    int tex_w = 0;
    int tex_h = 0;
};

SDL_Texture *MatToTexture(SDL_Renderer *renderer, const cv::Mat &mat) {
    if (renderer == nullptr || mat.empty()) {
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

    SDL_Texture *texture =
        SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STATIC, rgbMat.cols, rgbMat.rows);

    if (texture == nullptr) {
        std::cerr << "Nie udało się utworzyć tekstury SDL: " << SDL_GetError() << '\n';
        return nullptr;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);

    const int pitch = static_cast<int>(rgbMat.step);
    if (!SDL_UpdateTexture(texture, nullptr, rgbMat.data, pitch)) {
        std::cerr << "Nie udało się zaktualizować tekstury SDL: " << SDL_GetError() << '\n';
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    return texture;
}

void DestroyBackgroundTexture(AppContext &ctx) {
    if (ctx.background_texture != nullptr) {
        SDL_DestroyTexture(ctx.background_texture);
        ctx.background_texture = nullptr;
    }
}

void ShowDockSpace() {
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("DockSpace", nullptr, window_flags);

    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

    static bool first_frame = true;
    if (first_frame) {
        first_frame = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID center = 0;
        ImGuiID right = 0;

        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.25f, &right, &center);

        ImGui::DockBuilderDockWindow("Image", center);
        ImGui::DockBuilderDockWindow("Control Panel", right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id);

    ImGui::End();
}

LoadedImageResult LoadImageWorker(const std::string &path, Axis axis) {
    LoadedImageResult result{};
    result.path = path;

    cv::Mat new_image = cv::imread(path);

    if (new_image.empty()) {
        result.error = "Failed to load image: " + path;
        return result;
    }

    SeamCarver carver;
    carver.setImage(new_image);

    FastCarver fast_carver;
    const auto start = std::chrono::steady_clock::now();
    fast_carver.precompute(new_image, carver, axis);
    const auto end = std::chrono::steady_clock::now();

    result.success = true;
    result.image = new_image;
    result.carver = carver;
    result.fast_carver = std::move(fast_carver);
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return result;
}

void ApplyLoadedImageResult(AppContext &ctx, LoadedImageResult result) {
    ctx.loading_image = false;

    if (!result.success) {
        ctx.dropped_file_path = result.error;
        return;
    }

    cv::Mat new_image = result.image;

    ctx.dropped_file_path = result.path;
    ctx.current_image = new_image;
    ctx.carver = result.carver;
    ctx.fast_carver = std::move(result.fast_carver);

    std::cout << "Precomputation completed in " << result.elapsed_ms << " ms.\n";

    ctx.target_pct = 100;
    ctx.tex_w = new_image.cols;
    ctx.tex_h = new_image.rows;

    DestroyBackgroundTexture(ctx);
    ctx.background_texture = MatToTexture(ctx.renderer, new_image);
}

void StartImageLoad(AppContext &ctx, const std::string &path) {
    if (ctx.loading_image) {
        return;
    }

    ctx.loading_image = true;
    ctx.loading_text = "Precomputing image. Please wait...";
    ctx.dropped_file_path = "Loading: " + path;
    ctx.image_load_task = std::async(std::launch::async, LoadImageWorker, path, ctx.current_axis);
}

void PollImageLoad(AppContext &ctx) {
    if (!ctx.image_load_task.valid()) {
        return;
    }

    const auto status = ctx.image_load_task.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::ready) {
        ApplyLoadedImageResult(ctx, ctx.image_load_task.get());
    }
}

void SDLCALL OpenImageDialogCallback(void *userdata, const char *const *filelist, int filter) {
    static_cast<void>(filter);

    const auto *ctx = static_cast<const AppContext *>(userdata);
    if (ctx == nullptr || ctx->open_image_event == 0 || filelist == nullptr ||
        filelist[0] == nullptr) {
        return;
    }

    SDL_Event event{};
    event.type = ctx->open_image_event;
    event.user.data1 = SDL_strdup(filelist[0]);

    SDL_PushEvent(&event);
}

void RenderFrame(AppContext &ctx) {
    SDL_GetWindowSize(ctx.window, &ctx.window_w, &ctx.window_h);

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ShowDockSpace();

    ImGui::Begin("Image", nullptr, ImGuiWindowFlags_HorizontalScrollbar);
    if (ctx.loading_image) {
        const char *file_load_text = ctx.loading_text.c_str();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float wrap_width = avail.x * 0.7f;
        ImVec2 text_size = ImGui::CalcTextSize(file_load_text, nullptr, false, wrap_width);

        float start_x = ImGui::GetCursorPosX() + (avail.x - text_size.x) * 0.5f;
        float start_y = ImGui::GetCursorPosY() + (avail.y - text_size.y) * 0.5f;

        ImGui::SetCursorPos(ImVec2(start_x, start_y));
        ImGui::PushTextWrapPos(start_x + wrap_width);

        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", file_load_text);
        ImGui::PopTextWrapPos();
    } else if (ctx.background_texture != nullptr) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float w = ctx.tex_w > 0 ? static_cast<float>(ctx.tex_w) : avail.x;
        float h = ctx.tex_h > 0 ? static_cast<float>(ctx.tex_h) : avail.y;
        auto texture_id = reinterpret_cast<ImTextureID>(ctx.background_texture);

        ImGui::Image(texture_id, ImVec2(w, h));
    } else {
        const char *file_load_text = "Drag and drop an image here, or click the "
                                     "button below to choose a file.";

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float wrap_width = avail.x * 0.7f;
        ImVec2 text_size = ImGui::CalcTextSize(file_load_text, nullptr, false, wrap_width);

        float button_width = 140.0f;
        float button_height = ImGui::GetFrameHeight();
        float spacing = 12.0f;

        float total_height = text_size.y + spacing + button_height;

        float start_x = ImGui::GetCursorPosX() + (avail.x - text_size.x) * 0.5f;
        float start_y = ImGui::GetCursorPosY() + (avail.y - total_height) * 0.5f;

        ImGui::SetCursorPos(ImVec2(start_x, start_y));
        ImGui::PushTextWrapPos(start_x + wrap_width);

        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", file_load_text);

        ImGui::PopTextWrapPos();

        ImGui::Dummy(ImVec2(0.0f, spacing));

        float button_x = ImGui::GetCursorPosX() + (avail.x - button_width) * 0.5f;
        ImGui::SetCursorPosX(button_x);

        if (ImGui::Button("Load file", ImVec2(button_width, 0.0f))) {
            static constexpr std::array<SDL_DialogFileFilter, 2> filters = {{
                {.name = "Images", .pattern = "png;jpg;jpeg;bmp;webp"},
                {.name = "All files", .pattern = "*"},
            }};

            SDL_ShowOpenFileDialog(OpenImageDialogCallback, &ctx, ctx.window, filters.data(),
                                   static_cast<int>(filters.size()), nullptr, false);
        }
    }
    ImGui::End();

    ImGui::Begin("Control Panel");
    ImGui::TextWrapped("Loaded file path:");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", ctx.dropped_file_path.c_str());

    if (!ctx.loading_image && !ctx.current_image.empty()) {
        ImGui::Separator();
        ImGui::Text("Base Size: %dx%d", ctx.current_image.cols, ctx.current_image.rows);
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Precomputed Real-Time Seam Carving");

        int selected_axis = static_cast<int>(ctx.current_axis);
        const int last_axis = selected_axis;
        ImGui::RadioButton("Horizontal Axis (Width)", &selected_axis,
                           static_cast<int>(Axis::HORIZONTAL));
        ImGui::RadioButton("Vertical Axis (Height)", &selected_axis,
                           static_cast<int>(Axis::VERTICAL));
        ctx.current_axis = static_cast<Axis>(selected_axis);

        if (last_axis != static_cast<int>(ctx.current_axis)) {
            std::cout << "Zmieniono oś. Rozpoczynam prekomputację...\n";
            uint64_t start = SDL_GetTicks();
            ctx.fast_carver.precompute(ctx.current_image, ctx.carver, ctx.current_axis);
            uint64_t end = SDL_GetTicks();
            std::cout << "Prekomputacja zakończona w " << (end - start) << " ms.\n";

            ctx.target_pct = 100;
            int max_size = (ctx.current_axis == Axis::HORIZONTAL) ? ctx.current_image.cols
                                                                  : ctx.current_image.rows;
            cv::Mat scaled = ctx.fast_carver.getRealTimeImage(max_size);
            DestroyBackgroundTexture(ctx);
            ctx.background_texture = MatToTexture(ctx.renderer, scaled);
            ctx.tex_w = scaled.cols;
            ctx.tex_h = scaled.rows;
        }

        int max_size = (ctx.current_axis == Axis::HORIZONTAL) ? ctx.current_image.cols
                                                              : ctx.current_image.rows;
        const char *slider_label =
            (ctx.current_axis == Axis::HORIZONTAL) ? "Horizontal Scale (%)" : "Vertical Scale (%)";

        if (ImGui::SliderInt(slider_label, &ctx.target_pct, 1, 200)) {
            int target_size = std::max(1, (max_size * ctx.target_pct) / 100);
            cv::Mat scaled;
            if (ctx.use_seam_carving) {
                scaled = ctx.fast_carver.getRealTimeImage(target_size);
            } else {
                if (ctx.current_axis == Axis::HORIZONTAL) {
                    cv::resize(ctx.current_image, scaled,
                               cv::Size(target_size, ctx.current_image.rows));
                } else {
                    cv::resize(ctx.current_image, scaled,
                               cv::Size(ctx.current_image.cols, target_size));
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

    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ctx.renderer);

    SDL_RenderPresent(ctx.renderer);
}

int main(int argc, char *argv[]) {
    static_cast<void>(argc);
    static_cast<void>(argv);

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    SDL_SetMainReady();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << '\n';
        return -1;
    }

#pragma omp parallel
    {
#pragma omp master
        std::cout << "Number of available OpenMP threads: " << omp_get_num_threads() << '\n';
    }

    SDL_Window *window = SDL_CreateWindow("SeamCarver", 1280, 720, SDL_WINDOW_RESIZABLE);

    if (window == nullptr) {
        std::cerr << "Failed to create window: " << SDL_GetError() << '\n';
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);

    if (renderer == nullptr) {
        std::cerr << "Failed to create SDL renderer: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    AppContext ctx{};
    ctx.window = window;
    ctx.renderer = renderer;
    ctx.open_image_event = SDL_RegisterEvents(1);
    SDL_GetWindowSize(window, &ctx.window_w, &ctx.window_h);

    const SDL_WindowID window_id = SDL_GetWindowID(window);

    bool running = true;

    while (running) {
        SDL_Event event{};

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
                    StartImageLoad(ctx, event.drop.data);
                }
            }

            if (event.type == ctx.open_image_event) {
                SdlStringPtr path(static_cast<char *>(event.user.data1));

                if (path != nullptr) {
                    StartImageLoad(ctx, path.get());
                }
            }
        }

        PollImageLoad(ctx);
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
