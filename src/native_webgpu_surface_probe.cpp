#define WEBGPU_CPP_IMPLEMENTATION
// clang-format off
#include <webgpu/webgpu.hpp>

#include <SDL.h>
#include <SDL_syswm.h>
// clang-format on

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

namespace {

struct Args {
    int width = 640;
    int height = 360;
    int frames = 60;
    int scale = 1;
};

struct WebGpuContext {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
};

std::optional<std::pair<int, int>> parse_size(const std::string& text) {
    const std::size_t split = text.find('x');
    if (split == std::string::npos) {
        return std::nullopt;
    }
    const int width = std::atoi(text.substr(0, split).c_str());
    const int height = std::atoi(text.substr(split + 1).c_str());
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    return std::pair{width, height};
}

void print_usage() {
    std::cerr << "usage: ast-webgpu-surface-probe [--size 640x360] [--frames N] "
                 "[--scale N]\n";
}

std::optional<Args> parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg{argv[i]};
        auto next = [&]() -> std::optional<std::string> {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            ++i;
            return std::string{argv[i]};
        };

        if (arg == "--help" || arg == "-h") {
            return std::nullopt;
        }
        if (arg == "--size") {
            const std::optional<std::string> value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const std::optional<std::pair<int, int>> size = parse_size(*value);
            if (!size.has_value()) {
                return std::nullopt;
            }
            args.width = size->first;
            args.height = size->second;
            continue;
        }
        if (arg == "--frames") {
            const std::optional<std::string> value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frames = std::atoi(value->c_str());
            if (args.frames <= 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--scale") {
            const std::optional<std::string> value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.scale = std::atoi(value->c_str());
            if (args.scale <= 0) {
                return std::nullopt;
            }
            continue;
        }
        return std::nullopt;
    }
    return args;
}

bool make_context(WebGpuContext& out) {
    wgpu::InstanceDescriptor instance_descriptor{};
    out.instance = wgpu::createInstance(instance_descriptor);
    if (out.instance == WGPUInstance(nullptr)) {
        std::cerr << "could not create WebGPU instance\n";
        return false;
    }

    wgpu::RequestAdapterOptions adapter_options{};
    out.adapter = out.instance.requestAdapter(adapter_options);
    if (out.adapter == WGPUAdapter(nullptr)) {
        std::cerr << "could not request WebGPU adapter\n";
        return false;
    }

    wgpu::DeviceDescriptor device_descriptor{};
    out.device = out.adapter.requestDevice(device_descriptor);
    if (out.device == WGPUDevice(nullptr)) {
        std::cerr << "could not request WebGPU device\n";
        return false;
    }
    out.queue = out.device.getQueue();
    return true;
}

std::optional<wgpu::Surface> make_x11_surface(wgpu::Instance instance, SDL_Window* window) {
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE) {
        std::cerr << "SDL_GetWindowWMInfo failed: " << SDL_GetError() << '\n';
        return std::nullopt;
    }

    if (info.subsystem != SDL_SYSWM_X11) {
        std::cerr << "native WebGPU surface probe currently supports SDL X11 windows only\n";
        return std::nullopt;
    }

    wgpu::SurfaceDescriptorFromXlibWindow xlib_descriptor{};
    xlib_descriptor.chain.sType = wgpu::SType::SurfaceDescriptorFromXlibWindow;
    xlib_descriptor.display = info.info.x11.display;
    xlib_descriptor.window = static_cast<std::uint64_t>(info.info.x11.window);

    wgpu::SurfaceDescriptor descriptor{};
    descriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&xlib_descriptor.chain);
    wgpu::Surface surface = instance.createSurface(descriptor);
    if (surface == WGPUSurface(nullptr)) {
        std::cerr << "could not create WebGPU surface from SDL X11 window\n";
        return std::nullopt;
    }
    return surface;
}

wgpu::TextureFormat choose_surface_format(wgpu::Surface surface, wgpu::Adapter adapter) {
    const wgpu::TextureFormat preferred = surface.getPreferredFormat(adapter);
    if (preferred != wgpu::TextureFormat::Undefined) {
        return preferred;
    }

    wgpu::SurfaceCapabilities capabilities{};
    surface.getCapabilities(adapter, &capabilities);
    const wgpu::TextureFormat fallback =
        capabilities.formatCount > 0 ? capabilities.formats[0] : wgpu::TextureFormat::BGRA8Unorm;
    capabilities.freeMembers();
    return fallback;
}

bool render_surface_frames(WebGpuContext& context, wgpu::Surface surface, const Args& args) {
    const wgpu::TextureFormat format = choose_surface_format(surface, context.adapter);

    wgpu::SurfaceConfiguration config{};
    config.device = context.device;
    config.format = format;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.width = static_cast<std::uint32_t>(args.width * args.scale);
    config.height = static_cast<std::uint32_t>(args.height * args.scale);
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Opaque;
    surface.configure(config);

    for (int frame = 0; frame < args.frames; ++frame) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                surface.unconfigure();
                return true;
            }
        }

        wgpu::SurfaceTexture surface_texture{};
        surface.getCurrentTexture(&surface_texture);
        if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success ||
            surface_texture.texture == WGPUTexture(nullptr)) {
            std::cerr << "could not acquire WebGPU surface texture; status "
                      << static_cast<int>(surface_texture.status) << '\n';
            surface.unconfigure();
            return false;
        }

        wgpu::Texture surface_texture_handle{surface_texture.texture};
        wgpu::TextureView view = surface_texture_handle.createView();
        const float phase = static_cast<float>(frame % std::max(1, args.frames)) /
                            static_cast<float>(std::max(1, args.frames));
        wgpu::RenderPassColorAttachment attachment{};
        attachment.view = view;
        attachment.loadOp = wgpu::LoadOp::Clear;
        attachment.storeOp = wgpu::StoreOp::Store;
        attachment.clearValue = wgpu::Color{0.05 + 0.35 * phase, 0.12, 0.28, 1.0};

        wgpu::RenderPassDescriptor pass_descriptor{};
        pass_descriptor.colorAttachmentCount = 1;
        pass_descriptor.colorAttachments = &attachment;

        wgpu::CommandEncoder encoder = context.device.createCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.beginRenderPass(pass_descriptor);
        pass.end();
        context.queue.submit(encoder.finish());
        surface.present();
        context.device.poll(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    surface.unconfigure();
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::optional<Args> parsed = parse_args(argc, argv);
    if (!parsed.has_value()) {
        print_usage();
        return 2;
    }
    const Args args = *parsed;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "asm-shader-toy WebGPU surface probe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        args.width * args.scale, args.height * args.scale, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    WebGpuContext context;
    if (!make_context(context)) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    auto error_callback =
        context.device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
            std::cerr << "WebGPU error " << static_cast<int>(type) << ": "
                      << (message != nullptr ? message : "<no message>") << '\n';
        });

    const std::optional<wgpu::Surface> surface = make_x11_surface(context.instance, window);
    if (!surface.has_value()) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const bool ok = render_surface_frames(context, *surface, args);
    (void)error_callback;
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (!ok) {
        return 1;
    }

    std::cout << "native WebGPU SDL surface smoke passed\n";
    return 0;
}
