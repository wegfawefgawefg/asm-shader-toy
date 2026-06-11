#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "ast/assembler.hpp"
#include "ast/runtime.hpp"
#include "ast/wgsl.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint64_t uniform_float_count = 14 + 16 + 512 + 8 + 4 + 32 + 16;
constexpr std::uint64_t uniform_payload_byte_size = uniform_float_count * sizeof(float);
constexpr std::uint64_t uniform_byte_size = (uniform_payload_byte_size + 15U) & ~std::uint64_t{15};

struct Args {
    std::string program_path;
    std::string output_path;
    int width = 240;
    int height = 160;
    int frame = 0;
    float time = 0.0F;
    float time_delta = 1.0F / 60.0F;
    int max_steps = 4096;
    bool compare_cpu = false;
    int tolerance = 1;
};

struct SizePreset {
    std::string_view name;
    int width = 0;
    int height = 0;
};

constexpr std::array<SizePreset, 18> size_presets{{
    {"gb", 160, 144},
    {"gameboy", 160, 144},
    {"gbc", 160, 144},
    {"gameboycolor", 160, 144},
    {"gba", 240, 160},
    {"nes", 256, 240},
    {"snes", 256, 224},
    {"genesis", 320, 224},
    {"megadrive", 320, 224},
    {"sms", 256, 192},
    {"mastersystem", 256, 192},
    {"n64", 320, 240},
    {"ps1", 320, 240},
    {"psx", 320, 240},
    {"ds", 256, 192},
    {"nds", 256, 192},
    {"psp", 480, 272},
    {"spelunky", 320, 240},
}};

struct WebGpuContext {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
};

std::string lower_ascii(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char ch : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lowered;
}

std::optional<std::pair<int, int>> parse_size(std::string_view text) {
    const std::string lowered = lower_ascii(text);
    for (const SizePreset& preset : size_presets) {
        if (lowered == preset.name) {
            return std::pair{preset.width, preset.height};
        }
    }

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

void print_usage() {
    std::cerr
        << "usage: ast-webgpu-frame program.asm --output frame.ppm [--size gba|240x160]\n"
        << "       [--frame N] [--time seconds] [--max-steps N] [--compare-cpu]\n";
}

std::optional<Args> parse_args(int argc, char** argv) {
    Args args;
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
        if (arg == "--output" || arg == "-o") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.output_path = std::string{*value};
            continue;
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
        if (arg == "--frame") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frame = std::atoi(std::string{*value}.c_str());
            if (args.frame < 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--time") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.time = std::strtof(std::string{*value}.c_str(), nullptr);
            continue;
        }
        if (arg == "--time-delta") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.time_delta = std::strtof(std::string{*value}.c_str(), nullptr);
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
        if (arg == "--compare-cpu") {
            args.compare_cpu = true;
            continue;
        }
        if (arg == "--tolerance") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.tolerance = std::atoi(std::string{*value}.c_str());
            if (args.tolerance < 0 || args.tolerance > 255) {
                return std::nullopt;
            }
            continue;
        }
        if (!arg.empty() && arg.front() != '-' && args.program_path.empty()) {
            args.program_path = std::string{arg};
            continue;
        }
        return std::nullopt;
    }

    if (args.program_path.empty()) {
        return std::nullopt;
    }
    return args;
}

wgpu::ShaderModule make_shader_module(wgpu::Device device, const char* source) {
    wgpu::ShaderModuleWGSLDescriptor wgsl{};
    wgsl.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgsl.code = source;

    wgpu::ShaderModuleDescriptor descriptor{};
    descriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl.chain);
    return device.createShaderModule(descriptor);
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

std::vector<std::uint8_t> map_readback(wgpu::Device device, wgpu::Buffer buffer,
                                       std::uint64_t size) {
    bool mapped = false;
    bool map_failed = false;
    auto map_callback = buffer.mapAsync(wgpu::MapMode::Read, 0, size,
        [&](wgpu::BufferMapAsyncStatus status) {
            mapped = true;
            map_failed = status != wgpu::BufferMapAsyncStatus::Success;
        });
    while (!mapped) {
        device.poll(true);
    }
    if (map_failed) {
        return {};
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    const void* mapped_data = buffer.getConstMappedRange(0, size);
    std::memcpy(bytes.data(), mapped_data, bytes.size());
    buffer.unmap();
    (void)map_callback;
    return bytes;
}

wgpu::Texture make_empty_channel_texture(WebGpuContext& context) {
    wgpu::TextureDescriptor descriptor{wgpu::Default};
    descriptor.size = wgpu::Extent3D(1, 1, 1);
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    wgpu::Texture texture = context.device.createTexture(descriptor);

    const std::array<std::uint8_t, 4> transparent_black{0, 0, 0, 0};
    wgpu::ImageCopyTexture destination{};
    destination.texture = texture;
    wgpu::TextureDataLayout layout{};
    layout.bytesPerRow = 256;
    layout.rowsPerImage = 1;
    context.queue.writeTexture(destination, transparent_black.data(), transparent_black.size(),
                               layout, wgpu::Extent3D(1, 1, 1));
    return texture;
}

wgpu::BindGroupLayout make_asm_bind_group_layout(wgpu::Device device) {
    std::array<wgpu::BindGroupLayoutEntry, 6> entries{};
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Compute;
    entries[0].storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
    entries[0].storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
    entries[0].storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;

    entries[1].binding = 1;
    entries[1].visibility = wgpu::ShaderStage::Compute;
    entries[1].buffer.type = wgpu::BufferBindingType::Uniform;

    for (std::uint32_t i = 0; i < 4; ++i) {
        wgpu::BindGroupLayoutEntry& entry = entries[static_cast<std::size_t>(i + 2U)];
        entry.binding = i + 2U;
        entry.visibility = wgpu::ShaderStage::Compute;
        entry.texture.sampleType = wgpu::TextureSampleType::Float;
        entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;
    }

    wgpu::BindGroupLayoutDescriptor descriptor{};
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    return device.createBindGroupLayout(descriptor);
}

std::vector<float> make_uniforms(const Args& args) {
    std::vector<float> values(static_cast<std::size_t>(uniform_float_count), 0.0F);
    values[0] = args.time;
    values[1] = args.time_delta;
    values[2] = static_cast<float>(args.frame);
    values[3] = static_cast<float>(args.width);
    values[4] = static_cast<float>(args.height);
    values[13] = 1970.0F;
    values[14] = 1.0F;
    values[15] = 1.0F;
    return values;
}

std::vector<std::uint8_t> render_gpu_frame(WebGpuContext& context, const ast::Program& program,
                                           const Args& args) {
    const ast::WgslCompileResult compiled =
        ast::compile_wgsl(program, ast::WgslOptions{args.max_steps, 32});
    if (!compiled.ok()) {
        for (const ast::Diagnostic& diagnostic : compiled.diagnostics) {
            std::cerr << diagnostic.file << ':' << diagnostic.line << ": " << diagnostic.message
                      << '\n';
        }
        return {};
    }

    wgpu::TextureDescriptor output_descriptor{wgpu::Default};
    output_descriptor.size =
        wgpu::Extent3D(static_cast<std::uint32_t>(args.width),
                       static_cast<std::uint32_t>(args.height), 1);
    output_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    output_descriptor.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::CopySrc;
    wgpu::Texture output = context.device.createTexture(output_descriptor);
    wgpu::TextureView output_view = output.createView();

    wgpu::BufferDescriptor uniform_descriptor{};
    uniform_descriptor.size = uniform_byte_size;
    uniform_descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer uniform = context.device.createBuffer(uniform_descriptor);
    const std::vector<float> uniforms = make_uniforms(args);
    context.queue.writeBuffer(uniform, 0, uniforms.data(), uniform_payload_byte_size);

    std::array<wgpu::Texture, 4> channels{
        make_empty_channel_texture(context),
        make_empty_channel_texture(context),
        make_empty_channel_texture(context),
        make_empty_channel_texture(context),
    };
    std::array<wgpu::TextureView, 4> channel_views{
        channels[0].createView(),
        channels[1].createView(),
        channels[2].createView(),
        channels[3].createView(),
    };

    wgpu::BindGroupLayout bind_group_layout = make_asm_bind_group_layout(context.device);
    WGPUBindGroupLayout raw_bind_group_layout = bind_group_layout;
    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor{};
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &raw_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout =
        context.device.createPipelineLayout(pipeline_layout_descriptor);

    wgpu::ComputePipelineDescriptor pipeline_descriptor{};
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.compute.module = make_shader_module(context.device, compiled.source.c_str());
    pipeline_descriptor.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = context.device.createComputePipeline(pipeline_descriptor);
    if (pipeline == WGPUComputePipeline(nullptr)) {
        std::cerr << "could not create emitted asm WGSL compute pipeline\n";
        return {};
    }

    std::array<wgpu::BindGroupEntry, 6> entries{};
    entries[0].binding = 0;
    entries[0].textureView = output_view;
    entries[1].binding = 1;
    entries[1].buffer = uniform;
    entries[1].size = uniform_byte_size;
    for (std::size_t i = 0; i < channel_views.size(); ++i) {
        entries[i + 2].binding = static_cast<std::uint32_t>(i + 2);
        entries[i + 2].textureView = channel_views[i];
    }

    wgpu::BindGroupDescriptor bind_descriptor{};
    bind_descriptor.layout = bind_group_layout;
    bind_descriptor.entryCount = entries.size();
    bind_descriptor.entries = entries.data();
    wgpu::BindGroup bind_group = context.device.createBindGroup(bind_descriptor);
    if (bind_group == WGPUBindGroup(nullptr)) {
        std::cerr << "could not create emitted asm WGSL bind group\n";
        return {};
    }

    constexpr std::uint32_t bytes_per_row_alignment = 256;
    const std::uint32_t unpadded_bytes_per_row = static_cast<std::uint32_t>(args.width) * 4U;
    const std::uint32_t bytes_per_row =
        ((unpadded_bytes_per_row + bytes_per_row_alignment - 1U) / bytes_per_row_alignment) *
        bytes_per_row_alignment;
    const std::uint64_t readback_size =
        static_cast<std::uint64_t>(bytes_per_row) * static_cast<std::uint64_t>(args.height);
    wgpu::BufferDescriptor readback_descriptor{};
    readback_descriptor.size = readback_size;
    readback_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer readback = context.device.createBuffer(readback_descriptor);

    wgpu::CommandEncoder encoder = context.device.createCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bind_group, 0, nullptr);
    pass.dispatchWorkgroups((static_cast<std::uint32_t>(args.width) + 7U) / 8U,
                            (static_cast<std::uint32_t>(args.height) + 7U) / 8U, 1);
    pass.end();

    wgpu::ImageCopyTexture source{};
    source.texture = output;
    wgpu::ImageCopyBuffer destination{};
    destination.buffer = readback;
    destination.layout.bytesPerRow = bytes_per_row;
    destination.layout.rowsPerImage = static_cast<std::uint32_t>(args.height);
    encoder.copyTextureToBuffer(source, destination,
                                wgpu::Extent3D(static_cast<std::uint32_t>(args.width),
                                               static_cast<std::uint32_t>(args.height), 1));
    context.queue.submit(encoder.finish());

    const std::vector<std::uint8_t> padded =
        map_readback(context.device, readback, readback_size);
    if (padded.size() != readback_size) {
        std::cerr << "WebGPU texture readback map failed\n";
        return {};
    }

    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(args.width * args.height * 4));
    for (int y = 0; y < args.height; ++y) {
        const std::uint8_t* src =
            padded.data() + static_cast<std::size_t>(y) * bytes_per_row;
        std::uint8_t* dst = rgba.data() + static_cast<std::size_t>(y * args.width) * 4U;
        std::memcpy(dst, src, static_cast<std::size_t>(unpadded_bytes_per_row));
    }
    return rgba;
}

bool write_ppm(const std::string& path, int width, int height,
               const std::vector<std::uint8_t>& rgba) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "could not open output file: " << path << '\n';
        return false;
    }
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y * width + x) * 4U;
            const std::array<char, 3> rgb{
                static_cast<char>(rgba[offset + 0U]),
                static_cast<char>(rgba[offset + 1U]),
                static_cast<char>(rgba[offset + 2U]),
            };
            out.write(rgb.data(), rgb.size());
        }
    }
    return static_cast<bool>(out);
}

std::vector<std::uint8_t> render_cpu_frame(const ast::Program& program, const Args& args) {
    ast::FrameInputs inputs;
    inputs.width = args.width;
    inputs.height = args.height;
    inputs.time = args.time;
    inputs.time_delta = args.time_delta;
    inputs.frame = args.frame;
    inputs.year = 1970;
    inputs.month = 1;
    inputs.day = 1;

    std::vector<std::uint32_t> packed;
    ast::render_frame(program, inputs, packed, ast::RunLimits{args.max_steps, 32});

    std::vector<std::uint8_t> rgba(packed.size() * 4U);
    for (std::size_t i = 0; i < packed.size(); ++i) {
        const std::uint32_t pixel = packed[i];
        rgba[i * 4U + 0U] = static_cast<std::uint8_t>(pixel & 0xFFU);
        rgba[i * 4U + 1U] = static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU);
        rgba[i * 4U + 2U] = static_cast<std::uint8_t>((pixel >> 16U) & 0xFFU);
        rgba[i * 4U + 3U] = static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU);
    }
    return rgba;
}

bool compare_frames(const std::vector<std::uint8_t>& gpu, const std::vector<std::uint8_t>& cpu,
                    int tolerance) {
    if (gpu.size() != cpu.size()) {
        std::cerr << "CPU/GPU frame sizes differ\n";
        return false;
    }

    std::size_t mismatches = 0;
    int worst = 0;
    std::size_t worst_offset = 0;
    for (std::size_t i = 0; i < gpu.size(); ++i) {
        const int delta = std::abs(static_cast<int>(gpu[i]) - static_cast<int>(cpu[i]));
        if (delta > tolerance) {
            ++mismatches;
            if (delta > worst) {
                worst = delta;
                worst_offset = i;
            }
        }
    }

    if (mismatches == 0) {
        std::cout << "CPU/GPU comparison passed within tolerance " << tolerance << '\n';
        return true;
    }

    const std::size_t pixel = worst_offset / 4U;
    const std::size_t channel = worst_offset % 4U;
    std::cerr << "CPU/GPU comparison failed: " << mismatches << " byte mismatches, worst delta "
              << worst << " at pixel " << pixel << " channel " << channel << " (gpu "
              << static_cast<int>(gpu[worst_offset]) << ", cpu "
              << static_cast<int>(cpu[worst_offset]) << ")\n";
    return false;
}

void print_diagnostics(const std::vector<ast::Diagnostic>& diagnostics) {
    for (const ast::Diagnostic& diagnostic : diagnostics) {
        std::cerr << diagnostic.file << ':' << diagnostic.line << ": " << diagnostic.message
                  << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::optional<Args> parsed = parse_args(argc, argv);
    if (!parsed.has_value()) {
        print_usage();
        return 2;
    }
    const Args args = *parsed;

    ast::AssembleResult assembled = ast::assemble_file(args.program_path);
    if (!assembled.ok()) {
        print_diagnostics(assembled.diagnostics);
        return 1;
    }

    WebGpuContext context;
    if (!make_context(context)) {
        return 1;
    }

    auto error_callback = context.device.setUncapturedErrorCallback(
        [](wgpu::ErrorType type, char const* message) {
            std::cerr << "WebGPU error " << static_cast<int>(type) << ": "
                      << (message != nullptr ? message : "<no message>") << '\n';
        });

    const std::vector<std::uint8_t> gpu = render_gpu_frame(context, assembled.program, args);
    if (gpu.empty()) {
        return 1;
    }

    if (args.compare_cpu) {
        const std::vector<std::uint8_t> cpu = render_cpu_frame(assembled.program, args);
        if (!compare_frames(gpu, cpu, args.tolerance)) {
            return 1;
        }
    }

    if (!args.output_path.empty() && !write_ppm(args.output_path, args.width, args.height, gpu)) {
        return 1;
    }

    (void)error_callback;
    std::cout << "ok: rendered GPU frame";
    if (!args.output_path.empty()) {
        std::cout << " to " << args.output_path;
    }
    std::cout << '\n';
    return 0;
}
