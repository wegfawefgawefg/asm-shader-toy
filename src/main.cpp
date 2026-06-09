#include "ast/assembler.hpp"
#include "ast/runtime.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Args {
    std::string program_path = "examples/basics/plasma.asm";
    int width = 240;
    int height = 160;
    int scale = 4;
    int max_steps = 4096;
    int frames = -1;
    bool dry_run = false;
    bool no_graphics = false;
    std::string save_frame_path;
    std::array<std::string, 4> channel_paths;
};

std::optional<std::pair<int, int>> parse_size(std::string_view text) {
    const std::size_t split = text.find('x');
    if (split == std::string_view::npos) {
        return std::nullopt;
    }

    const int width = std::atoi(std::string{text.substr(0, split)}.c_str());
    const int height = std::atoi(std::string{text.substr(split + 1)}.c_str());
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    return std::pair{width, height};
}

std::optional<Args> parse_args(int argc, char** argv) {
    Args args;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto next = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            ++i;
            return std::string_view{argv[i]};
        };

        if (arg == "--help" || arg == "-h") {
            return std::nullopt;
        }
        if (arg == "--size") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto size = parse_size(*value);
            if (!size.has_value()) {
                return std::nullopt;
            }
            args.width = size->first;
            args.height = size->second;
            continue;
        }
        if (arg == "--scale" || arg == "--dimscale") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.scale = std::atoi(std::string{*value}.c_str());
            if (args.scale <= 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--max-steps") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.max_steps = std::atoi(std::string{*value}.c_str());
            if (args.max_steps <= 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--frames") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frames = std::atoi(std::string{*value}.c_str());
            if (args.frames < 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--dry-run" || arg == "--dryrun") {
            args.dry_run = true;
            args.no_graphics = true;
            if (args.frames < 0) {
                args.frames = 0;
            }
            continue;
        }
        if (arg == "--save-frame") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.save_frame_path = std::string{*value};
            args.no_graphics = true;
            if (args.frames < 0) {
                args.frames = 1;
            }
            continue;
        }
        if (arg == "--no-graphics" || arg == "--nographics") {
            args.no_graphics = true;
            if (args.frames < 0) {
                args.frames = 1;
            }
            continue;
        }
        if (arg == "--channel0" || arg == "--channel1" || arg == "--channel2" ||
            arg == "--channel3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int channel = static_cast<int>(arg.back() - '0');
            args.channel_paths[static_cast<std::size_t>(channel)] = std::string{*value};
            continue;
        }
        if (!arg.empty() && arg.front() == '-') {
            return std::nullopt;
        }
        args.program_path = std::string{arg};
    }

    return args;
}

void print_usage() {
    std::cerr << "usage: asm-shader-toy [program.asm] [--size 240x160] [--scale N]\n"
              << "       --dimscale is accepted as an alias for --scale\n"
              << "       --channel0 path through --channel3 path load image inputs\n"
              << "       --dry-run assembles and validates image inputs without rendering\n"
              << "       --no-graphics --frames N renders N CPU frames without a window\n"
              << "       --save-frame path.png renders one headless frame to a PNG\n";
}

bool has_channel_paths(const Args& args) {
    for (const std::string& path : args.channel_paths) {
        if (!path.empty()) {
            return true;
        }
    }
    return false;
}

bool load_channel_image(const std::string& path, ast::ImageChannel& out) {
    SDL_Surface* loaded = IMG_Load(path.c_str());
    if (loaded == nullptr) {
        std::cerr << "IMG_Load failed for " << path << ": " << IMG_GetError() << '\n';
        return false;
    }

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(loaded);
    if (converted == nullptr) {
        std::cerr << "SDL_ConvertSurfaceFormat failed for " << path << ": " << SDL_GetError()
                  << '\n';
        return false;
    }

    out.width = converted->w;
    out.height = converted->h;
    out.pixels.resize(static_cast<std::size_t>(out.width * out.height));
    const auto* bytes = static_cast<const std::uint8_t*>(converted->pixels);
    for (int y = 0; y < out.height; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(
            bytes + static_cast<std::size_t>(y) * static_cast<std::size_t>(converted->pitch));
        for (int x = 0; x < out.width; ++x) {
            out.pixels[static_cast<std::size_t>(y * out.width + x)] =
                row[static_cast<std::size_t>(x)];
        }
    }

    SDL_FreeSurface(converted);
    return true;
}

bool save_frame_png(const std::string& path, int width, int height,
                    std::vector<std::uint32_t>& pixels) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels.data(), width, height, 32, width * static_cast<int>(sizeof(std::uint32_t)),
        SDL_PIXELFORMAT_ABGR8888);
    if (surface == nullptr) {
        std::cerr << "SDL_CreateRGBSurfaceWithFormatFrom failed: " << SDL_GetError() << '\n';
        return false;
    }

    const int result = IMG_SavePNG(surface, path.c_str());
    SDL_FreeSurface(surface);
    if (result != 0) {
        std::cerr << "IMG_SavePNG failed for " << path << ": " << IMG_GetError() << '\n';
        return false;
    }
    return true;
}

ast::FrameInputs make_frame_inputs(const Args& args, const ast::ChannelSet& channels, int frame,
                                   float time, float time_delta, float mouse_x = 0.0F,
                                   float mouse_y = 0.0F, float mouse_down = 0.0F,
                                   float mouse_click_x = 0.0F, float mouse_click_y = 0.0F) {
    const std::time_t wall_time = std::time(nullptr);
    const std::tm* local_time = std::localtime(&wall_time);

    ast::FrameInputs frame_inputs;
    frame_inputs.width = args.width;
    frame_inputs.height = args.height;
    frame_inputs.time = time;
    frame_inputs.time_delta = time_delta;
    frame_inputs.frame = frame;
    frame_inputs.mouse_x = mouse_down != 0.0F ? mouse_x : 0.0F;
    frame_inputs.mouse_y = mouse_down != 0.0F ? mouse_y : 0.0F;
    frame_inputs.mouse_down = mouse_down;
    frame_inputs.mouse_click_x = mouse_click_x;
    frame_inputs.mouse_click_y = mouse_click_y;
    frame_inputs.wall_clock_seconds =
        local_time != nullptr
            ? static_cast<float>((local_time->tm_hour * 60 + local_time->tm_min) * 60 +
                                 local_time->tm_sec)
            : 0.0F;
    frame_inputs.year = local_time != nullptr ? local_time->tm_year + 1900 : 1970;
    frame_inputs.month = local_time != nullptr ? local_time->tm_mon + 1 : 1;
    frame_inputs.day = local_time != nullptr ? local_time->tm_mday : 1;
    frame_inputs.channels = &channels;
    return frame_inputs;
}

} // namespace

int main(int argc, char** argv) {
    const auto parsed_args = parse_args(argc, argv);
    if (!parsed_args.has_value()) {
        print_usage();
        return 2;
    }
    const Args args = *parsed_args;

    ast::AssembleResult assembled = ast::assemble_file(args.program_path);
    if (!assembled.ok()) {
        for (const ast::Diagnostic& diagnostic : assembled.diagnostics) {
            std::cerr << diagnostic.file << ":" << diagnostic.line << ": " << diagnostic.message
                      << '\n';
        }
        return 1;
    }

    if (args.dry_run && !has_channel_paths(args)) {
        std::cout << "ok: assembled " << args.program_path << '\n';
        return 0;
    }

    const std::uint32_t sdl_init_flags =
        args.no_graphics ? static_cast<std::uint32_t>(SDL_INIT_TIMER)
                         : static_cast<std::uint32_t>(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (SDL_Init(sdl_init_flags) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) == 0) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    ast::ChannelSet channels;
    for (std::size_t i = 0; i < args.channel_paths.size(); ++i) {
        if (!args.channel_paths[i].empty() &&
            !load_channel_image(args.channel_paths[i], channels.image[i])) {
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
    }

    if (args.dry_run) {
        std::cout << "ok: assembled " << args.program_path;
        if (has_channel_paths(args)) {
            std::cout << " and loaded image channels";
        }
        std::cout << '\n';
        IMG_Quit();
        SDL_Quit();
        return 0;
    }

    if (args.no_graphics) {
        std::vector<std::uint32_t> pixels;
        const int frame_count = args.frames >= 0 ? args.frames : 1;
        for (int frame = 0; frame < frame_count; ++frame) {
            const float time = static_cast<float>(frame) / 60.0F;
            const ast::FrameInputs frame_inputs =
                make_frame_inputs(args, channels, frame, time, 1.0F / 60.0F);
            ast::render_frame(assembled.program, frame_inputs, pixels,
                              ast::RunLimits{args.max_steps});
        }
        if (!args.save_frame_path.empty() &&
            !save_frame_png(args.save_frame_path, args.width, args.height, pixels)) {
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
        std::cout << "ok: rendered " << frame_count << " frame";
        if (frame_count != 1) {
            std::cout << "s";
        }
        if (!args.save_frame_path.empty()) {
            std::cout << " to " << args.save_frame_path;
        }
        std::cout << " without graphics\n";
        IMG_Quit();
        SDL_Quit();
        return 0;
    }

    SDL_Window* window =
        SDL_CreateWindow("asm-shader-toy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         args.width * args.scale, args.height * args.scale, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                             SDL_TEXTUREACCESS_STREAMING, args.width, args.height);
    if (texture == nullptr) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

    bool running = true;
    std::vector<std::uint32_t> pixels;
    const auto start = std::chrono::steady_clock::now();
    auto previous_frame_time = start;
    int frame = 0;
    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    float mouse_down = 0.0F;
    float mouse_click_x = 0.0F;
    float mouse_click_y = 0.0F;

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                mouse_x = static_cast<float>(event.motion.x) / static_cast<float>(args.scale);
                mouse_y = static_cast<float>(event.motion.y) / static_cast<float>(args.scale);
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                mouse_down = 1.0F;
                mouse_click_x = static_cast<float>(event.button.x) / static_cast<float>(args.scale);
                mouse_click_y = static_cast<float>(event.button.y) / static_cast<float>(args.scale);
                mouse_x = mouse_click_x;
                mouse_y = mouse_click_y;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                mouse_down = 0.0F;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsed = now - start;
        const std::chrono::duration<float> delta = now - previous_frame_time;
        previous_frame_time = now;

        const ast::FrameInputs frame_inputs =
            make_frame_inputs(args, channels, frame, elapsed.count(), delta.count(), mouse_x,
                              mouse_y, mouse_down, mouse_click_x, mouse_click_y);
        ast::render_frame(assembled.program, frame_inputs, pixels, ast::RunLimits{args.max_steps});
        ++frame;
        if (args.frames >= 0 && frame >= args.frames) {
            running = false;
        }

        SDL_UpdateTexture(texture, nullptr, pixels.data(),
                          args.width * static_cast<int>(sizeof(std::uint32_t)));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
