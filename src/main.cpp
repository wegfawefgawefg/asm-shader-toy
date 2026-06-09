#include "ast/assembler.hpp"
#include "ast/runtime.hpp"

#include <SDL.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Args {
    std::string program_path = "examples/plasma.asm";
    int width = 240;
    int height = 160;
    int scale = 4;
    int max_steps = 4096;
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
        if (!arg.empty() && arg.front() == '-') {
            return std::nullopt;
        }
        args.program_path = std::string{arg};
    }

    return args;
}

void print_usage() {
    std::cerr << "usage: asm-shader-toy [program.asm] [--size 240x160] [--scale N]\n"
              << "       --dimscale is accepted as an alias for --scale\n";
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("asm-shader-toy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         args.width * args.scale, args.height * args.scale, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
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
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                             SDL_TEXTUREACCESS_STREAMING, args.width, args.height);
    if (texture == nullptr) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

    bool running = true;
    std::vector<std::uint32_t> pixels;
    const auto start = std::chrono::steady_clock::now();

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsed = now - start;
        ast::render_frame(assembled.program, args.width, args.height, elapsed.count(), pixels,
                          ast::RunLimits{args.max_steps});

        SDL_UpdateTexture(texture, nullptr, pixels.data(),
                          args.width * static_cast<int>(sizeof(std::uint32_t)));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
