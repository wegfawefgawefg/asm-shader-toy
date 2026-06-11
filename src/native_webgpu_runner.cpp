#define AST_WEBGPU_FRAME_LIBRARY
// Reuse the headless native WebGPU implementation so channel loading, WGSL
// dispatch, and buffer semantics stay identical while this runner stabilizes.
#include "native_webgpu_frame.cpp"

// clang-format off
#include <SDL_syswm.h>
// clang-format on

#include <chrono>
#include <limits>
#include <thread>

namespace {

struct RunnerArgs {
    Args frame_args;
    int scale = 4;
    int frames = -1;
};

void print_runner_usage() {
    std::cerr << "usage: ast-webgpu-run program.asm [--size gba|240x160] [--scale N] "
                 "[--frames N]\n"
              << "       [--time seconds] [--time-delta seconds] [--max-steps N]\n"
              << "       [--channel0 path] through [--channel3 path] load image inputs\n"
              << "       [--noise0 seed] through [--noise3 seed] load generated noise textures\n"
              << "       [--video0 path] through [--video3 path] sample video inputs with ffmpeg\n"
              << "       [--audio0 path] through [--audio3 path] sample audio inputs with ffmpeg\n"
              << "       [--buffer0 path] through [--buffer3 path] run feedback buffer passes\n";
}

std::optional<RunnerArgs> parse_runner_args(int argc, char** argv) {
    RunnerArgs args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
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
            args.frame_args.width = size->first;
            args.frame_args.height = size->second;
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
        if (arg == "--frames") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frames = std::atoi(std::string{*value}.c_str());
            if (args.frames <= 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--time") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frame_args.time = std::strtof(std::string{*value}.c_str(), nullptr);
            continue;
        }
        if (arg == "--time-delta") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frame_args.time_delta = std::strtof(std::string{*value}.c_str(), nullptr);
            continue;
        }
        if (arg == "--max-steps") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frame_args.max_steps = std::atoi(std::string{*value}.c_str());
            if (args.frame_args.max_steps <= 0) {
                return std::nullopt;
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
            args.frame_args.channel_paths[static_cast<std::size_t>(channel)] = std::string{*value};
            continue;
        }
        if (arg == "--noise0" || arg == "--noise1" || arg == "--noise2" || arg == "--noise3") {
            const int channel = static_cast<int>(arg.back() - '0');
            std::string seed = std::to_string(channel + 1);
            if (i + 1 < argc) {
                const std::string_view possible_seed{argv[i + 1]};
                if (possible_seed.empty() || possible_seed.front() != '-') {
                    ++i;
                    seed = std::string{possible_seed};
                }
            }
            args.frame_args.noise_specs[static_cast<std::size_t>(channel)] = std::move(seed);
            continue;
        }
        if (arg == "--video0" || arg == "--video1" || arg == "--video2" || arg == "--video3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int channel = static_cast<int>(arg.back() - '0');
            args.frame_args.video_paths[static_cast<std::size_t>(channel)] = std::string{*value};
            continue;
        }
        if (arg == "--audio0" || arg == "--audio1" || arg == "--audio2" || arg == "--audio3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int channel = static_cast<int>(arg.back() - '0');
            args.frame_args.audio_paths[static_cast<std::size_t>(channel)] = std::string{*value};
            continue;
        }
        if (arg == "--buffer0" || arg == "--buffer1" || arg == "--buffer2" || arg == "--buffer3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int buffer = static_cast<int>(arg.back() - '0');
            args.frame_args.buffer_paths[static_cast<std::size_t>(buffer)] = std::string{*value};
            continue;
        }
        if (!arg.empty() && arg.front() != '-' && args.frame_args.program_path.empty()) {
            args.frame_args.program_path = std::string{arg};
            continue;
        }
        return std::nullopt;
    }

    if (args.frame_args.program_path.empty()) {
        return std::nullopt;
    }
    return args;
}

std::optional<wgpu::Surface> make_x11_surface(wgpu::Instance instance, SDL_Window* window) {
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE) {
        std::cerr << "SDL_GetWindowWMInfo failed: " << SDL_GetError() << '\n';
        return std::nullopt;
    }

    if (info.subsystem != SDL_SYSWM_X11) {
        std::cerr << "native WebGPU runner currently supports SDL X11 windows only\n";
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

constexpr const char* present_shader_source = R"wgsl(
struct PresentUniforms {
    source_size_scale: vec4<f32>,
};

@group(0) @binding(0) var source_texture: texture_2d<f32>;
@group(0) @binding(1) var<uniform> present_uniforms: PresentUniforms;

struct VertexOut {
    @builtin(position) position: vec4<f32>,
};

@vertex
fn vs(@builtin(vertex_index) vertex_index: u32) -> VertexOut {
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    var out: VertexOut;
    out.position = vec4<f32>(positions[vertex_index], 0.0, 1.0);
    return out;
}

@fragment
fn fs(@builtin(position) position: vec4<f32>) -> @location(0) vec4<f32> {
    let width = i32(present_uniforms.source_size_scale.x);
    let height = i32(present_uniforms.source_size_scale.y);
    let scale = present_uniforms.source_size_scale.z;
    let raw = vec2<i32>(floor(position.xy / vec2<f32>(scale, scale)));
    let coord = clamp(raw, vec2<i32>(0, 0), vec2<i32>(width - 1, height - 1));
    return textureLoad(source_texture, coord, 0);
}
)wgsl";

struct PresentResources {
    wgpu::RenderPipeline pipeline;
    wgpu::BindGroupLayout bind_group_layout;
    wgpu::Buffer uniform;
};

std::optional<PresentResources> make_present_resources(WebGpuContext& context,
                                                       wgpu::TextureFormat format) {
    wgpu::ShaderModule shader = make_shader_module(context.device, present_shader_source);

    std::array<wgpu::BindGroupLayoutEntry, 2> entries{};
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Fragment;
    entries[0].texture.sampleType = wgpu::TextureSampleType::Float;
    entries[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;
    entries[1].binding = 1;
    entries[1].visibility = wgpu::ShaderStage::Fragment;
    entries[1].buffer.type = wgpu::BufferBindingType::Uniform;

    wgpu::BindGroupLayoutDescriptor layout_descriptor{};
    layout_descriptor.entryCount = entries.size();
    layout_descriptor.entries = entries.data();
    wgpu::BindGroupLayout bind_group_layout =
        context.device.createBindGroupLayout(layout_descriptor);
    if (bind_group_layout == WGPUBindGroupLayout(nullptr)) {
        std::cerr << "could not create present bind group layout\n";
        return std::nullopt;
    }

    WGPUBindGroupLayout raw_layout = bind_group_layout;
    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor{};
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &raw_layout;
    wgpu::PipelineLayout pipeline_layout =
        context.device.createPipelineLayout(pipeline_layout_descriptor);

    wgpu::ColorTargetState color_target{};
    color_target.format = format;
    wgpu::FragmentState fragment{};
    fragment.module = shader;
    fragment.entryPoint = "fs";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::RenderPipelineDescriptor pipeline_descriptor{};
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.vertex.module = shader;
    pipeline_descriptor.vertex.entryPoint = "vs";
    pipeline_descriptor.fragment = &fragment;
    pipeline_descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipeline_descriptor.multisample.count = 1;
    wgpu::RenderPipeline pipeline = context.device.createRenderPipeline(pipeline_descriptor);
    if (pipeline == WGPURenderPipeline(nullptr)) {
        std::cerr << "could not create present render pipeline\n";
        return std::nullopt;
    }

    wgpu::BufferDescriptor uniform_descriptor{};
    uniform_descriptor.size = 16;
    uniform_descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer uniform = context.device.createBuffer(uniform_descriptor);
    return PresentResources{pipeline, bind_group_layout, uniform};
}

bool present_texture(WebGpuContext& context, wgpu::Surface surface, PresentResources& present,
                     wgpu::TextureView source_view, const Args& args, int scale) {
    const std::array<float, 4> uniform_values{
        static_cast<float>(args.width),
        static_cast<float>(args.height),
        static_cast<float>(scale),
        0.0F,
    };
    context.queue.writeBuffer(present.uniform, 0, uniform_values.data(),
                              uniform_values.size() * sizeof(float));

    wgpu::SurfaceTexture surface_texture{};
    surface.getCurrentTexture(&surface_texture);
    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success ||
        surface_texture.texture == WGPUTexture(nullptr)) {
        std::cerr << "could not acquire WebGPU surface texture; status "
                  << static_cast<int>(surface_texture.status) << '\n';
        return false;
    }

    wgpu::Texture surface_texture_handle{surface_texture.texture};
    wgpu::TextureView surface_view = surface_texture_handle.createView();

    std::array<wgpu::BindGroupEntry, 2> entries{};
    entries[0].binding = 0;
    entries[0].textureView = source_view;
    entries[1].binding = 1;
    entries[1].buffer = present.uniform;
    entries[1].size = 16;

    wgpu::BindGroupDescriptor bind_descriptor{};
    bind_descriptor.layout = present.bind_group_layout;
    bind_descriptor.entryCount = entries.size();
    bind_descriptor.entries = entries.data();
    wgpu::BindGroup bind_group = context.device.createBindGroup(bind_descriptor);
    if (bind_group == WGPUBindGroup(nullptr)) {
        std::cerr << "could not create present bind group\n";
        return false;
    }

    wgpu::RenderPassColorAttachment attachment{};
    attachment.view = surface_view;
    attachment.loadOp = wgpu::LoadOp::Clear;
    attachment.storeOp = wgpu::StoreOp::Store;
    attachment.clearValue = wgpu::Color{0.0, 0.0, 0.0, 1.0};

    wgpu::RenderPassDescriptor pass_descriptor{};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &attachment;

    wgpu::CommandEncoder encoder = context.device.createCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.beginRenderPass(pass_descriptor);
    pass.setPipeline(present.pipeline);
    pass.setBindGroup(0, bind_group, 0, nullptr);
    pass.draw(3, 1, 0, 0);
    pass.end();
    context.queue.submit(encoder.finish());
    surface.present();
    return true;
}

bool render_present_loop(WebGpuContext& context, wgpu::Surface surface, PresentResources& present,
                         const ast::Program& image_program,
                         const std::array<std::optional<ast::Program>, 4>& buffer_programs,
                         const RunnerArgs& runner_args, const ast::ChannelSet& base_channels) {
    const Args& args = runner_args.frame_args;
    std::array<wgpu::Texture, 4> base_textures = make_base_channel_textures(context, base_channels);
    std::array<wgpu::TextureView, 4> base_views = make_views(base_textures);

    std::array<wgpu::Texture, 4> previous_buffers{};
    std::array<wgpu::Texture, 4> current_buffers{};
    std::array<wgpu::TextureView, 4> previous_buffer_views{};
    std::array<wgpu::TextureView, 4> current_buffer_views{};
    for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
        if (!buffer_programs[i].has_value()) {
            continue;
        }
        previous_buffers[i] = make_zero_render_texture(context, args);
        current_buffers[i] = make_zero_render_texture(context, args);
        previous_buffer_views[i] = previous_buffers[i].createView();
        current_buffer_views[i] = current_buffers[i].createView();
    }

    wgpu::Texture output = make_render_texture(context, args);
    wgpu::TextureView output_view = output.createView();
    const int total_frames =
        runner_args.frames > 0 ? runner_args.frames : std::numeric_limits<int>::max();

    for (int offset = 0; offset < total_frames; ++offset) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                return true;
            }
        }

        const Args frame_args = args_for_frame(args, offset);
        ast::ChannelSet previous_channels = base_channels;
        std::array<wgpu::TextureView, 4> previous_views = base_views;
        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                previous_channels.image[i] = make_buffer_metadata_channel(args.width, args.height);
                previous_views[i] = previous_buffer_views[i];
            }
        }

        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (!buffer_programs[i].has_value()) {
                continue;
            }
            if (!dispatch_program(context, *buffer_programs[i], frame_args, previous_channels,
                                  previous_views, current_buffer_views[i])) {
                return false;
            }
        }

        ast::ChannelSet final_channels = base_channels;
        std::array<wgpu::TextureView, 4> final_views = base_views;
        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                final_channels.image[i] = make_buffer_metadata_channel(args.width, args.height);
                final_views[i] = current_buffer_views[i];
            }
        }

        if (!dispatch_program(context, image_program, frame_args, final_channels, final_views,
                              output_view)) {
            return false;
        }
        if (!present_texture(context, surface, present, output_view, args, runner_args.scale)) {
            return false;
        }

        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                std::swap(previous_buffers[i], current_buffers[i]);
                std::swap(previous_buffer_views[i], current_buffer_views[i]);
            }
        }

        context.device.poll(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::optional<RunnerArgs> parsed = parse_runner_args(argc, argv);
    if (!parsed.has_value()) {
        print_runner_usage();
        return 2;
    }
    const RunnerArgs runner_args = *parsed;

    ast::AssembleResult assembled = ast::assemble_file(runner_args.frame_args.program_path);
    if (!assembled.ok()) {
        print_diagnostics(assembled.diagnostics);
        return 1;
    }

    std::array<std::optional<ast::Program>, 4> buffer_programs;
    if (!assemble_buffer_programs(runner_args.frame_args, buffer_programs)) {
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) == 0) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    ast::ChannelSet channels;
    if (!load_channels(runner_args.frame_args, channels)) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    WebGpuContext context;
    if (!make_context(context)) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    auto error_callback =
        context.device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
            std::cerr << "WebGPU error " << static_cast<int>(type) << ": "
                      << (message != nullptr ? message : "<no message>") << '\n';
        });

    SDL_Window* window =
        SDL_CreateWindow("asm-shader-toy WebGPU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         runner_args.frame_args.width * runner_args.scale,
                         runner_args.frame_args.height * runner_args.scale, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    std::optional<wgpu::Surface> surface = make_x11_surface(context.instance, window);
    if (!surface.has_value()) {
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    const wgpu::TextureFormat format = choose_surface_format(*surface, context.adapter);
    wgpu::SurfaceConfiguration config{};
    config.device = context.device;
    config.format = format;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.width = static_cast<std::uint32_t>(runner_args.frame_args.width * runner_args.scale);
    config.height = static_cast<std::uint32_t>(runner_args.frame_args.height * runner_args.scale);
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Opaque;
    surface->configure(config);

    const std::optional<PresentResources> maybe_present = make_present_resources(context, format);
    if (!maybe_present.has_value()) {
        surface->unconfigure();
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    PresentResources present = *maybe_present;

    const bool ok = render_present_loop(context, *surface, present, assembled.program,
                                        buffer_programs, runner_args, channels);
    surface->unconfigure();
    (void)error_callback;
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    if (!ok) {
        return 1;
    }

    std::cout << "ok: presented GPU frames\n";
    return 0;
}
