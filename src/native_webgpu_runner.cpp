#define AST_WEBGPU_FRAME_LIBRARY
// Reuse the headless native WebGPU implementation so channel loading, WGSL
// dispatch, and buffer semantics stay identical while this runner stabilizes.
#include "native_webgpu_frame.cpp"

// clang-format off
#include <SDL_syswm.h>
// clang-format on

#include <atomic>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <limits>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {

struct RunnerArgs {
    Args frame_args;
    int scale = 4;
    int frames = -1;
    std::array<std::string, 4> webcam_paths;
};

struct WebcamChannel {
    int width = 320;
    int height = 240;
    FILE* pipe = nullptr;
    std::vector<std::uint8_t> pending_frame;
    std::vector<std::uint32_t> pixels;
    std::size_t pending_bytes = 0;
    std::mutex mutex;
    std::thread worker;
    std::atomic_bool running{false};
    std::atomic_bool failed{false};
    std::atomic_bool has_frame{false};

    WebcamChannel() = default;
    WebcamChannel(const WebcamChannel&) = delete;
    WebcamChannel& operator=(const WebcamChannel&) = delete;

    ~WebcamChannel() {
        close();
    }

    [[nodiscard]] bool loaded() const {
        return pipe != nullptr && width > 0 && height > 0;
    }

    void close() {
        running = false;
        if (worker.joinable()) {
            worker.join();
        }
        if (pipe != nullptr) {
            pclose(pipe);
            pipe = nullptr;
        }
    }
};

void print_runner_usage() {
    std::cerr << "usage: ast-webgpu-run program.asm [--size gba|240x160] [--scale N] "
                 "[--frames N]\n"
              << "       [--time seconds] [--time-delta seconds] [--max-steps N]\n"
              << "       [--channel0 path] through [--channel3 path] load image inputs\n"
              << "       [--noise0 seed] through [--noise3 seed] load generated noise textures\n"
              << "       [--video0 path] through [--video3 path] sample video inputs with ffmpeg\n"
              << "       [--webcam0 [device]] through [--webcam3 [device]] stream webcam inputs\n"
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
        if (arg == "--webcam0" || arg == "--webcam1" || arg == "--webcam2" || arg == "--webcam3") {
            const int channel = static_cast<int>(arg.back() - '0');
            std::string device = "/dev/video" + std::to_string(channel);
            if (i + 1 < argc) {
                const std::string_view possible_device{argv[i + 1]};
                if (possible_device.empty() || possible_device.front() != '-') {
                    ++i;
                    device = std::string{possible_device};
                }
            }
            args.webcam_paths[static_cast<std::size_t>(channel)] = std::move(device);
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

bool has_webcam_paths(const RunnerArgs& args) {
    for (const std::string& path : args.webcam_paths) {
        if (!path.empty()) {
            return true;
        }
    }
    return false;
}

bool set_pipe_nonblocking(FILE* pipe) {
    const int fd = fileno(pipe);
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void pack_webcam_frame(WebcamChannel& webcam) {
    std::vector<std::uint32_t> packed;
    packed.resize(static_cast<std::size_t>(webcam.width) * static_cast<std::size_t>(webcam.height));
    for (int y = 0; y < webcam.height; ++y) {
        for (int x = 0; x < webcam.width; ++x) {
            const auto src_pixel =
                static_cast<std::size_t>(y * webcam.width + (webcam.width - 1 - x));
            const auto dst_pixel = static_cast<std::size_t>(y * webcam.width + x);
            const std::uint8_t r = webcam.pending_frame[src_pixel * 4U + 0U];
            const std::uint8_t g = webcam.pending_frame[src_pixel * 4U + 1U];
            const std::uint8_t b = webcam.pending_frame[src_pixel * 4U + 2U];
            const std::uint8_t a = webcam.pending_frame[src_pixel * 4U + 3U];
            packed[dst_pixel] =
                (static_cast<std::uint32_t>(a) << 24U) | (static_cast<std::uint32_t>(b) << 16U) |
                (static_cast<std::uint32_t>(g) << 8U) | static_cast<std::uint32_t>(r);
        }
    }

    const std::lock_guard<std::mutex> lock(webcam.mutex);
    webcam.pixels = std::move(packed);
    webcam.has_frame = true;
}

bool read_webcam_initial_frame(WebcamChannel& webcam) {
    if (webcam.pipe == nullptr) {
        return false;
    }

    const std::size_t frame_bytes =
        static_cast<std::size_t>(webcam.width) * static_cast<std::size_t>(webcam.height) * 4U;
    webcam.pending_frame.resize(frame_bytes);
    webcam.pending_bytes = 0;

    const int fd = fileno(webcam.pipe);
    while (webcam.pending_bytes < frame_bytes) {
        const ssize_t bytes_read = read(fd, webcam.pending_frame.data() + webcam.pending_bytes,
                                        frame_bytes - webcam.pending_bytes);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        webcam.pending_bytes += static_cast<std::size_t>(bytes_read);
    }

    pack_webcam_frame(webcam);
    webcam.pending_bytes = 0;
    return true;
}

bool drain_webcam_frame(WebcamChannel& webcam);

bool open_webcam_channel(const std::string& device, WebcamChannel& out) {
    out.close();
    out.width = 320;
    out.height = 240;
    const std::string command =
        "ffmpeg -v error -fflags nobuffer -flags low_delay -f v4l2 -framerate 30 "
        "-video_size 320x240 -i " +
        shell_quote(device) + " -f rawvideo -pix_fmt rgba - 2>/dev/null";
    out.pipe = popen(command.c_str(), "r");
    if (out.pipe == nullptr) {
        std::cerr << "failed to start webcam capture for " << device << '\n';
        return false;
    }

    if (!read_webcam_initial_frame(out)) {
        std::cerr << "failed to read webcam frame from " << device << '\n';
        pclose(out.pipe);
        out.pipe = nullptr;
        return false;
    }
    if (!set_pipe_nonblocking(out.pipe)) {
        std::cerr << "failed to set webcam pipe nonblocking for " << device << '\n';
        pclose(out.pipe);
        out.pipe = nullptr;
        return false;
    }
    out.failed = false;
    out.running = true;
    out.worker = std::thread([&out]() {
        while (out.running) {
            if (!drain_webcam_frame(out)) {
                out.failed = true;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
    });
    return true;
}

bool drain_webcam_frame(WebcamChannel& webcam) {
    if (webcam.pipe == nullptr) {
        return false;
    }

    const std::size_t frame_bytes =
        static_cast<std::size_t>(webcam.width) * static_cast<std::size_t>(webcam.height) * 4U;
    webcam.pending_frame.resize(frame_bytes);
    const int fd = fileno(webcam.pipe);
    bool got_complete_frame = false;

    while (true) {
        const ssize_t bytes_read = read(fd, webcam.pending_frame.data() + webcam.pending_bytes,
                                        frame_bytes - webcam.pending_bytes);
        if (bytes_read > 0) {
            webcam.pending_bytes += static_cast<std::size_t>(bytes_read);
            if (webcam.pending_bytes == frame_bytes) {
                pack_webcam_frame(webcam);
                webcam.pending_bytes = 0;
                got_complete_frame = true;
            }
            continue;
        }

        if (bytes_read == 0) {
            return false;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }
        return false;
    }

    return got_complete_frame || webcam.has_frame;
}

bool update_webcam_channels(ast::ChannelSet& channels,
                            std::array<WebcamChannel, 4>& webcam_channels, float time) {
    for (std::size_t i = 0; i < webcam_channels.size(); ++i) {
        WebcamChannel& webcam = webcam_channels[i];
        if (webcam.pipe == nullptr) {
            continue;
        }
        if (webcam.failed) {
            std::cerr << "webcam stream ended on channel " << i << '\n';
            return false;
        }
        channels.image[i].width = webcam.width;
        channels.image[i].height = webcam.height;
        channels.image[i].time = time;
        channels.image[i].sample_rate = 0.0F;
        channels.image[i].external_pixels = nullptr;
        {
            const std::lock_guard<std::mutex> lock(webcam.mutex);
            channels.image[i].pixels = webcam.pixels;
        }
    }
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

void write_channel_texture(WebGpuContext& context, wgpu::Texture texture,
                           const ast::ImageChannel& channel) {
    if (!channel.loaded()) {
        return;
    }

    constexpr std::uint32_t bytes_per_row_alignment = 256;
    const std::uint32_t unpadded_bytes_per_row = static_cast<std::uint32_t>(channel.width) * 4U;
    const std::uint32_t bytes_per_row = align_to(unpadded_bytes_per_row, bytes_per_row_alignment);
    std::vector<std::uint8_t> padded(static_cast<std::size_t>(bytes_per_row) *
                                     static_cast<std::size_t>(channel.height));
    const std::vector<std::uint32_t>& pixels = channel.pixel_data();
    for (int y = 0; y < channel.height; ++y) {
        std::uint8_t* dst = padded.data() + static_cast<std::size_t>(y) * bytes_per_row;
        const auto* src = reinterpret_cast<const std::uint8_t*>(
            pixels.data() + static_cast<std::size_t>(y * channel.width));
        std::memcpy(dst, src, static_cast<std::size_t>(unpadded_bytes_per_row));
    }

    wgpu::ImageCopyTexture destination{};
    destination.texture = texture;
    wgpu::TextureDataLayout layout{};
    layout.bytesPerRow = bytes_per_row;
    layout.rowsPerImage = static_cast<std::uint32_t>(channel.height);
    context.queue.writeTexture(destination, padded.data(), padded.size(), layout,
                               wgpu::Extent3D(static_cast<std::uint32_t>(channel.width),
                                              static_cast<std::uint32_t>(channel.height), 1));
}

void upload_webcam_channels(WebGpuContext& context, const RunnerArgs& runner_args,
                            const ast::ChannelSet& channels,
                            std::array<wgpu::Texture, 4>& textures) {
    for (std::size_t i = 0; i < runner_args.webcam_paths.size(); ++i) {
        if (!runner_args.webcam_paths[i].empty()) {
            write_channel_texture(context, textures[i], channels.image[i]);
        }
    }
}

bool render_present_loop(WebGpuContext& context, wgpu::Surface surface, PresentResources& present,
                         const ast::Program& image_program,
                         const std::array<std::optional<ast::Program>, 4>& buffer_programs,
                         const RunnerArgs& runner_args, ast::ChannelSet base_channels,
                         std::array<WebcamChannel, 4>& webcam_channels) {
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
        if (has_webcam_paths(runner_args)) {
            if (!update_webcam_channels(base_channels, webcam_channels, frame_args.time)) {
                return false;
            }
            upload_webcam_channels(context, runner_args, base_channels, base_textures);
        }

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

    std::array<WebcamChannel, 4> webcam_channels;
    for (std::size_t i = 0; i < runner_args.webcam_paths.size(); ++i) {
        if (!runner_args.webcam_paths[i].empty() &&
            !open_webcam_channel(runner_args.webcam_paths[i], webcam_channels[i])) {
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
    }
    if (!update_webcam_channels(channels, webcam_channels, runner_args.frame_args.time)) {
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
                                        buffer_programs, runner_args, channels, webcam_channels);
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
